#pragma once

extern "C" {
#include "game/resource.h"
#include "scenario/property.h"
}

#include "building/building_fwd.h"

class Building;

#include <string>
#include <vector>

namespace building_type_registry_impl {

enum class ProductionMethodKind {
    None,
    Farm,
    Workshop
};

struct ProductionResourceAmount {
    resource_type resource = RESOURCE_NONE;
    int amount = 0;
};

struct ClimateProductionBonus {
    scenario_climate climate = CLIMATE_CENTRAL;
    int percent_delta = 0;
};

class ProductionMethod {
public:
    explicit ProductionMethod(std::string path);

    const char *path() const;

    void set_kind(ProductionMethodKind kind);
    ProductionMethodKind kind() const;

    void set_output_resource(resource_type resource);
    resource_type output_resource() const;

    void set_base_monthly_production(int production);
    void override_base_monthly_production(int production);
    void reset_base_monthly_production_override();
    int base_monthly_production() const;
    int default_base_monthly_production() const;

    void set_batch_size(int batch_size);
    int batch_size() const;

    void set_treasury_cost_per_cycle(int cost);
    int treasury_cost_per_cycle() const;

    void add_input(ProductionResourceAmount input);
    const std::vector<ProductionResourceAmount> &inputs() const;

    int add_climate_bonus(ClimateProductionBonus bonus);
    const std::vector<ClimateProductionBonus> &climate_bonuses() const;
    int climate_bonus_percent(scenario_climate climate) const;

    int is_farm() const;
    int is_workshop() const;
    int refreshes_farm_image() const;
    int uses_blessing_multiplier() const;
    int effective_monthly_production() const;
    int max_progress_for(const Building &building) const;
    int has_required_inputs(const Building &building) const;
    int scaled_input_amount(const ProductionResourceAmount &input) const;
    int labor_access_for(const Building &building) const;
    int can_start_cycle(const Building &building) const;

private:
    std::string path_;
    ProductionMethodKind kind_ = ProductionMethodKind::None;
    resource_type output_resource_ = RESOURCE_NONE;
    int base_monthly_production_ = 0;
    int default_base_monthly_production_ = 0;
    int batch_size_ = 1;
    int treasury_cost_per_cycle_ = 0;
    std::vector<ProductionResourceAmount> inputs_;
    std::vector<ClimateProductionBonus> climate_bonuses_;
};

} // namespace building_type_registry_impl
