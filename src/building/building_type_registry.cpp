#include "building/building_type_registry_internal.h"
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

building_type building_type_for_housing_level(int level, int footprint_size)
{
    const HousingType *housing_type = find_housing_type_definition_for_level(level);
    if (!housing_type) {
        return BUILDING_NONE;
    }

    building_type fallback = BUILDING_NONE;
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_housing() || definition->housing_type() != housing_type) {
            continue;
        }
        if (definition->is_vacant_lot()) {
            continue;
        }
        const int declared_size = definition->declared_model_size();
        const int size = declared_size > 0 ? declared_size : 1;
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
    const int first_level = housing_type_level_at(0);
    return first_level < 0 ? BUILDING_NONE : building_type_for_housing_level(first_level, 1);
}

}
