#include "clone.h"

#include "building/building.h"
#include "figure/figure.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/variant.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"

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
        if (building_type_registry_impl::type_attr_is(type, mapping.gate)) {
            return building_type_registry_impl::type_from_attr(mapping.wall);
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
        return building_type_registry_impl::vacant_lot_fill_type();
    }

    if (building_type_registry_impl::type_attr_is(clone_type, "reservoir")) {
        return building_type_registry_impl::type_from_attr("draggable_reservoir");
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
    if (building_type_registry_impl::type_attr_is_any(
        clone_type, native_text_ids, sizeof(native_text_ids) / sizeof(native_text_ids[0]))) {
        return BUILDING_NONE;
    }

    if (building_type_registry_impl::type_attr_is(clone_type, "burning_ruin")) {
        if (b && b->id()) {
            building *before_fire = building_get(map_building_rubble_building_id(b->grid_offset()));
            building_type type = before_fire ?
                building_clone_type_from_building_type(static_cast<building_type>(before_fire->data.rubble.og_type)) :
                BUILDING_NONE;
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
    Building b = Building(building_get(building_id)).main();
    if (!b.id()) {
        return 0;
    }
    building_type type = b.type ? b.type->type() : BUILDING_NONE;
    if (building_variant_has_variants(type)) {
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
            Building b = Building(building_get(building_id)).main();
            building_type type = b.type ? b.type->type() : BUILDING_NONE;
            return get_clone_type_from_building(&b, type);
        }
    } else if (terrain & TERRAIN_RUBBLE) {
        building *old_building = building_get(map_building_rubble_building_id(grid_offset));
        return old_building ?
            building_clone_type_from_building_type(static_cast<building_type>(old_building->data.rubble.og_type)) :
            BUILDING_NONE;
    } else if (terrain & TERRAIN_AQUEDUCT) {
        return building_type_registry_impl::type_from_attr("aqueduct");
    } else if (terrain & TERRAIN_WALL) {
        return building_type_registry_impl::type_from_attr("wall");
    } else if (terrain & TERRAIN_GARDEN) {
        return map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset) ?
            building_type_registry_impl::type_from_attr("overgrown_gardens") :
            building_type_registry_impl::type_from_attr("gardens");
    } else if (terrain & TERRAIN_ROAD) {
        if (terrain & TERRAIN_WATER) {
            return building_type_registry_impl::type_from_roadblock_bridge(map_sprite_bridge_at(grid_offset) > 6 ?
                building_type_registry_impl::RoadblockBridgeType::Ship :
                building_type_registry_impl::RoadblockBridgeType::Low);
        } else if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
            return building_type_registry_impl::type_from_attr("plaza");
        }
        return building_type_registry_impl::type_from_attr("road");
    } else if (terrain & TERRAIN_HIGHWAY) {
        return building_type_registry_impl::type_from_attr("highway");
    }

    return BUILDING_NONE;
}
