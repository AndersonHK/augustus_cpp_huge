#include "roamer_preview.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "building/properties.h"
#include "building/rotation.h"
#include "core/config.h"
#include "figure/figure.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/movement.h"
#include "figure/route.h"
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

static int determine_road_access(int x, int y, int size, building_type type, map_point *road)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (definition && definition->is_warehouse()) {
        return map_has_road_access_warehouse(x, y, road);
    }
    if (definition && definition->is_hippodrome()) {
        int building_orientation = building_rotation_get_building_orientation(building_rotation_get_rotation());
        return map_has_road_access_hippodrome_rotation(x, y, road, building_orientation);
    }
    if (definition && definition->is_granary()) {
        return map_has_road_access_granary(x, y, road);
    }
    return map_has_road_access(x, y, size, road);
}

void figure_roamer_preview_create(building_type b_type, int x, int y)
{
    if (!config_get(CONFIG_UI_SHOW_ROAMING_PATH)) {
        figure_roamer_preview_reset_building_types();
        return;
    }

    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b_type);
    figure_type fig_type = definition->preview_figure_type();
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

    int b_size = definition->is_farm() ? 3 : building_properties_for_type(b_type)->size;

    map_point road;
    if (!determine_road_access(x, y, b_size, b_type, &road)) {
        return;
    }

    int figure_walks_into_building = figure_enters_exits_building(fig_type);

    int x_road, y_road;
    int has_closest_road = map_closest_road_within_radius(x, y, b_size, 2, &x_road, &y_road);

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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    figure_type fig_type = definition->preview_figure_type();
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
