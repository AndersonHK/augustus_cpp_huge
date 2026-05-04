#include "building/production_method.h"

#include "building/building_type_registry_internal.h"

extern "C" {
#include "building/industry.h"
#include "building/local_workforce.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/finance.h"
#include "core/calc.h"
#include "game/time.h"
#include "scenario/property.h"
}

#include <utility>

namespace building_type_registry_impl {

namespace {

int building_type_requires_water_access(const ::building &building)
{
    const BuildingType *definition = definition_for_type(building.type);
    return definition && definition->water_access_mode() != WaterAccessMode::None;
}

} // namespace

ProductionMethod::ProductionMethod(std::string path)
    : path_(std::move(path))
{
}

const char *ProductionMethod::path() const
{
    return path_.c_str();
}

void ProductionMethod::set_kind(ProductionMethodKind kind)
{
    kind_ = kind;
}

ProductionMethodKind ProductionMethod::kind() const
{
    return kind_;
}

void ProductionMethod::set_output_resource(resource_type resource)
{
    output_resource_ = resource;
}

resource_type ProductionMethod::output_resource() const
{
    return output_resource_;
}

void ProductionMethod::set_batch_size(int batch_size)
{
    batch_size_ = batch_size;
}

int ProductionMethod::batch_size() const
{
    return batch_size_;
}

void ProductionMethod::set_treasury_cost_per_cycle(int cost)
{
    treasury_cost_per_cycle_ = cost;
}

int ProductionMethod::treasury_cost_per_cycle() const
{
    return treasury_cost_per_cycle_;
}

void ProductionMethod::add_input(ProductionResourceAmount input)
{
    inputs_.push_back(input);
}

const std::vector<ProductionResourceAmount> &ProductionMethod::inputs() const
{
    return inputs_;
}

int ProductionMethod::add_climate_bonus(ClimateProductionBonus bonus)
{
    for (const ClimateProductionBonus &existing : climate_bonuses_) {
        if (existing.climate == bonus.climate) {
            return 0;
        }
    }

    climate_bonuses_.push_back(bonus);
    return 1;
}

const std::vector<ClimateProductionBonus> &ProductionMethod::climate_bonuses() const
{
    return climate_bonuses_;
}

int ProductionMethod::climate_bonus_percent(scenario_climate climate) const
{
    for (const ClimateProductionBonus &bonus : climate_bonuses_) {
        if (bonus.climate == climate) {
            return bonus.percent_delta;
        }
    }
    return 0;
}

int ProductionMethod::is_farm() const
{
    return kind_ == ProductionMethodKind::Farm;
}

int ProductionMethod::is_workshop() const
{
    return kind_ == ProductionMethodKind::Workshop;
}

int ProductionMethod::refreshes_farm_image() const
{
    return is_farm();
}

int ProductionMethod::uses_blessing_multiplier() const
{
    return is_farm();
}

int ProductionMethod::effective_monthly_production() const
{
    if (output_resource_ == RESOURCE_NONE) {
        return 0;
    }

    int monthly_production = resource_base_production_per_month(output_resource_);
    monthly_production += (monthly_production * climate_bonus_percent(scenario_property_climate())) / 100;
    return monthly_production;
}

int ProductionMethod::max_progress_for(const ::building &building) const
{
    if (output_resource_ == RESOURCE_NONE) {
        return 0;
    }

    const int monthly_production = effective_monthly_production();
    if (monthly_production <= 0) {
        return 0;
    }

    const int base_max_progress =
        calc_percentage(GAME_TIME_DAYS_PER_MONTH * 2 * model_get_building(building.type)->laborers, monthly_production);
    return base_max_progress * batch_size_;
}

int ProductionMethod::has_required_inputs(const ::building &building) const
{
    for (const ProductionResourceAmount &input : inputs_) {
        if (input.resource <= RESOURCE_NONE || input.resource >= RESOURCE_MAX) {
            return 0;
        }
        const int resource_index = static_cast<int>(input.resource);
        if (building.resources[resource_index] < scaled_input_amount(input)) {
            return 0;
        }
    }
    return 1;
}

int ProductionMethod::scaled_input_amount(const ProductionResourceAmount &input) const
{
    return input.amount * batch_size_;
}

int ProductionMethod::labor_access_for(const ::building &building) const
{
    // Native production follows the building's declared labor model instead of legacy coverage unconditionally.
    if (building_local_workforce_is_workforce_building(&building)) {
        return building_local_workforce_access_score(&building);
    }
    return building.houses_covered > 0 ? building.houses_covered : 0;
}

int ProductionMethod::can_start_cycle(const ::building &building) const
{
    // This is the shared production eligibility contract; live Production only mutates progress once it passes.
    if (labor_access_for(building) <= 0 || building.num_workers <= 0 || building.strike_duration_days > 0) {
        return 0;
    }
    if (max_progress_for(building) <= 0) {
        return 0;
    }
    if (building_type_requires_water_access(building) && !building.has_water_access) {
        return 0;
    }
    if (treasury_cost_per_cycle_ > 0 && building.data.industry.progress == 0 && city_finance_out_of_money()) {
        return 0;
    }
    if (!has_required_inputs(building)) {
        return 0;
    }
    const int output_resource = static_cast<int>(output_resource_);
    if (!resource_is_storable(output_resource_) && building.data.industry.progress == 0 &&
        !building_has_workshop_for_raw_material_with_room(output_resource, building.road_network_id) &&
        !building_monument_get_monument(building.x, building.y, output_resource, building.road_network_id, 0)) {
        return 0;
    }
    return 1;
}

} // namespace building_type_registry_impl
