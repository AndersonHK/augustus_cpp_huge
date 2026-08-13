#include "building/HousingProfileDef.h"

#include <utility>

namespace building_type_registry_impl {

HousingProfileDef::HousingProfileDef(std::string path_value)
    : path_id(std::move(path_value))
{
}

} // namespace building_type_registry_impl
