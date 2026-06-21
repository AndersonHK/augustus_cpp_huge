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

#include "assets/image_group_payload.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_graphics.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"
#include "core/log.h"
#include "graphics/runtime_texture.h"


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

#include <cstdio>
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

enum farm_ghost_object {
    FARM_GHOST_NO_DRAW,
    FARM_GHOST_FARMHOUSE,
    FARM_GHOST_CROP
};

enum {
    TILE_FORBIDDEN = 1,
    TILE_ALLOWED = 0,
    TILE_DISCOURAGED = -1
};

static const int FORT_GROUND_GRID_OFFSETS[4][4] = {
    { GRID_OFFSET(3, -1),  GRID_OFFSET(4, -1), GRID_OFFSET(4, 0),  GRID_OFFSET(3, 0)   },
    { GRID_OFFSET(-1, -4), GRID_OFFSET(0, -4), GRID_OFFSET(0, -3), GRID_OFFSET(-1, -3) },
    { GRID_OFFSET(-4, 0),  GRID_OFFSET(-3, 0), GRID_OFFSET(-3, 1), GRID_OFFSET(-4, 1)  },
    { GRID_OFFSET(0, 3),   GRID_OFFSET(1, 3),  GRID_OFFSET(1, 4),  GRID_OFFSET(0, 4)   }
};
static const int FORT_GROUND_X_VIEW_OFFSETS[4] = { 120, 90, -120, -90 };
static const int FORT_GROUND_Y_VIEW_OFFSETS[4] = { 30, -75, -60, 45 };

static const int RESERVOIR_GRID_OFFSETS[4] = {
    GRID_OFFSET(-1, -1), GRID_OFFSET(1, -1), GRID_OFFSET(1, 1), GRID_OFFSET(-1, 1)
};

static const int HIPPODROME_X_VIEW_OFFSETS[4] = { 150, 150, -150, -150 };
static const int HIPPODROME_Y_VIEW_OFFSETS[4] = { 75, -75, -75, 75 };

static const int FARM_TILES[4][9] = {
    {
        FARM_GHOST_FARMHOUSE, FARM_GHOST_NO_DRAW, FARM_GHOST_NO_DRAW,
        FARM_GHOST_NO_DRAW, FARM_GHOST_CROP, FARM_GHOST_CROP,
        FARM_GHOST_CROP, FARM_GHOST_CROP, FARM_GHOST_CROP
    },
    {
        FARM_GHOST_CROP, FARM_GHOST_FARMHOUSE, FARM_GHOST_CROP,
        FARM_GHOST_NO_DRAW, FARM_GHOST_NO_DRAW, FARM_GHOST_CROP,
        FARM_GHOST_NO_DRAW, FARM_GHOST_CROP, FARM_GHOST_CROP
    },
    {
        FARM_GHOST_CROP, FARM_GHOST_CROP, FARM_GHOST_CROP,
        FARM_GHOST_FARMHOUSE, FARM_GHOST_CROP, FARM_GHOST_CROP,
        FARM_GHOST_NO_DRAW, FARM_GHOST_NO_DRAW, FARM_GHOST_NO_DRAW
    },
    {
        FARM_GHOST_CROP, FARM_GHOST_CROP, FARM_GHOST_FARMHOUSE,
        FARM_GHOST_NO_DRAW, FARM_GHOST_CROP, FARM_GHOST_NO_DRAW,
        FARM_GHOST_CROP, FARM_GHOST_NO_DRAW, FARM_GHOST_CROP
    },
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

static int is_warehouse_type(building_type type)
{
    return building_type_attr_is(type, "warehouse");
}

static int is_granary_type(building_type type)
{
    return building_type_attr_is(type, "granary");
}

static int is_vacant_lot_fill_type(building_type type)
{
    return type == building_type_registry_get_vacant_lot_fill_type();
}

static int is_plaza_type(building_type type)
{
    return building_type_attr_is(type, "plaza");
}

static int is_roadblock_type(building_type type)
{
    return building_type_attr_is(type, "roadblock");
}

static int is_gatehouse_type(building_type type)
{
    return building_type_attr_is(type, "gatehouse");
}

static int is_triumphal_arch_type(building_type type)
{
    return building_type_attr_is(type, "triumphal_arch");
}

static int is_draggable_reservoir_type(building_type type)
{
    return building_type_attr_is(type, "draggable_reservoir");
}

static int is_aqueduct_type(building_type type)
{
    return building_type_attr_is(type, "aqueduct");
}

static int is_hippodrome_type(building_type type)
{
    return building_type_attr_is(type, "hippodrome");
}

static int is_road_type(building_type type)
{
    return building_type_attr_is(type, "road");
}

static int is_highway_type(building_type type)
{
    return building_type_attr_is(type, "highway");
}

static int is_grand_temple_neptune_type(building_type type)
{
    return building_type_attr_is(type, "grand_temple_neptune");
}

static int is_dock_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), "dock") == 0;
}

static int is_bridge_type(building_type type)
{
    return building_type_attr_is(type, "low_bridge") || building_type_attr_is(type, "ship_bridge");
}

static int is_ship_bridge_type(building_type type)
{
    return building_type_attr_is(type, "ship_bridge");
}

static int is_waterside_type(building_type type)
{
    if (is_dock_type(type)) {
        return 1;
    }
    static const char *const text_ids[] = { "shipyard", "wharf" };
    return building_type_attr_is_any(type, text_ids, sizeof(text_ids) / sizeof(text_ids[0]));
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

static int graphics_definition_is_data_only_for_ghost(building_type type)
{
    return building_is_farm(type);
}

static int type_uses_native_ghost_graphics(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->has_graphic() && !graphics_definition_is_data_only_for_ghost(type);
}

static int ghost_building_size(building_type type)
{
    return is_warehouse_type(type) ? 3 : building_properties_for_type(type)->size;
}

static void prepare_ghost_water_access_state(
    const building_type_registry_impl::BuildingType *definition,
    building &ghost)
{
    if (!definition) {
        return;
    }
    if (definition->water_access().has_requirements() || definition->has_water_access_provider()) {
        ghost.has_water_access = static_cast<unsigned char>(water_access_runtime_building_has_required_access(&ghost));
    }
}

static void prepare_ghost_building(int grid_offset, building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    data.ghost_building = {};
    data.ghost_building.type = type;
    data.ghost_building.grid_offset = grid_offset;
    data.ghost_building.state = BUILDING_STATE_IN_USE;
    data.ghost_building.size = static_cast<unsigned char>(ghost_building_size(type));
    data.ghost_building.num_workers = model_get_building(type)->laborers;
    data.ghost_building.data.entertainment.days1 = 1;
    data.ghost_building.data.entertainment.days2 = 1;
    if (definition && definition->is_granary()) {
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
    if (is_waterside_type(type)) {
        int waterside_orientation_abs = -1;
        map_water_determine_orientation(building_x, building_y, data.ghost_building.size, 0,
            &waterside_orientation_abs, 0, 0, 0);
        if (waterside_orientation_abs >= 0) {
            data.ghost_building.subtype.orientation = static_cast<short>(waterside_orientation_abs);
            if (is_dock_type(type)) {
                data.ghost_building.data.dock.orientation = static_cast<signed char>(waterside_orientation_abs);
            }
        }
    }
}

static void log_native_ghost_draw_failure(building_type type, int grid_offset)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }
    prepare_ghost_building(grid_offset, type);
    Building building(data.ghost_building, definition);
    if (!building_runtime_graphics_image_id(building)) {
        return;
    }
    char detail[256];
    snprintf(
        detail,
        sizeof(detail),
        "building_attr=%s grid_offset=%d",
        definition->attr(),
        grid_offset);
    log_info("Native ghost graphics draw failed after image lookup succeeded: ", detail, 0);
}

static const ImageGroupEntry *runtime_ghost_entry(int grid_offset, building_type type, int include_data_only)
{
    if (!include_data_only && graphics_definition_is_data_only_for_ghost(type)) {
        return nullptr;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_graphic()) {
        return nullptr;
    }
    prepare_ghost_building(grid_offset, type);
    Building building(data.ghost_building, definition);
    const building_type_registry_impl::GraphicsTarget *target =
        definition->resolve_graphics_target(building);
    if (!target) {
        return nullptr;
    }
    building_type_registry_impl::GraphicsTarget resolved_target =
        target->resolved_option(static_cast<unsigned char>(building_runtime_graphics_selected_option(building, *target)));
    if (!resolved_target.has_path() || !image_group_payload_load(resolved_target.path())) {
        return nullptr;
    }
    const ImageGroupPayload *payload = image_group_payload_get(resolved_target.path());
    if (!payload) {
        return nullptr;
    }
    return resolved_target.has_image() ? payload->entry_for(resolved_target.image()) : payload->default_entry();
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

static void draw_runtime_payload_entry(const ImageGroupEntry &entry, int x, int y, color_t color)
{
    if (const RuntimeDrawSlice *footprint = entry.footprint()) {
        runtime_texture_draw(*footprint, x, y, color, data.scale);
    }
    if (const RuntimeDrawSlice *top = entry.top()) {
        runtime_texture_draw(*top, x, y, color, data.scale);
    }
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

static int draw_runtime_regular_building(building_type type, int grid_offset, int x, int y, int building_size, color_t color)
{
    if (graphics_definition_is_data_only_for_ghost(type)) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_graphic()) {
        return 0;
    }
    prepare_ghost_building(grid_offset, type);
    Building building(data.ghost_building, definition);
    int x_draw = 0;
    int y_draw = 0;
    draw_tile_view_offset(building_size, &x_draw, &y_draw);
    x += x_draw;
    y += y_draw;
    if (!building.draw_footprint({ x, y, grid_offset, color, data.scale, 1 })) {
        return 0;
    }
    building.draw_top({ x, y, grid_offset, color, data.scale, 1 });
    draw_runtime_ghost_animation(building, grid_offset, x, y, color);
    return 1;
}

static int draw_runtime_farmhouse(building_type type, int grid_offset, int x, int y, color_t color)
{
    const ImageGroupEntry *entry = runtime_ghost_entry(grid_offset, type, 1);
    if (!entry) {
        return 0;
    }
    int x_draw = 0;
    int y_draw = 0;
    draw_tile_view_offset(2, &x_draw, &y_draw);
    x += x_draw;
    y += y_draw;
    draw_runtime_payload_entry(*entry, x, y, color);
    return 1;
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

static void draw_warehouse_image(int image_id, int x, int y, color_t color)
{
    int image_id_space = Image::group(GROUP_BUILDING_WAREHOUSE_STORAGE_EMPTY);
    int building_orientation = building_rotation_get_building_orientation(building_rotation_get_rotation());
    int corner = building_rotation_get_corner(building_orientation);
    for (int i = 0; i < 9; i++) {
        int x_offset = view_offset_x(i);
        int y_offset = view_offset_y(i);
        if (i == corner) {
            draw_building(image_id, x + x_offset, y + y_offset, color);
            Image::from_id(Image::group(GROUP_BUILDING_WAREHOUSE) + 17).draw(x + x_offset - 4, y + y_offset - 42, color, data.scale);
        } else {
            draw_building(image_id_space, x + x_offset, y + y_offset, color);
        }
    }
}

static void draw_farm_image(building_type type, int image_id, int x, int y, int grid_offset, color_t color)
{
    // Custom draw order to properly draw isometric tops
    const int draw_order[9] = { 0, 2, 5, 1, 3, 7, 4, 6, 8 };
    int orientation_index = city_view_orientation() / 2;
    int crop_image = building_image_get_base_farm_crop(type);
    for (int i = 0; i < 9; i++) {
        int j = draw_order[i];
        int x_offset = view_offset_x(j);
        int y_offset = view_offset_y(j);
        switch (FARM_TILES[orientation_index][j]) {
            case FARM_GHOST_CROP:
                draw_building(crop_image, x + x_offset, y + y_offset, color);
                break;
            case FARM_GHOST_FARMHOUSE:
                if (!draw_runtime_farmhouse(type, grid_offset + tile_grid_offset(orientation_index, j),
                    x + x_offset, y + y_offset, color)) {
                    draw_building(image_id, x + x_offset, y + y_offset, color);
                }
                break;
            default:
                break;
        }
    }
}

static void draw_regular_building(building_type type, int image_id, int x, int y, int grid_offset,
    int num_tiles, int *blocked_tiles)
{
    const int has_blocked = has_blocked_tiles(num_tiles, blocked_tiles);
    building_construction_set_can_place(!has_blocked);
    color_t color = has_blocked ?
        COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    if (building_is_farm(type)) {
        draw_farm_image(type, image_id, x, y, grid_offset, color);
    } else if (is_warehouse_type(type)) {
        draw_warehouse_image(image_id, x, y, color);
    } else if (is_granary_type(type)) {
        Image::from_id(image_id).draw_isometric_footprint(x, y, color, data.scale);
        Image::from_id(image_id + 1).draw(x - 32, y - 64, color, data.scale);
    } else if (is_vacant_lot_fill_type(type)) {
        draw_building(Image::group(GROUP_BUILDING_HOUSE_VACANT_LOT), x, y, color);
    } else if (is_triumphal_arch_type(type)) {
        draw_building(image_id, x, y, color);
        if (image_id == Image::group(GROUP_BUILDING_TRIUMPHAL_ARCH)) {
            Image::from_id(image_id + 1).draw(x + 4, y - 51, color, data.scale);
        } else {
            Image::from_id(image_id + 1).draw(x - 33, y - 56, color, data.scale);
        }
    } else if (!building_construction_is_land_work_type(type)) {
        draw_building(image_id, x, y, color);
    }
    draw_building_tiles(x, y, num_tiles, blocked_tiles);
}

static int get_building_image_id(int map_x, int map_y, building_type type, const building_properties *props)
{
    int image_id;
    image_id = Image::group(props->image_group) + props->image_offset;

    if (is_gatehouse_type(type)) {
        int orientation = map_orientation_for_gatehouse(map_x, map_y);
        int image_offset;
        if (orientation == 2) {
            image_offset = 1;
        } else if (orientation == 1) {
            image_offset = 0;
        } else {
            image_offset = building_rotation_get_road_orientation() == 2 ? 1 : 0;
        }
        int map_orientation = city_view_orientation();
        if (map_orientation == DIR_6_LEFT || map_orientation == DIR_2_RIGHT) {
            image_offset = 1 - image_offset;
        }
        image_id += image_offset;
    } else if (is_triumphal_arch_type(type)) {
        int orientation = map_orientation_for_triumphal_arch(map_x, map_y);
        int image_offset;
        if (orientation == 2) {
            image_offset = 2;
        } else if (orientation == 1) {
            image_offset = 0;
        } else {
            image_offset = building_rotation_get_road_orientation() == 2 ? 2 : 0;
        }
        int map_orientation = city_view_orientation();
        if (map_orientation == DIR_6_LEFT || map_orientation == DIR_2_RIGHT) {
            image_offset = 2 - image_offset;
        }
        image_id += image_offset;
    }
    return image_id;
}

static int get_new_building_image_id(int grid_offset, building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_graphic()) {
        return 0;
    }
    prepare_ghost_building(grid_offset, type);
    Building building(data.ghost_building, definition);
    return building_runtime_graphics_image_id(building);
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

static int is_fully_blocked(int map_x, int map_y, building_type type, int building_size, int grid_offset)
{
    // determine x and y offset
    int x = 0, y = 0;
    get_building_base_xy(map_x, map_y, building_size, &x, &y);

    if (!building_construction_can_place_on_terrain(x, y, 0, 0)) {
        return 1;
    }
    if (building_type_attr_is(type, "senate") && city_buildings_has_senate()) {
        return 1;
    }
    if (building_type_attr_is(type, "city_mint") && (!city_buildings_has_senate() || city_buildings_has_city_mint())) {
        return 1;
    }
    if (building_type_attr_is(type, "caravanserai") && city_buildings_has_caravanserai()) {
        return 1;
    }
    if (building_type_attr_is(type, "barracks") && city_buildings_has_barracks() &&
        !config_get(CONFIG_GP_CH_MULTIPLE_BARRACKS)) {
        return 1;
    }
    if (building_type_attr_is(type, "mess_hall") && city_buildings_has_mess_hall()) {
        return 1;
    }
    if (is_plaza_type(type) && !map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        return 1;
    }
    if (is_roadblock_type(type) && !map_terrain_is(grid_offset, TERRAIN_ROAD)) {
        return 1;
    }
    if (!building_monument_type_is_mini_monument(type) && building_monument_get_id(type)) {
        return 1;
    }
    if (building_monument_is_grand_temple(type) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        return 1;
    }
    if (city_finance_out_of_money()) {
        return 1;
    }
    return 0;
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

static void draw_desirability_range(const map_tile *tile, building_type type, int building_size)
{
    const model_building *model = model_get_building(type);
    int desirability_value = model->desirability_value;
    int desirability_step_size = model->desirability_step_size;
    int desirability_range = model->desirability_range;
    int negative_range = 0;
    if (desirability_value == 0 || desirability_range == 0) {
        return;         // If there is no desirability - do not draw
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);

    // Add bonuses from GT Venus
    if (building_is_statue_garden_temple(type) && building_monument_working_grand_temple_for_god(GOD_VENUS)) {
        int value_bonus = ((desirability_value / 4) > 1) ? (desirability_value / 4) : 1;
        desirability_value += value_bonus;
        if (!definition || !definition->is_temple()) {
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

static void draw_water_access_context_overlays(const map_tile *tile, building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }

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

static void draw_distribution_context_overlays(const map_tile *tile, building_type type, int building_size)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (definition && definition->has_market() &&
        config_get(CONFIG_UI_SHOW_MARKET_RANGE) &&
        config_get(CONFIG_GP_CH_MARKET_RANGE)) {
        building market_record = {};
        market_record.type = type;
        market_record.state = BUILDING_STATE_IN_USE;
        Market market(market_record, definition);
        city_view_foreach_tile_in_range(tile->grid_offset, building_size, market.max_supplier_distance(),
            draw_market_range_tile);
    }
}

static void draw_default(const map_tile *tile, int x_view, int y_view, building_type type)
{
    const building_properties *props = building_properties_for_type(type);
    int building_size = is_warehouse_type(type) ? 3 : props->size;
    // Warehouse is size 1 in legacy props, since only the corner tile owns the building.
    //It's manually adjusted for sizing purposes that should affect entire 3x3 building.
    int image_id = 0;

    // check if we can place building
    int grid_offset = tile->grid_offset;
    int fully_blocked = is_fully_blocked(tile->x, tile->y, type, building_size, grid_offset);

    int num_tiles = building_size * building_size;
    int blocked_tiles[MAX_TILES];
    int orientation_index = city_view_orientation() / 2;

    if (building_connectable_gate_type(type) && map_terrain_get(grid_offset) & TERRAIN_ROAD) {
        type = static_cast<building_type>(building_connectable_gate_type(type));
    }

    int check_figure = ((!is_plaza_type(type) && !is_roadblock_type(type)) || props->size != 1) ? 1 : 0;
    int force_place_clear_cost = 0;
    int force_place_valid = building_construction_force_place_active() &&
        building_construction_force_place_assess(type, tile->x, tile->y, 0, &force_place_clear_cost);
    if (force_place_valid) {
        building_construction_set_force_place_clear_cost(force_place_clear_cost);
    }

    for (int i = 0; i < num_tiles; i++) {
        int tile_offset = grid_offset + tile_grid_offset(orientation_index, i);
        int forbidden_terrain = map_terrain_get(tile_offset) & TERRAIN_NOT_CLEAR;
        int discouraged_terrain = forbidden_terrain;
        // forbidden terrain cannot be built on
        // discouraged terrain can be built on, but is still highlighted red,
        // to suggest e.g. that it will become unusable/be overwritten
        if (!fully_blocked) {
            if (is_road_surface_type(type)) {
                forbidden_terrain &= ~TERRAIN_ROAD;
                discouraged_terrain &= ~TERRAIN_ROAD;
            }
            if (is_gatehouse_type(type)) {
                forbidden_terrain &= ~(TERRAIN_HIGHWAY | TERRAIN_WALL | TERRAIN_ROAD);
                discouraged_terrain &= ~(TERRAIN_HIGHWAY | TERRAIN_WALL | TERRAIN_ROAD);
                if (map_terrain_is(tile_offset, TERRAIN_WALL)) {
                    forbidden_terrain &= ~TERRAIN_BUILDING;
                    discouraged_terrain &= ~TERRAIN_BUILDING;
                }
            }
            if (building_type_attr_is(type, "tower")) {
                forbidden_terrain &= ~TERRAIN_WALL & ~TERRAIN_BUILDING;
                discouraged_terrain &= ~TERRAIN_WALL & ~TERRAIN_BUILDING;
            }
            if (config_get(CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT)) {
                if (is_warehouse_type(type)) {
                    forbidden_terrain &= ~TERRAIN_ROAD;
                    if (building_construction_is_warehouse_corner(i)) {
                        discouraged_terrain &= ~TERRAIN_ROAD;
                    }
                } else if (is_granary_type(type)) {
                    forbidden_terrain &= ~TERRAIN_ROAD;
                    if (building_construction_is_granary_cross_tile(i)) {
                        discouraged_terrain &= ~TERRAIN_ROAD;
                    }
                }
            }
        }
        int force_clearable_forbidden = force_place_valid && force_place_can_clear_terrain(forbidden_terrain);
        int force_clearable_discouraged = force_place_valid && force_place_can_clear_terrain(discouraged_terrain);

        if (check_figure && map_has_figure_at(tile_offset)) {
            blocked_tiles[i] = TILE_FORBIDDEN;
            figure_animal_try_nudge_at(grid_offset, tile_offset, building_size);
        } else if (fully_blocked || (forbidden_terrain && !force_clearable_forbidden)) {
            blocked_tiles[i] = TILE_FORBIDDEN;
        } else {
            if (discouraged_terrain && !force_clearable_discouraged) { //allow some leeway
                blocked_tiles[i] = TILE_DISCOURAGED;
            } else {
                blocked_tiles[i] = TILE_ALLOWED;
            }
        }
    }
    const int has_blocked = has_blocked_tiles(num_tiles, blocked_tiles);
    building_construction_set_can_place(!has_blocked);
    color_t color = has_blocked ?
        COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    draw_water_access_context_overlays(tile, type);
    draw_distribution_context_overlays(tile, type, building_size);
    if (draw_runtime_regular_building(type, grid_offset, x_view, y_view, building_size, color)) {
        draw_building_tiles(x_view, y_view, num_tiles, blocked_tiles);
    } else if (type_uses_native_ghost_graphics(type)) {
        log_native_ghost_draw_failure(type, grid_offset);
        draw_building_tiles(x_view, y_view, num_tiles, blocked_tiles);
    } else {
        image_id = props->image_group > 0 ? get_building_image_id(tile->x, tile->y, type, props) : 0;
        draw_regular_building(type, image_id, x_view, y_view, grid_offset, num_tiles, blocked_tiles);
    }

    set_roamer_path(type, building_size, tile, has_blocked);
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

static void draw_fort(const map_tile *tile, int x, int y, building_type type)
{
    int blocked = 0;
    building_type fort_ground_type = building_type_from_attr("fort_ground");

    int building_size_fort = building_properties_for_type(type)->size;
    int num_tiles_fort = building_size_fort * building_size_fort;
    int building_size_ground = building_properties_for_type(fort_ground_type)->size;
    int num_tiles_ground = building_size_ground * building_size_ground;

    int grid_offset_fort = tile->grid_offset;
    int grid_offset_ground = grid_offset_fort +
        FORT_GROUND_GRID_OFFSETS[building_rotation_get_rotation()][city_view_orientation() / 2];
    int blocked_tiles_fort[9];
    int blocked_tiles_ground[16];

    if (formation_get_num_legions_cached() >= formation_get_max_legions() || city_finance_out_of_money()) {
        blocked = 1;
        for (int i = 0; i < num_tiles_fort; i++) {
            blocked_tiles_fort[i] = 1;
        }
        for (int i = 0; i < num_tiles_ground; i++) {
            blocked_tiles_ground[i] = 1;
        }
    } else {
        blocked |= is_blocked_for_building(grid_offset_fort, building_size_fort, blocked_tiles_fort, 1);
        blocked |= is_blocked_for_building(grid_offset_ground, building_size_ground, blocked_tiles_ground, 0);
    }

    int orientation_index = building_rotation_get_building_orientation(building_rotation_get_rotation()) / 2;
    int x_ground = x + FORT_GROUND_X_VIEW_OFFSETS[orientation_index];
    int y_ground = y + FORT_GROUND_Y_VIEW_OFFSETS[orientation_index];

    color_t color_mask = blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;

    int image_id = get_new_building_image_id(tile->grid_offset, type);
    int image_id_grounds = Image::group(GROUP_BUILDING_FORT) + 1;
    if (orientation_index == 0 || orientation_index == 3) {
        // draw fort first, then ground
        draw_building(image_id, x, y, color_mask);
        draw_building_tiles(x, y, num_tiles_fort, blocked_tiles_fort);
        draw_building(image_id_grounds, x_ground, y_ground, color_mask);
        draw_building_tiles(x_ground, y_ground, num_tiles_ground, blocked_tiles_ground);
    } else {
        // draw ground first, then fort
        draw_building(image_id_grounds, x_ground, y_ground, color_mask);
        draw_building_tiles(x_ground, y_ground, num_tiles_ground, blocked_tiles_ground);
        draw_building(image_id, x, y, color_mask);
        draw_building_tiles(x, y, num_tiles_fort, blocked_tiles_fort);
    }
}

static void draw_hippodrome(const map_tile *tile, int x, int y, building_type type)
{
    int blocked = 0;
    if (city_buildings_has_hippodrome() || city_finance_out_of_money()) {
        blocked = 1;
    }
    const int building_block_size = 5;
    const int num_tiles = 25;

    building_rotation_force_two_orientations();
    int orientation_index = building_rotation_get_building_orientation(building_rotation_get_rotation()) / 2;
    int grid_offset1 = tile->grid_offset;
    int grid_offset2 = grid_offset1 + building_rotation_get_delta_with_rotation(5);
    int grid_offset3 = grid_offset1 + building_rotation_get_delta_with_rotation(10);

    int blocked_tiles1[25];
    int blocked_tiles2[25];
    int blocked_tiles3[25];
    if (city_buildings_has_hippodrome() || city_finance_out_of_money()) {
        blocked = 1;
        for (int i = 0; i < num_tiles; i++) {
            blocked_tiles1[i] = 1;
            blocked_tiles2[i] = 1;
            blocked_tiles3[i] = 1;
        }
    } else {
        blocked |= is_blocked_for_building(grid_offset1, building_block_size, blocked_tiles1, 1);
        blocked |= is_blocked_for_building(grid_offset2, building_block_size, blocked_tiles2, 1);
        blocked |= is_blocked_for_building(grid_offset3, building_block_size, blocked_tiles3, 1);
    }

    int x_part1 = x;
    int y_part1 = y;
    int x_part2 = x_part1 + HIPPODROME_X_VIEW_OFFSETS[orientation_index];
    int y_part2 = y_part1 + HIPPODROME_Y_VIEW_OFFSETS[orientation_index];
    int x_part3 = x_part2 + HIPPODROME_X_VIEW_OFFSETS[orientation_index];
    int y_part3 = y_part2 + HIPPODROME_Y_VIEW_OFFSETS[orientation_index];

    color_t color_mask;
    if (blocked) {
        color_mask = COLOR_MASK_BUILDING_GHOST_RED;
    } else {
        color_mask = COLOR_MASK_BUILDING_GHOST;
    }
    if (orientation_index == 0) {
        int image_id = Image::group(GROUP_BUILDING_HIPPODROME_2);
        // part 1, 2, 3
        draw_building(image_id, x_part1, y_part1, color_mask);
        draw_building_tiles(x_part1, y_part1, num_tiles, blocked_tiles1);
        draw_building(image_id + 2, x_part2, y_part2, color_mask);
        draw_building_tiles(x_part2, y_part2, num_tiles, blocked_tiles2);
        draw_building(image_id + 4, x_part3, y_part3, color_mask);
        draw_building_tiles(x_part3, y_part3, num_tiles, blocked_tiles3);
    } else if (orientation_index == 1) {
        int image_id = Image::group(GROUP_BUILDING_HIPPODROME_1);
        // part 3, 2, 1
        draw_building(image_id, x_part3, y_part3, color_mask);
        draw_building_tiles(x_part3, y_part3, num_tiles, blocked_tiles3);
        draw_building(image_id + 2, x_part2, y_part2, color_mask);
        draw_building_tiles(x_part2, y_part2, num_tiles, blocked_tiles2);
        draw_building(image_id + 4, x_part1, y_part1, color_mask);
        draw_building_tiles(x_part1, y_part1, num_tiles, blocked_tiles1);
    } else if (orientation_index == 2) {
        int image_id = Image::group(GROUP_BUILDING_HIPPODROME_2);
        // part 1, 2, 3
        draw_building(image_id + 4, x_part1, y_part1, color_mask);
        draw_building_tiles(x_part1, y_part1, num_tiles, blocked_tiles1);
        draw_building(image_id + 2, x_part2, y_part2, color_mask);
        draw_building_tiles(x_part2, y_part2, num_tiles, blocked_tiles2);
        draw_building(image_id, x_part3, y_part3, color_mask);
        draw_building_tiles(x_part3, y_part3, num_tiles, blocked_tiles3);
    } else if (orientation_index == 3) {
        int image_id = Image::group(GROUP_BUILDING_HIPPODROME_1);
        // part 3, 2, 1
        draw_building(image_id + 4, x_part3, y_part3, color_mask);
        draw_building_tiles(x_part3, y_part3, num_tiles, blocked_tiles3);
        draw_building(image_id + 2, x_part2, y_part2, color_mask);
        draw_building_tiles(x_part2, y_part2, num_tiles, blocked_tiles2);
        draw_building(image_id, x_part1, y_part1, color_mask);
        draw_building_tiles(x_part1, y_part1, num_tiles, blocked_tiles1);
    }
    int is_blocked = has_blocked_tiles(num_tiles, blocked_tiles1) || has_blocked_tiles(num_tiles, blocked_tiles2) ||
        has_blocked_tiles(num_tiles, blocked_tiles3);
    set_roamer_path(type, building_block_size, tile, is_blocked);
}

static void draw_waterside_building(const map_tile *tile, int x, int y, building_type type)
{
    int dir_absolute = 0;
    int dir_relative = 0;
    const building_properties *props = building_properties_for_type(type);
    int blocked_tiles[9];
    int blocked = map_water_determine_orientation(tile->x, tile->y, props->size, 1, &dir_absolute, &dir_relative,
        0, blocked_tiles);
    if (city_finance_out_of_money()) {
        blocked = 1;
    }

    const waterside_tile_loop *loop = map_water_get_waterside_tile_loop(dir_absolute, props->size);
    int land_tiles[5 * MAP_WATER_WATERSIDE_ROWS_NEEDED];
    int has_water_in_front = map_water_has_water_in_front(tile->x, tile->y, 1, loop, land_tiles);
    int force_place_clear_cost = 0;
    int force_place_valid = building_construction_force_place_active() &&
        building_construction_force_place_assess(type, tile->x, tile->y, 0, &force_place_clear_cost);
    if (force_place_valid) {
        building_construction_set_force_place_clear_cost(force_place_clear_cost);
        blocked = 0;
        for (int i = 0; i < props->size * props->size; i++) {
            blocked_tiles[i] = 0;
        }
    }

    color_t color = blocked || !has_water_in_front ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;

    if (!has_water_in_front) {
        for (int i = 0; i < props->size * props->size; i++) {
            blocked_tiles[i] = 1;
        }
    }

    int can_place = !blocked && has_water_in_front;
    building_construction_set_can_place(can_place);
    if (draw_runtime_regular_building(type, tile->grid_offset, x, y, props->size, color)) {
        draw_building_tiles(x, y, props->size * props->size, blocked_tiles);
    } else {
        int offset_multiplier = is_dock_type(type) ? 12 : 1;
        int image_id = Image::group(props->image_group) + props->image_offset + dir_relative * offset_multiplier;
        draw_building(image_id, x, y, color);
        draw_building_tiles(x, y, props->size * props->size, blocked_tiles);
    }

    if (blocked || has_water_in_front || force_place_valid) {
        return;
    }

    loop = map_water_get_waterside_tile_loop(dir_relative, props->size);
    int dx = loop->start.x;
    int dy = loop->start.y;
    int index = 0;

    for (int outer = 0; outer < MAP_WATER_WATERSIDE_ROWS_NEEDED; outer++) {
        for (int inner = 0; inner < loop->inner_length; inner++) {
            // Don't highlight over the dock, which is already highlighted by the blocked tiles
            if (outer > 0 || inner == 0 || inner == loop->inner_length - 1) {
                if (land_tiles[index]) {
                    Image::blend_footprint_color(x + X_VIEW_OFFSET(dx, dy), y + Y_VIEW_OFFSET(dx, dy), COLOR_MASK_RED, data.scale);
                } else {
                    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw_isometric_footprint(x + X_VIEW_OFFSET(dx, dy), y + Y_VIEW_OFFSET(dx, dy), COLOR_MASK_FOOTPRINT_GHOST, data.scale);
                }
            }
            dx += loop->inner_step.x;
            dy += loop->inner_step.y;
            index++;
        }
        if (loop->outer_step.y) {
            dx = loop->start.x;
            dy += loop->outer_step.y;
        } else {
            dy = loop->start.y;
            dx += loop->outer_step.x;
        }
    }
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

static void draw_highway(const map_tile *tile, int x, int y, building_type type)
{
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

    int image_id = get_new_building_image_id(grid_offset, type);
    draw_regular_building(type, image_id, x, y, grid_offset, num_tiles, blocked_tiles);
}

static void draw_grand_temple_neptune_range(int x, int y, int grid_offset)
{
    color_t color_mask = data.reservoir_range.blocked ? COLOR_MASK_GRAY : COLOR_MASK_BLUE;
    draw_water_range_overlay(x, y, color_mask);
}

static void draw_grand_temple_neptune(const map_tile *tile, int x, int y, building_type type)
{
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
    int image_id = get_new_building_image_id(tile->grid_offset, type);
    draw_regular_building(type, image_id, x, y, tile->grid_offset, num_tiles, blocked);
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

static void draw_partial_grid(int grid_offset, int x, int y, building_type type)
{
    int size = building_properties_for_type(type)->size;
    int orientation_index = city_view_orientation() / 2;
    if (building_is_farm(type) || is_draggable_reservoir_type(type) || is_warehouse_type(type)) {
        size = 3;
        if (is_draggable_reservoir_type(type)) {
            grid_offset += RESERVOIR_GRID_OFFSETS[orientation_index];
        }
    }
    draw_grid_around_building(grid_offset, size, orientation_index, x, y);
    if (building_is_fort(type)) {
        grid_offset += FORT_GROUND_GRID_OFFSETS[building_rotation_get_rotation()][orientation_index];
        int ground_index = building_rotation_get_building_orientation(building_rotation_get_rotation()) / 2;
        int x_ground = x + FORT_GROUND_X_VIEW_OFFSETS[ground_index];
        int y_ground = y + FORT_GROUND_Y_VIEW_OFFSETS[ground_index];
        draw_grid_around_building(grid_offset, 4, ground_index, x_ground, y_ground);
    } else if (is_hippodrome_type(type)) {
        building_rotation_force_two_orientations();
        orientation_index = building_rotation_get_building_orientation(building_rotation_get_rotation()) / 2;
        int grid_offset2 = grid_offset + building_rotation_get_delta_with_rotation(5);
        int grid_offset3 = grid_offset + building_rotation_get_delta_with_rotation(10);
        int x_part2 = x + HIPPODROME_X_VIEW_OFFSETS[orientation_index];
        int y_part2 = y + HIPPODROME_Y_VIEW_OFFSETS[orientation_index];
        int x_part3 = x_part2 + HIPPODROME_X_VIEW_OFFSETS[orientation_index];
        int y_part3 = y_part2 + HIPPODROME_Y_VIEW_OFFSETS[orientation_index];
        draw_grid_around_building(grid_offset2, size, orientation_index, x_part2, y_part2);
        draw_grid_around_building(grid_offset3, size, orientation_index, x_part3, y_part3);
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

void draw_hippodrome_desirability(const map_tile *tile, building_type type)
{
    int size = building_properties_for_type(type)->size;
    building_rotation_force_two_orientations();
    int grid_offset1 = tile->grid_offset;
    int grid_offset3 = grid_offset1 + building_rotation_get_delta_with_rotation(10);
    map_tile tile_part3 = *tile;
    tile_part3.grid_offset = grid_offset3;
    draw_desirability_range(tile, type, size);
    draw_desirability_range(&tile_part3, type, size);
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
    create_tile_offsets();
    data.scale = city_view_get_scale() / 100.0f;
    int x, y;
    city_view_get_selected_tile_pixels(&x, &y);

    const building_properties *props = building_properties_for_type(type);
    if ((config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL) && building_type_registry_has_definition(type)) ||
        (config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE) && props->draw_desirability_range)) {
        int building_size = (is_draggable_reservoir_type(type) || is_warehouse_type(type)) ? 3 : props->size;

        if (is_hippodrome_type(type)) {
            draw_hippodrome_desirability(tile, type);
        } else if (is_draggable_reservoir_type(type)) {
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
            draw_desirability_range(&shifted_tile, type, building_size);
        } else {
            draw_desirability_range(tile, type, building_size);
        }
    }

    if (!config_get(CONFIG_UI_SHOW_GRID) && config_get(CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION)) {
        draw_partial_grid(tile->grid_offset, x, y, type);
    }

    if (is_bridge_type(type)) {
        draw_bridge(tile, x, y, type);
        return;
    }

    if (is_draggable_reservoir_type(type)) {
        draw_draggable_reservoir(tile, x, y, type);
    } else if (is_aqueduct_type(type)) {
        draw_aqueduct(tile, x, y, type);
    } else if (building_is_fort(type)) {
        draw_fort(tile, x, y, type);
    } else if (is_hippodrome_type(type)) {
        draw_hippodrome(tile, x, y, type);
    } else if (is_waterside_type(type)) {
        draw_waterside_building(tile, x, y, type);
    } else if (is_road_type(type)) {
        draw_road(tile, x, y);
    } else if (is_highway_type(type)) {
        draw_highway(tile, x, y, type);
    } else if (is_grand_temple_neptune_type(type)) {
        draw_grand_temple_neptune(tile, x, y, type);
    } else {
        draw_default(tile, x, y, type);
    }
}
