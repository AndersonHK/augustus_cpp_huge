#include "building/industry.h"
#include "building/local_workforce.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/production_method.h"

#include "building/building_type_registry_internal.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/calc.h"
#include "game/time.h"
#include "map/water.h"
#include "scenario/property.h"

#include <utility>

namespace building_type_registry_impl {

namespace {

int building_type_requires_water_access(const Building &building)
{
    const BuildingType *definition = building.type;
    return definition && definition->water_access().has_requirements();
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

int ProductionMethod::has_resource_output() const
{
    return output_resource_ != RESOURCE_NONE;
}

void ProductionMethod::set_output_destination(ProductionOutputDestination destination)
{
    output_destination_ = destination;
}

ProductionOutputDestination ProductionMethod::output_destination() const
{
    return output_destination_;
}

int ProductionMethod::uses_cart_output() const
{
    return output_destination_ == ProductionOutputDestination::Cart;
}

int ProductionMethod::outputs_to_building_storage() const
{
    return output_destination_ == ProductionOutputDestination::BuildingStorage;
}

int ProductionMethod::outputs_to_treasury() const
{
    return output_destination_ == ProductionOutputDestination::Treasury;
}

void ProductionMethod::set_output_effect(ProductionOutputEffect effect)
{
    output_effect_ = effect;
}

ProductionOutputEffect ProductionMethod::output_effect() const
{
    return output_effect_;
}

int ProductionMethod::has_effect_output() const
{
    return output_effect_ != ProductionOutputEffect::None;
}

int ProductionMethod::spawns_fishing_boat() const
{
    return output_effect_ == ProductionOutputEffect::SpawnFishingBoat;
}

void ProductionMethod::set_output_source(ProductionOutputSource source)
{
    output_source_ = source;
}

int ProductionMethod::is_figure_delivery_output() const
{
    return output_source_ == ProductionOutputSource::FigureDelivery;
}

void ProductionMethod::set_base_monthly_production(int production)
{
    base_monthly_production_ = production;
    default_base_monthly_production_ = production;
}

void ProductionMethod::override_base_monthly_production(int production)
{
    base_monthly_production_ = production < 0 ? 0 : production;
}

void ProductionMethod::reset_base_monthly_production_override()
{
    base_monthly_production_ = default_base_monthly_production_;
}

int ProductionMethod::base_monthly_production() const
{
    return base_monthly_production_;
}

int ProductionMethod::default_base_monthly_production() const
{
    return default_base_monthly_production_;
}

void ProductionMethod::set_batch_size(int batch_size)
{
    batch_size_ = batch_size;
}

int ProductionMethod::batch_size() const
{
    return batch_size_;
}

void ProductionMethod::set_cart_loads(int numerator, int denominator)
{
    cart_load_numerator_ = numerator;
    cart_load_denominator_ = denominator > 0 ? denominator : 1;
}

int ProductionMethod::cart_load_numerator() const
{
    return cart_load_numerator_ > 0 ? cart_load_numerator_ : batch_size_;
}

int ProductionMethod::cart_load_denominator() const
{
    return cart_load_numerator_ > 0 ? cart_load_denominator_ : 1;
}

int ProductionMethod::cart_loads_per_cycle() const
{
    return cart_load_numerator() / cart_load_denominator();
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

int ProductionMethod::uses_blessing_multiplier() const
{
    return is_farm();
}

int ProductionMethod::is_enabled() const
{
    if (output_resource_ <= RESOURCE_NONE || output_resource_ == resource_denarii()) {
        return 1;
    }
    return !city_resource_is_mothballed(output_resource_);
}

int ProductionMethod::is_disabled() const
{
    return !is_enabled();
}

int ProductionMethod::effective_monthly_production() const
{
    if ((!has_resource_output() && !has_effect_output()) || base_monthly_production_ <= 0) {
        return 0;
    }

    int monthly_production = base_monthly_production_;
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

    const int base_max_progress =
        calc_percentage(
            GAME_TIME_DAYS_PER_MONTH * 2 * (building.type ? building.type->required_workers() : 0),
            monthly_production);
    return base_max_progress * batch_size_;
}

int ProductionMethod::has_required_inputs(const Building &building) const
{
    for (const ProductionResourceAmount &input : inputs_) {
        if (input.resource <= RESOURCE_NONE || input.resource >= RESOURCE_SLOT_COUNT) {
            return 0;
        }
        if (building.resource_amount(input.resource) < scaled_input_amount(input)) {
            return 0;
        }
    }
    return 1;
}

int ProductionMethod::scaled_input_amount(const ProductionResourceAmount &input) const
{
    return input.amount * batch_size_;
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
    if (treasury_cost_per_cycle_ > 0 && record->data.industry.progress == 0 && city_finance_out_of_money()) {
        return 0;
    }
    if (!has_required_inputs(building)) {
        return 0;
    }
    if (spawns_fishing_boat() && !map_water_shipyard_can_spawn_fishing_boat(building)) {
        return 0;
    }
    const int output_resource = static_cast<int>(output_resource_);
    if (has_resource_output() && uses_cart_output() && !resource_is_storable(output_resource_) &&
        record->data.industry.progress == 0 &&
        !building_has_workshop_for_raw_material_with_room(output_resource, building.road_network_id()) &&
        !building_monument_get_monument(building.x(), building.y(), output_resource, building.road_network_id(), 0)) {
        return 0;
    }
    return 1;
}

} // namespace building_type_registry_impl
