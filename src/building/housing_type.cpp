#include "building/housing_type.h"

#include <utility>

namespace building_type_registry_impl {

HousingType::HousingType(std::string path)
    : path_(std::move(path))
{
}

const char *HousingType::path() const
{
    return path_.c_str();
}

void HousingType::set_resident_class(HousingResidentClass resident_class)
{
    resident_class_ = resident_class;
}

HousingResidentClass HousingType::resident_class() const
{
    return resident_class_;
}

void HousingType::set_devolve_desirability(int value)
{
    model_.devolve_desirability = value;
}

void HousingType::set_evolve_desirability(int value)
{
    model_.evolve_desirability = value;
}

void HousingType::set_entertainment(int value)
{
    model_.entertainment = value;
}

void HousingType::set_water(int value)
{
    model_.water = value;
}

void HousingType::set_religion(int value)
{
    model_.religion = value;
}

void HousingType::set_education(int value)
{
    model_.education = value;
}

void HousingType::set_barber(int value)
{
    model_.barber = value;
}

void HousingType::set_bathhouse(int value)
{
    model_.bathhouse = value;
}

void HousingType::set_health(int value)
{
    model_.health = value;
}

void HousingType::set_food_types(int value)
{
    model_.food_types = value;
}

void HousingType::set_pottery(int value)
{
    model_.pottery = value;
}

void HousingType::set_oil(int value)
{
    model_.oil = value;
}

void HousingType::set_furniture(int value)
{
    model_.furniture = value;
}

void HousingType::set_wine(int value)
{
    model_.wine = value;
}

void HousingType::set_prosperity(int value)
{
    model_.prosperity = value;
}

void HousingType::set_capacity(int value)
{
    model_.max_people = value;
}

void HousingType::set_tax_multiplier(int value)
{
    model_.tax_multiplier = value;
}

const model_house &HousingType::model() const
{
    return model_;
}

} // namespace building_type_registry_impl
