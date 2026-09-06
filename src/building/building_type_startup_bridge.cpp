#include "building/building_type_startup_bridge.h"

#include "building/building_type_registry_internal.h"
#include "core/xml_definition.h"
#include "game/mod_manager.h"

#include "building/properties.h"

#include <string>

namespace {

std::string selected_building_type_path()
{
    return mod_manager::mod_path() + "BuildingType/";
}

std::string g_selected_building_type_path;

} // namespace

const char *building_type_startup_bridge_get_building_type_path(void)
{
    g_selected_building_type_path = selected_building_type_path();
    return g_selected_building_type_path.c_str();
}

int building_type_startup_bridge_validate_mod(void)
{
    if (!mod_manager::validate_mod_path()) {
        return 0;
    }
    for (const std::string &mod_path : mod_manager::mod_paths()) {
        const std::string building_type_path = mod_path + "BuildingType/";
        if (xml_definition::directory_exists(building_type_path.c_str())) {
            return 1;
        }
    }
    return 0;
}

void building_type_startup_bridge_apply_model_overrides(void)
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
        }
        if (definition->has_labor() && definition->labor().has_employee_count()) {
            model->laborers = definition->labor().employee_count();
        }
        if (definition->has_sound() && definition->sound().has_city_sound()) {
            building_properties_apply_xml_sound_id(definition->type(), definition->sound().city_sound());
        }
        if (definition->has_flags()) {
            const BuildingFlagsDefinition &flags = definition->flags();
            if (flags.has_fire_proof()) {
                building_properties_apply_xml_fire_proof(definition->type(), flags.fire_proof());
            }
            if (flags.has_draw_desirability_range()) {
                building_properties_apply_xml_draw_desirability_range(
                    definition->type(),
                    flags.draw_desirability_range());
            }
            if (flags.has_venus_gt_bonus()) {
                building_properties_apply_xml_venus_gt_bonus(definition->type(), flags.venus_gt_bonus());
            }
        }
    }
    model_capture_mod_defaults();
}
