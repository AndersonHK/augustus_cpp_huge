#include "building/production_method.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/calc.h"
#include "game/time.h"
#include "map/water.h"
#include "scenario/property.h"

namespace building_type_registry_impl {

namespace {

int building_type_requires_water_access(const Building &building)
{
    const BuildingType *definition = building.type;
    return definition && definition->water_access().has_requirements();
}

} // namespace

int ProductionMethod::is_enabled() const
{
    if (output_resource() <= RESOURCE_NONE || output_resource() == resource_denarii()) {
        return 1;
    }
    return !city_resource_is_mothballed(output_resource());
}

int ProductionMethod::is_disabled() const
{
    return !is_enabled();
}

int ProductionMethod::effective_monthly_production() const
{
    if ((!has_resource_output() && !has_effect_output()) || base_monthly_production() <= 0) {
        return 0;
    }

    int monthly_production = base_monthly_production();
    monthly_production += (monthly_production * climate_bonus_percent(scenario_property_climate())) / 100;
    return monthly_production;
}

int ProductionMethod::max_progress_for(const Building &building) const
{
    if (is_figure_delivery_output() || (!has_resource_output() && !has_effect_output())) {
        return 0;
    }

    const int monthly_production = effective_monthly_production();
    if (monthly_production <= 0) {
        return 0;
    }

    return calc_percentage(
        GAME_TIME_DAYS_PER_MONTH * 2 * building.employment_required_workers(),
        monthly_production);
}

int ProductionMethod::has_required_inputs(const Building &building) const
{
    for (const ProductionResourceAmount &input : inputs()) {
        if (input.resource <= RESOURCE_NONE || input.resource >= RESOURCE_SLOT_COUNT) {
            return 0;
        }
        if (building.storage_resource_amount(input.resource, StorageRole::Input) < scaled_input_amount(input)) {
            return 0;
        }
    }
    return 1;
}

int ProductionMethod::labor_access_for(const Building &building) const
{
    return static_cast<int>(building.labor_access_score());
}

int ProductionMethod::can_start_cycle(const Building &building) const
{
    const ::building *record = building_get(building.id());
    if (!record) {
        return 0;
    }
    // This is the shared production eligibility contract; live Production only mutates progress once it passes.
    if (is_disabled() || labor_access_for(building) <= 0 || !building.worker_count() || record->strike_duration_days > 0) {
        return 0;
    }
    if (max_progress_for(building) <= 0) {
        return 0;
    }
    if (building_type_requires_water_access(building) && !building.has_water_access()) {
        return 0;
    }
    if (treasury_cost_per_cycle() > 0 && record->data.industry.progress == 0 && city_finance_out_of_money()) {
        return 0;
    }
    if (!has_required_inputs(building)) {
        return 0;
    }
    if (spawns_fishing_boat() && !map_water_shipyard_can_spawn_fishing_boat(building)) {
        return 0;
    }
    return 1;
}

int BuildingType::production_is_enabled() const
{
    if (production_methods().empty()) {
        return 1;
    }
    for (const ProductionMethod *method : production_methods()) {
        if (method && method->is_enabled()) {
            return 1;
        }
    }
    return 0;
}

} // namespace building_type_registry_impl
