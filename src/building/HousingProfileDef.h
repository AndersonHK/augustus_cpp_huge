#pragma once

#include <string>
#include <cstdint>

namespace building_type_registry_impl {

enum class HousingResidentClass {
    None,
    Plebeian,
    Patrician
};

enum class HousingWaterRequirement {
    None,
    Well,
    Fountain,
    LatrineOrFountain
};

// Exact authored annual units per resident; fractional consumed units live in HousingState.
struct HousingGoodsRate {
    int numerator = 0;
    int denominator = 1;
    HousingGoodsRate(int value = 0) : numerator(value) {}
    explicit operator bool() const { return numerator > 0; }
    bool operator>(int value) const { return numerator > static_cast<int64_t>(value) * denominator; }
    double annual_amount(int population) const;
    int consume(int population, int events_per_month, double &remainder) const;
    int stock_target(int population, int events_per_month, int buffered_events) const;
    static bool parse(const char *text, HousingGoodsRate &result);
};

struct HousingEvolutionThresholds {
    int devolve_desirability = 0;
    int evolve_desirability = 0;
};

struct HousingRequirements {
    bool has_required_water(bool well, bool fountain, bool latrine) const {
        return water == HousingWaterRequirement::None || fountain ||
            (water == HousingWaterRequirement::Well && well) || (water == HousingWaterRequirement::LatrineOrFountain && latrine);
    }
    int entertainment = 0;
    HousingWaterRequirement water = HousingWaterRequirement::None;
    int religion = 0;
    int education = 0;
    int barber = 0;
    int bathhouse = 0;
    int health = 0;
    int food_types = 0;
    HousingGoodsRate pottery;
    HousingGoodsRate oil;
    HousingGoodsRate furniture;
    HousingGoodsRate wine;
    int wine_sources = 0;
};

class HousingProfileDef {
public:
    explicit HousingProfileDef(std::string path);

    std::string path_id;
    std::string variant_of;
    int compatibility_level = -1;
    HousingResidentClass resident_class_value = HousingResidentClass::None;
    HousingEvolutionThresholds evolution;
    HousingRequirements requirements;
    int prosperity = 0;
    int tax_multiplier = 0;
};

} // namespace building_type_registry_impl
