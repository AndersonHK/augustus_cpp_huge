#include "building/building_type_registry_internal.h"
#include "building/production_method_registry.h"
#include "building/storage_type_registry.h"

extern "C" {
#include "building/properties.h"
#include "platform/file_manager.h"
}

namespace building_type_registry_impl {

std::string g_building_type_path;
std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
ParseState g_parse_state;

static int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

void refresh_building_type_path()
{
    g_building_type_path = std::string(mod_manager_get_mod_path()) + "BuildingType/";
}

const BuildingType *definition_for_type(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return nullptr;
    }
    return g_building_types[type].get();
}

}

extern "C" const char *building_type_registry_get_building_type_path(void)
{
    building_type_registry_impl::refresh_building_type_path();
    return building_type_registry_impl::g_building_type_path.c_str();
}

extern "C" int building_type_registry_validate_mod(void)
{
    using namespace building_type_registry_impl;
    refresh_building_type_path();
    return static_cast<int>(
        static_cast<bool>(mod_manager_validate_mod_path()) &&
        static_cast<bool>(directory_exists(g_building_type_path.c_str())));
}

extern "C" void building_type_registry_apply_model_overrides(void)
{
    using namespace building_type_registry_impl;

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }

        model_building *model = model_get_building(definition->type());
        if (!model) {
            continue;
        }
        if (definition->has_model()) {
            const BuildModelDefinition &xml_model = definition->model();
            if (xml_model.has_cost()) {
                model->cost = xml_model.cost();
            }
            if (xml_model.has_desirability_value()) {
                model->desirability_value = xml_model.desirability_value();
            }
            if (xml_model.has_desirability_step()) {
                model->desirability_step = xml_model.desirability_step();
            }
            if (xml_model.has_desirability_step_size()) {
                model->desirability_step_size = xml_model.desirability_step_size();
            }
            if (xml_model.has_desirability_range()) {
                model->desirability_range = xml_model.desirability_range();
            }
            if (xml_model.has_laborers()) {
                model->laborers = xml_model.laborers();
            }
        }
        if (definition->has_labor() && definition->labor().has_employee_count()) {
            model->laborers = definition->labor().employee_count();
        }
    }
}

extern "C" int building_type_registry_has_definition(building_type type)
{
    return building_type_registry_impl::definition_for_type(type) ? 1 : 0;
}

extern "C" const char *building_type_registry_get_name_key(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_identity()) {
        return 0;
    }
    return definition->identity().name_key();
}

extern "C" int building_type_registry_get_model_size(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_model() || !definition->model().has_size()) {
        return 0;
    }
    return definition->model().size();
}

extern "C" const char *building_type_registry_get_foundation_policy(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_foundation()) {
        return 0;
    }
    return definition->foundation().policy();
}

extern "C" const char *building_type_registry_get_button_group(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_group()) {
        return 0;
    }
    return definition->button().group();
}

extern "C" int building_type_registry_get_button_order(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_order()) {
        return 0;
    }
    return definition->button().order();
}

extern "C" const char *building_type_registry_get_button_icon(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_icon()) {
        return 0;
    }
    return definition->button().icon();
}

extern "C" const char *building_type_registry_get_button_text_key(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_text_key()) {
        return 0;
    }
    return definition->button().text_key();
}
