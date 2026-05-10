#pragma once

extern "C" {
#include "building/properties.h"
}

#include <string>

namespace building_type_registry_impl {

enum class HousingResidentClass {
    None,
    Plebeian,
    Patrician
};

class HousingType {
public:
    explicit HousingType(std::string path);

    const char *path() const;

    void set_resident_class(HousingResidentClass resident_class);
    HousingResidentClass resident_class() const;

    void set_devolve_desirability(int value);
    void set_evolve_desirability(int value);
    void set_entertainment(int value);
    void set_water(int value);
    void set_religion(int value);
    void set_education(int value);
    void set_barber(int value);
    void set_bathhouse(int value);
    void set_health(int value);
    void set_food_types(int value);
    void set_pottery(int value);
    void set_oil(int value);
    void set_furniture(int value);
    void set_wine(int value);
    void set_prosperity(int value);
    void set_capacity(int value);
    void set_tax_multiplier(int value);

    const model_house &model() const;

private:
    std::string path_;
    HousingResidentClass resident_class_ = HousingResidentClass::None;
    model_house model_ = {};
};

} // namespace building_type_registry_impl
