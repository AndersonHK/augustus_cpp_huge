#include "building/culture_module.h"

#include <utility>

namespace building_type_registry_impl {

CultureModule::CultureModule(std::string path)
    : path_(std::move(path))
{
}

const char *CultureModule::path() const
{
    return path_.c_str();
}

void CultureModule::set_type(CultureModuleType type)
{
    type_ = type;
}

CultureModuleType CultureModule::type() const
{
    return type_;
}

} // namespace building_type_registry_impl
