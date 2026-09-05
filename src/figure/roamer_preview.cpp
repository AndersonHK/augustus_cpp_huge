#include "roamer_preview.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/CompositionDef.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "building/rotation.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "map/building.h"
#include "map/grid.h"
#include "map/road_access.h"

#define TOTAL_ROAMERS 4
#define MAX_STORED_BUILDING_TYPES 2
#define SHOWN_BUILDING_OFFSET 12

static struct {
    grid_u8 travelled_tiles;
    building_type types[MAX_STORED_BUILDING_TYPES];
    int stored_building_types;
} data;

static figure_type preview_figure_type_for(building_type type)
{
    if (type == BUILDING_NONE) {
        return FIGURE_NONE;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition->preview_figure_type();
}

static int roam_length_for_figure_type(figure_type type)
{
    return figure_type_registry_impl::default_profile_for(type)->movement_profile().max_roam_length;
}

static int figure_enters_exits_building(figure_type type)
{
    switch (type) {
        case FIGURE_TAX_COLLECTOR:
        case FIGURE_ENGINEER:
        case FIGURE_PREFECT:
            return 1;
        default:
            return 0;
    }
}

static void init_roaming(Figure *f, int roam_dir, int x, int y)
{
    f->progress_on_tile = FIGURE_TILE_PROGRESS_MAX;
    f->roam_choose_destination = 0;
    f->roam_ticks_until_next_turn = -1;
    f->roam_turn_direction = 2;
    f->roam_length = 0;

    if (config_get(CONFIG_GP_CH_ROAMERS_DONT_SKIP_CORNERS)) {
        f->disallow_diagonal = 1;
    }
    switch (roam_dir) {
        case DIR_0_TOP: y -= 8; break;
        case DIR_2_RIGHT: x += 8; break;
        case DIR_4_BOTTOM: y += 8; break;
        case DIR_6_LEFT: x -= 8; break;
    }
    map_grid_bound(&x, &y);
    int x_road, y_road;
    if (map_closest_road_within_radius(x, y, 1, 6, &x_road, &y_road)) {
        f->destination_x = static_cast<unsigned char>(x_road);
        f->destination_y = static_cast<unsigned char>(y_road);
    } else {
        f->roam_choose_destination = 1;
    }
}

struct PreviewFoundationBounds {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    int valid() const { return width > 0 && height > 0; }
};

static int preview_rotation(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int requested_rotation)
{
    if (requested_rotation >= 0) {
        return building_type_registry_impl::normalize_composition_rotation(requested_rotation);
    }
    for (Building &candidate : Building::of_type(definition.type())) {
        const building *record = candidate.record();
        const bool is_fixed_child = candidate.Composition && candidate.Composition->is_child();
        const bool is_dynamic_bridge_child = candidate.is_dynamic_bridge_segment();
        if (record && !is_fixed_child && !is_dynamic_bridge_child && record->x == x && record->y == y) {
            return building_type_registry_impl::normalize_composition_rotation(
                record->subtype.orientation);
        }
    }
    return building_type_registry_impl::normalize_composition_rotation(building_rotation_get_rotation());
}

static PreviewFoundationBounds preview_foundation_bounds(
    const building_type_registry_impl::BuildingType &definition,
    int x,
    int y,
    int rotation)
{
    if (definition.has_composition()) {
        const building_type_registry_impl::CompositionLayoutResult layout =
            building_type_registry_impl::build_composition_layout(
                &definition, definition.composition(), x, y, rotation);
        if (!layout.valid()) {
            return {};
        }
        return {
            layout.bounds.min_x,
            layout.bounds.min_y,
            layout.bounds.width(),
            layout.bounds.height()
        };
    }

    const building_type_registry_impl::FoundationDef *foundation = definition.foundation_def();
    if (!foundation) {
        return {};
    }
    const int foundation_rotation = foundation->rotates() ? rotation : 0;
    return {
        x,
        y,
        foundation->rotated_width(foundation_rotation),
        foundation->rotated_height(foundation_rotation)
    };
}

static int determine_road_access(
    int owner_x,
    int owner_y,
    const PreviewFoundationBounds &bounds,
    map_point *road)
{
    if (map_building_exists_at(map_grid_offset(owner_x, owner_y))) {
        return map_has_road_access_building(owner_x, owner_y, road);
    }
    return map_has_road_access_rectangle(bounds.x, bounds.y, bounds.width, bounds.height, road);
}

void figure_roamer_preview_create(building_type b_type, int x, int y, int requested_rotation)
{
    if (!config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
        figure_roamer_preview_reset_building_types();
        return;
    }
    if (b_type == BUILDING_NONE) {
        return;
    }

    const figure_type fig_type = preview_figure_type_for(b_type);
    if (fig_type == FIGURE_NONE) {
        return;
    }

    if (fig_type == FIGURE_LABOR_SEEKER && config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        return;
    }

    int grid_offset = map_grid_offset(x, y);

    if (data.travelled_tiles.items[grid_offset] == SHOWN_BUILDING_OFFSET) {
        return;
    }

    data.travelled_tiles.items[grid_offset] = SHOWN_BUILDING_OFFSET;

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b_type);
    if (!definition) {
        return;
    }
    const int rotation = preview_rotation(*definition, x, y, requested_rotation);
    const PreviewFoundationBounds bounds = preview_foundation_bounds(*definition, x, y, rotation);
    if (!bounds.valid()) {
        return;
    }

    map_point road;
    if (!determine_road_access(x, y, bounds, &road)) {
        return;
    }

    int figure_walks_into_building = figure_enters_exits_building(fig_type);

    int x_road, y_road;
    int has_closest_road = map_closest_road_within_radius_rectangle(
        bounds.x, bounds.y, bounds.width, bounds.height, 2, &x_road, &y_road);

    if (figure_walks_into_building && !has_closest_road) {
        return;
    }

    int roam_length = roam_length_for_figure_type(fig_type);

    int should_return = fig_type != FIGURE_SCHOOL_CHILD;

    for (int i = 0; i < TOTAL_ROAMERS; i++) {
        Figure roamer{};

        roamer.source_x = roamer.destination_x = roamer.previous_tile_x = static_cast<unsigned char>(road.x);
        roamer.source_y = roamer.destination_y = roamer.previous_tile_y = static_cast<unsigned char>(road.y);
        roamer.terrain_usage = static_cast<unsigned char>(TERRAIN_USAGE_ROADS);
        roamer.direction = static_cast<signed char>(DIR_0_TOP);
        roamer.faction_id = FIGURE_FACTION_ROAMER_PREVIEW;
        roamer.type = static_cast<unsigned char>(fig_type);
        roamer.max_roam_length = static_cast<short>(roam_length);

        if (figure_walks_into_building) {
            roamer.x = static_cast<unsigned char>(x_road);
            roamer.y = static_cast<unsigned char>(y_road);
        } else {
            roamer.x = static_cast<unsigned char>(road.x);
            roamer.y = static_cast<unsigned char>(road.y);
        }
        roamer.grid_offset = static_cast<short>(map_grid_offset(roamer.x, roamer.y));
        if (map_grid_is_valid_offset(roamer.grid_offset)) {
            data.travelled_tiles.items[roamer.grid_offset] = FIGURE_ROAMER_PREVIEW_EXIT_TILE;
        }
        init_roaming(&roamer, i * 2, roamer.x, roamer.y);
        while (++roamer.roam_length < roamer.max_roam_length) {
            if (roamer.progress_on_tile == 0 && data.travelled_tiles.items[roamer.grid_offset] < FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
                data.travelled_tiles.items[roamer.grid_offset]++;
            }
            figure_movement_roam_ticks(&roamer, 1);
        }
        Route::remove(&roamer);
        figure_movement_roam_ticks(&roamer, 1);
        if (!should_return || !has_closest_road) {
            Route::remove(&roamer);
            continue;
        }
        roamer.destination_x = static_cast<unsigned char>(x_road);
        roamer.destination_y = static_cast<unsigned char>(y_road);
        while (roamer.direction != DIR_FIGURE_AT_DESTINATION &&
            roamer.direction != DIR_FIGURE_REROUTE && roamer.direction != DIR_FIGURE_LOST) {
            if (data.travelled_tiles.items[roamer.grid_offset] < FIGURE_ROAMER_PREVIEW_MAX_PASSAGES) {
                data.travelled_tiles.items[roamer.grid_offset]++;
            }
            roamer.progress_on_tile = FIGURE_TILE_PROGRESS_MAX;
            figure_movement_move_ticks(&roamer, 1);
        }
        Route::remove(&roamer);
        if (roamer.direction == DIR_FIGURE_AT_DESTINATION) {
            int tile_type = data.travelled_tiles.items[roamer.grid_offset];
            data.travelled_tiles.items[roamer.grid_offset] = tile_type < FIGURE_ROAMER_PREVIEW_EXIT_TILE ?
                FIGURE_ROAMER_PREVIEW_ENTRY_TILE : FIGURE_ROAMER_PREVIEW_ENTRY_EXIT_TILE;
        }
    }
}

void figure_roamer_preview_create_all_for_building_type(building_type type)
{
    if (type == BUILDING_NONE) {
        return;
    }
    if (!config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
        figure_roamer_preview_reset_building_types();
        return;
    }
    for (int i = 0; i < data.stored_building_types; i++) {
        if (data.types[i] == type) {
            return;
        }
    }
    if (data.stored_building_types == MAX_STORED_BUILDING_TYPES) {
        return;
    }
    for (Building *building = Building::first_of_type(type); building; building = building->next_of_type()) {
        figure_roamer_preview_create(type, building->x(), building->y());
    }
    data.types[data.stored_building_types] = type;
    data.stored_building_types++;
}

void figure_roamer_preview_reset(building_type type)
{
    map_grid_clear_u8(data.travelled_tiles.items);
    int show_other_roamers = 0;
    figure_type fig_type = preview_figure_type_for(type);
    if (fig_type == FIGURE_LABOR_SEEKER && config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        fig_type = FIGURE_NONE;
    }
    if (fig_type == FIGURE_NONE) {
        show_other_roamers = 1;
    } else {
        for (int i = 0; i < data.stored_building_types; i++) {
            const building_type_registry_impl::BuildingType *stored_definition =
                building_type_registry_impl::definition_for_type(data.types[i]);
            if (stored_definition->preview_figure_type() == fig_type) {
                show_other_roamers = 1;
                break;
            }
        }
    }
    if (show_other_roamers) {
        for (int i = 0; i < data.stored_building_types; i++) {
            for (Building *building = Building::first_of_type(data.types[i]); building; building = building->next_of_type()) {
                figure_roamer_preview_create(data.types[i], building->x(), building->y());
            }
        }
    }
}

void figure_roamer_preview_reset_building_types(void)
{
    data.stored_building_types = 0;
    figure_roamer_preview_reset(BUILDING_NONE);
}

int figure_roamer_preview_get_frequency(int grid_offset)
{
    return map_grid_is_valid_offset(grid_offset) ? data.travelled_tiles.items[grid_offset] : 0;
}
