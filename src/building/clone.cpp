#include "clone.h"

#include "building/building.h"
#include "building/building_type_api.h"
#include "building/variant.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"

static building_type xml_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    building_type resolved = xml_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

static int type_matches_any(building_type type, const char *const *text_ids, int count)
{
    for (int i = 0; i < count; i++) {
        if (type_matches(type, text_ids[i])) {
            return 1;
        }
    }
    return 0;
}

static building_type clone_target_for_gate(building_type type)
{
    struct gate_clone_mapping {
        const char *gate;
        const char *wall;
    };
    static const gate_clone_mapping mappings[] = {
        {"garden_wall_gate", "roofed_garden_wall"},
        {"panelled_garden_gate", "panelled_garden_wall"},
        {"looped_garden_gate", "looped_garden_wall"},
        {"hedge_gate_light", "hedge_light"},
        {"hedge_gate_dark", "hedge_dark"},
        {"palisade_gate", "palisade"},
    };
    for (const gate_clone_mapping &mapping : mappings) {
        if (type_matches(type, mapping.gate)) {
            return xml_type(mapping.wall);
        }
    }
    return BUILDING_NONE;
}

/**
 * Takes a building and retrieve its proper type for cloning.
 * For example, given a fort, return the enumaration value corresponding to
 * the specific type of fort rather than the general value
 *
 * @param b Building to examine (can be null for destroyed building)
 * @param clone_type Type of the building to clone (can be original building type before a fire)
 * @return the building_type value to clone, or BUILDING_NONE if not cloneable
 */
static building_type get_clone_type_from_building(const Building *b, building_type clone_type)
{
    if (building_is_house(clone_type)) {
        return building_type_registry_get_vacant_lot_fill_type();
    }

    if (clone_type == xml_type("reservoir")) {
        return xml_type("draggable_reservoir");
    }

    static const char *const native_text_ids[] = {
        "native_crops",
        "native_hut",
        "native_hut_alt",
        "native_meeting",
        "native_decor",
        "native_monument",
        "native_watchtower",
    };
    if (type_matches_any(clone_type, native_text_ids, sizeof(native_text_ids) / sizeof(native_text_ids[0]))) {
        return BUILDING_NONE;
    }

    if (type_matches(clone_type, "burning_ruin")) {
        if (b && b->id()) {
            Building before_fire = Building::from_id(map_building_rubble_building_id(b->grid_offset()));
            building_type type = building_clone_type_from_building_type(before_fire.rubble_original_type_id());
            return type;
        }
        return BUILDING_NONE;
    }

    building_type gate_target = clone_target_for_gate(clone_type);
    if (gate_target != BUILDING_NONE) {
        return gate_target;
    }
    return clone_type;
}

building_type building_clone_type_from_building_type(building_type type)
{
    return get_clone_type_from_building(nullptr, type);
}

int building_clone_rotation_from_grid_offset(int grid_offset)
{
    int building_id = map_building_at(grid_offset);
    if (!building_id) {
        return 0;
    }
    Building b = Building::from_id(building_id).main();
    if (!b.id()) {
        return 0;
    }
    if (building_variant_has_variants(b.type_id())) {
        return b.variant();
    } else if (b.orientation()) {
        return b.orientation();
    } else {
        return 0;
    }
}

building_type building_clone_type_from_grid_offset(int grid_offset)
{
    int terrain = map_terrain_get(grid_offset);

    if (terrain & TERRAIN_BUILDING) {
        int building_id = map_building_at(grid_offset);
        if (building_id) {
            Building b = Building::from_id(building_id).main();
            return get_clone_type_from_building(&b, b.type_id());
        }
    } else if (terrain & TERRAIN_RUBBLE) {
        Building old_building = Building::from_id(map_building_rubble_building_id(grid_offset));
        building_type type = building_clone_type_from_building_type(old_building.rubble_original_type_id());
        return type;
    } else if (terrain & TERRAIN_AQUEDUCT) {
        return xml_type("aqueduct");
    } else if (terrain & TERRAIN_WALL) {
        return xml_type("wall");
    } else if (terrain & TERRAIN_GARDEN) {
        return map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset) ?
            xml_type("overgrown_gardens") : xml_type("gardens");
    } else if (terrain & TERRAIN_ROAD) {
        if (terrain & TERRAIN_WATER) {
            if (map_sprite_bridge_at(grid_offset) > 6) {
                return xml_type("ship_bridge");
            }
            return xml_type("low_bridge");
        } else if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
            return xml_type("plaza");
        }
        return xml_type("road");
    } else if (terrain & TERRAIN_HIGHWAY) {
        return xml_type("highway");
    }

    return BUILDING_NONE;
}
