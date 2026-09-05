#pragma once

#include <string>

namespace building_type_registry_impl {

enum class HousingResidentClass {
    None,
    Plebeian,
    Patrician
};

enum class HousingWaterRequirement {
    None,
    Well,
    Fountain
};

struct HousingEvolutionThresholds {
    int devolve_desirability = 0;
    int evolve_desirability = 0;
};

struct HousingRequirements {
    int entertainment = 0;
    HousingWaterRequirement water = HousingWaterRequirement::None;
    int religion = 0;
    int education = 0;
    int barber = 0;
    int bathhouse = 0;
    int health = 0;
    int food_types = 0;
    int pottery = 0;
    int oil = 0;
    int furniture = 0;
    int wine = 0;
};

class HousingProfileDef {
public:
    explicit HousingProfileDef(std::string path);

    std::string path_id;
    int compatibility_level = -1;
    HousingResidentClass resident_class_value = HousingResidentClass::None;
    HousingEvolutionThresholds evolution;
    HousingRequirements requirements;
    int prosperity = 0;
    int tax_multiplier = 0;
};

} // namespace building_type_registry_impl
