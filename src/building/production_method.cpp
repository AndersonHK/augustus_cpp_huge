#include "building/production_method.h"

#include <utility>

namespace building_type_registry_impl {

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

void ProductionMethod::set_cart_capacity(int loads)
{
    cart_capacity_ = loads;
}

int ProductionMethod::cart_capacity() const
{
    const int produced_loads = cart_loads_per_cycle();
    return cart_capacity_ > 0 ? cart_capacity_ : (produced_loads > 0 ? produced_loads : 1);
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

int ProductionMethod::scaled_input_amount(const ProductionResourceAmount &input) const
{
    return input.amount * batch_size_;
}

} // namespace building_type_registry_impl
