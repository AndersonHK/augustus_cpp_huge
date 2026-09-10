#pragma once

#include "building/building.h"
#include "building/granary.h"
#include "building/storage.h"
#include "building/warehouse.h"
#include "city/data_private.h"
#include "city/resource.h"
#include "city/trade_ledger.h"
#include "scenario/property.h"

#include <cstdio>
#include <vector>

inline bool run_food_production_accounting_test()
{
    std::vector<uint8_t> saved_ledger(65536);
    buffer ledger;
    for (;;) {
        buffer_init(&ledger, saved_ledger.data(), saved_ledger.size());
        city_trade_ledger_save(&ledger);
        if (!ledger.overflow) break;
        if (saved_ledger.size() >= 64 * 1024 * 1024) return false;
        saved_ledger.resize(saved_ledger.size() * 2);
    }
    const auto resource_state = city_data.resource;
    const int before = resource_state.food_produced_this_month;
    city_trade_ledger_produced(resource_wheat(), 20);
    city_trade_ledger_produced(resource_fish(), 100);
    city_trade_ledger_produced(resource_wine(), 100);
    bool passed = city_data.resource.food_produced_this_month == before + 120;
    if (Building *granary = building_granary_first()) {
        const int free_space = granary->resource_amount(RESOURCE_NONE);
        const int wheat = granary->resource_amount(resource_wheat());
        granary->set_resource_amount(RESOURCE_NONE, 1);
        const int delivered = building_granary_try_add_resource(*granary, resource_wheat(), 1, 1, 0);
        if (delivered != 1 || city_data.resource.food_produced_this_month != before + 120) passed = false;
        granary->set_resource_amount(RESOURCE_NONE, free_space);
        granary->set_resource_amount(resource_wheat(), wheat);
    }
    buffer_reset(&ledger);
    city_trade_ledger_load(&ledger);
    city_data.resource = resource_state;
    fprintf(passed ? stdout : stderr, "Food production accounting %s: harvests and landed fish count once; transport does not produce food.\n", passed ? "passed" : "FAILED");
    return passed;
}

inline bool run_food_stock_test()
{
    if (!run_food_production_accounting_test()) return false;
    if (scenario_property_rome_supplies_wheat()) return true;
    city_resource_calculate_food_stocks_and_supply_wheat();
    const int stored = city_resource_food_stored();
    const int granaries = city_data.resource.granary_total_stored;
    const auto resource_state = city_data.resource;
    bool passed = true;
    int checked_warehouses = 0;
    Building::for_each(BuildingRuntimeList::Warehouses, [&](Building *warehouse) {
        if (!warehouse->is_in_use() || !warehouse->has_cached_road_access() || warehouse->distance_from_entry() <= 0 ||
            !building_storage_get_permission(BUILDING_STORAGE_PERMISSION_MARKET, *warehouse)) return;
        int food = 0;
        for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
            if (resource_is_food(resource) && !city_resource_is_stockpiled(resource)) food += building_warehouse_get_available_amount(*warehouse, resource);
        }
        if (!food) return;
        // Denying market access must remove exactly this warehouse's food, in inventory units.
        building_storage_set_permission(BUILDING_STORAGE_PERMISSION_MARKET, *warehouse, 1);
        const int restricted = city_resource_food_stored();
        building_storage_set_permission(BUILDING_STORAGE_PERMISSION_MARKET, *warehouse, 0);
        if (stored - restricted != food * resource_units_per_load()) passed = false;
        ++checked_warehouses;
    });
    for (resource_type resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        if (resource_is_food(resource) && !city_resource_is_stockpiled(resource)) {
            city_resource_toggle_stockpiled(resource);
        }
    }
    if (city_resource_food_stored() != granaries) passed = false;
    city_data.resource = resource_state;
    if (city_resource_food_stored() != stored) passed = false;
    if (!passed) {
        fprintf(stderr, "Food stock contract failed: market permissions, stockpiling, or inventory units were ignored.\n");
        return false;
    }
    fprintf(stdout, "Food stock contract passed: granary_units=%d available_units=%d monthly_need=%d supply_months=%d checked_warehouses=%d\n",
        granaries, stored, city_resource_food_needed(), city_resource_food_supply_months(), checked_warehouses);
    return true;
}
