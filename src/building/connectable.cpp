#include "figure/figure.h"
#include "building/building_record.h"
#include "connectable.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/construction.h"
#include "building/rotation.h"
#include "building/water_access_type.h"
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
    const unsigned char option_for_orientation[4];
    const int rotation;
    const unsigned char terrain_tiles[MAX_TILES];
    const int use_terrain;
    const int max_random;
} connectable_graphics_pattern;

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
    "aqueduct",
};

// 0 = no match
// 1 = match
// 2 = don't care

// For rotation
// -1 any, otherwise shown value

static const connectable_graphics_pattern hedge_patterns[18] = {
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

static const connectable_graphics_pattern path_intersection_patterns[9] = {
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

static const connectable_graphics_pattern tree_path_patterns[8] = {
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, {  0, 68,  0, 68 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, { 68,  0, 68,  0 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0, 68,  0, 68 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 68,  0, 68,  0 }, -1 },
};

static const connectable_graphics_pattern treeless_path_patterns[8] = {
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, { 140, 0,  140,  0 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, {  0, 140,  0, 140 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, { 140, 0,  140,  0 },  0 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  0, 140,  0, 140 }, -1 },
};

static const connectable_graphics_pattern garden_gate_patterns[14] = {
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

static const connectable_graphics_pattern palisade_patterns[18] = {
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

static const connectable_graphics_pattern aqueduct_patterns[16] = {
    { { 1, 2, 1, 2, 0, 2, 0, 2 }, {  4,  7,  6,  5 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 0, 2 }, {  5,  4,  7,  6 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 1, 2 }, {  6,  5,  4,  7 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 1, 2 }, {  7,  6,  5,  4 }, -1 },
    { { 1, 2, 0, 2, 1, 2, 0, 2 }, {  2,  3,  2,  3 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 1, 2 }, {  3,  2,  3,  2 }, -1 },
    { { 1, 2, 0, 2, 0, 2, 0, 2 }, {  2,  3,  2,  3 }, -1 },
    { { 0, 2, 1, 2, 0, 2, 0, 2 }, {  3,  2,  3,  2 }, -1 },
    { { 0, 2, 0, 2, 1, 2, 0, 2 }, {  2,  3,  2,  3 }, -1 },
    { { 0, 2, 0, 2, 0, 2, 1, 2 }, {  3,  2,  3,  2 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 0, 2 }, { 10, 13, 12, 11 }, -1 },
    { { 0, 2, 1, 2, 1, 2, 1, 2 }, { 11, 10, 13, 12 }, -1 },
    { { 1, 2, 0, 2, 1, 2, 1, 2 }, { 12, 11, 10, 13 }, -1 },
    { { 1, 2, 1, 2, 0, 2, 1, 2 }, { 13, 12, 11, 10 }, -1 },
    { { 1, 2, 1, 2, 1, 2, 1, 2 }, { 14, 14, 14, 14 }, -1 },
    { { 2, 2, 2, 2, 2, 2, 2, 2 }, {  2,  2,  2,  2 }, -1 },
};

static struct {
    const connectable_graphics_pattern *patterns;
    int size;
} pattern_sets[] = {
    { hedge_patterns, 18 },
    { hedge_patterns, 18 },
    { tree_path_patterns, 8 },
    { path_intersection_patterns, 9 },
    { treeless_path_patterns, 8 },
    { hedge_patterns, 18 },
    { garden_gate_patterns, 14},
    { palisade_patterns, 18 },
    { aqueduct_patterns, 16 },
};

static const int *aqueduct_preview_grid_offsets = nullptr;
static int aqueduct_preview_grid_offset_count = 0;

void building_connectable_set_aqueduct_preview(const int *grid_offsets, int count)
{
    aqueduct_preview_grid_offsets = grid_offsets;
    aqueduct_preview_grid_offset_count = grid_offsets && count > 0 ? count : 0;
}

static int aqueduct_preview_contains(int grid_offset)
{
    for (int i = 0; i < aqueduct_preview_grid_offset_count; i++) {
        if (aqueduct_preview_grid_offsets[i] == grid_offset) {
            return 1;
        }
    }
    return 0;
}

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

building_type building_connectable_preview_type(building_type type, int grid_offset)
{
    const building_type gate_type = static_cast<building_type>(building_connectable_gate_type(type));
    return gate_type != BUILDING_NONE && map_terrain_is(grid_offset, TERRAIN_ROAD) ? gate_type : type;
}

static int pattern_matches_neighbors(const connectable_graphics_pattern *pattern,
    const int tiles[MAX_TILES], int rotation, int terrain_tiles[MAX_TILES])
{
    for (int i = 0; i < MAX_TILES; i++) {
        if (pattern->use_terrain) {
            if (pattern->terrain_tiles[i] != 2 && terrain_tiles[i] != pattern->terrain_tiles[i]) {
                return 0;
            }
        }
        if (pattern->tiles[i] != 2 && tiles[i] != pattern->tiles[i]) {
            return 0;
        }
    }

    return pattern->rotation == -1 || pattern->rotation == rotation;
}

static int select_graphics_option(int group, int tiles[MAX_TILES], int rotation, int terrain_tiles[MAX_TILES], int grid_offset)
{
    const connectable_graphics_pattern *patterns = pattern_sets[group].patterns;
    int size = pattern_sets[group].size;
    for (int i = 0; i < size; i++) {
        if (pattern_matches_neighbors(&patterns[i], tiles, rotation, terrain_tiles)) {
            int option = patterns[i].option_for_orientation[city_view_orientation() / 2];
            if (patterns[i].max_random) {
                option += map_random_get(grid_offset) % patterns[i].max_random;
            }
            return option;
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
    return select_graphics_option(CONTEXT_HEDGES, tiles, rotation, 0, grid_offset);
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
    return select_graphics_option(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
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
    return select_graphics_option(CONTEXT_COLONNADE, tiles, rotation, 0, grid_offset);
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
    return select_graphics_option(CONTEXT_GARDEN_WALLS, tiles, rotation, 0, grid_offset);
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
    return select_graphics_option(context, tiles, rotation, 0, grid_offset);
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
    return select_graphics_option(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
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
    return select_graphics_option(CONTEXT_PALISADES, tiles, rotation, 0, grid_offset);
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
    return select_graphics_option(CONTEXT_GARDEN_GATE, tiles, rotation, terrain_tiles, grid_offset);
}

static int building_has_aqueduct_connection_node_at(const Building &building, int aqueduct_grid_offset)
{
    Building *owner = building.Composition ? building.Composition->owner() : const_cast<Building *>(&building);
    const ::building *record = owner ? owner->record() : nullptr;
    if (!record || !owner || !owner->type) {
        return 0;
    }

    const uint8_t aqueduct_mask = building_type_registry_impl::water_access_mask_from_text("aqueduct");
    const int aqueduct_x = map_grid_offset_to_x(aqueduct_grid_offset);
    const int aqueduct_y = map_grid_offset_to_y(aqueduct_grid_offset);
    const auto &water = owner->type->water_access();
    for (const building_type_registry_impl::WaterAccessProvideRule &rule : water.provide_rules()) {
        if (rule.origin != building_type_registry_impl::WaterAccessOrigin::Nodes ||
            !(rule.mask & aqueduct_mask)) {
            continue;
        }
        for (const building_type_registry_impl::WaterAccessNode &node : water.provider_nodes()) {
            if (record->x + node.x == aqueduct_x && record->y + node.y == aqueduct_y) {
                return 1;
            }
        }
    }
    return 0;
}

static int aqueduct_connects_to_tile(int grid_offset, int offset)
{
    if (aqueduct_preview_contains(offset)) {
        return 1;
    }
    if (map_terrain_is(offset, TERRAIN_AQUEDUCT)) {
        return 1;
    }
    if (map_property_is_constructing(offset) &&
        building_type_registry_impl::type_attr_is(building_construction_type(), "aqueduct")) {
        return 1;
    }
    return map_building_exists_at(offset) &&
        building_has_aqueduct_connection_node_at(map_building_at(offset), grid_offset);
}

int building_connectable_get_aqueduct_offset(int grid_offset)
{
    int tiles[MAX_TILES] = { 0 };
    for (int i = 0; i < MAX_TILES; i += 2) {
        const int offset = grid_offset + map_grid_direction_delta(i);
        if (aqueduct_connects_to_tile(grid_offset, offset)) {
            tiles[i] = 1;
        }
    }
    return select_graphics_option(CONTEXT_AQUEDUCT, tiles, 0, 0, grid_offset);
}

static int dense_gate_graphics_option(int selected_option)
{
    return selected_option <= 0 ? 0 : 1;
}

static int dense_path_graphics_option(int selected_option)
{
    return selected_option == 0 ? 9 : 10;
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
    if (building_type_registry_impl::type_attr_is(type, "aqueduct")) {
        return building_connectable_get_aqueduct_offset(grid_offset);
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
    if (building_type_registry_impl::type_attr_is(type, "aqueduct")) {
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
    for (Building *runtime_building = Building::first_of_type(type);
         runtime_building;
         runtime_building = runtime_building->next_of_type()) {
        building *b = const_cast<::building *>(runtime_building->record());
        if (!b) {
            continue;
        }
        if (b->state != BUILDING_STATE_CREATED &&
            b->state != BUILDING_STATE_IN_USE &&
            b->state != BUILDING_STATE_MOTHBALLED) {
            continue;
        }
        if (!map_building_exists_at(b->grid_offset) ||
            map_building_at(b->grid_offset).id != runtime_building->id) {
            continue;
        }
        runtime_building->refresh_graphic();
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
