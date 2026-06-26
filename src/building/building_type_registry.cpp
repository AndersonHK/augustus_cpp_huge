#pragma once

#include "building/building_type_registry_internal.h"
#include "building/building_type_api.h"
#include "building/building_type_legacy_migration.h"
#include "building/housing_type.h"
#include "building/housing_type_registry.h"
#include "building/production_method_registry.h"
#include "building/storage_type_registry.h"
#include "building/water_access_type_id_bridge.h"
#include "game/mod_manager.h"

#include "building/properties.h"
#include "game/resource.h"

#include <cstdio>
#include <string_view>

namespace building_type_registry_impl {

std::string g_building_type_path;
std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
ParseState g_parse_state;

void refresh_building_type_path()
{
    g_building_type_path = mod_manager::mod_path() + "BuildingType/";
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
    return type_from_attr(text_id);
}

building_type type_from_attr(std::string_view attr)
{
    if (attr.empty()) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        if (std::string_view(definition->attr()) == attr) {
            return definition->type();
        }
    }
    return BUILDING_NONE;
}

int type_attr_is(building_type type, std::string_view attr)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->attr_is(attr) ? 1 : 0;
}

int type_attr_is_any(building_type type, std::initializer_list<std::string_view> attrs)
{
    for (std::string_view attr : attrs) {
        if (type_attr_is(type, attr)) {
            return 1;
        }
    }
    return 0;
}

int type_attr_is_any(building_type type, const char *const *attrs, int count)
{
    for (int i = 0; i < count; i++) {
        if (attrs[i] && type_attr_is(type, attrs[i])) {
            return 1;
        }
    }
    return 0;
}

int type_is_bridge(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->roadblock().is_bridge() ? 1 : 0;
}

int type_is_ship_bridge(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->roadblock().is_ship_bridge() ? 1 : 0;
}

int type_is_wall_foundation(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition &&
        definition->foundation().policy_type() == FoundationPolicy::Wall ? 1 : 0;
}

int type_is_wall_gate(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->roadblock().is_wall_gate() ? 1 : 0;
}

int type_has_water_foundation(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->has_foundation() &&
        definition->foundation().has_water_requirement() ? 1 : 0;
}

building_type type_from_roadblock_bridge(RoadblockBridgeType bridge_type)
{
    if (bridge_type == RoadblockBridgeType::None) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition && definition->roadblock().bridge_type() == bridge_type) {
            return definition->type();
        }
    }
    return BUILDING_NONE;
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


int building_type_registry_has_distribution(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_distribution() ? 1 : 0;
}

int building_type_registry_has_definition(building_type type)
{
    return building_type_registry_impl::definition_for_type(type) ? 1 : 0;
}

const char *building_type_registry_get_name_key(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_identity()) {
        return 0;
    }
    return definition->identity().name_key();
}

int building_type_registry_get_model_size(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_model() || !definition->model().has_size()) {
        return 0;
    }
    return definition->model().size();
}

const char *building_type_registry_get_foundation_policy(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->foundation().has_policy()) {
        return 0;
    }
    return definition->foundation().policy();
}

int building_type_registry_get_foundation_required_terrain(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition ? definition->foundation_required_terrain() : 0;
}

const char *building_type_registry_get_button_group(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_group()) {
        return 0;
    }
    return definition->button().group();
}

int building_type_registry_get_button_order(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_order()) {
        return 0;
    }
    return definition->button().order();
}

const char *building_type_registry_get_button_icon(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_icon()) {
        return 0;
    }
    return definition->button().icon();
}

const char *building_type_registry_get_button_icon_image(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_icon_image()) {
        return 0;
    }
    return definition->button().icon_image();
}

const char *building_type_registry_get_button_text_key(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_button() || !definition->button().has_text_key()) {
        return 0;
    }
    return definition->button().text_key();
}

int building_type_registry_has_labor_seeker(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_labor() || !definition->labor().has_seeker_policy()) {
        return 0;
    }
    return definition->labor().seeker_policy().method != building_type_registry_impl::LaborSeekerMethod::None;
}

figure_type building_type_registry_get_preview_figure(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return FIGURE_NONE;
    }
    if (!definition->has_housing()) {
        for (const building_type_registry_impl::SpawnDelayGroup &group : definition->spawn_groups()) {
            for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
                if (policy.spawn_figure != FIGURE_NONE) {
                    return policy.spawn_figure;
                }
            }
        }
    }
    if (definition->has_temple()) {
        return FIGURE_PRIEST;
    }
    if (building_type_registry_has_labor_seeker(type)) {
        return FIGURE_LABOR_SEEKER;
    }
    return FIGURE_NONE;
}

int building_type_registry_get_sound_id(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_sound() || !definition->sound().has_city_sound()) {
        return 0;
    }
    return definition->sound().city_sound();
}

int building_type_registry_get_sound_mute_on_enemies(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_sound() ? definition->sound().mute_on_enemies() : 0;
}

int building_type_registry_get_sound_always_play(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_sound() ? definition->sound().always_play() : 0;
}

int building_type_registry_get_sound_requires_water_access(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_sound()) {
        return 0;
    }
    return definition->water_access().has_requirements();
}

int building_type_registry_has_water_access_requirements(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->water_access().has_requirements() ? 1 : 0;
}

int building_type_registry_has_phased_construction(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? 1 : 0;
}

int building_type_registry_has_construction(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_construction() ? 1 : 0;
}

int building_type_registry_get_construction_phase_count(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? definition->construction().phase_count() : 0;
}

int building_type_registry_get_construction_road_update_radius(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_phased_construction() ? definition->construction().road_update_radius() : 0;
}

int building_type_registry_get_instant_construction_requirement(building_type type, int resource)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || definition->construction().mode() != building_type_registry_impl::ConstructionMode::Instant ||
        resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return definition->construction().instant_requirement_amount(static_cast<resource_type>(resource));
}

int building_type_registry_get_construction_requirement(building_type type, int resource, int phase)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_phased_construction() ||
        resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT || phase < 1) {
        return 0;
    }
    return definition->construction().requirement_amount(static_cast<resource_type>(resource), phase);
}

int building_type_registry_has_housing(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_housing() ? 1 : 0;
}

int building_type_registry_is_vacant_lot(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->is_vacant_lot() ? 1 : 0;
}

const model_house *building_type_registry_get_housing_model(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_housing() || !definition->housing_type()) {
        return 0;
    }
    return &definition->housing_type()->model();
}

int building_type_registry_get_housing_resident_class(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_housing() || !definition->housing_type()) {
        return 0;
    }
    switch (definition->housing_type()->resident_class()) {
        case building_type_registry_impl::HousingResidentClass::Plebeian:
            return BUILDING_TYPE_HOUSING_RESIDENT_PLEBEIAN;
        case building_type_registry_impl::HousingResidentClass::Patrician:
            return BUILDING_TYPE_HOUSING_RESIDENT_PATRICIAN;
        case building_type_registry_impl::HousingResidentClass::None:
        default:
            return 0;
    }
}

int building_type_registry_housing_has_resident_class(building_type type, int resident_class)
{
    const int actual_class = building_type_registry_get_housing_resident_class(type);
    return actual_class != BUILDING_TYPE_HOUSING_RESIDENT_NONE && actual_class == resident_class ? 1 : 0;
}

int building_type_registry_get_housing_level(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_housing() && definition->housing_type() ? definition->housing_type()->level() : -1;
}

int building_type_registry_get_housing_capacity(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_housing() ? definition->housing_capacity() : 0;
}

int building_type_registry_get_housing_level_count(void)
{
    return building_type_registry_impl::housing_type_level_count();
}

int building_type_registry_get_housing_level_at(int index)
{
    return building_type_registry_impl::housing_type_level_at(index);
}

building_type building_type_registry_get_housing_type_for_level(int level, int footprint_size)
{
    const building_type_registry_impl::HousingType *housing_type =
        building_type_registry_impl::find_housing_type_definition_for_level(level);
    if (!housing_type) {
        return BUILDING_NONE;
    }

    building_type fallback = BUILDING_NONE;
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (!definition || !definition->has_housing() || definition->housing_type() != housing_type) {
            continue;
        }
        if (definition->is_vacant_lot()) {
            continue;
        }
        const int size = definition->has_model() && definition->model().has_size() ? definition->model().size() : 1;
        if (size == footprint_size) {
            return definition->type();
        }
        if (fallback == BUILDING_NONE || size == 1) {
            fallback = definition->type();
        }
    }
    return fallback;
}

building_type building_type_registry_get_housing_transition(building_type type, int transition)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_housing()) {
        return BUILDING_NONE;
    }

    using building_type_registry_impl::HousingTransitionKind;
    switch (transition) {
        case BUILDING_TYPE_HOUSING_TRANSITION_EVOLVE_TO:
            return definition->housing_transition_type(HousingTransitionKind::EvolveTo);
        case BUILDING_TYPE_HOUSING_TRANSITION_DEVOLVE_TO:
            return definition->housing_transition_type(HousingTransitionKind::DevolveTo);
        case BUILDING_TYPE_HOUSING_TRANSITION_MERGE_TO:
            return definition->housing_transition_type(HousingTransitionKind::MergeTo);
        case BUILDING_TYPE_HOUSING_TRANSITION_SPLIT_TO:
            return definition->housing_transition_type(HousingTransitionKind::SplitTo);
        default:
            return BUILDING_NONE;
    }
}

building_type building_type_registry_get_vacant_lot_fill_type(void)
{
    building_type vacant_lot = building_type_registry_impl::type_from_attr("vacant_lot");
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(vacant_lot);
    if (definition && definition->is_vacant_lot()) {
        return vacant_lot;
    }
    return building_type_registry_get_vacant_lot_occupancy_type();
}

building_type building_type_registry_get_vacant_lot_occupancy_type(void)
{
    building_type vacant_lot = building_type_registry_impl::type_from_attr("vacant_lot");
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(vacant_lot);
    if (definition && definition->is_vacant_lot() && definition->vacant_lot_fill_type() != BUILDING_NONE) {
        return definition->vacant_lot_fill_type();
    }
    const int first_level = building_type_registry_impl::housing_type_level_at(0);
    return first_level < 0 ? BUILDING_NONE : building_type_registry_get_housing_type_for_level(first_level, 1);
}
