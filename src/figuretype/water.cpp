#include "building/building.h"
#include "map/water.h"

#include "water.h"

#include "city/god.h"

#include "building/building_record.h"
#include "core/image.h"
#include "core/random.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figuretype/fishing_boat.h"
#include "map/figure.h"
#include "map/grid.h"
#include "scenario/map.h"

static const int FLOTSAM_RESOURCE_IDS[] = {
    3, 1, 3, 2, 1, 3, 2, 3, 2, 1, 3, 3, 2, 3, 3, 3, 1, 2, 0, 1
};
static const int FLOTSAM_WAIT_TICKS[] = {
    10, 50, 100, 130, 200, 250, 400, 430, 500, 600, 70, 750, 820, 830, 900, 980, 1010, 1030, 1200, 1300
};

static const int FLOTSAM_TYPE_0[] = {0, 1, 2, 3, 4, 4, 4, 3, 2, 1, 0, 0};
static const int FLOTSAM_TYPE_12[] = {
    0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 3, 2, 1, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0
};
static const int FLOTSAM_TYPE_3[] = {
    0, 0, 1, 1, 2, 2, 3, 3, 4, 4, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1
};

void figure_create_flotsam(void)
{
    if (!scenario_map_has_river_entry() || !scenario_map_has_river_exit() || !scenario_map_has_flotsam()) {
        return;
    }
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state && f->type == FIGURE_FLOTSAM) {
            f->remove();
        }
    }

    map_point river_entry = scenario_map_river_entry();
    for (int i = 0; i < 20; i++) {
        Figure *f = Figure::create(FIGURE_FLOTSAM, river_entry.x, river_entry.y, DIR_0_TOP);
        f->action_state = FIGURE_ACTION_128_FLOTSAM_CREATED;
        f->resource_id = FLOTSAM_RESOURCE_IDS[i];
        f->wait_ticks = FLOTSAM_WAIT_TICKS[i];
    }
}

void figure_flotsam_action(Figure *f)
{
    f->is_boat = 2;
    if (!scenario_map_has_river_exit()) {
        return;
    }
    f->is_ghost = 0;
    f->cart_image_id = 0;
    f->terrain_usage = TERRAIN_USAGE_ANY;
    switch (f->action_state) {
        case FIGURE_ACTION_128_FLOTSAM_CREATED:
            f->is_ghost = 1;
            f->wait_ticks--;
            if (f->wait_ticks <= 0) {
                f->action_state = FIGURE_ACTION_129_FLOTSAM_FLOATING;
                f->wait_ticks = 0;
                if (!f->resource_id && city_god_neptune_create_shipwreck_flotsam()) {
                    f->min_max_seen = 1;
                }
                map_point river_exit = scenario_map_river_exit();
                f->destination_x = river_exit.x;
                f->destination_y = river_exit.y;
            }
            break;
        case FIGURE_ACTION_129_FLOTSAM_FLOATING:
            if (f->flotsam_visible) {
                f->flotsam_visible = 0;
            } else {
                f->flotsam_visible = 1;
                f->wait_ticks++;
                figure_movement_move_ticks(f, 1);
                f->is_ghost = 0;
                f->height_adjusted_ticks = 0;
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->action_state = FIGURE_ACTION_130_FLOTSAM_OFF_MAP;
                }
            }
            break;
        case FIGURE_ACTION_130_FLOTSAM_OFF_MAP:
            f->is_ghost = 1;
            f->min_max_seen = 0;
            f->action_state = FIGURE_ACTION_128_FLOTSAM_CREATED;
            if (f->wait_ticks >= 400) {
                f->wait_ticks = random_byte() & 7;
            } else if (f->wait_ticks >= 200) {
                f->wait_ticks = 50 + (random_byte() & 0xf);
            } else if (f->wait_ticks >= 100) {
                f->wait_ticks = 100 + (random_byte() & 0x1f);
            } else if (f->wait_ticks >= 50) {
                f->wait_ticks = 200 + (random_byte() & 0x3f);
            } else {
                f->wait_ticks = 300 + random_byte();
            }
            map_figure_delete(f);
            map_point river_entry = scenario_map_river_entry();
            f->x = river_entry.x;
            f->y = river_entry.y;
            f->grid_offset = map_grid_offset(f->x, f->y);
            f->cross_country_x = 15 * f->x;
            f->cross_country_y = 15 * f->y;
            break;
    }
    if (f->resource_id == 0) {
        figure_image_increase_offset(f, 12);
        if (f->min_max_seen) {
            f->image_id = image_group(GROUP_FIGURE_FLOTSAM_SHEEP) + FLOTSAM_TYPE_0[f->image_offset];
        } else {
            f->image_id = image_group(GROUP_FIGURE_FLOTSAM_0) + FLOTSAM_TYPE_0[f->image_offset];
        }
    } else if (f->resource_id == 1) {
        figure_image_increase_offset(f, 24);
        f->image_id = image_group(GROUP_FIGURE_FLOTSAM_1) + FLOTSAM_TYPE_12[f->image_offset];
    } else if (f->resource_id == 2) {
        figure_image_increase_offset(f, 24);
        f->image_id = image_group(GROUP_FIGURE_FLOTSAM_2) + FLOTSAM_TYPE_12[f->image_offset];
    } else if (f->resource_id == 3) {
        figure_image_increase_offset(f, 24);
        if (FLOTSAM_TYPE_3[f->image_offset] == -1) {
            f->image_id = 0;
        } else {
            f->image_id = image_group(GROUP_FIGURE_FLOTSAM_3) + FLOTSAM_TYPE_3[f->image_offset];
        }
    }
}

void figure_shipwreck_action(Figure *f)
{
    f->is_ghost = 0;
    f->height_adjusted_ticks = 0;
    f->is_boat = 1;
    figure_image_increase_offset(f, 128);
    if (f->wait_ticks < 1000) {
        map_figure_delete(f);
        map_point tile;
        if (map_water_find_shipwreck_tile(f, &tile)) {
            f->x = tile.x;
            f->y = tile.y;
            f->grid_offset = map_grid_offset(f->x, f->y);
            f->cross_country_x = 15 * f->x + 7;
            f->cross_country_y = 15 * f->y + 7;
        }
        map_figure_add(f);
        f->wait_ticks = 1000;
    }
    f->wait_ticks++;
    if (f->wait_ticks > 2000) {
        f->state = FIGURE_STATE_DEAD;
    }
    f->image_id = image_group(GROUP_FIGURE_SHIPWRECK) + f->image_offset / 16;
}

void figure_sink_all_ships(void)
{
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (f->type == FIGURE_TRADE_SHIP) {
            building_get(f->destination_building.id())->data.dock.trade_ship_id = 0;
        } else if (f->type == FIGURE_FISHING_BOAT) {
            FishingBoat::from(*f).sink();
            continue;
        } else {
            continue;
        }
        f->building = Building(nullptr);
        f->type = FIGURE_SHIPWRECK;
        f->wait_ticks = 0;
    }
}

void figure_sink_half_ships(void)
{
    int fishing_to_destroy = 0;
    int trade_to_destroy = 0;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (f->type == FIGURE_TRADE_SHIP) {
            trade_to_destroy++;
        } else if (f->type == FIGURE_FISHING_BOAT) {
            fishing_to_destroy++;
        }
    }
    int fishing_destroyed = 0;
    int trade_destroyed = 0;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (f->type == FIGURE_TRADE_SHIP && (trade_destroyed < (int)trade_to_destroy / 2 )) {
            building_get(f->destination_building.id())->data.dock.trade_ship_id = 0;
            trade_destroyed++;
        } else if (f->type == FIGURE_FISHING_BOAT && (fishing_destroyed < (int)fishing_to_destroy / 2 )) {
            FishingBoat::from(*f).sink();
            fishing_destroyed++;
            continue;
        } else {
            continue;
        }
        f->building = Building(nullptr);
        f->type = FIGURE_SHIPWRECK;
        f->wait_ticks = 0;
    }
}
