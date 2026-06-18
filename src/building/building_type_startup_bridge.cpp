#include "building/building_type_startup_bridge.h"

#include "building/building_type_legacy_migration.h"
#include "building/building_type_registry_internal.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"

extern "C" {
#include "building/properties.h"
}

namespace building_type_registry_impl {

int stop_on_first_entry(const char *name, long unused)
{
    return LIST_MATCH;
}

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

}

extern "C" const char *building_type_startup_bridge_get_building_type_path(void)
{
    building_type_registry_impl::refresh_building_type_path();
    return building_type_registry_impl::g_building_type_path.c_str();
}

extern "C" int building_type_startup_bridge_validate_mod(void)
{
    building_type_registry_impl::refresh_building_type_path();
    return static_cast<int>(
        static_cast<bool>(mod_manager::validate_mod_path()) &&
        static_cast<bool>(
            building_type_registry_impl::directory_exists(
                building_type_registry_impl::g_building_type_path.c_str())));
}

extern "C" void building_type_startup_bridge_apply_model_overrides(void)
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
            if (xml_model.has_size()) {
                building_properties_apply_xml_model_size(definition->type(), xml_model.size());
            }
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
        if (definition->has_event_data() && definition->event_data().has_attr()) {
            building_properties_apply_xml_event_attr(definition->type(), definition->event_data().attr());
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
}

extern "C" building_type building_type_startup_bridge_runtime_id_from_text(const char *text_id)
{
    return building_type_registry_impl::runtime_id_from_text(text_id);
}
