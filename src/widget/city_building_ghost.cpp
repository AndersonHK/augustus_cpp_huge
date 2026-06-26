#include "building/building_type.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/granary.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "building/water_access_runtime.h"
#include "figure/roamer_preview.h"
#include "figuretype/animal.h"
#include "game/state.h"
#include "graphics/image.h"
#include "graphics/runtime_overlay_images.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/image_context.h"
#include "map/orientation.h"
#include "map/road_aqueduct.h"
#include "map/tiles.h"
#include "map/water.h"
#include "map/water_supply.h"
#include "widget/city.h"
#include "widget/city_bridge.h"
#include "widget/city_water_ghost.h"

#include "city_building_ghost.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/construction_plan.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"


#include "assets/assets.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/building_type_api.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/view.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "graphics/renderer.h"
#include "input/scroll.h"
#include "map/data.h"
#include "map/desirability.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/routing_terrain.h"
#include "map/sprite.h"
#include "map/terrain.h"

#include <cstring>

// Note: If we ever end up creating larger buildings than 7 * 7, we should update this
#define MAX_TILES (7 * 7)

#define GRID_OFFSET(x, y) ((x) + GRID_SIZE * (y))
#define X_VIEW_OFFSET(x, y) (((x) - (y)) * 30)
#define Y_VIEW_OFFSET(x, y) (((x) + (y)) * 15)

typedef struct {
    int x;
    int y;
} tile_xy_offsets;

enum {
    TILE_FORBIDDEN = 1,
    TILE_ALLOWED = 0,
    TILE_DISCOURAGED = -1
};

static const int RESERVOIR_GRID_OFFSETS[4] = {
    GRID_OFFSET(-1, -1), GRID_OFFSET(1, -1), GRID_OFFSET(1, 1), GRID_OFFSET(-1, 1)
};

static struct {
    struct {
        int blocked;
    } reservoir_range;
    building ghost_building;
    float scale;
    struct {
        int grid_offset;
        building_type type;
    } roamer_preview;
    struct {
        building_type type;
        int cursor;
    } animation_preview;
    tile_xy_offsets offsets[4][MAX_TILES];
} data = {
    .scale = SCALE_NONE
};

static const building_type_registry_impl::BuildingType *building_type_definition_from_attr(const char *text_id)
{
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && std::strcmp(definition->attr(), text_id) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

static building_type building_type_from_attr(const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition = building_type_definition_from_attr(text_id);
    return definition ? definition->type() : BUILDING_NONE;
}

static int building_type_attr_is(building_type type, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

static int building_type_attr_is_any(building_type type, const char *const *text_ids, int text_id_count)
{
    for (int i = 0; i < text_id_count; ++i) {
        if (building_type_attr_is(type, text_ids[i])) {
            return 1;
        }
    }
    return 0;
}

static int is_plaza_type(building_type type)
{
    return building_type_attr_is(type, "plaza");
}

static int is_roadblock_type(building_type type)
{
    return building_type_attr_is(type, "roadblock");
}

static int is_bridge_type(building_type type)
{
    return building_type_attr_is(type, "low_bridge") || building_type_attr_is(type, "ship_bridge");
}

static int is_ship_bridge_type(building_type type)
{
    return building_type_attr_is(type, "ship_bridge");
}

static int definition_foundation_policy_is(
    const building_type_registry_impl::BuildingType &definition,
    const char *policy)
{
    return definition.foundation().has_policy() && std::strcmp(definition.foundation().policy(), policy) == 0;
}

static int is_waterside_definition(const building_type_registry_impl::BuildingType &definition)
{
    if (!definition.has_foundation()) {
        return 0;
    }
    const building_type_registry_impl::FoundationDefinition &foundation = definition.foundation();
    if (definition_foundation_policy_is(definition, "shoreline") ||
        definition_foundation_policy_is(definition, "water")) {
        return 1;
    }
    for (const building_type_registry_impl::FoundationCellDefinition &cell : foundation.cells()) {
        if (cell.requirement == building_type_registry_impl::FoundationCellRequirement::Water) {
            return 1;
        }
    }
    return 0;
}

static int is_waterside_type(building_type type)
{
    return is_waterside_definition(*building_type_registry_impl::definition_for_type(type));
}

static int is_definition_attr(const building_type_registry_impl::BuildingType &definition, const char *text_id)
{
    return std::strcmp(definition.attr(), text_id) == 0;
}

static int is_bridge_definition(const building_type_registry_impl::BuildingType &definition)
{
    return definition.roadblock().kind() == building_type_registry_impl::RoadblockKind::Bridge;
}

static int is_road_surface_type(building_type type)
{
    static const char *const text_ids[] = {
        "plaza",
        "roadblock",
        "garden_wall_gate",
        "panelled_garden_gate",
        "looped_garden_gate",
        "hedge_gate_dark",
        "hedge_gate_light",
        "palisade_gate",
        "gatehouse",
        "triumphal_arch"
    };
    return building_type_attr_is_any(type, text_ids, sizeof(text_ids) / sizeof(text_ids[0])) ||
        is_bridge_type(type);
}

static inline int view_offset_x(int index)
{
    return X_VIEW_OFFSET(data.offsets[0][index].x, data.offsets[0][index].y);
}

static inline int view_offset_y(int index)
{
    return Y_VIEW_OFFSET(data.offsets[0][index].x, data.offsets[0][index].y);
}

static inline int tile_grid_offset(int orientation, int index)
{
    return GRID_OFFSET(data.offsets[orientation][index].x, data.offsets[orientation][index].y);
}

static int is_reservoir_side_connection_tile(int tile_no)
{
    // The 3x3 ghost uses a diagonal/isometric index order, so these five
    // indices correspond to the center row/column where aqueducts may pass.
    return tile_no == 1 ||
        tile_no == 2 ||
        tile_no == 3 ||
        tile_no == 6 ||
        tile_no == 7;
}

static int force_place_can_clear_terrain(int terrain)
{
    return terrain && !(terrain & ~(TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD));
}

static int is_blocked_for_building(int grid_offset, int building_size, int *blocked_tiles, int check_figures)
{
    int orientation_index = city_view_orientation() / 2;
    int blocked = 0;
    int num_tiles = building_size * building_size;
    int force_place_active = building_construction_force_place_active();
    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = grid_offset + tile_grid_offset(orientation_index, i);
        int tile_blocked = 0;
        int blocked_terrain = map_terrain_get(tile_offset) & TERRAIN_NOT_CLEAR;
        if (blocked_terrain && (!force_place_active || !force_place_can_clear_terrain(blocked_terrain))) {
            tile_blocked = 1;
        }
        if (map_has_figure_at(tile_offset)) {
            tile_blocked = check_figures || force_place_active;
            figure_animal_try_nudge_at(grid_offset, tile_offset, building_size);
        }
        blocked_tiles[i] = tile_blocked;
        blocked += tile_blocked;
    }
    return blocked;
}

static int has_blocked_tiles(int num_tiles, int *blocked_tiles)
{
    for (int i = 0; i < num_tiles; i++) {
        if (blocked_tiles[i] != TILE_ALLOWED) {
            return 1;
        }
    }
    return 0;
}

static void draw_building_tiles(int x, int y, int num_tiles, int *blocked_tiles)
{
    for (int i = 0; i < num_tiles; i++) {
        int x_offset = x + view_offset_x(i);
        int y_offset = y + view_offset_y(i);
        if (blocked_tiles[i] == TILE_FORBIDDEN || blocked_tiles[i] == TILE_DISCOURAGED) {
            //FORBIDDEN means real problem, DISCOURAGED means suggested problem, like a road that will disappear.
            Image::blend_footprint_color(x_offset, y_offset, COLOR_MASK_RED, data.scale);
        } else {
            Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(x_offset, y_offset, COLOR_MASK_FOOTPRINT_GHOST, data.scale);
        }
    }
}

static void draw_building(int image_id, int x, int y, color_t color)
{
    Image::from_id(image_id).draw_isometric_footprint(x, y, color, data.scale);
    Image::from_id(image_id).draw_isometric_top(x, y, color, data.scale);
}

static int composed_placement_size(const building_type_registry_impl::BuildingType &definition)
{
    if (!definition.has_composition()) {
        return 0;
    }
    const building_type_registry_impl::ComposedBuildingDefinition &composition = definition.composition();
    return composition.footprint_width() > composition.footprint_height() ?
        composition.footprint_width() :
        composition.footprint_height();
}

static int ghost_building_size(const building_type_registry_impl::BuildingType &definition)
{
    int composed_size = composed_placement_size(definition);
    return composed_size > 0 ? composed_size : building_properties_for_type(definition.type())->size;
}

static void prepare_ghost_water_access_state(
    const building_type_registry_impl::BuildingType &definition,
    building &ghost)
{
    if (definition.water_access().has_requirements() || definition.has_water_access_provider()) {
        ghost.has_water_access = static_cast<unsigned char>(water_access_runtime_building_has_required_access(&ghost));
    }
}

static void prepare_ghost_building_with_size(
    int grid_offset,
    const building_type_registry_impl::BuildingType &definition,
    int building_size)
{
    const building_type type = definition.type();
    data.ghost_building = {};
    data.ghost_building.type = type;
    data.ghost_building.grid_offset = grid_offset;
    data.ghost_building.state = BUILDING_STATE_IN_USE;
    data.ghost_building.size = static_cast<unsigned char>(building_size);
    data.ghost_building.num_workers = model_get_building(type)->laborers;
    data.ghost_building.data.entertainment.days1 = 1;
    data.ghost_building.data.entertainment.days2 = 1;
    if (definition.is_granary()) {
        data.ghost_building.resources[RESOURCE_NONE] = FULL_GRANARY;
    }

    int building_x = map_grid_offset_to_x(grid_offset);
    int building_y = map_grid_offset_to_y(grid_offset);
    building_construction_offset_start_from_orientation(&building_x, &building_y, data.ghost_building.size);
    if (map_grid_is_inside(building_x, building_y, data.ghost_building.size)) {
        data.ghost_building.x = static_cast<unsigned char>(building_x);
        data.ghost_building.y = static_cast<unsigned char>(building_y);
        int desirability = map_desirability_get_max(building_x, building_y, data.ghost_building.size);
        if (desirability > 127) {
            desirability = 127;
        } else if (desirability < -128) {
            desirability = -128;
        }
        data.ghost_building.desirability = static_cast<signed char>(desirability);
        prepare_ghost_water_access_state(definition, data.ghost_building);
    }

    if (building_variant_has_variants(type)) {
        data.ghost_building.variant = building_rotation_get_rotation_with_limit(
            building_variant_get_number_of_variants(type));
    } else {
        data.ghost_building.variant = 0;
    }
    if (building_properties_for_type(type)->rotation_offset) {
        data.ghost_building.subtype.orientation = building_rotation_get_rotation();
    } else {
        data.ghost_building.subtype.orientation = 0;
    }
    if (is_waterside_definition(definition)) {
        int waterside_orientation_abs = -1;
        map_water_determine_orientation(building_x, building_y, data.ghost_building.size, 0,
            &waterside_orientation_abs, 0, 0, 0);
        if (waterside_orientation_abs >= 0) {
            data.ghost_building.subtype.orientation = static_cast<short>(waterside_orientation_abs);
            if (definition_foundation_policy_is(definition, "shoreline")) {
                data.ghost_building.data.dock.orientation = static_cast<signed char>(waterside_orientation_abs);
            }
        }
    }
}

static void prepare_ghost_building(int grid_offset, const building_type_registry_impl::BuildingType &definition)
{
    prepare_ghost_building_with_size(grid_offset, definition, ghost_building_size(definition));
}

static void draw_tile_view_offset(int building_size, int *x_offset, int *y_offset)
{
    int dx = 0;
    int dy = 0;
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            dy = building_size - 1;
            break;
        case DIR_2_RIGHT:
            break;
        case DIR_4_BOTTOM:
            dx = building_size - 1;
            break;
        case DIR_6_LEFT:
            dx = building_size - 1;
            dy = building_size - 1;
            break;
        default:
            break;
    }
    *x_offset = X_VIEW_OFFSET(dx, dy);
    *y_offset = Y_VIEW_OFFSET(dx, dy);
}

static void draw_runtime_ghost_animation(Building &building, int animation_cursor, int x, int y, color_t color)
{
    const building_type type = building.type ? building.type->type() : BUILDING_NONE;
    if (data.animation_preview.type != type) {
        data.animation_preview.type = type;
        data.animation_preview.cursor = 0;
    }

    // Ghosts reuse the real grid offset as an animation cursor. Save/restore the
    // map sprite byte so preview animation never leaks into the city map state.
    const int saved_cursor = map_sprite_animation_at(animation_cursor);
    map_sprite_animation_set(animation_cursor, data.animation_preview.cursor);
    building.draw_animation({ x, y, animation_cursor, color, data.scale, 1 });
    data.animation_preview.cursor = map_sprite_animation_at(animation_cursor);
    map_sprite_animation_set(animation_cursor, saved_cursor);
}

static void draw_runtime_ghost_record(
    const building_type_registry_impl::BuildingType &definition,
    building &record,
    int x,
    int y,
    color_t color)
{
    Building building(record, &definition);
    building.draw_footprint({ x, y, record.grid_offset, color, data.scale, 1 });
    building.draw_top({ x, y, record.grid_offset, color, data.scale, 1 });
    draw_runtime_ghost_animation(building, record.grid_offset, x, y, color);
}

static void prepare_composed_ghost_part(
    building &part_record,
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    const building &main_record)
{
    const building_type type = definition.type();
    const building_properties *props = building_properties_for_type(type);
    part_record = {};
    part_record.type = type;
    part_record.state = BUILDING_STATE_IN_USE;
    part_record.size = static_cast<unsigned char>(props ? props->size : 1);
    const model_building *model = model_get_building(type);
    part_record.num_workers = model ? model->laborers : 0;
    part_record.x = static_cast<unsigned char>(x);
    part_record.y = static_cast<unsigned char>(y);
    part_record.grid_offset = map_grid_offset(x, y);
    part_record.variant = main_record.variant;
    part_record.subtype.orientation = main_record.subtype.orientation;
    part_record.desirability = main_record.desirability;
    part_record.data.entertainment.days1 = 1;
    part_record.data.entertainment.days2 = 1;
    prepare_ghost_water_access_state(definition, part_record);
}

static void draw_runtime_single_building(
    const building_type_registry_impl::BuildingType &definition,
    int grid_offset,
    int x,
    int y,
    int building_size,
    color_t color)
{
    prepare_ghost_building_with_size(grid_offset, definition, building_size);
    int x_draw = 0;
    int y_draw = 0;
    draw_tile_view_offset(building_size, &x_draw, &y_draw);
    draw_runtime_ghost_record(definition, data.ghost_building, x + x_draw, y + y_draw, color);
}

static void composed_part_view_offset(
    const building &part_record,
    const building_type_registry_impl::ComposedPartOffset &offset,
    int *x,
    int *y)
{
    int tile_x = X_VIEW_OFFSET(offset.x, offset.y);
    int tile_y = Y_VIEW_OFFSET(offset.x, offset.y);
    int size_x = 0;
    int size_y = 0;
    draw_tile_view_offset(part_record.size, &size_x, &size_y);
    *x = tile_x + size_x;
    *y = tile_y + size_y;
}

static void draw_runtime_composed_building(
    const building_type_registry_impl::BuildingType &definition,
    int grid_offset,
    int x,
    int y,
    color_t color)
{
    const building_type type = definition.type();
    prepare_ghost_building(grid_offset, definition);
    building main_record = data.ghost_building;
    const int origin_x = main_record.x;
    const int origin_y = main_record.y;
    const int rotation = building_rotation_get_rotation();
    const building_type_registry_impl::ComposedBuildingDefinition &composition = definition.composition();
    const building_type_registry_impl::ComposedPartOffset main_offset =
        composition.main_offset_for_rotation(rotation);

    for (const building_type_registry_impl::ComposedPartDefinition &part : composition.parts()) {
        const building_type_registry_impl::ComposedPartOffset offset = part.offset_for_rotation(rotation);
        if (part.type == BUILDING_NONE || !offset.has_value) {
            continue;
        }
        const building_type_registry_impl::BuildingType &part_definition =
            *building_type_registry_impl::definition_for_type(part.type);
        building part_record = {};
        prepare_composed_ghost_part(
            part_record, part_definition, origin_x + offset.x, origin_y + offset.y, main_record);
        int x_part = 0;
        int y_part = 0;
        composed_part_view_offset(part_record, offset, &x_part, &y_part);
        draw_runtime_ghost_record(part_definition, part_record, x + x_part, y + y_part, color);
    }

    main_record.size = static_cast<unsigned char>(building_properties_for_type(type)->size);
    main_record.x = static_cast<unsigned char>(origin_x + main_offset.x);
    main_record.y = static_cast<unsigned char>(origin_y + main_offset.y);
    main_record.grid_offset = map_grid_offset(main_record.x, main_record.y);
    int x_main = 0;
    int y_main = 0;
    composed_part_view_offset(main_record, main_offset, &x_main, &y_main);
    draw_runtime_ghost_record(definition, main_record, x + x_main, y + y_main, color);
}

static void draw_runtime_regular_building(
    const building_type_registry_impl::BuildingType &definition,
    int grid_offset,
    int x,
    int y,
    int building_size,
    color_t color)
{
    if (definition.has_composition()) {
        draw_runtime_composed_building(definition, grid_offset, x, y, color);
        return;
    }
    draw_runtime_single_building(definition, grid_offset, x, y, building_size, color);
}

static void draw_blocked_tile(int x, int y, int grid_offset)
{
    Image::blend_footprint_color(x, y, COLOR_MASK_RED, data.scale);
}

static void city_building_ghost_draw_malus_range(int x, int y, int grid_offset)
{
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_NEGATIVE_RANGE, data.scale);
}

static void city_building_ghost_draw_bonus_range(int x, int y, int grid_offset)
{
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_POSITIVE_RANGE, data.scale);
}

static void draw_water_range_overlay(int x, int y, color_t color)
{
    const image *overlay = runtime_overlay_image_get(RUNTIME_OVERLAY_IMAGE_WATER_RANGE);
    if (overlay) {
        Image::from_legacy(*(overlay)).draw(x, y, color, data.scale);
    }
}

void city_building_ghost_draw_well_range(int x, int y, int grid_offset)
{
    draw_water_range_overlay(x, y, COLOR_MASK_DARK_BLUE);
}

void city_building_ghost_draw_fountain_range(int x, int y, int grid_offset)
{
    draw_water_range_overlay(x, y, COLOR_MASK_BLUE);
}

static void city_building_ghost_draw_reservoir_range_colored(int x, int y, color_t color)
{
    draw_water_range_overlay(x, y, color);
}

void city_building_ghost_draw_reservoir_range(int x, int y, int grid_offset)
{
    city_building_ghost_draw_reservoir_range_colored(x, y, COLOR_MASK_RESERVOIR_RANGE);
}

void city_building_ghost_draw_aqueduct_range(int x, int y, int grid_offset)
{
    city_building_ghost_draw_reservoir_range_colored(x, y, COLOR_MASK_RESERVOIR_RANGE);
}

void city_building_ghost_draw_latrines_range(int x, int y, int grid_offset)
{
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_DARK_GREEN & ALPHA_FONT_SEMI_TRANSPARENT, data.scale);
}

static void draw_runtime_building_with_tiles(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int grid_offset,
    int building_size,
    int num_tiles,
    int *blocked_tiles)
{
    const int has_blocked = has_blocked_tiles(num_tiles, blocked_tiles);
    building_construction_set_can_place(!has_blocked);
    color_t color = has_blocked ?
        COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    draw_runtime_regular_building(definition, grid_offset, x, y, building_size, color);
    draw_building_tiles(x, y, num_tiles, blocked_tiles);
}

static void get_building_base_xy(int map_x, int map_y, int building_size, int *x, int *y)
{
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            *x = map_x;
            *y = map_y;
            break;
        case DIR_2_RIGHT:
            *x = map_x - building_size + 1;
            *y = map_y;
            break;
        case DIR_4_BOTTOM:
            *x = map_x - building_size + 1;
            *y = map_y - building_size + 1;
            break;
        case DIR_6_LEFT:
            *x = map_x;
            *y = map_y - building_size + 1;
            break;
        default:
            *x = *y = 0;
    }
}

static void set_roamer_path(building_type type, int size, const map_tile *tile, int is_blocked)
{
    if (data.roamer_preview.grid_offset == tile->grid_offset && data.roamer_preview.type == type) {
        return;
    }
    figure_roamer_preview_reset(type);
    data.roamer_preview.type = type;
    data.roamer_preview.grid_offset = tile->grid_offset;
    int grid_x = tile->x;
    int grid_y = tile->y;
    building_construction_offset_start_from_orientation(&grid_x, &grid_y, size);

    if (!is_blocked) {
        figure_roamer_preview_create(type, grid_x, grid_y);
    } else {
        int building_id = map_building_at(tile->grid_offset);
        if (!building_id) {
            return;
        }
        building *b = building_main(building_get(building_id));
        if (b->type == type && b->x == grid_x && b->y == grid_y) {
            figure_roamer_preview_create(type, grid_x, grid_y);
        }
    }
}

static void draw_desirability_range(
    const map_tile *tile,
    const building_type_registry_impl::BuildingType &definition,
    int building_size)
{
    int desirability_value = definition.model().desirability_value();
    int desirability_step_size = definition.model().desirability_step_size();
    int desirability_range = definition.model().desirability_range();
    int negative_range = 0;
    if (desirability_value == 0 || desirability_range == 0) {
        return;         // If there is no desirability - do not draw
    }

    // Add bonuses from GT Venus
    if (building_is_statue_garden_temple(definition.type()) &&
        building_monument_working_grand_temple_for_god(GOD_VENUS)) {
        int value_bonus = ((desirability_value / 4) > 1) ? (desirability_value / 4) : 1;
        desirability_value += value_bonus;
        if (!definition.is_temple()) {
            desirability_range += 1;
        }
    }

    // Calculating the Radius of Negative Desirability
    while (desirability_value < 0 && negative_range < desirability_range) {
        desirability_value += desirability_step_size;
        negative_range++;
    }
    //Positive radius - the remainder of the max_range
    int positive_range = desirability_range - negative_range;
    //First draw the outer positive zone(if any)
    if (positive_range > 0) {
        city_view_foreach_tile_in_range(tile->grid_offset, building_size, desirability_range, city_building_ghost_draw_bonus_range);
    }
    //Then draw the inner negative zone
    if (negative_range > 0) {
        city_view_foreach_tile_in_range(tile->grid_offset, building_size, negative_range, city_building_ghost_draw_malus_range);
    }
}

static void draw_water_access_context_overlays(
    const map_tile *tile,
    const building_type_registry_impl::BuildingType &definition)
{
    const building_type type = definition.type();

    const int uses_reservoir_range =
        water_access_runtime_building_type_requires_access_text(type, "reservoir") ||
        water_access_runtime_building_type_provides_access_text(type, "reservoir");
    if (uses_reservoir_range && config_get(CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES)) {
        city_water_ghost_draw_reservoir_ranges();
    }

    if (!building_is_house(type) &&
        config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE) &&
        (water_access_runtime_building_type_requires_access_text(type, "fountain") ||
         water_access_runtime_building_type_requires_access_text(type, "well"))) {
        city_water_ghost_draw_water_structure_ranges();
    }

    if (water_access_runtime_building_type_provides_access(type) &&
        config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE)) {
        city_water_ghost_draw_preview(type, tile->grid_offset, 0);
    }

    if (building_is_house(type) && config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE_HOUSES)) {
        city_water_ghost_draw_water_structure_ranges();
    }
}

static void draw_market_range_tile(int x, int y, int grid_offset)
{
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_GRAY, data.scale);
}

static void draw_distribution_context_overlays(
    const map_tile *tile,
    const building_type_registry_impl::BuildingType &definition,
    int building_size)
{
    if (definition.has_market() &&
        config_get(CONFIG_UI_SHOW_MARKET_RANGE) &&
        config_get(CONFIG_GP_CH_MARKET_RANGE)) {
        building market_record = {};
        market_record.type = definition.type();
        market_record.state = BUILDING_STATE_IN_USE;
        Market market(market_record, &definition);
        city_view_foreach_tile_in_range(tile->grid_offset, building_size, market.max_supplier_distance(),
            draw_market_range_tile);
    }
}

static building make_plan_ghost_record(
    const building_construction::ConstructionPlacementPlan &plan,
    const building_construction::ConstructionPlacementPart &part,
    const building_type_registry_impl::BuildingType &root_definition)
{
    const building_type_registry_impl::BuildingType &part_definition = *part.definition;
    building record = {};
    record.type = part.type;
    record.grid_offset = part.grid_offset;
    record.x = static_cast<unsigned char>(part.x);
    record.y = static_cast<unsigned char>(part.y);
    record.state = BUILDING_STATE_IN_USE;
    record.size = static_cast<unsigned char>(part.size);
    record.num_workers = part_definition.required_workers();
    record.data.entertainment.days1 = 1;
    record.data.entertainment.days2 = 1;
    if (part_definition.is_granary()) {
        record.resources[RESOURCE_NONE] = FULL_GRANARY;
    }
    if (building_variant_has_variants(part.type)) {
        record.variant = building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(part.type));
    } else if (building_variant_has_variants(root_definition.type())) {
        record.variant =
            building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(root_definition.type()));
    }
    if (plan.waterside_orientation_absolute() >= 0 &&
        part_definition.foundation().has_policy() &&
        std::strcmp(part_definition.foundation().policy(), "shoreline") == 0) {
        record.subtype.orientation = static_cast<short>(plan.waterside_orientation_absolute());
        record.data.dock.orientation = static_cast<signed char>(plan.waterside_orientation_absolute());
    } else if (building_rotation_type_has_rotations(root_definition.type()) ||
        part_definition.has_composition()) {
        record.subtype.orientation = building_rotation_get_rotation();
    }
    prepare_ghost_water_access_state(part_definition, record);
    return record;
}

static void draw_plan_tiles(
    const building_construction::ConstructionPlacementPart &part,
    int x,
    int y,
    int global_blocked)
{
    for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
        const int tile_x = x + X_VIEW_OFFSET(tile.dx, tile.dy);
        const int tile_y = y + Y_VIEW_OFFSET(tile.dx, tile.dy);
        if (global_blocked || tile.state == building_construction::PlacementTileState::Forbidden) {
            Image::blend_footprint_color(tile_x, tile_y, COLOR_MASK_RED, data.scale);
        } else {
            Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).
                draw_isometric_footprint(tile_x, tile_y, COLOR_MASK_FOOTPRINT_GHOST, data.scale);
        }
    }
}

static void draw_plan_part(
    const building_construction::ConstructionPlacementPlan &plan,
    const building_construction::ConstructionPlacementPart &part,
    int x_view,
    int y_view,
    const building_type_registry_impl::BuildingType &root_definition,
    color_t color,
    int global_blocked)
{
    int x_size_offset = 0;
    int y_size_offset = 0;
    draw_tile_view_offset(part.size, &x_size_offset, &y_size_offset);
    const int tile_x = x_view + X_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
    const int tile_y = y_view + Y_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
    const int draw_x = tile_x + x_size_offset;
    const int draw_y = tile_y + y_size_offset;
    building record = make_plan_ghost_record(plan, part, root_definition);
    draw_runtime_ghost_record(*part.definition, record, draw_x, draw_y, color);
    draw_plan_tiles(part, tile_x, tile_y, global_blocked);
}

static const building_type_registry_impl::BuildingType &effective_ghost_definition(
    const building_type_registry_impl::BuildingType &definition,
    int grid_offset)
{
    const int gate_type = building_connectable_gate_type(definition.type());
    if (gate_type && map_terrain_get(grid_offset) & TERRAIN_ROAD) {
        return *building_type_registry_impl::definition_for_type(static_cast<building_type>(gate_type));
    }
    return definition;
}

static void draw_default(
    const map_tile *tile,
    int x_view,
    int y_view,
    const building_type_registry_impl::BuildingType &root_definition)
{
    const building_type_registry_impl::BuildingType &definition =
        effective_ghost_definition(root_definition, tile->grid_offset);
    const building_type type = definition.type();

    const int force_place_active = building_construction_force_place_active();
    building_construction::ConstructionPlacementPlan plan(definition, tile->x, tile->y, 0, force_place_active);

    int force_place_clear_cost = 0;
    const int construction_allows = force_place_active ?
        building_construction_force_place_assess(type, tile->x, tile->y, 0, &force_place_clear_cost) :
        building_construction_assess_building(type, tile->x, tile->y, 0);
    if (force_place_active && construction_allows) {
        building_construction_set_force_place_clear_cost(force_place_clear_cost);
    }
    const int blocked = city_finance_out_of_money() || !construction_allows || !plan.can_place();
    building_construction_set_can_place(!blocked);

    const color_t color = blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    draw_water_access_context_overlays(tile, definition);
    draw_distribution_context_overlays(tile, definition, plan.placement_size());
    for (const building_construction::ConstructionPlacementPart &part : plan.parts()) {
        draw_plan_part(plan, part, x_view, y_view, definition, color, blocked);
    }

    set_roamer_path(type, plan.placement_size(), tile, blocked);
}

static void draw_single_reservoir(int grid_offset, int x, int y, color_t color, int has_water, int draw_blocked)
{
    int image_id = Image::group(GROUP_BUILDING_RESERVOIR);
    draw_building(image_id, x, y, color);
    if (has_water) {
        const image *img = image_get(image_id);
        if (img->animation) {
            int x_water = x - FOOTPRINT_WIDTH + img->animation->sprite_offset_x - 2;
            int top_height = img->top ? img->top->original.height : 0;
            int y_water = y + img->animation->sprite_offset_y - top_height + FOOTPRINT_HALF_HEIGHT * 3;
            Image::from_id(image_id + 1).draw(x_water, y_water, color, data.scale);
        }
    }
    if (data.reservoir_range.blocked && draw_blocked) {
        for (int i = 0; i < 9; i++) {
            Image::blend_footprint_color(x + view_offset_x(i), y + view_offset_y(i), COLOR_MASK_RED, data.scale);
        }
    }
    if (grid_offset) {
        int num_tiles = 9;
        int orientation_index = city_view_orientation() / 2;

        grid_offset += GRID_OFFSET(-1, -1);

        for (int i = 0; i < num_tiles; i++) {
            int tile_offset = grid_offset + tile_grid_offset(orientation_index, i);

            if (map_has_figure_at(tile_offset)) {
                figure_animal_try_nudge_at(grid_offset, tile_offset, 3);
            }
        }
    }
}

static void draw_draggable_reservoir(const map_tile *tile, int x, int y, building_type type)
{
    int map_x = tile->x - 1;
    int map_y = tile->y - 1;
    int blocked = 0;
    int blocked_tiles[9];
    if (building_construction_in_progress()) {
        if (!building_construction_cost()) {
            blocked = 1;
        }
    } else {
        if (map_building_is_reservoir(map_x, map_y)) {
            blocked = 0;
        } else if (!map_tiles_are_clear_with_terrain_exception(map_x, map_y, 3, TERRAIN_ALL, TERRAIN_AQUEDUCT, 1)) {
            //reservoir allowed over aqueducts
            blocked = 1;
        }
    }
    if (city_finance_out_of_money()) {
        blocked = 1;
    }
    data.reservoir_range.blocked = blocked;
    color_t color = blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    int draw_later = 0;
    int x_start = 0;
    int y_start = 0;
    int offset = 0;
    int has_water = map_terrain_exists_tile_in_area_with_type(map_x - 1, map_y - 1, 5, TERRAIN_WATER) ||
        map_water_supply_has_aqueduct_access(map_grid_offset(map_x, map_y));
    int orientation_index = city_view_orientation() / 2;
    int preview_primary_grid_offset = tile->grid_offset + RESERVOIR_GRID_OFFSETS[orientation_index];
    int preview_secondary_grid_offset = 0;
    int drawing_two_reservoirs = building_construction_in_progress();
    if (drawing_two_reservoirs) {
        building_construction_get_view_position(&x_start, &y_start);
        y_start -= 30;
        offset = building_construction_get_start_grid_offset();
        if (offset == tile->grid_offset) {
            drawing_two_reservoirs = 0;
        } else {
            int map_x_start = map_grid_offset_to_x(offset) - 1;
            int map_y_start = map_grid_offset_to_y(offset) - 1;
            if (!has_water) {
                has_water = map_terrain_exists_tile_in_area_with_type(
                    map_x_start - 1, map_y_start - 1, 5, TERRAIN_WATER);
            }
            switch (city_view_orientation()) {
                case DIR_0_TOP:
                    draw_later = map_x_start > map_x || map_y_start > map_y;
                    break;
                case DIR_2_RIGHT:
                    draw_later = map_x_start < map_x || map_y_start > map_y;
                    break;
                case DIR_4_BOTTOM:
                    draw_later = map_x_start < map_x || map_y_start < map_y;
                    break;
                case DIR_6_LEFT:
                    draw_later = map_x_start > map_x || map_y_start < map_y;
                    break;
            }
            preview_secondary_grid_offset = offset + RESERVOIR_GRID_OFFSETS[orientation_index];
            if (!draw_later) {
                draw_single_reservoir(0, x_start, y_start, color, has_water, 1);
            }
        }
    }
    if (!drawing_two_reservoirs) {
        int grid_offset = tile->grid_offset + RESERVOIR_GRID_OFFSETS[orientation_index];
        for (int i = 0; i < 9; i++) {
            int tile_offset = grid_offset + tile_grid_offset(orientation_index, i);
            int terrain = map_terrain_get(tile_offset);

            int forbidden_terrain = terrain & TERRAIN_NOT_CLEAR;
            int discouraged_terrain = terrain & TERRAIN_NOT_CLEAR;

            // Reservoir is allowed over aqueducts
            if (forbidden_terrain & TERRAIN_AQUEDUCT) {
                forbidden_terrain &= ~(TERRAIN_AQUEDUCT | TERRAIN_BUILDING);
            }

            // Only the side-middle aqueduct path through the reservoir should
            // be treated as a valid connector instead of a discouraged overlap.
            if (is_reservoir_side_connection_tile(i)) {
                if (discouraged_terrain & TERRAIN_AQUEDUCT) {
                    discouraged_terrain &= ~(TERRAIN_AQUEDUCT | TERRAIN_BUILDING);
                }
            }

            if (forbidden_terrain) {
                blocked_tiles[i] = TILE_FORBIDDEN;
            } else if (map_has_figure_at(tile_offset)) {
                blocked_tiles[i] = TILE_FORBIDDEN;
                figure_animal_try_nudge_at(grid_offset, tile_offset, 3);
            } else if (discouraged_terrain) {
                blocked_tiles[i] = TILE_DISCOURAGED;
            } else {
                blocked_tiles[i] = TILE_ALLOWED;
            }
        }

    }
    // mouse pointer = center tile of reservoir instead of north, correct here:
    y -= 30;
    if (config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE)) {
        city_water_ghost_draw_preview(type, preview_primary_grid_offset, preview_secondary_grid_offset);
    }
    draw_single_reservoir(tile->grid_offset, x, y, color, has_water, drawing_two_reservoirs);
    if (!drawing_two_reservoirs) {
        draw_building_tiles(x, y, 9, blocked_tiles);
    }
    if (draw_later) {
        draw_single_reservoir(0, x_start, y_start, color, has_water, 1);
    }
}

static void draw_aqueduct(const map_tile *tile, int x, int y, building_type type)
{
    int grid_offset = tile->grid_offset;
    int blocked = city_finance_out_of_money();
    if (!blocked) {
        if (building_construction_in_progress()) {
            if (!building_construction_cost()) {
                blocked = 1;
            }
        } else {
            if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
                blocked = !map_is_straight_road_for_aqueduct(grid_offset);
                if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
                    blocked = 1;
                }
                if (map_terrain_count_directly_adjacent_with_types(grid_offset, TERRAIN_ROAD | TERRAIN_AQUEDUCT)) {
                    blocked = 1;
                }
            } else if (!map_can_place_aqueduct_on_highway(grid_offset, 0)) {
                blocked = 1;
            } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR ^ TERRAIN_HIGHWAY)) {
                blocked = 1;
            }
        }
    }
    int image_id = Image::group(GROUP_BUILDING_AQUEDUCT);
    const terrain_image *img = map_image_context_get_aqueduct(grid_offset, 1);
    if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        int group_offset = img->group_offset;
        if (!img->aqueduct_offset) {
            if (map_terrain_is(grid_offset + map_grid_delta(0, -1), TERRAIN_ROAD)) {
                group_offset = 3;
            } else {
                group_offset = 2;
            }
        }
        if (map_tiles_is_paved_road(grid_offset)) {
            image_id += group_offset + 13;
        } else {
            image_id += group_offset + 21;
        }
    } else {
        image_id += img->group_offset + 15;
    }
    if (config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE)) {
        city_water_ghost_draw_preview(type, grid_offset, 0);
    }
    draw_building(image_id, x, y, blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST);
    draw_building_tiles(x, y, 1, &blocked);
}

static void draw_bridge(const map_tile *tile, int x, int y, building_type type)
{
    int length, direction;
    grid_slice blocked_tiles = { .size = 0 };
    int end_grid_offset = map_bridge_calculate_length_direction(tile->x, tile->y, &length, &direction, &blocked_tiles);

    int dir = direction - city_view_orientation();
    if (dir < 0) {
        dir += 8;
    }
    int blocked = 0;
    int is_ship_bridge = is_ship_bridge_type(type);
    if (is_ship_bridge && length < 5) {
        blocked = 1;
    } else if (!end_grid_offset) {
        blocked = 1;
    } else if (blocked_tiles.size > 0) {
        blocked = 1;
    }
    if (city_finance_out_of_money()) {
        blocked = 1;
    }
    int x_delta, y_delta;
    switch (dir) {
        case DIR_0_TOP:
            x_delta = 29;
            y_delta = -15;
            break;
        case DIR_2_RIGHT:
            x_delta = 29;
            y_delta = 15;
            break;
        case DIR_4_BOTTOM:
            x_delta = -29;
            y_delta = 15;
            break;
        case DIR_6_LEFT:
            x_delta = -29;
            y_delta = -15;
            break;
        default:
            return;
    }
    color_t color_mask;
    if (blocked) {
        Image::blend_footprint_color(x, y, length > 0 ? COLOR_MASK_GREEN : COLOR_MASK_RED, data.scale);
        if (length > 1) {
            color_t end_tile_colour = map_grid_slice_contains(end_grid_offset, &blocked_tiles) ?
                COLOR_MASK_RED : COLOR_MASK_GREEN;
            Image::blend_footprint_color(x + x_delta * (length - 1), y + y_delta * (length - 1), end_tile_colour, data.scale);
        }
        building_construction_set_cost(0);
        color_mask = COLOR_MASK_BUILDING_GHOST_RED;
    } else {
        color_mask = COLOR_MASK_BUILDING_GHOST;
    }
    if (dir == DIR_0_TOP || dir == DIR_6_LEFT) {
        for (int i = length - 1; i >= 0; i--) {
            int sprite_id = map_bridge_get_sprite_id(i, length, dir, is_ship_bridge);
            city_draw_bridge_tile(x + x_delta * i, y + y_delta * i, data.scale, sprite_id, color_mask);
        }
    } else {
        for (int i = 0; i < length; i++) {
            int sprite_id = map_bridge_get_sprite_id(i, length, dir, is_ship_bridge);
            city_draw_bridge_tile(x + x_delta * i, y + y_delta * i, data.scale, sprite_id, color_mask);
        }
    }
    for (int i = 0; i < blocked_tiles.size; i++) {
        city_view_foreach_tile_in_range(blocked_tiles.grid_offsets[i], 0, 0, draw_blocked_tile);
    }
    building_construction_set_cost(model_get_building(type)->cost * length);
}

static void draw_road(const map_tile *tile, int x, int y)
{
    int grid_offset = tile->grid_offset;
    int blocked = 0;
    int image_id = 0;
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        image_id = Image::group(GROUP_BUILDING_AQUEDUCT);
        if (map_can_place_road_under_aqueduct(grid_offset)) {
            image_id += map_get_aqueduct_with_road_image(grid_offset);
        } else {
            blocked = 1;
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && map_routing_is_gate_transformable(grid_offset)) {
        image_id = building_image_get_garden_gate_image(grid_offset);
    } else if (map_terrain_is(grid_offset, TERRAIN_NOT_CLEAR)) {
        blocked = 1;
    } else {
        image_id = Image::group(GROUP_TERRAIN_ROAD);
        if (!map_terrain_has_adjacent_x_with_type(grid_offset, TERRAIN_ROAD) &&
            map_terrain_has_adjacent_y_with_type(grid_offset, TERRAIN_ROAD)) {
            image_id++;
        }
    }
    if (city_finance_out_of_money()) {
        blocked = 1;
    }
    if (blocked) {
        Image::blend_footprint_color(x, y, COLOR_MASK_RED, data.scale);
    } else {
        draw_building(image_id, x, y, COLOR_MASK_BUILDING_GHOST);
    }
}

static void draw_highway(
    const map_tile *tile,
    int x,
    int y,
    const building_type_registry_impl::BuildingType &definition)
{
    const building_type type = definition.type();
    const building_properties *props = building_properties_for_type(type);

    // check if we can place building
    int grid_offset = tile->grid_offset;
    int fully_blocked = city_finance_out_of_money();

    int num_tiles = props->size * props->size;
    int blocked_tiles[MAX_TILES];
    int orientation_index = city_view_orientation() / 2;

    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = grid_offset + tile_grid_offset(orientation_index, i);
        int terrain = map_terrain_get(tile_offset);
        int has_forbidden_terrain = terrain & TERRAIN_NOT_CLEAR & ~TERRAIN_HIGHWAY & ~TERRAIN_ROAD;
        if (fully_blocked || (has_forbidden_terrain && !(terrain & TERRAIN_AQUEDUCT)) ||
            !map_can_place_highway_under_aqueduct(tile_offset, 0)) {
            blocked_tiles[i] = 1;
        } else {
            blocked_tiles[i] = 0;
        }
    }

    draw_runtime_building_with_tiles(definition, x, y, grid_offset, props->size, num_tiles, blocked_tiles);
}

static void draw_grand_temple_neptune_range(int x, int y, int grid_offset)
{
    color_t color_mask = data.reservoir_range.blocked ? COLOR_MASK_GRAY : COLOR_MASK_BLUE;
    draw_water_range_overlay(x, y, color_mask);
}

static void draw_grand_temple_neptune(
    const map_tile *tile,
    int x,
    int y,
    const building_type_registry_impl::BuildingType &definition)
{
    const building_type type = definition.type();
    const building_properties *props = building_properties_for_type(type);
    int num_tiles = props->size * props->size;
    int blocked[MAX_TILES];
    int blocked_state = 0;
    if (city_finance_out_of_money()) {
        blocked_state = 1;
        for (int i = 0; i < num_tiles; ++i) {
            blocked[i] = 1;
        }
    } else {
        blocked_state = is_blocked_for_building(tile->grid_offset, props->size, blocked, 1);
    }
    if (blocked_state) {
        Image::blend_footprint_color(x, y, COLOR_MASK_RED, data.scale);
    }
    data.reservoir_range.blocked = has_blocked_tiles(num_tiles, blocked);
    int radius = map_water_supply_reservoir_radius();
    // need to add 2 for the bonus the Neptune GT will add
    if (!building_monument_working_grand_temple_for_god(GOD_NEPTUNE)) {
        radius += 2;
    }
    city_view_foreach_tile_in_range(tile->grid_offset, props->size, radius, draw_grand_temple_neptune_range);
    draw_runtime_building_with_tiles(definition, x, y, tile->grid_offset, props->size, num_tiles, blocked);
    set_roamer_path(type, props->size, tile, data.reservoir_range.blocked);
}

int city_building_ghost_mark_deleting(const map_tile *tile)
{
    building_type construction_type = building_construction_type();
    if (!building_construction_is_land_work_type(construction_type)) {
        return 0;
    }
    if (!tile->grid_offset || building_construction_draw_as_constructing() || scroll_in_progress()) {
        return 1;
    }
    if (!building_construction_in_progress()) { // no construction, clear bits
        map_property_clear_constructing_and_deleted();
    }
    map_building_tiles_mark_deleting(tile->grid_offset);

    if (map_terrain_is(tile->grid_offset, TERRAIN_HIGHWAY) && !map_terrain_is(tile->grid_offset, TERRAIN_AQUEDUCT)) {
        map_tiles_clear_highway(tile->grid_offset, 1);
    }
    return 1;
}

static int is_water_building(void)
{
    return is_bridge_type(data.ghost_building.type) || is_waterside_type(data.ghost_building.type);
}

static void draw_grid_tile(int x, int y, int grid_offset)
{
    static int image_id = 0;
    if (!image_id) {
        image_id = assets_get_image_id("UI\\Grid_Full", "Grid_Full");
    }
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_terrain_is(grid_offset, TERRAIN_ROCK) ||
        map_terrain_is(grid_offset, TERRAIN_ACCESS_RAMP) || map_terrain_is(grid_offset, TERRAIN_ELEVATION) ||
        (map_terrain_is(grid_offset, TERRAIN_WATER) && !is_water_building())) {
        return;
    }
    Image::from_id(image_id).draw(x, y, COLOR_GRID, data.scale);
}

static void draw_grid_around_building(int grid_offset, int size, int orientation, int x, int y)
{
    int num_tiles = size * size;
    city_view_foreach_tile_in_range(grid_offset, size, 2, draw_grid_tile);
    for (int i = 0; i < num_tiles; i++) {
        draw_grid_tile(x + view_offset_x(i), y + view_offset_y(i), grid_offset + tile_grid_offset(orientation, i));
    }
}

static void draw_partial_grid(
    int grid_offset,
    int x,
    int y,
    const building_type_registry_impl::BuildingType &definition)
{
    const building_type type = definition.type();
    if (is_definition_attr(definition, "draggable_reservoir")) {
        int size = 3;
        int orientation_index = city_view_orientation() / 2;
        grid_offset += RESERVOIR_GRID_OFFSETS[orientation_index];
        draw_grid_around_building(grid_offset, size, orientation_index, x, y);
        return;
    }

    const int cursor_x = map_grid_offset_to_x(grid_offset);
    const int cursor_y = map_grid_offset_to_y(grid_offset);
    building_construction::ConstructionPlacementPlan plan(definition, cursor_x, cursor_y, 0, 0);
    const int orientation_index = city_view_orientation() / 2;
    for (const building_construction::ConstructionPlacementPart &part : plan.parts()) {
        const int tile_x = x + X_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
        const int tile_y = y + Y_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
        draw_grid_around_building(part.grid_offset, part.size, orientation_index, tile_x, tile_y);
    }
}

static void create_tile_offsets(void)
{
    if (data.offsets[0][MAX_TILES - 1].x) {
        return;
    }

    static const tile_xy_offsets steps[4] = { { 1, 1 }, { -1, 1 }, { -1, -1 }, { 1, -1 } };

    for (int dir = 0; dir < 4; dir++) {
        const tile_xy_offsets *step = &steps[dir];
        int index = 0;
        int column = 0;
        int row = 0;
        int *x = dir & 1 ? &row : &column;
        int *y = dir & 1 ? &column : &row;
        tile_xy_offsets *offset = data.offsets[dir];

        while (index < MAX_TILES) {
            for (column = 0; column < row; column++) {
                offset[index].x = *x * step->x;
                offset[index].y = *y * step->y;
                offset[index + 1].x = *y * step->x;
                offset[index + 1].y = *x * step->y;
                index += 2;
            }
            offset[index].x = row * step->x;
            offset[index].y = row * step->y;
            index++;
            row++;
        }
    }
}

void city_building_ghost_draw(const map_tile *tile)
{
    if (!tile->grid_offset || scroll_in_progress()) {
        return;
    }
    building_construction_set_hover_tile(tile->x, tile->y, tile->grid_offset);
    building_type type = building_construction_type();
    data.ghost_building.type = type;
    if (building_construction_draw_as_constructing() || type == BUILDING_NONE
        || building_construction_is_land_work_type(type)) {
        return;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }
    create_tile_offsets();
    data.scale = city_view_get_scale() / 100.0f;
    int x, y;
    city_view_get_selected_tile_pixels(&x, &y);

    const building_properties *props = building_properties_for_type(type);
    if (config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL) ||
        (config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE) && definition->flags().draw_desirability_range())) {
        const int is_draggable_reservoir = is_definition_attr(*definition, "draggable_reservoir");
        int building_size = (is_draggable_reservoir || definition->is_warehouse()) ? 3 : props->size;

        if (is_draggable_reservoir) {
            map_tile shifted_tile = *tile;
            switch (city_view_orientation()) {
                case DIR_0_TOP:
                    shifted_tile.x -= 1;
                    shifted_tile.y -= 1;
                    break;
                case DIR_2_RIGHT:
                    shifted_tile.x += 1;
                    shifted_tile.y -= 1;
                    break;
                case DIR_4_BOTTOM:
                    shifted_tile.x += 1;
                    shifted_tile.y += 1;
                    break;
                case DIR_6_LEFT:
                    shifted_tile.x -= 1;
                    shifted_tile.y += 1;
                    break;
            }
            shifted_tile.grid_offset = map_grid_offset(shifted_tile.x, shifted_tile.y);
            draw_desirability_range(&shifted_tile, *definition, building_size);
        } else {
            building_construction::ConstructionPlacementPlan plan(*definition, tile->x, tile->y, 0, 0);
            for (const building_construction::ConstructionPlacementPart &part : plan.parts()) {
                map_tile part_tile = *tile;
                part_tile.x = part.x;
                part_tile.y = part.y;
                part_tile.grid_offset = part.grid_offset;
                draw_desirability_range(&part_tile, *part.definition, part.size);
            }
        }
    }

    if (!config_get(CONFIG_UI_SHOW_GRID) && config_get(CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION)) {
        draw_partial_grid(tile->grid_offset, x, y, *definition);
    }

    if (is_bridge_definition(*definition)) {
        draw_bridge(tile, x, y, type);
        return;
    }

    if (is_definition_attr(*definition, "draggable_reservoir")) {
        draw_draggable_reservoir(tile, x, y, type);
    } else if (is_definition_attr(*definition, "aqueduct")) {
        draw_aqueduct(tile, x, y, type);
    } else if (is_definition_attr(*definition, "road")) {
        draw_road(tile, x, y);
    } else if (is_definition_attr(*definition, "highway")) {
        draw_highway(tile, x, y, *definition);
    } else if (is_definition_attr(*definition, "grand_temple_neptune")) {
        draw_grand_temple_neptune(tile, x, y, *definition);
    } else {
        draw_default(tile, x, y, *definition);
    }
}
