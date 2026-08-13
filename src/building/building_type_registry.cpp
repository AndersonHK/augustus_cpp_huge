#include "building/building_type_registry_internal.h"
#include "building/building_type_legacy_migration.h"
#include "building/housing_profile_registry.h"
#include "building/production_method_registry.h"
#include "building/storage_type_registry.h"
#include "building/water_access_type_id_bridge.h"

#include "building/properties.h"
#include "core/xml_definition.h"
#include "game/resource.h"

#include <cstdio>
#include <string_view>

namespace building_type_registry_impl {

std::array<std::unique_ptr<BuildingType>, BUILDING_TYPE_MAX> g_building_types;
mod_definition::DefinitionOverlayTracker g_building_type_overlays;
ParseState g_parse_state;

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

const mod_definition::DefinitionOverlayEntry *find_building_type_definition_overlay(const char *identity)
{
    if (!identity || !*identity) {
        return nullptr;
    }
    return g_building_type_overlays.find(xml_definition::normalize_path(identity));
}

int type_attr_is(building_type type, std::string_view attr)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->attr_is(attr) ? 1 : 0;
}

int type_has_housing(building_type type)
{
    const BuildingType *definition = definition_for_type(type);
    return definition && definition->housing_def().profile;
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

building_type type_from_bridge(BridgeType bridge_type)
{
    if (bridge_type == BridgeType::None) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (definition && definition->bridge().type() == bridge_type) {
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

building_type building_type_for_housing_compatibility_level(int level, int footprint_size)
{
    const HousingProfileDef *profile = find_housing_profile_definition_for_compatibility_level(level);
    if (!profile) {
        return BUILDING_NONE;
    }

    building_type fallback = BUILDING_NONE;
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || definition->housing_def().profile != profile) {
            continue;
        }
        if (definition->is_vacant_lot()) {
            continue;
        }
        const FoundationDef *foundation = definition->foundation_def();
        const int size = foundation ? std::max(foundation->width(), foundation->height()) : 1;
        if (size == footprint_size) {
            return definition->type();
        }
        if (fallback == BUILDING_NONE || size == 1) {
            fallback = definition->type();
        }
    }
    return fallback;
}

building_type vacant_lot_fill_type()
{
    building_type vacant_lot = type_from_attr("vacant_lot");
    const BuildingType *definition = definition_for_type(vacant_lot);
    if (definition && definition->is_vacant_lot()) {
        return vacant_lot;
    }
    return vacant_lot_occupancy_type();
}

building_type vacant_lot_occupancy_type()
{
    building_type vacant_lot = type_from_attr("vacant_lot");
    const BuildingType *definition = definition_for_type(vacant_lot);
    if (definition && definition->is_vacant_lot() && definition->vacant_lot_fill_type() != BUILDING_NONE) {
        return definition->vacant_lot_fill_type();
    }
    const int first_level = housing_profile_compatibility_level_at(0);
    return first_level < 0 ? BUILDING_NONE : building_type_for_housing_compatibility_level(first_level, 1);
}

}
