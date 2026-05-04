#include "building/building_type_registry_internal.h"
#include "building/building_type_legacy_migration.h"
#include "building/production_method_registry.h"
#include "building/storage_type_registry.h"
#include "assets/image_group_payload.h"

extern "C" {
#include "assets/assets.h"
#include "building/building.h"
#include "building/properties.h"
#include "game/resource.h"
#include "platform/file_manager.h"
}

#include <string_view>

extern "C" {
constinit building_type BUILDING_THEATER = BUILDING_NONE;
constinit building_type BUILDING_WELL = BUILDING_NONE;
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

building_type runtime_id_from_text(const char *text_id)
{
    if (!text_id || !*text_id) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        if (std::string_view(definition->attr()) == text_id) {
            return definition->type();
        }
    }
    return BUILDING_NONE;
}

void refresh_known_building_type_ids()
{
    BUILDING_THEATER = runtime_id_from_text(BUILDING_TEXT_ID_THEATER);
    BUILDING_WELL = runtime_id_from_text(BUILDING_TEXT_ID_WELL);
}

void clear_xml_runtime_property_fields()
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const char *text_id = building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(type));
        if (type >= BUILDING_DYNAMIC_TYPE_FIRST || building_type_legacy_migration_text_id_is_xml_owned(text_id)) {
            building_properties_clear_xml_runtime_fields(type);
        }
    }
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
                building_properties_apply_xml_draw_desirability_range(definition->type(), flags.draw_desirability_range());
            }
            if (flags.has_venus_gt_bonus()) {
                building_properties_apply_xml_venus_gt_bonus(definition->type(), flags.venus_gt_bonus());
            }
        }
    }
}

extern "C" building_type building_type_registry_runtime_id_from_text(const char *text_id)
{
    return building_type_registry_impl::runtime_id_from_text(text_id);
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
    if (!definition || !definition->foundation().has_policy()) {
        return 0;
    }
    return definition->foundation().policy();
}

extern "C" int building_type_registry_get_foundation_required_terrain(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition ? definition->foundation_required_terrain() : 0;
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

extern "C" int building_type_registry_has_labor_seeker(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_labor() || !definition->labor().has_seeker_policy()) {
        return 0;
    }
    return definition->labor().seeker_policy().method != building_type_registry_impl::LaborSeekerMethod::None;
}

extern "C" int building_type_registry_get_sound_id(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_sound() || !definition->sound().has_city_sound()) {
        return 0;
    }
    return definition->sound().city_sound();
}

extern "C" int building_type_registry_get_sound_mute_on_enemies(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_sound() ? definition->sound().mute_on_enemies() : 0;
}

extern "C" int building_type_registry_get_sound_always_play(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_sound() ? definition->sound().always_play() : 0;
}

extern "C" int building_type_registry_get_sound_requires_water_access(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_sound()) {
        return 0;
    }
    return definition->water_access_mode() != building_type_registry_impl::WaterAccessMode::None ||
        (definition->has_water_access_provider() &&
            definition->water_access().requirement() != building_type_registry_impl::WaterAccessRequirement::None);
}

extern "C" int building_type_registry_get_graphics_image_id(const building *b)
{
    if (!b) {
        return 0;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (!definition || !definition->has_graphic()) {
        return 0;
    }

    const building_type_registry_impl::GraphicsTarget *target =
        building_type_registry_impl::BuildingType::resolve_graphics_target_for_image(definition, *b);
    if (!target || !target->has_path()) {
        return 0;
    }

    const char *image_name = target->has_image() ? target->image() : nullptr;
    if (!image_name || !*image_name) {
        if (!image_group_payload_load(target->path())) {
            return 0;
        }
        const ImageGroupPayload *payload = image_group_payload_get(target->path());
        image_name = payload ? payload->default_image_id() : nullptr;
    }
    return assets_get_image_id_from_path_or_name(target->path(), image_name);
}

extern "C" int building_type_registry_has_phased_construction(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? 1 : 0;
}

extern "C" int building_type_registry_has_construction(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_construction() ? 1 : 0;
}

extern "C" int building_type_registry_get_construction_phase_count(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? definition->construction().phase_count() : 0;
}

extern "C" int building_type_registry_get_construction_road_update_radius(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? definition->construction().road_update_radius() : 0;
}

extern "C" int building_type_registry_get_instant_construction_requirement(building_type type, int resource)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || definition->construction().mode() != building_type_registry_impl::ConstructionMode::Instant ||
        resource <= RESOURCE_NONE || resource >= RESOURCE_MAX) {
        return 0;
    }
    return definition->construction().instant_requirement_amount(static_cast<resource_type>(resource));
}

extern "C" int building_type_registry_get_construction_requirement(building_type type, int resource, int phase)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_phased_construction() ||
        resource < RESOURCE_NONE || resource >= RESOURCE_MAX || phase < 1) {
        return 0;
    }
    return definition->construction().requirement_amount(static_cast<resource_type>(resource), phase);
}
