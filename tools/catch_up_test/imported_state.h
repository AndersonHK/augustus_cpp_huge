#pragma once
#include "game/augustus_model_bridge.h"
#include "game/augustus_accounting_bridge.h"
#include "building/properties.h"
#include "game/resource.h"
#include "building/building_type_id_bridge.h"
#include "scenario/definition_overrides.h"
#include "city/trade_ledger.h"
#include <cstdio>

inline bool validate_imported_state(const std::vector<augustus_save::ModelException> &models, const augustus_save::AccountingArchive &source)
{
    constexpr int model_building::*fields[] = {&model_building::cost, &model_building::desirability_value,
        &model_building::desirability_step, &model_building::desirability_step_size, &model_building::desirability_range, &model_building::laborers};
    for (const auto &entry : models) {
        const auto type = building_type_id_bridge_runtime_from_text(entry.target.c_str());
        const int actual = entry.housing ? scenario_house_model_value(type, entry.field) : model_get_building(type)->*fields[entry.field];
        if (actual != entry.value) { std::fprintf(stderr, "Imported model mismatch: %s field=%d expected=%d actual=%d\n", entry.target.c_str(), entry.field, entry.value, actual); return false; }
    }
    const auto &periods = city_trade_ledger_periods();
    if (!source.years.empty()) {
        if (periods.empty()) return false;
        const auto &native = periods.front();
        constexpr const char *names[] = {"", "wheat", "vegetables", "fruit", "meat", "fish", "clay", "timber", "olives", "vines", "iron", "marble", "gold", "sand", "stone", "pottery", "furniture", "oil", "wine", "weapons", "concrete", "bricks"};
        // Source period quantities use cartloads except consumption, which is
        // already individual units. Compare against the archived source values.
        for (int id = 1; id < 22; ++id) {
            const auto resource = resource_type_from_text_id(names[id]);
            if (resource == RESOURCE_NONE) continue;
            const auto found = native.resources.find(resource_text_id(resource));
            if (found == native.resources.end()) return false;
            const auto &actual = found->second;
            const auto &expected = source.years.front().resources;
            if (actual.imported != int64_t(expected[1][id]) * resource_units_per_load() ||
                actual.exported != int64_t(expected[2][id]) * resource_units_per_load() ||
                actual.produced != int64_t(expected[3][id]) * resource_units_per_load() ||
                actual.consumed != expected[4][id] || actual.balance() != expected[5][id] || actual.complete_cash_flows) {
                std::fprintf(stderr, "Imported accounting mismatch: resource=%s\n", resource_text_id(resource)); return false;
            }
        }
        if (native.transactions.size() != source.transactions[0].size()) return false;
        for (size_t index = 0; index < native.transactions.size(); ++index) {
            const auto &actual = native.transactions[index]; const auto &expected = source.transactions[0][index];
            if (actual.direction_known || actual.visit || actual.storage || actual.source_trader != expected.trader || actual.source_storage != expected.storage || actual.units != int64_t(expected.quantity) * resource_units_per_load() || actual.price != expected.price) {
                std::fprintf(stderr, "Imported transaction mismatch: index=%zu\n", index); return false;
            }
        }
    }
    std::fprintf(stdout, "Imported state invariants passed: all authored model exceptions, resource totals/balances and opaque transaction identities.\n");
    return true;
}
