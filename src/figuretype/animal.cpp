#include "building/building_record.h"
#include "animal.h"

#include "building/building.h"
#include "city/view.h"
#include "city/race_bet.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/log.h"
#include "core/random.h"
#include "figure/formation.h"
#include "figure/image.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "graphics/lang_text.h"
#include "graphics/GraphicsDefinition.h"
#include "graphics/screen.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/point.h"
#include "map/random.h"
#include "map/terrain.h"
#include "scenario/map.h"
#include "scenario/property.h"
#include "window/building/common.h"

#include <climits>
#include <exception>
#include <string>

void map_figure_add(Figure *f);
void map_figure_delete(Figure *f);

static const map_point SEAGULL_OFFSETS[] = {
    {0, 0}, {0, -2}, {-2, 0}, {1, 2}, {2, 0}, {-3, 1}, {4, -3}, {-2, 4}, {0, 0}
};

struct HippodromeGraphicsOffsetSchedule {
    int orientation;
    int max_wait_ticks[7];
    int x[7];
    int y[7];
};

static constexpr HippodromeGraphicsOffsetSchedule HIPPODROME_GRAPHICS_OFFSETS[] = {
    { DIR_0_TOP, { 10, 11, 12, 13, 20, 21, INT_MAX }, { 10, 10, 10, 10, 10, 10, 10 }, { -2, -10, -18, -16, -14, -10, -2 } },
    { DIR_2_RIGHT, { 9, 10, 11, 13, 20, 21, INT_MAX }, { -10, -10, -15, -15, -10, -10, -10 }, { -12, 4, 2, 0, -2, -6, -12 } },
    { DIR_4_BOTTOM, { 9, 10, 11, 13, 20, 21, INT_MAX }, { 20, 30, 30, 20, 20, 20, 20 }, { 4, 4, -4, -6, -12, -10, -2 } },
    { DIR_6_LEFT, { 9, 10, 11, 13, 20, 21, INT_MAX }, { -10, -10, -10, -10, -10, -10, -10 }, { -12, 4, 2, 0, -2, -6, -12 } },
};

void figuretype::Animal::draw(building_info_context *c)
{
    draw_big_people_image(c->x_offset + 28, c->y_offset + 112);
    lang_text_draw(current_string_key(64, type), c->x_offset + 92, c->y_offset + 139,
        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
}

enum {
    HORSE_CREATED = 0,
    HORSE_RACING = 1,
    HORSE_FINISHED = 2
};

static void create_fishing_point(int x, int y)
{
    random_generate_next();
    Figure *fish = Figure::create(FIGURE_FISH_GULLS, x, y, DIR_0_TOP);
    fish->image_offset = random_byte() & 0x1f;
    fish->progress_on_tile = random_byte() & 7;
    figure_movement_set_cross_country_direction(fish,
        fish->cross_country_x, fish->cross_country_y,
        figure_movement_tile_to_cross_country(fish->destination_x),
        figure_movement_tile_to_cross_country(fish->destination_y), 0);
}

void figure_create_fishing_points(void)
{
    scenario_map_foreach_fishing_point(create_fishing_point);
}

static void create_herd(int x, int y)
{
    const FormationType *definition =
        formation_type_registry_impl::find_herd_spawn_for_location(scenario_property_climate(), x, y);
    if (!definition) {
        return;
    }
    const int formation_id = formation_create_herd(*definition, x, y);
    if (formation_id > 0) {
        for (int fig = 0; fig < definition->spawn.initial_count; fig++) {
            random_generate_next();
            Figure *f = Figure::create(definition->primary_unit()->figure_type_id(), x, y, DIR_0_TOP);
            f->action_state = FIGURE_ACTION_196_HERD_ANIMAL_AT_REST;
            f->formation_id = formation_id;
            f->wait_ticks = f->id() & 0x1f;
        }
    }
}

void figure_create_herds(void)
{
    scenario_map_foreach_herd_point(create_herd);
}

void figure_seagulls_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->is_ghost = 0;
    f->use_cross_country = 1;
    if (!(f->image_offset & 3) && figure_movement_move_ticks_cross_country(f, 1)) {
        f->progress_on_tile++;
        if (f->progress_on_tile > 8) {
            f->progress_on_tile = 0;
        }
        figure_movement_set_cross_country_destination(f,
            f->source_x + SEAGULL_OFFSETS[f->progress_on_tile].x,
            f->source_y + SEAGULL_OFFSETS[f->progress_on_tile].y);
    }
    if (f->id() & 1) {
        figure_image_increase_offset(f, 54);
    } else {
        figure_image_increase_offset(f, 72);
    }
    figure_seagulls_update_graphics(f);
}

void figure_seagulls_update_graphics(Figure *f)
{
    figure_runtime_graphics_select_default_entry_frame(f, f->id() & 1 ? "variant_a" : "variant_b", f->image_offset / 3 + 1);
}

void figuretype::Animal::action()
{
    formation *owner = formation_get(formation_id);
    if (!owner) {
        log_error("Herd member references an unknown formation", 0, static_cast<int>(formation_id));
        std::terminate();
    }
    owner->update_herd_member(*this);
}

void figuretype::Animal::update_graphics()
{
    formation *owner = formation_get(formation_id);
    if (!owner) {
        log_error("Herd member graphics reference an unknown formation", 0, static_cast<int>(formation_id));
        std::terminate();
    }
    owner->update_herd_member_graphics(*this);
}

static int terrain_blocked_for_animals(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER |
        TERRAIN_BUILDING | TERRAIN_SHRUB );
}

void figure_animal_try_nudge_at(int building_center_tile_grid_offset, int animal_tile_offset, int building_size)
{
    int figure_id = map_figure_at(animal_tile_offset);
    Figure *f = Figure::get(figure_id);
    if (f->is_herd() && !f->is_aggressive_herd()
        && f->action_state == FIGURE_ACTION_196_HERD_ANIMAL_AT_REST) {
        const int num_tiles = building_size * 4;
        const int *tile_deltas = map_grid_adjacent_offsets(building_size);
        const int random_value = map_random_get(animal_tile_offset);
        for (int i = 0; i < num_tiles; i++) {
            int current_tile_delta = tile_deltas[(random_value + i) % num_tiles];
            int target_grid_offset = building_center_tile_grid_offset + current_tile_delta;
            if (terrain_blocked_for_animals(target_grid_offset)) {
                continue;
            }
            f->action_state = FIGURE_ACTION_197_HERD_ANIMAL_MOVING;
            f->destination_x = static_cast<unsigned char>(map_grid_offset_to_x(target_grid_offset));
            f->destination_y = static_cast<unsigned char>(map_grid_offset_to_y(target_grid_offset));
            f->roam_length = 0;
            break;
        }
    }
}

static const building_type_registry_impl::RaceDefinition *figure_race_definition(Figure *f)
{
    const Building *owner = f && f->building ? f->building : nullptr;
    return owner && owner->type && owner->type->has_race() ? &owner->type->race() : nullptr;
}

static building_type_registry_impl::RaceRoutePoint race_route_point(const building_type_registry_impl::RaceDefinition &race, int index, int rotation)
{
    const building_type_registry_impl::RaceRoutePoint &source = race.route[index % race.route.size()];
    return rotation == 0 ? source : building_type_registry_impl::RaceRoutePoint{ source.y, source.x };
}

static int race_route_start_index(const building_type_registry_impl::RaceDefinition &race, int rotation, int view_orientation)
{
    const int shifted = rotation == 0 ?
        (view_orientation != DIR_0_TOP && view_orientation != DIR_6_LEFT) :
        (view_orientation != DIR_0_TOP && view_orientation != DIR_2_RIGHT);
    return shifted ? static_cast<int>(race.route.size() / 2) : 0;
}

static void set_horse_destination(Figure *f, int state)
{
    building *b = f && f->building ? const_cast<building *>(f->building->record()) : nullptr;
    const building_type_registry_impl::RaceDefinition *race = figure_race_definition(f);
    if (!b || !race || race->route.size() < 2 || f->resource_id >= race->teams.size()) return;
    const int orientation = city_view_orientation();
    const int rotation = b->subtype.orientation;
    const int lane = race->teams[f->resource_id].lane;
    const int start_index = race_route_start_index(*race, rotation, orientation);
    if (state == HORSE_CREATED) {
        map_figure_delete(f);
        const building_type_registry_impl::RaceRoutePoint point = race_route_point(*race, start_index, rotation);
        f->destination_x = static_cast<unsigned char>(b->x + point.x + (rotation != 0 && (lane & 1)));
        f->destination_y = static_cast<unsigned char>(b->y + point.y + (rotation == 0 && (lane & 1)));
        f->x = f->destination_x;
        f->y = f->destination_y;
        f->cross_country_x = figure_movement_tile_to_cross_country(f->x);
        f->cross_country_y = figure_movement_tile_to_cross_country(f->y);
        f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
        map_figure_add(f);
    } else if (state == HORSE_RACING) {
        const building_type_registry_impl::RaceRoutePoint point = race_route_point(*race, start_index + f->wait_ticks_missile, rotation);
        f->destination_x = static_cast<unsigned char>(b->x + point.x);
        f->destination_y = static_cast<unsigned char>(b->y + point.y);
    } else if (state == HORSE_FINISHED) {
        const building_type_registry_impl::RaceRoutePoint first = race_route_point(*race, start_index, rotation);
        const building_type_registry_impl::RaceRoutePoint second = race_route_point(*race, start_index + 1, rotation);
        const int dx = calc_bound(second.x - first.x, -1, 1);
        const int dy = calc_bound(second.y - first.y, -1, 1);
        f->destination_x = static_cast<unsigned char>(b->x + first.x - dx - dy * (lane & 1));
        f->destination_y = static_cast<unsigned char>(b->y + first.y - dy + dx * (lane & 1));
    }
}

void figure_hippodrome_horse_action(Figure *f)
{
    f->use_cross_country = 1;
    f->is_ghost = 0;
    figure_image_increase_offset(f, 8);
    building *owner = f->building ? const_cast<building *>(f->building->record()) : nullptr;
    if (!owner || !owner->state) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    const building_type_registry_impl::RaceDefinition *race = figure_race_definition(f);
    if (!race || f->resource_id >= race->teams.size()) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    race_bet_register_participant(static_cast<unsigned int>(owner->id), f->resource_id);
    switch (f->action_state) {
        case FIGURE_ACTION_200_HIPPODROME_HORSE_CREATED:
            f->image_offset = 0;
            f->wait_ticks_missile = 0;
            set_horse_destination(f, HORSE_CREATED);
            f->wait_ticks++;
            if (f->wait_ticks > race->ready_ticks) {
                f->action_state = FIGURE_ACTION_201_HIPPODROME_HORSE_RACING;
                f->wait_ticks = 0;
            }
            break;
        case FIGURE_ACTION_201_HIPPODROME_HORSE_RACING:
            f->direction = static_cast<signed char>(
                calc_general_direction(f->x, f->y, f->destination_x, f->destination_y));
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks_missile++;
                if (f->wait_ticks_missile >= race->route.size()) {
                    f->wait_ticks_missile = 0;
                    f->leading_figure_id++;
                    if (f->leading_figure_id >= race->laps) {
                        f->wait_ticks = 0;
                        f->action_state = FIGURE_ACTION_202_HIPPODROME_HORSE_DONE;
                    }
                }
                f->speed_multiplier = static_cast<unsigned char>(race->minimum_speed +
                    (((f->id() + random_byte()) % race->speed_roll) == 0 ?
                        race->maximum_speed - race->minimum_speed : 0));
                set_horse_destination(f, HORSE_RACING);
                f->direction = static_cast<signed char>(
                    calc_general_direction(f->x, f->y, f->destination_x, f->destination_y));
                figure_movement_set_cross_country_direction(f,
                    f->cross_country_x, f->cross_country_y,
                    figure_movement_tile_to_cross_country(f->destination_x),
                    figure_movement_tile_to_cross_country(f->destination_y),
                    0);
            }
            if (f->action_state != FIGURE_ACTION_202_HIPPODROME_HORSE_DONE) {
                figure_movement_move_ticks_cross_country(f, f->speed_multiplier);
            }
            break;
        case FIGURE_ACTION_202_HIPPODROME_HORSE_DONE:
            if (!f->wait_ticks) {
                set_horse_destination(f, HORSE_FINISHED);
                race_bet_register_finish(static_cast<unsigned int>(owner->id), f->resource_id);
                f->direction = static_cast<signed char>(
                    calc_general_direction(f->x, f->y, f->destination_x, f->destination_y));
                figure_movement_set_cross_country_direction(f,
                    f->cross_country_x, f->cross_country_y,
                    figure_movement_tile_to_cross_country(f->destination_x),
                    figure_movement_tile_to_cross_country(f->destination_y),
                    0);
            }
            if (f->direction != DIR_FIGURE_AT_DESTINATION) {
                figure_movement_move_ticks_cross_country(f, 1);
            }
            f->wait_ticks++;
            if (f->wait_ticks > 30) {
                f->image_offset = 0;
            }
            if (f->wait_ticks > 150) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }

    figure_hippodrome_horse_update_graphics(f);
}

static map_point hippodrome_world_offset(int orientation, int wait_ticks)
{
    for (const HippodromeGraphicsOffsetSchedule &schedule : HIPPODROME_GRAPHICS_OFFSETS) {
        if (schedule.orientation != orientation) continue;
        for (int index = 0; index < 7; ++index) {
            if (wait_ticks <= schedule.max_wait_ticks[index]) return { schedule.x[index], schedule.y[index] };
        }
    }
    return {};
}

static map_point race_team_lane_offset(int direction, int distance)
{
    static constexpr int PERPENDICULAR_X[] = { 1, 1, 0, -1, -1, -1, 0, 1 };
    static constexpr int PERPENDICULAR_Y[] = { 0, 1, 1, 1, 0, -1, -1, -1 };
    const int normalized_direction = ((direction % 8) + 8) % 8;
    return { PERPENDICULAR_X[normalized_direction] * distance, PERPENDICULAR_Y[normalized_direction] * distance };
}

void figure_hippodrome_horse_update_graphics(Figure *f)
{
    const int orientation = city_view_orientation();
    const int horse_direction = figure_image_direction(f);
    const int cart_direction = horse_direction;
    const int cart_offset_index = (cart_direction + 4) % 8;
    const building_type_registry_impl::RaceDefinition *race = figure_race_definition(f);
    if (!race || f->resource_id >= race->teams.size()) return;
    const building_type_registry_impl::RaceTeamDefinition &team = race->teams[f->resource_id];
    map_point world_offset = hippodrome_world_offset(orientation, f->wait_ticks_missile);
    const map_point lane_offset = race_team_lane_offset(cart_direction, race->lane_render_distance(team.lane));
    world_offset.x += lane_offset.x;
    world_offset.y += lane_offset.y;
    std::string horse_entry = team.body_entry;
    horse_entry += '_';
    horse_entry += graphics_direction8_suffix(horse_direction);
    figure_runtime_graphics_begin_update(f);
    figure_runtime_graphics_select_default_entry_frame(f, horse_entry.c_str(), f->image_offset + 1);
    figure_runtime_graphics_set_default_offset(f, world_offset.x, world_offset.y);
    if (!team.vehicle_entry.empty()) {
        std::string cart_entry = team.vehicle_entry;
        cart_entry += '_';
        cart_entry += graphics_direction8_suffix(cart_direction);
        figure_runtime_graphics_add_required_layer(f, "cart", cart_entry.c_str(), 0,
            team.vehicle_offsets[cart_offset_index].x, team.vehicle_offsets[cart_offset_index].y,
            team.vehicle_behind[cart_offset_index]);
    }
}

void figure_hippodrome_horse_reroute(void)
{
    if (!race_bet_any_active()) {
        return;
    }
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->state == FIGURE_STATE_ALIVE && figure_race_definition(f)) {
            f->wait_ticks_missile = 0;
            set_horse_destination(f, HORSE_CREATED);
        }
    }
}
