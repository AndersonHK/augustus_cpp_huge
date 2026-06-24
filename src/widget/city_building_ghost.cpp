#include "building/building_type.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/granary.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "building/water_access_runtime.h"
#include "figure/roamer_preview.h"
#include "figuretype/animal.h"
#include "graphics/image.h"
#include "graphics/runtime_overlay_images.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/tiles.h"
#include "map/water.h"
#include "widget/city.h"
#include "widget/city_water_ghost.h"

#include "city_building_ghost.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"
#include "core/log.h"


#include "assets/assets.h"
#include "building/monument.h"
#include "building/building_type_api.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/view.h"
#include "core/config.h"
#include "figure/figure.h"
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
#define MAX_COMPOSED_GHOST_PARTS 16

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

static int force_place_can_clear_terrain(int terrain)
{
    return terrain && !(terrain & ~(TERRAIN_TREE | TERRAIN_SHRUB | TERRAIN_ROAD));
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

static void prepare_ghost_building_with_size(int grid_offset, building_type type, int building_size)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    data.ghost_building = {};
    data.ghost_building.type = type;
    data.ghost_building.grid_offset = grid_offset;
    data.ghost_building.state = BUILDING_STATE_IN_USE;
    data.ghost_building.size = static_cast<unsigned char>(building_size);
    data.ghost_building.num_workers = definition->required_workers();
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
    if (building_rotation_type_has_rotations(type) || (definition && definition->has_composition())) {
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
    char detail[256];
    snprintf(
        detail,
        sizeof(detail),
        "building_attr=%s grid_offset=%d",
        definition->attr(),
        grid_offset);
    log_info("Native ghost graphics draw failed: ", detail, 0);
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

static void prepare_composed_ghost_part(
    building &part_record,
    building_type type,
    int x,
    int y,
    const building &main_record)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    part_record = {};
    part_record.type = type;
    part_record.state = BUILDING_STATE_IN_USE;
    part_record.size = static_cast<unsigned char>(definition->model().size());
    part_record.num_workers = definition->required_workers();
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

static int draw_runtime_ghost_record(building &record, int x, int y, color_t color)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(record.type);
    if (!definition || !definition->has_graphic()) {
        return 0;
    }
    Building building(record, definition);
    if (!building.draw_footprint({ x, y, record.grid_offset, color, data.scale, 1 })) {
        return 0;
    }
    building.draw_top({ x, y, record.grid_offset, color, data.scale, 1 });
    draw_runtime_ghost_animation(building, record.grid_offset, x, y, color);
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

struct GhostPreviewPart {
    building record = {};
    int tile_x = 0;
    int tile_y = 0;
    int draw_x = 0;
    int draw_y = 0;
    int num_tiles = 0;
    int blocked_tiles[MAX_TILES] = {};
};

static int type_size(const building_type_registry_impl::BuildingType &type)
{
    return type.model().size();
}

static int placement_size(const building_type_registry_impl::BuildingType &type)
{
    if (!type.has_composition()) {
        return type_size(type);
    }
    const building_type_registry_impl::ComposedBuildingDefinition &composition = type.composition();
    return composition.footprint_width() > composition.footprint_height() ?
        composition.footprint_width() :
        composition.footprint_height();
}

static building_type selected_ghost_type(int grid_offset, building_type type)
{
    if (building_connectable_gate_type(type) && map_terrain_get(grid_offset) & TERRAIN_ROAD) {
        return static_cast<building_type>(building_connectable_gate_type(type));
    }
    return type;
}

static Building create_ghost_building(int grid_offset, building_type type)
{
    const building_type_registry_impl::BuildingType &definition =
        *building_type_registry_impl::definition_for_type(type);
    prepare_ghost_building_with_size(grid_offset, type, placement_size(definition));
    data.ghost_building.num_workers = definition.required_workers();
    return Building(data.ghost_building, &definition);
}

static int grid_offset_from_origin(int origin_grid_offset, int x, int y)
{
    return origin_grid_offset + map_grid_delta(x, y);
}

static void set_part_draw_position(GhostPreviewPart &part, int x, int y, int x_offset, int y_offset)
{
    int size_x = 0;
    int size_y = 0;
    draw_tile_view_offset(part.record.size, &size_x, &size_y);
    part.tile_x = x + X_VIEW_OFFSET(x_offset, y_offset);
    part.tile_y = y + Y_VIEW_OFFSET(x_offset, y_offset);
    part.draw_x = part.tile_x + size_x;
    part.draw_y = part.tile_y + size_y;
}

static GhostPreviewPart make_regular_part(const Building &building, int x, int y)
{
    GhostPreviewPart part = {};
    part.record = *building.record();
    part.num_tiles = part.record.size * part.record.size;
    set_part_draw_position(part, x, y, 0, 0);
    return part;
}

static GhostPreviewPart make_composed_part(
    const building &main_record,
    building_type type,
    int origin_x,
    int origin_y,
    int x,
    int y,
    const building_type_registry_impl::ComposedPartOffset &offset)
{
    GhostPreviewPart part = {};
    prepare_composed_ghost_part(part.record, type, origin_x + offset.x, origin_y + offset.y, main_record);
    part.num_tiles = part.record.size * part.record.size;
    set_part_draw_position(part, x, y, offset.x, offset.y);
    return part;
}

static int collect_ghost_parts(const Building &building, int x, int y, GhostPreviewPart *parts)
{
    if (!building.type->has_composition()) {
        parts[0] = make_regular_part(building, x, y);
        return 1;
    }

    const ::building record = *building.record();
    const int origin_x = record.x;
    const int origin_y = record.y;
    const int rotation = building.orientation();
    const building_type_registry_impl::ComposedBuildingDefinition &composition = building.type->composition();
    int count = 0;
    for (const building_type_registry_impl::ComposedPartDefinition &part_definition : composition.parts()) {
        const building_type_registry_impl::ComposedPartOffset offset = part_definition.offset_for_rotation(rotation);
        if (part_definition.type == BUILDING_NONE || !offset.has_value || count >= MAX_COMPOSED_GHOST_PARTS) {
            continue;
        }
        parts[count++] = make_composed_part(
            record,
            part_definition.type,
            origin_x,
            origin_y,
            x,
            y,
            offset);
    }

    const building_type_registry_impl::ComposedPartOffset main_offset =
        composition.main_offset_for_rotation(rotation);
    ::building main_record = record;
    main_record.size = static_cast<unsigned char>(type_size(*building.type));
    main_record.x = static_cast<unsigned char>(origin_x + main_offset.x);
    main_record.y = static_cast<unsigned char>(origin_y + main_offset.y);
    main_record.grid_offset = grid_offset_from_origin(record.grid_offset, main_offset.x, main_offset.y);
    parts[count].record = main_record;
    parts[count].num_tiles = main_record.size * main_record.size;
    set_part_draw_position(parts[count], x, y, main_offset.x, main_offset.y);
    return count + 1;
}

static int terrain_allowed_for_part(building_type type, int tile_index, int terrain)
{
    if (is_road_surface_type(type)) {
        terrain &= ~TERRAIN_ROAD;
    }
    if (is_gatehouse_type(type)) {
        terrain &= ~(TERRAIN_HIGHWAY | TERRAIN_WALL | TERRAIN_ROAD);
        if (terrain & TERRAIN_WALL) {
            terrain &= ~TERRAIN_BUILDING;
        }
    }
    if (building_type_attr_is(type, "tower")) {
        terrain &= ~TERRAIN_WALL & ~TERRAIN_BUILDING;
    }
    if (config_get(CONFIG_GP_CH_WAREHOUSES_GRANARIES_OVER_ROAD_PLACEMENT)) {
        if (is_warehouse_type(type)) {
            terrain &= ~TERRAIN_ROAD;
        } else if (is_granary_type(type) && building_construction_is_granary_cross_tile(tile_index)) {
            terrain &= ~TERRAIN_ROAD;
        }
    }
    return terrain;
}

static int validate_ghost_part(
    GhostPreviewPart &part,
    int root_blocked,
    int force_place_valid,
    int check_figures)
{
    int blocked = root_blocked;
    const int orientation_index = city_view_orientation() / 2;
    for (int i = 0; i < part.num_tiles; i++) {
        const int tile_offset = part.record.grid_offset + tile_grid_offset(orientation_index, i);
        const int raw_terrain = map_terrain_get(tile_offset) & TERRAIN_NOT_CLEAR;
        const int forbidden_terrain = terrain_allowed_for_part(part.record.type, i, raw_terrain);
        const int force_clearable = force_place_valid && force_place_can_clear_terrain(forbidden_terrain);
        if (check_figures && map_has_figure_at(tile_offset)) {
            part.blocked_tiles[i] = TILE_FORBIDDEN;
            figure_animal_try_nudge_at(part.record.grid_offset, tile_offset, part.record.size);
        } else if (root_blocked || (forbidden_terrain && !force_clearable)) {
            part.blocked_tiles[i] = TILE_FORBIDDEN;
        } else if (forbidden_terrain) {
            part.blocked_tiles[i] = TILE_DISCOURAGED;
        } else {
            part.blocked_tiles[i] = TILE_ALLOWED;
        }
        blocked |= part.blocked_tiles[i] != TILE_ALLOWED;
    }
    return blocked;
}

static int placement_blocked_by_city_state(const Building &building, const map_tile *tile)
{
    const int type = building.type->type();
    int x = tile->x;
    int y = tile->y;
    building_construction_offset_start_from_orientation(&x, &y, placement_size(*building.type));
    if (!building_construction_can_place_on_terrain(x, y, 0, 0)) {
        return 1;
    }
    if (building_type_attr_is(static_cast<building_type>(type), "senate") && city_buildings_has_senate()) {
        return 1;
    }
    if (building_type_attr_is(static_cast<building_type>(type), "city_mint") &&
        (!city_buildings_has_senate() || city_buildings_has_city_mint())) {
        return 1;
    }
    if (building_type_attr_is(static_cast<building_type>(type), "caravanserai") && city_buildings_has_caravanserai()) {
        return 1;
    }
    if (building_type_attr_is(static_cast<building_type>(type), "barracks") && city_buildings_has_barracks() &&
        !config_get(CONFIG_GP_CH_MULTIPLE_BARRACKS)) {
        return 1;
    }
    if (building_type_attr_is(static_cast<building_type>(type), "mess_hall") && city_buildings_has_mess_hall()) {
        return 1;
    }
    if (is_plaza_type(static_cast<building_type>(type)) && !map_terrain_is(tile->grid_offset, TERRAIN_ROAD)) {
        return 1;
    }
    if (is_roadblock_type(static_cast<building_type>(type)) && !map_terrain_is(tile->grid_offset, TERRAIN_ROAD)) {
        return 1;
    }
    if (!building_monument_type_is_mini_monument(static_cast<building_type>(type)) &&
        building_monument_get_id(static_cast<building_type>(type))) {
        return 1;
    }
    if (building_monument_is_grand_temple(static_cast<building_type>(type)) &&
        building_monument_count_grand_temples() >= config_get(CONFIG_GP_CH_MAX_GRAND_TEMPLES)) {
        return 1;
    }
    return city_finance_out_of_money();
}

static void draw_ghost_part(GhostPreviewPart &part, color_t color)
{
    if (!draw_runtime_ghost_record(part.record, part.draw_x, part.draw_y, color)) {
        log_native_ghost_draw_failure(part.record.type, part.record.grid_offset);
    }
    draw_building_tiles(part.tile_x, part.tile_y, part.num_tiles, part.blocked_tiles);
}

static void set_roamer_path(const Building &building, const map_tile *tile, int is_blocked)
{
    const building_type type = building.type->type();
    if (data.roamer_preview.grid_offset == tile->grid_offset && data.roamer_preview.type == type) {
        return;
    }
    figure_roamer_preview_reset(type);
    data.roamer_preview.type = type;
    data.roamer_preview.grid_offset = tile->grid_offset;
    if (!is_blocked) {
        figure_roamer_preview_create(type, building.x(), building.y());
    }
}

static void draw_desirability_range_at(
    int grid_offset,
    int building_size,
    const building_type_registry_impl::BuildingType &type)
{
    int desirability_value = type.model().desirability_value();
    int desirability_step_size = type.model().desirability_step_size();
    int desirability_range = type.model().desirability_range();
    int negative_range = 0;
    if (desirability_value == 0 || desirability_range == 0) {
        return;
    }
    if (building_is_statue_garden_temple(type.type()) && building_monument_working_grand_temple_for_god(GOD_VENUS)) {
        int value_bonus = ((desirability_value / 4) > 1) ? (desirability_value / 4) : 1;
        desirability_value += value_bonus;
        if (!type.is_temple()) {
            desirability_range += 1;
        }
    }
    while (desirability_value < 0 && negative_range < desirability_range) {
        desirability_value += desirability_step_size;
        negative_range++;
    }
    const int positive_range = desirability_range - negative_range;
    if (positive_range > 0) {
        city_view_foreach_tile_in_range(grid_offset, building_size, desirability_range,
            city_building_ghost_draw_bonus_range);
    }
    if (negative_range > 0) {
        city_view_foreach_tile_in_range(grid_offset, building_size, negative_range,
            city_building_ghost_draw_malus_range);
    }
}

static void draw_desirability_range(const Building &building)
{
    if (!config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE_ALL) &&
        !(config_get(CONFIG_UI_SHOW_DESIRABILITY_RANGE) &&
            building.type->flags().draw_desirability_range())) {
        return;
    }

    GhostPreviewPart parts[MAX_COMPOSED_GHOST_PARTS + 1] = {};
    const int part_count = collect_ghost_parts(building, 0, 0, parts);
    for (int i = 0; i < part_count; i++) {
        const building_type_registry_impl::BuildingType *type =
            building_type_registry_impl::definition_for_type(parts[i].record.type);
        if (type) {
            draw_desirability_range_at(parts[i].record.grid_offset, parts[i].record.size, *type);
        }
    }
}

static void draw_context_overlays(const Building &building, const map_tile *tile)
{
    draw_water_access_context_overlays(tile, building.type->type());
    draw_distribution_context_overlays(tile, building.type->type(), building.size());
}

static void draw_default(const map_tile *tile, int x_view, int y_view, Building &building)
{
    GhostPreviewPart parts[MAX_COMPOSED_GHOST_PARTS + 1] = {};
    const int part_count = collect_ghost_parts(building, x_view, y_view, parts);
    int force_place_clear_cost = 0;
    const int force_place_valid = building_construction_force_place_active() &&
        building_construction_force_place_assess(building.type->type(), tile->x, tile->y, 0, &force_place_clear_cost);
    if (force_place_valid) {
        building_construction_set_force_place_clear_cost(force_place_clear_cost);
    }
    const int root_blocked = placement_blocked_by_city_state(building, tile);
    int blocked = root_blocked;
    for (int i = 0; i < part_count; i++) {
        const int check_figures =
            ((!is_plaza_type(parts[i].record.type) && !is_roadblock_type(parts[i].record.type)) ||
                parts[i].record.size != 1);
        blocked |= validate_ghost_part(parts[i], root_blocked, force_place_valid, check_figures);
    }
    building_construction_set_can_place(!blocked);
    const color_t color = blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    draw_context_overlays(building, tile);
    for (int i = 0; i < part_count; i++) {
        draw_ghost_part(parts[i], color);
    }
    set_roamer_path(building, tile, blocked);
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

static void draw_partial_grid(const Building &building, int x, int y)
{
    GhostPreviewPart parts[MAX_COMPOSED_GHOST_PARTS + 1] = {};
    const int part_count = collect_ghost_parts(building, x, y, parts);
    const int orientation_index = building_rotation_get_building_orientation(building.orientation()) / 2;
    for (int i = 0; i < part_count; i++) {
        draw_grid_around_building(
            parts[i].record.grid_offset,
            parts[i].record.size,
            orientation_index,
            parts[i].tile_x,
            parts[i].tile_y);
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
    if (!building_construction_has_active_tool()) {
        return;
    }
    if (building_construction_is_updatable()) {
        return;
    }
    building_type type = selected_ghost_type(tile->grid_offset, building_construction_type());
    data.ghost_building.type = type;
    if (building_construction_draw_as_constructing() // || type == BUILDING_NONE we keep checking for none everywhere, none type, none building, we should never check for none, if it's none it's an invalid state, crash. It should crash and then we fix what caused it. Stop checking for none everywhere
        || building_construction_is_land_work_type(type)) {
        return;
    }
    create_tile_offsets();
    data.scale = city_view_get_scale() / 100.0f;
    int x, y;
    city_view_get_selected_tile_pixels(&x, &y);
    Building building = create_ghost_building(tile->grid_offset, type);

	// const building_properties *props = building_properties_for_type(type); we should be just getting the building type directly from the instanciated building

    /* why in the world are we re - teaching the code how to draw desirability when we already have a desirability overlay drawing routine in city overlays?
    * this should be a single function call taking the building as the singular input. The building already has all the information needed to draw this.
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
    }*/
    draw_desirability_range(building);

    if (!config_get(CONFIG_UI_SHOW_GRID) && config_get(CONFIG_UI_SHOW_PARTIAL_GRID_AROUND_CONSTRUCTION)) {
        draw_partial_grid(building, x, y);


    }

    draw_default(tile, x, y, building); // we instanciate the buildings ephemerally, exactly as the live ones will be when they are placed, re-using the exact same code the placement uses so what we see is what we get, we render them through the same pipeline and with default methods

    /* all this should be deleted
    if (is_bridge_type(type)) { //this
        draw_bridge(tile, x, y, type);
        return;
    }

    if (is_draggable_reservoir_type(type)) { // and this must go
        draw_draggable_reservoir(tile, x, y, type);
    } else if (is_aqueduct_type(type)) { // and this
        draw_aqueduct(tile, x, y, type);
    } else if (building_is_fort(type)) { // you get the point...
        draw_fort(tile, x, y, type);
    } else if (is_hippodrome_type(type)) {
        draw_native_composed_building(tile, x, y, type);
    } else if (is_waterside_type(type)) {
        draw_waterside_building(tile, x, y, type);
    } else if (is_road_type(type)) {
        draw_road(tile, x, y);
    } else if (is_highway_type(type)) {
        draw_highway(tile, x, y, type);
    } else if (is_grand_temple_neptune_type(type)) {
        draw_grand_temple_neptune(tile, x, y, type);
    } else {
        (tile, x, y, type);
    }*/
}
