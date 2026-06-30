#include "building/building_type.h"
#include "building/connectable.h"
#include "building/construction.h"
#include "building/construction_building.h"
#include "building/granary.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/roadblock.h"
#include "building/rotation.h"
#include "building/religion.h"
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
#include "map/orientation.h"
#include "map/road_aqueduct.h"
#include "map/tiles.h"
#include "map/water_supply.h"
#include "widget/city.h"
#include "widget/city_water_ghost.h"

#include "city_building_ghost.h"

#include "building/BuildingGraphicsState.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/construction_plan.h"
#include "building/construction_routed.h"
#include "building/building_type_registry_internal.h"
#include "building/market.h"


#include "assets/assets.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/view.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/PathingMode.h"
#include "graphics/renderer.h"
#include "input/scroll.h"
#include "map/data.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"

#include <cstddef>
#include <vector>

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
    int animation_preview_cursor_by_type[BUILDING_TYPE_MAX];
    tile_xy_offsets offsets[4][MAX_TILES];
} data = {
    .scale = SCALE_NONE
};

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

static void draw_building(int image_id, int x, int y, color_t color)
{
    Image::from_id(image_id).draw_isometric_footprint(x, y, color, data.scale);
    Image::from_id(image_id).draw_isometric_top(x, y, color, data.scale);
}

static void prepare_ghost_water_access_state(
    const building_type_registry_impl::BuildingType &definition,
    building &ghost)
{
    if (definition.water_access().has_requirements() || definition.has_water_access_provider()) {
        BuildingGraphicsState graphics_state;
        Building ghost_building(ghost, graphics_state);
        ghost.has_water_access = static_cast<unsigned char>(water_access_runtime_building_has_required_access(&ghost_building));
    }
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
    const int cursor_index = static_cast<int>(type);
    if (cursor_index <= BUILDING_NONE || cursor_index >= BUILDING_TYPE_MAX) {
        return;
    }

    // Ghosts reuse the real grid offset as an animation cursor. Save/restore the
    // map sprite byte so preview animation never leaks into the city map state.
    const int saved_cursor = map_sprite_animation_at(animation_cursor);
    map_sprite_animation_set(animation_cursor, data.animation_preview_cursor_by_type[cursor_index]);
    building.draw_animation({ x, y, animation_cursor, color, data.scale, 1 });
    data.animation_preview_cursor_by_type[cursor_index] = map_sprite_animation_at(animation_cursor);
    map_sprite_animation_set(animation_cursor, saved_cursor);
}

static void draw_blocked_tile(int x, int y, int grid_offset)
{
    (void) grid_offset;
    Image::blend_footprint_color(x, y, COLOR_MASK_RED, data.scale);
}

static void city_building_ghost_draw_malus_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_NEGATIVE_RANGE, data.scale);
}

static void city_building_ghost_draw_bonus_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
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
    (void) grid_offset;
    draw_water_range_overlay(x, y, COLOR_MASK_DARK_BLUE);
}

void city_building_ghost_draw_fountain_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    draw_water_range_overlay(x, y, COLOR_MASK_BLUE);
}

void city_building_ghost_draw_reservoir_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    draw_water_range_overlay(x, y, COLOR_MASK_RESERVOIR_RANGE);
}

void city_building_ghost_draw_aqueduct_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    draw_water_range_overlay(x, y, COLOR_MASK_RESERVOIR_RANGE);
}

void city_building_ghost_draw_latrines_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    Image::from_id(Image::group(GROUP_TERRAIN_FLAT_TILE)).draw(x, y, COLOR_MASK_DARK_GREEN & ALPHA_FONT_SEMI_TRANSPARENT, data.scale);
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
        if (!map_building_exists_at(tile->grid_offset)) {
            return;
        }
        Building existing = map_building_at(tile->grid_offset).main();
        const building *b = existing.record();
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
    if (definition.flags().venus_gt_bonus() &&
        grand_temple_for_god(GOD_VENUS, true)) {
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
    const int uses_reservoir_range =
        water_access_runtime_building_type_requires_access_text(&definition, "reservoir") ||
        water_access_runtime_building_type_provides_access_text(&definition, "reservoir");
    if (uses_reservoir_range && config_get(CONFIG_UI_BUILD_SHOW_RESERVOIR_RANGES)) {
        city_water_ghost_draw_reservoir_ranges();
    }

    if (!definition.has_housing() &&
        config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE) &&
        (water_access_runtime_building_type_requires_access_text(&definition, "fountain") ||
         water_access_runtime_building_type_requires_access_text(&definition, "well"))) {
        city_water_ghost_draw_water_structure_ranges();
    }

    if (water_access_runtime_building_type_provides_access(&definition) &&
        config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE)) {
        city_water_ghost_draw_preview(&definition, tile->grid_offset, 0);
    }

    if (definition.has_housing() && config_get(CONFIG_UI_SHOW_WATER_STRUCTURE_RANGE_HOUSES)) {
        city_water_ghost_draw_water_structure_ranges();
    }
}

static void draw_market_range_tile(int x, int y, int grid_offset)
{
    (void) grid_offset;
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
        BuildingGraphicsState graphics_state;
        Building preview_building(market_record, &definition, graphics_state);
        Market market(preview_building);
        city_view_foreach_tile_in_range(tile->grid_offset, building_size, market.max_supplier_distance(),
            draw_market_range_tile);
    }
}

static void draw_grand_temple_neptune_range(int x, int y, int grid_offset)
{
    (void) grid_offset;
    color_t color_mask = data.reservoir_range.blocked ? COLOR_MASK_GRAY : COLOR_MASK_BLUE;
    draw_water_range_overlay(x, y, color_mask);
}

static void draw_grand_temple_neptune_context_overlay(
    const map_tile *tile,
    const building_type_registry_impl::BuildingType &definition,
    int building_size,
    int blocked)
{
    if (!definition.is_temple(GOD_NEPTUNE, building_type_registry_impl::ReligionTier::Grand)) {
        return;
    }
    data.reservoir_range.blocked = blocked;
    int radius = map_water_supply_reservoir_radius();
    if (!grand_temple_for_god(GOD_NEPTUNE, true)) {
        radius += 2;
    }
    city_view_foreach_tile_in_range(tile->grid_offset, building_size, radius, draw_grand_temple_neptune_range);
}

static building make_plan_ghost_record(
    const building_construction::ConstructionPlacementPlan &plan,
    const building_construction::ConstructionPlacementPart &part,
    const building_type_registry_impl::BuildingType &root_definition)
{
    const building_type_registry_impl::BuildingType &part_definition = *part.definition;
    building record = {};
    record.type = part.type;
    record.grid_offset = static_cast<short>(part.grid_offset);
    record.x = static_cast<unsigned char>(part.x);
    record.y = static_cast<unsigned char>(part.y);
    record.state = BUILDING_STATE_IN_USE;
    record.size = static_cast<unsigned char>(part.size);
    record.num_workers = static_cast<short>(part_definition.required_workers());
    record.data.entertainment.days1 = 1;
    record.data.entertainment.days2 = 1;
    if (part_definition.is_granary()) {
        record.resources[RESOURCE_NONE] = FULL_GRANARY;
    }
    if (plan.waterside_orientation_absolute() >= 0 &&
        part_definition.foundation().policy_type() == building_type_registry_impl::FoundationPolicy::Shoreline) {
        record.subtype.orientation = static_cast<short>(plan.waterside_orientation_absolute());
        record.data.dock.orientation = static_cast<signed char>(plan.waterside_orientation_absolute());
    } else if (building_rotation_type_has_rotations(root_definition.type()) ||
        part_definition.has_composition()) {
        record.subtype.orientation = static_cast<short>(building_rotation_get_rotation());
    }
    prepare_ghost_water_access_state(part_definition, record);
    return record;
}

static BuildingGraphicsState make_plan_ghost_graphics_state(
    const building_construction::ConstructionPlacementPart &part,
    const building_type_registry_impl::BuildingType &root_definition)
{
    BuildingGraphicsState state;
    if (building_variant_has_variants(part.type)) {
        state.set_variant(building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(part.type)));
    } else if (building_variant_has_variants(root_definition.type())) {
        state.set_variant(
            building_rotation_get_rotation_with_limit(building_variant_get_number_of_variants(root_definition.type())));
    }
    return state;
}

static int plan_main_part_index(const building_construction::ConstructionPlacementPlan &plan)
{
    const std::vector<building_construction::ConstructionPlacementPart> &parts = plan.parts();
    for (size_t i = 0; i < parts.size(); i++) {
        if (parts[i].type == plan.type()) {
            return static_cast<int>(i);
        }
    }
    return parts.empty() ? -1 : 0;
}

static void link_plan_ghost_records(std::vector<building> &records, int main_index)
{
    if (records.empty() || main_index < 0 || static_cast<size_t>(main_index) >= records.size()) {
        return;
    }

    for (size_t i = 0; i < records.size(); i++) {
        records[i].id = static_cast<unsigned int>(i + 1);
        records[i].prev_part_building_id = 0;
        records[i].next_part_building_id = 0;
    }

    std::vector<size_t> chain;
    chain.reserve(records.size());
    chain.push_back(static_cast<size_t>(main_index));
    for (size_t i = 0; i < records.size(); i++) {
        if (i != static_cast<size_t>(main_index)) {
            chain.push_back(i);
        }
    }

    for (size_t i = 0; i < chain.size(); i++) {
        building &record = records[chain[i]];
        record.prev_part_building_id =
            i == 0 ? 0 : static_cast<short>(records[chain[i - 1]].id);
        record.next_part_building_id =
            i + 1 < chain.size() ? static_cast<short>(records[chain[i + 1]].id) : 0;
    }
}

static void build_plan_ghost_runtime(
    const building_construction::ConstructionPlacementPlan &plan,
    const building_type_registry_impl::BuildingType &root_definition,
    std::vector<building> &records,
    std::vector<building_runtime_impl::EphemeralBuildingRuntimeBinding> &bindings)
{
    const std::vector<building_construction::ConstructionPlacementPart> &parts = plan.parts();
    records.clear();
    bindings.clear();
    records.reserve(parts.size());
    bindings.reserve(parts.size());

    for (const building_construction::ConstructionPlacementPart &part : parts) {
        records.push_back(make_plan_ghost_record(plan, part, root_definition));
    }

    const int main_index = plan_main_part_index(plan);
    link_plan_ghost_records(records, main_index);
    if (records.empty() || main_index < 0) {
        return;
    }

    const unsigned int main_runtime_id = records[static_cast<size_t>(main_index)].id;
    for (size_t i = 0; i < parts.size(); i++) {
        building_runtime_impl::EphemeralBuildingRuntimeBinding binding;
        binding.runtime_id = records[i].id;
        binding.main_runtime_id = main_runtime_id;
        binding.record = &records[i];
        binding.definition = parts[i].definition;
        binding.graphics_state = make_plan_ghost_graphics_state(parts[i], root_definition);
        bindings.push_back(binding);
    }
}

static void draw_plan_tiles(
    const building_construction::ConstructionPlacementPart &part,
    int x,
    int y,
    int show_blocked_tiles)
{
    for (const building_construction::ConstructionPlacementTile &tile : part.tiles) {
        const int tile_x = x + X_VIEW_OFFSET(tile.dx, tile.dy);
        const int tile_y = y + Y_VIEW_OFFSET(tile.dx, tile.dy);
        if (show_blocked_tiles && tile.state == building_construction::PlacementTileState::Forbidden) {
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
    Building &preview,
    int x_view,
    int y_view,
    color_t color,
    int show_blocked_tiles)
{
    const building *record = preview.record();
    if (!record) {
        return;
    }

    int x_size_offset = 0;
    int y_size_offset = 0;
    draw_tile_view_offset(part.size, &x_size_offset, &y_size_offset);
    const int tile_x = x_view + X_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
    const int tile_y = y_view + Y_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
    const int draw_x = tile_x + x_size_offset;
    const int draw_y = tile_y + y_size_offset;
    preview.draw_footprint({ draw_x, draw_y, record->grid_offset, color, data.scale, 1 });
    preview.draw_top({ draw_x, draw_y, record->grid_offset, color, data.scale, 1 });
    draw_runtime_ghost_animation(preview, record->grid_offset, draw_x, draw_y, color);
    draw_plan_tiles(part, tile_x, tile_y, show_blocked_tiles);
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
    const building_type_registry_impl::BuildingType &root_definition,
    int exact_coordinates = 0,
    int update_can_place = 1,
    int force_blocked = 0,
    int show_plan_blocked_tiles = 1)
{
    const building_type_registry_impl::BuildingType &definition =
        effective_ghost_definition(root_definition, tile->grid_offset);
    const building_type type = definition.type();

    const int force_place_active = building_construction_force_place_active();
    building_construction_assessment assessment =
        building_construction_assess_placement(definition, tile->x, tile->y, exact_coordinates, force_place_active);
    const building_construction::ConstructionPlacementPlan &plan = assessment.placement;
    if (force_place_active && assessment.can_place) {
        building_construction_set_force_place_clear_cost(assessment.clear_cost);
    }
    const int plan_blocked = !plan.can_place();
    const int construction_blocked = !assessment.can_place;
    const int blocked = city_finance_out_of_money() || construction_blocked || plan_blocked || force_blocked;
    const int global_blocked = city_finance_out_of_money() || assessment.global_blocked || force_blocked;
    if (update_can_place) {
        building_construction_set_can_place(!blocked);
    }

    const color_t color = global_blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST;
    draw_water_access_context_overlays(tile, definition);
    draw_grand_temple_neptune_context_overlay(tile, definition, plan.placement_size(), blocked);
    draw_distribution_context_overlays(tile, definition, plan.placement_size());
    std::vector<building> ghost_records;
    std::vector<building_runtime_impl::EphemeralBuildingRuntimeBinding> ghost_bindings;
    build_plan_ghost_runtime(plan, definition, ghost_records, ghost_bindings);
    building_runtime_impl::ScopedEphemeralBuildingRuntime ghost_runtime(ghost_bindings);
    const std::vector<building_construction::ConstructionPlacementPart> &parts = plan.parts();
    for (size_t i = 0; i < parts.size() && i < ghost_records.size(); i++) {
        building_runtime *runtime = building_runtime_impl::get_ephemeral_instance(&ghost_records[i]);
        if (!runtime) {
            continue;
        }
        draw_plan_part(plan, parts[i], runtime->building, x_view, y_view, color,
            !global_blocked && show_plan_blocked_tiles);
    }

    set_roamer_path(type, plan.placement_size(), tile, blocked);
}

static building make_aqueduct_ghost_record(
    const building_type_registry_impl::BuildingType &definition,
    int grid_offset,
    unsigned int id)
{
    building record = {};
    record.id = id;
    record.type = definition.type();
    record.grid_offset = static_cast<short>(grid_offset);
    record.x = static_cast<unsigned char>(map_grid_offset_to_x(grid_offset));
    record.y = static_cast<unsigned char>(map_grid_offset_to_y(grid_offset));
    record.state = BUILDING_STATE_IN_USE;
    record.size = 1;
    record.num_workers = static_cast<short>(definition.required_workers());
    prepare_ghost_water_access_state(definition, record);
    return record;
}

static void draw_aqueduct_preview_route(
    const map_tile *cursor_tile,
    int cursor_x_view,
    int cursor_y_view,
    const building_type_registry_impl::BuildingType &definition,
    const grid_slice &route,
    color_t color)
{
    if (route.size <= 0) {
        return;
    }

    std::vector<building> records;
    std::vector<building_runtime_impl::EphemeralBuildingRuntimeBinding> bindings;
    records.reserve(route.size);
    bindings.reserve(route.size);

    for (int i = 0; i < route.size; i++) {
        records.push_back(make_aqueduct_ghost_record(
            definition,
            route.grid_offsets[i],
            static_cast<unsigned int>(i + 1)));
    }
    for (size_t i = 0; i < records.size(); i++) {
        building_runtime_impl::EphemeralBuildingRuntimeBinding binding;
        binding.runtime_id = records[i].id;
        binding.main_runtime_id = records[i].id;
        binding.record = &records[i];
        binding.definition = &definition;
        bindings.push_back(binding);
    }

    building_connectable_set_aqueduct_preview(route.grid_offsets, route.size);
    {
        building_runtime_impl::ScopedEphemeralBuildingRuntime aqueduct_runtime(bindings);
        for (building &record : records) {
            building_runtime *runtime = building_runtime_impl::get_ephemeral_instance(&record);
            if (!runtime) {
                continue;
            }
            const int draw_x = cursor_x_view + X_VIEW_OFFSET(record.x - cursor_tile->x, record.y - cursor_tile->y);
            const int draw_y = cursor_y_view + Y_VIEW_OFFSET(record.x - cursor_tile->x, record.y - cursor_tile->y);
            runtime->building.draw_footprint({ draw_x, draw_y, record.grid_offset, color, data.scale, 1 });
            runtime->building.draw_top({ draw_x, draw_y, record.grid_offset, color, data.scale, 1 });
        }
    }
    building_connectable_set_aqueduct_preview(nullptr, 0);
}

static const building_type_registry_impl::BuildingType *reservoir_ghost_definition()
{
    const building_type reservoir_type = building_type_registry_impl::type_from_attr("reservoir");
    return building_type_registry_impl::definition_for_type(reservoir_type);
}

static map_tile reservoir_origin_tile_from_center(const map_tile *center_tile)
{
    map_tile origin_tile = *center_tile;
    origin_tile.x -= 1;
    origin_tile.y -= 1;
    origin_tile.grid_offset = map_grid_offset(origin_tile.x, origin_tile.y);
    return origin_tile;
}

static void reservoir_origin_view_from_center(int center_x, int center_y, int *origin_x, int *origin_y)
{
    *origin_x = center_x + X_VIEW_OFFSET(-1, -1);
    *origin_y = center_y + Y_VIEW_OFFSET(-1, -1);
}

static int draggable_reservoir_ghost_blocked()
{
    if (city_finance_out_of_money()) {
        return 1;
    }
    return building_construction_in_progress() &&
        (!building_construction_can_place() || !building_construction_cost());
}

static void draw_draggable_reservoir(const map_tile *tile, int x, int y, building_type type)
{
    (void) type;
    const building_type_registry_impl::BuildingType *reservoir_definition = reservoir_ghost_definition();
    if (!reservoir_definition) {
        return;
    }

    const int blocked = draggable_reservoir_ghost_blocked();
    data.reservoir_range.blocked = blocked;

    map_tile end_origin_tile = reservoir_origin_tile_from_center(tile);
    int end_origin_x = 0;
    int end_origin_y = 0;
    reservoir_origin_view_from_center(x, y, &end_origin_x, &end_origin_y);

    int draw_later = 0;
    map_tile start_origin_tile = {};
    int start_origin_x = 0;
    int start_origin_y = 0;
    const int drawing_two_reservoirs =
        building_construction_in_progress() &&
        building_construction_get_start_grid_offset() != tile->grid_offset;
    grid_slice aqueduct_route = {};
    building_type aqueduct_type = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *aqueduct_definition = nullptr;
    if (drawing_two_reservoirs &&
        building_construction_get_reservoir_aqueduct_preview_route(&aqueduct_route, &aqueduct_type)) {
        aqueduct_definition = building_type_registry_impl::definition_for_type(aqueduct_type);
    }
    if (drawing_two_reservoirs) {
        const int start_center_grid_offset = building_construction_get_start_grid_offset();
        map_tile start_center_tile = {};
        start_center_tile.x = map_grid_offset_to_x(start_center_grid_offset);
        start_center_tile.y = map_grid_offset_to_y(start_center_grid_offset);
        start_center_tile.grid_offset = start_center_grid_offset;
        start_origin_tile = reservoir_origin_tile_from_center(&start_center_tile);
        building_construction_get_view_position(&start_origin_x, &start_origin_y);
        reservoir_origin_view_from_center(start_origin_x, start_origin_y, &start_origin_x, &start_origin_y);

        switch (city_view_orientation()) {
            case DIR_0_TOP:
                draw_later = start_origin_tile.x > end_origin_tile.x || start_origin_tile.y > end_origin_tile.y;
                break;
            case DIR_2_RIGHT:
                draw_later = start_origin_tile.x < end_origin_tile.x || start_origin_tile.y > end_origin_tile.y;
                break;
            case DIR_4_BOTTOM:
                draw_later = start_origin_tile.x < end_origin_tile.x || start_origin_tile.y < end_origin_tile.y;
                break;
            case DIR_6_LEFT:
                draw_later = start_origin_tile.x > end_origin_tile.x || start_origin_tile.y < end_origin_tile.y;
                break;
        }
        if (!draw_later) {
            draw_default(&start_origin_tile, start_origin_x, start_origin_y, *reservoir_definition, 1, 0, blocked);
        }
    }

    if (aqueduct_definition) {
        draw_aqueduct_preview_route(
            tile,
            x,
            y,
            *aqueduct_definition,
            aqueduct_route,
            blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST);
    }
    draw_default(&end_origin_tile, end_origin_x, end_origin_y, *reservoir_definition, 1, !drawing_two_reservoirs, blocked);
    if (draw_later) {
        draw_default(&start_origin_tile, start_origin_x, start_origin_y, *reservoir_definition, 1, 0, blocked);
    }
}

static void draw_aqueduct(
    const map_tile *tile,
    int x,
    int y,
    const building_type_registry_impl::BuildingType &definition)
{
    int blocked = city_finance_out_of_money();
    grid_slice route = {};
    if (!blocked) {
        if (building_construction_in_progress()) {
            const int start_grid_offset = building_construction_get_start_grid_offset();
            int cost = 0;
            if (!building_construction_preview_aqueduct_route(
                    definition.type(),
                    map_grid_offset_to_x(start_grid_offset),
                    map_grid_offset_to_y(start_grid_offset),
                    tile->x,
                    tile->y,
                    &route,
                    &cost)) {
                blocked = 1;
            } else {
                blocked = !building_construction_can_place() || !cost;
            }
        } else {
            route.grid_offsets[route.size++] = tile->grid_offset;
            blocked = !building_construction_can_place_aqueduct_endpoint(tile->grid_offset);
        }
    }
    if (route.size == 0) {
        route.grid_offsets[route.size++] = tile->grid_offset;
    }

    draw_water_access_context_overlays(tile, definition);
    draw_aqueduct_preview_route(
        tile,
        x,
        y,
        definition,
        route,
        blocked ? COLOR_MASK_BUILDING_GHOST_RED : COLOR_MASK_BUILDING_GHOST);
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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    int blocked = 0;
    int is_ship_bridge = definition && definition->roadblock().is_ship_bridge();
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
    if (definition && length > 0) {
        std::vector<building> records;
        std::vector<BuildingGraphicsState> graphics_states;
        std::vector<building_runtime_impl::EphemeralBuildingRuntimeBinding> bindings;
        records.reserve(length);
        graphics_states.reserve(length);
        bindings.reserve(length);

        int current = tile->grid_offset;
        int delta = 0;
        switch (direction) {
            case DIR_0_TOP: delta = map_grid_delta(0, -1); break;
            case DIR_2_RIGHT: delta = map_grid_delta(1, 0); break;
            case DIR_4_BOTTOM: delta = map_grid_delta(0, 1); break;
            case DIR_6_LEFT: delta = map_grid_delta(-1, 0); break;
        }

        for (int i = 0; i < length && delta; i++, current += delta) {
            building record = {};
            record.id = static_cast<unsigned int>(i + 1);
            record.type = type;
            record.grid_offset = static_cast<short>(current);
            record.x = static_cast<unsigned char>(map_grid_offset_to_x(current));
            record.y = static_cast<unsigned char>(map_grid_offset_to_y(current));
            record.state = BUILDING_STATE_IN_USE;
            record.size = 1;
            record.subtype.orientation = static_cast<short>(direction);
            record.prev_part_building_id = i == 0 ? 0 : static_cast<short>(i);
            record.next_part_building_id = i + 1 < length ? static_cast<short>(i + 2) : 0;
            records.push_back(record);

            BuildingGraphicsState state;
            state.set_variant(map_bridge_graphics_variant_for_piece(i, length, dir, is_ship_bridge));
            graphics_states.push_back(state);
        }

        for (size_t i = 0; i < records.size(); i++) {
            building_runtime_impl::EphemeralBuildingRuntimeBinding binding;
            binding.runtime_id = records[i].id;
            binding.main_runtime_id = records.empty() ? 0 : records.front().id;
            binding.record = &records[i];
            binding.definition = definition;
            binding.graphics_state = graphics_states[i];
            bindings.push_back(binding);
        }

        building_runtime_impl::ScopedEphemeralBuildingRuntime bridge_runtime(bindings);
        auto draw_piece = [&](int index) {
            building_runtime *runtime = building_runtime_impl::get_ephemeral_instance(&records[index]);
            if (!runtime) {
                return;
            }
            runtime->building.draw_footprint(
                { x + x_delta * index, y + y_delta * index, records[index].grid_offset, color_mask, data.scale, 1 });
            runtime->building.draw_top(
                { x + x_delta * index, y + y_delta * index, records[index].grid_offset, color_mask, data.scale, 1 });
        };
        if (dir == DIR_0_TOP || dir == DIR_6_LEFT) {
            for (int i = static_cast<int>(records.size()) - 1; i >= 0; i--) {
                draw_piece(i);
            }
        } else {
            for (int i = 0; i < static_cast<int>(records.size()); i++) {
                draw_piece(i);
            }
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
    } else if (map_terrain_is(grid_offset, TERRAIN_BUILDING) && figure_type_registry_impl::PathingMode::gateIsTransformable(grid_offset)) {
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

static void draw_grid_tile(int x, int y, int grid_offset)
{
    static int image_id = 0;
    if (!image_id) {
        image_id = assets_get_image_id("UI\\Grid_Full", "Grid_Full");
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(data.ghost_building.type);
    const int water_allowed = definition &&
        (definition->roadblock().is_bridge() ||
            (definition->has_foundation() && definition->foundation().has_water_requirement()));
    if (map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_terrain_is(grid_offset, TERRAIN_ROCK) ||
        map_terrain_is(grid_offset, TERRAIN_ACCESS_RAMP) || map_terrain_is(grid_offset, TERRAIN_ELEVATION) ||
        (map_terrain_is(grid_offset, TERRAIN_WATER) && !water_allowed)) {
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
    const int cursor_x = map_grid_offset_to_x(grid_offset);
    const int cursor_y = map_grid_offset_to_y(grid_offset);
    if (definition.tool().is_draggable_reservoir()) {
        const building_type_registry_impl::BuildingType *reservoir_definition = reservoir_ghost_definition();
        if (!reservoir_definition) {
            return;
        }
        map_tile center_tile = { cursor_x, cursor_y, grid_offset };
        const map_tile origin_tile = reservoir_origin_tile_from_center(&center_tile);
        int origin_x = 0;
        int origin_y = 0;
        reservoir_origin_view_from_center(x, y, &origin_x, &origin_y);
        building_construction::ConstructionPlacementPlan plan(*reservoir_definition, origin_tile.x, origin_tile.y, 1, 0);
        int orientation_index = city_view_orientation() / 2;
        for (const building_construction::ConstructionPlacementPart &part : plan.parts()) {
            const int tile_x = origin_x + X_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
            const int tile_y = origin_y + Y_VIEW_OFFSET(part.x - plan.cursor_x(), part.y - plan.cursor_y());
            draw_grid_around_building(part.grid_offset, part.size, orientation_index, tile_x, tile_y);
        }
        return;
    }

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
        const int is_draggable_reservoir = definition->tool().is_draggable_reservoir();
        const building_type_registry_impl::BuildingType *range_definition =
            is_draggable_reservoir ? reservoir_ghost_definition() : definition;
        int building_size = range_definition && is_draggable_reservoir ? range_definition->declared_model_size() :
            (definition->is_warehouse() ? 3 : props->size);

        if (is_draggable_reservoir) {
            if (!range_definition) {
                return;
            }
            const map_tile origin_tile = reservoir_origin_tile_from_center(tile);
            draw_desirability_range(&origin_tile, *range_definition, building_size);
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

    if (definition->roadblock().is_bridge()) {
        draw_bridge(tile, x, y, type);
        return;
    }

    if (definition->tool().is_draggable_reservoir()) {
        draw_draggable_reservoir(tile, x, y, type);
    } else if (definition->tool().is_aqueduct()) {
        draw_aqueduct(tile, x, y, *definition);
    } else if (definition->tool().is_road()) {
        draw_road(tile, x, y);
    } else {
        draw_default(tile, x, y, *definition);
    }
}
