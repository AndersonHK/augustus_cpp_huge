#include "figure/figure.h"
#include "building/building_record.h"
#include "connectable.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/construction.h"
#include "building/image.h"
#include "building/rotation.h"
#include "city/view.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"
#include "map/tiles.h"

#include <string_view>

#define MAX_TILES 8

typedef struct {
    const unsigned char tiles[MAX_TILES];
    const unsigned char offset_for_orientation[4];
    const int rotation;
    const unsigned char terrain_tiles[MAX_TILES];
    const int use_terrain;
    const int max_random;
} building_image_context;

static constexpr std::string_view connectable_buildings[] = {
    "hedge_dark",
    "hedge_light",
    "colonnade",
    "garden_path",
    "date_path",
    "elm_path",
    "fig_path",
    "fir_path",
    "oak_path",
    "palm_path",
    "pine_path",
    "plum_path",
    "looped_garden_wall",
    "roofed_garden_wall",
    "panelled_garden_wall",
    "looped_garden_gate",
    "garden_wall_gate",
    "panelled_garden_gate",
    "palisade",
    "wall",
    "hedge_gate_dark",
    "hedge_gate_light",
    "palisade_gate",
};

// 0 = no match
// 1 = match
// 2 = don't care

// For rotation
// -1 any, otherwise shown value

static const  building_image_context building_images_hedges[18] = {
    { { 1, 2, 1, 2, 0, 2, 0, 2 }, {  4,  5,  2,  3 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 0, 2 }, {  3,  4,  5,  2 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 1, 2 }, {  2,  3,  4,  5 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 1, 2 }, {  5,  2,  3,  4 }, -1 },
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, {  1,  0,  1,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, {  0,  1,  0,  1 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, {  1,  0,  1,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, {  0,  1,  0,  1 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, {  1,  0,  1,  0 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, {  0,  1,  0,  1 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 0, 2 }, {  9,  7,  6,  8 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 1, 2 }, {  8,  9,  7,  6 }, -1 },
    { { 1, 2, 0, 2, 1, 2, 1, 2 }, {  6,  8,  9,  7 }, -1 },
    { { 1, 2, 1, 2, 0, 2, 1, 2 }, {  7,  6,  8,  9 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 1, 2 }, { 10, 10, 10, 10 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  1,  0,  1,  0 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0,  1,  0,  1 },  1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 10, 10, 10, 10 }, -1 },
};

static const building_image_context building_images_path_intersection[9] = {
    { { 1, 2, 1, 2, 0, 2, 0, 2 }, { 2, 3, 0, 1 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 0, 2 }, { 1, 2, 3, 0 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 1, 2 }, { 0, 1, 2, 3 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 1, 2 }, { 3, 0, 1, 2 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 0, 2 }, { 5, 6, 7, 4 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 1, 2 }, { 4, 5, 6, 7 }, -1 },
    { { 1, 2, 0, 2, 1, 2, 1, 2 }, { 7, 4, 5, 6 }, -1 },
    { { 1, 2, 1, 2, 0, 2, 1, 2 }, { 6, 7, 4, 5 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 1, 2 }, { 8, 8, 8, 8 }, -1 },
};

static const building_image_context building_images_tree_path[8] = {
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0, 68,  0, 68 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 68,  0, 68,  0 }, -1 },
};

static const building_image_context building_images_treeless_path[8] = {
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 140, 0,  140,  0 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0, 140,  0, 140 }, -1 },
};

static const building_image_context building_images_garden_gate[14] = {
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, { 2, 0, 2, 0 }, -1,},
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, { 0, 2, 0, 2 }, -1,},
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, { 2, 0, 2, 0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, { 0, 2, 0, 2 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, { 2, 0, 2, 0 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, { 0, 2, 0, 2 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 2, 0, 2 }, -1, { 1, 2, 0, 2, 1, 2, 0, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 0, 2, 0 }, -1, { 0, 2, 1, 2, 0, 2, 1, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 2, 0, 2 }, -1, { 1, 2, 0, 2, 0, 2, 0, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 0, 2, 0 }, -1, { 0, 2, 1, 2, 0, 2, 0, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 2, 0, 2 }, -1, { 0, 2, 0, 2, 1, 2, 0, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 0, 2, 0 }, -1, { 0, 2, 0, 2, 0, 2, 1, 2 }, 1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 2, 0, 2, 0 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 0, 2, 0, 2 }, -1 },
};

static const  building_image_context building_images_palisades[18] = {
    { { 1, 2, 1, 2, 0, 2, 0, 2 }, {  15,  14,  13,  12 }, -1, { 0 }, 0, 0 },
    { { 0, 2, 1, 2, 1, 2, 0, 2 }, {  12,  15,  14,  13 }, -1, { 0 }, 0, 0 },
    { { 0, 2, 0, 2, 1, 2, 1, 2 }, {  13,  12,  15,  14 }, -1, { 0 }, 0, 0 },
    { { 1, 2, 0, 2, 0, 2, 1, 2 }, {  14,  13,  12,  15 }, -1, { 0 }, 0, 0 },
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, {  6,  0,  6,  0 }, -1, { 0 }, 0, 6 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, {  0,  6,  0,  6 }, -1, { 0 }, 0, 6 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, {  6,  0,  6,  0 }, -1, { 0 }, 0, 6 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, {  0,  6,  0,  6 }, -1, { 0 }, 0, 6 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, {  6,  0,  6,  0 }, -1, { 0 }, 0, 6 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, {  0,  6,  0,  6 }, -1, { 0 }, 0, 6 },
    { { 1, 2, 1, 2, 1, 2, 0, 2 }, {  19,  17,  16,  18 }, -1, { 0 }, 0, 0 },
    { { 0, 2, 1, 2, 1, 2, 1, 2 }, {  18,  19,  17,  16 }, -1, { 0 }, 0, 0 },
    { { 1, 2, 0, 2, 1, 2, 1, 2 }, {  16,  18,  19,  17 }, -1, { 0 }, 0, 0 },
    { { 1, 2, 1, 2, 0, 2, 1, 2 }, {  17,  16,  18,  19 }, -1, { 0 }, 0, 0 },
    { { 1, 2, 1, 2, 1, 2, 1, 2 }, { 20, 20, 20, 20 }, -1, { 0 }, 0, 0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  6,  0,  6,  0 },  0, { 0 }, 0, 6 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0,  6,  0,  6 },  1, { 0 }, 0, 6 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 20, 20, 20, 20 }, -1, { 0 }, 0, 0 },
};

static struct {
    const building_image_context *context;
    int size;
} context_pointers[] = {
    { building_images_hedges, 18 },
    { building_images_hedges, 18 },
    { building_images_tree_path, 8 },
    { building_images_path_intersection, 9 },
    { building_images_treeless_path, 8 },
    { building_images_hedges, 18 },
    { building_images_garden_gate, 14},
    { building_images_palisades, 18 },
};

int building_connectable_gate_type(building_type type)
{
    struct gate_mapping {
        const char *wall;
        const char *gate;
    };
    static const gate_mapping mappings[] = {
        {"looped_garden_wall", "looped_garden_gate"},
        {"panelled_garden_wall", "panelled_garden_gate"},
        {"roofed_garden_wall", "garden_wall_gate"},
        {"hedge_dark", "hedge_gate_dark"},
        {"hedge_light", "hedge_gate_light"},
        {"palisade", "palisade_gate"},
    };
    for (const gate_mapping &mapping : mappings) {
        if (building_type_registry_impl::type_attr_is(type, mapping.wall)) {
            return building_type_registry_impl::type_from_attr(mapping.gate);
        }
    }
    return 0;
}

static int context_matches_tiles(const building_image_context *context,
    const int tiles[MAX_TILES], int rotation, int terrain_tiles[MAX_TILES])
{
    for (int i = 0; i < MAX_TILES; i++) {
        if (context->use_terrain) {
            if (context->terrain_tiles[i] != 2 && terrain_tiles[i] != context->terrain_tiles[i]) {
                return 0;
            }
        }
        if (context->tiles[i] != 2 && tiles[i] != context->tiles[i]) {
            return 0;
        }
    }

    return context->rotation == -1 || context->rotation == rotation;
}

static int get_image_offset(int group, int tiles[MAX_TILES], int rotation, int terrain_tiles[MAX_TILES], int grid_offset)
{
    const building_image_context *context = context_pointers[group].context;
    int size = context_pointers[group].size;
    for (int i = 0; i < size; i++) {
        if (context_matches_tiles(&context[i], tiles, rotation, terrain_tiles)) {
            int offset = context[i].offset_for_orientation[city_view_orientation() / 2];
            if (context[i].max_random) {
                offset += map_random_get(grid_offset) % context[i].max_random;
            }
            return offset;
        }
    }
    return -1;
}

static int is_hedge_wall_or_gate(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
        "hedge_dark",
        "hedge_gate_dark",
        "hedge_light",
        "hedge_gate_light",
    });
}

static int is_hedge_wall(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {"hedge_dark", "hedge_light"});
}

static building_type map_runtime_building_type_at(int grid_offset)
{
    if (!map_building_exists_at(grid_offset)) {
        return BUILDING_NONE;
    }
    const Building &building = map_building_at(grid_offset);
    return building.type ? building.type->type() : BUILDING_NONE;
}

static int map_runtime_building_rotation_or_default(int grid_offset, int rotation_limit)
{
    if (map_building_exists_at(grid_offset)) {
        return map_building_at(grid_offset).orientation();
    }
    return building_rotation_get_rotation_with_limit(rotation_limit);
}

int building_connectable_get_hedge_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);
        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_hedge_wall_or_gate(type) ||
            (map_property_is_constructing(offset) && (is_hedge_wall(building_construction_type())))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES);
    return get_image_offset(CONTEXT_HEDGES, tiles, rotation, 0, grid_offset);
}

int building_connectable_get_hedge_gate_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    int terrain_tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);

        if (map_terrain_is(offset, TERRAIN_ROAD)) {
            terrain_tiles[i] = 1;
        }

        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_hedge_wall(type) || (map_property_is_constructing(offset) && !map_terrain_is(offset, TERRAIN_ROAD)
            && is_hedge_wall(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS);
    return get_image_offset(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
}

int building_connectable_get_colonnade_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);
        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (building_type_registry_impl::type_attr_is(type, "colonnade") ||
            (map_property_is_constructing(offset) &&
                building_type_registry_impl::type_attr_is(building_construction_type(), "colonnade"))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES);
    return get_image_offset(CONTEXT_COLONNADE, tiles, rotation, 0, grid_offset);
}

static int is_garden_path(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
        "date_path",
        "elm_path",
        "fig_path",
        "fir_path",
        "oak_path",
        "palm_path",
        "pine_path",
        "plum_path",
        "garden_path",
    });
}

static int is_garden_wall_or_gate(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
        "looped_garden_wall",
        "roofed_garden_wall",
        "garden_wall_gate",
        "looped_garden_gate",
        "panelled_garden_wall",
        "panelled_garden_gate",
    });
}

static int is_garden_wall(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {
        "looped_garden_wall",
        "roofed_garden_wall",
        "panelled_garden_wall",
    });
}

int building_connectable_get_garden_wall_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);
        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_garden_wall_or_gate(type) || (map_property_is_constructing(offset) && is_garden_wall_or_gate(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES);
    return get_image_offset(CONTEXT_GARDEN_WALLS, tiles, rotation, 0, grid_offset);
}

int building_connectable_get_garden_path_offset(int grid_offset, int context)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);
        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_garden_path(type) || (map_property_is_constructing(offset) && is_garden_path(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS);
    return get_image_offset(context, tiles, rotation, 0, grid_offset);
}

int building_connectable_get_garden_gate_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    int terrain_tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);

        if (map_terrain_is(offset, TERRAIN_ROAD)) {
            terrain_tiles[i] = 1;
        }

        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_garden_wall(type) ||
            (map_property_is_constructing(offset) && !map_terrain_is(offset, TERRAIN_ROAD) && is_garden_wall(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS);
    return get_image_offset(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
}

static int is_palisade_wall_or_gate(building_type type)
{
    return building_type_registry_impl::type_attr_is_any(type, {"palisade", "palisade_gate"});
}

static int is_palisade_wall(building_type type)
{
    return building_type_registry_impl::type_attr_is(type, "palisade");
}

int building_connectable_get_palisade_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);
        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_palisade_wall_or_gate(type) ||
            (map_property_is_constructing(offset) && is_palisade_wall_or_gate(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES);
    return get_image_offset(CONTEXT_PALISADES, tiles, rotation, 0, grid_offset);
}

int building_connectable_get_palisade_gate_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    int terrain_tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        int offset = grid_offset + map_grid_direction_delta(i);

        if (map_terrain_is(offset, TERRAIN_ROAD)) {
            terrain_tiles[i] = 1;
        }

        if (!map_terrain_is(offset, TERRAIN_BUILDING) && !map_property_is_constructing(offset)) {
            continue;
        }
        building_type type = map_runtime_building_type_at(offset);
        if (is_palisade_wall(type) || (map_property_is_constructing(offset) && !map_terrain_is(offset, TERRAIN_ROAD)
            && is_palisade_wall(building_construction_type()))) {
            tiles[i] = 1;
        }
    }
    int rotation = map_runtime_building_rotation_or_default(grid_offset, BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS);
    return get_image_offset(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
}

static int dense_gate_graphics_option(int legacy_offset)
{
    return legacy_offset <= 0 ? 0 : 1;
}

static int dense_path_graphics_option(int legacy_offset)
{
    return legacy_offset == 0 ? 9 : 10;
}

int building_connectable_graphics_option(const Building &building_obj)
{
    if (!building_obj.type) {
        return 0;
    }

    building_type type = building_obj.type->type();
    int grid_offset = building_obj.grid_offset();
    if (is_hedge_wall(type)) {
        return building_connectable_get_hedge_offset(grid_offset);
    }
    if (building_type_registry_impl::type_attr_is(type, "colonnade")) {
        return building_connectable_get_colonnade_offset(grid_offset);
    }
    if (building_type_registry_impl::type_attr_is(type, "wall")) {
        return map_tiles_wall_image_offset(grid_offset);
    }
    if (is_garden_wall(type)) {
        return building_connectable_get_garden_wall_offset(grid_offset);
    }
    if (is_garden_path(type)) {
        int intersection_option = building_connectable_get_garden_path_offset(
            grid_offset,
            CONTEXT_GARDEN_PATH_INTERSECTION);
        if (intersection_option >= 0) {
            return intersection_option;
        }
        int context = building_type_registry_impl::type_attr_is(type, "garden_path") ?
            CONTEXT_GARDEN_TREELESS_PATH : CONTEXT_GARDEN_TREE_PATH;
        return dense_path_graphics_option(building_connectable_get_garden_path_offset(grid_offset, context));
    }
    if (is_garden_wall_or_gate(type)) {
        return dense_gate_graphics_option(building_connectable_get_garden_gate_offset(grid_offset));
    }
    if (is_hedge_wall_or_gate(type)) {
        return dense_gate_graphics_option(building_connectable_get_hedge_gate_offset(grid_offset));
    }
    if (is_palisade_wall(type)) {
        return building_connectable_get_palisade_offset(grid_offset);
    }
    if (building_type_registry_impl::type_attr_is(type, "palisade_gate")) {
        return dense_gate_graphics_option(building_connectable_get_palisade_gate_offset(grid_offset));
    }
    return 0;
}

int building_is_connectable(building_type type)
{
    for (std::string_view attr : connectable_buildings) {
        if (building_type_registry_impl::type_attr_is(type, attr)) {
            return 1;
        }
    }
    return 0;
}

int building_connectable_num_variants(building_type type)
{
    if (!building_is_connectable(type)) {
        return 0;
    }
    if (building_type_registry_impl::type_attr_is_any(type, {
        "hedge_dark",
        "hedge_light",
        "colonnade",
        "looped_garden_wall",
        "roofed_garden_wall",
        "panelled_garden_wall",
        "palisade",
        "wall",
    })) {
        return BUILDING_CONNECTABLE_ROTATION_LIMIT_HEDGES;
    }
    return BUILDING_CONNECTABLE_ROTATION_LIMIT_PATHS;
}

void building_connectable_update_connections_for_type(building_type type)
{
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_RUBBLE) {
            continue;
        }
        if (Building(b).refresh_graphic_if_native()) {
            continue;
        }
        int image_id;
        Building building_obj(b);
        building_type type = building_obj.type ? building_obj.type->type() : BUILDING_NONE;
        if (building_connectable_gate_type(type) && map_terrain_is(b->grid_offset, TERRAIN_ROAD)) {
            image_id = building_image_get_garden_gate_image(b->grid_offset);
        } else {
            image_id = building_image_get(b);
        }
        map_image_set(b->grid_offset, image_id);
    }
}

void building_connectable_update_connections(void)
{
    for (std::string_view attr : connectable_buildings) {
        building_type type = building_type_registry_impl::type_from_attr(attr);
        if (type != BUILDING_NONE) {
            building_connectable_update_connections_for_type(type);
        }
    }
}
