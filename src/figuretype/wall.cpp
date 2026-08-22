#include "building/building_record.h"
#include "wall.h"

#include "building/building.h"
#include "city/view.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/FigureGraphics.h"
#include "figure/enemy_army.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/unit_type.h"
#include "figuretype/missile.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_access.h"
#include "map/terrain.h"
#include "sound/effect.h"

static const UnitRangedAbility *ranged_ability_for(const Figure &figure)
{
    const UnitType *unit =
        unit_type_registry_impl::find_unit_type(static_cast<figure_type>(figure.type));
    return unit ? unit->ranged_ability() : nullptr;
}

void figure_ballista_action(Figure *f)
{
    Building *tower = f ? f->building : nullptr;
    building *b = tower ? const_cast<building *>(tower->record()) : nullptr;
    if (!b) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    f->terrain_usage = TERRAIN_USAGE_WALLS;
    f->use_cross_country = 0;
    f->is_ghost = 1;
    f->height_adjusted_ticks = 10;
    f->current_height = 45;

    if (b->state != BUILDING_STATE_IN_USE || b->figure_id4 != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    if (b->num_workers <= 0 || b->figure_id <= 0) {
        f->state = FIGURE_STATE_DEAD;
    }
    map_figure_delete(f);
    switch (city_view_orientation()) {
        case DIR_0_TOP: f->x = b->x; f->y = b->y; break;
        case DIR_2_RIGHT: f->x = b->x + 1; f->y = b->y; break;
        case DIR_4_BOTTOM: f->x = b->x + 1; f->y = b->y + 1; break;
        case DIR_6_LEFT: f->x = b->x; f->y = b->y + 1; break;
    }
    f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
    map_figure_add(f);

    switch (f->action_state) {
        case FIGURE_ACTION_149_CORPSE:
            f->state = FIGURE_STATE_DEAD;
            break;
        case FIGURE_ACTION_180_BALLISTA_CREATED:
            f->wait_ticks++;
            if (f->wait_ticks > 20) {
                f->wait_ticks = 0;
                map_point tile;
                const UnitRangedAbility *ranged = ranged_ability_for(*f);
                if (ranged &&
                    figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->action_state = FIGURE_ACTION_181_BALLISTA_FIRING;
                    f->wait_ticks_missile = static_cast<unsigned char>(ranged->cooldown);
                }
            }
            break;
        case FIGURE_ACTION_181_BALLISTA_FIRING:
            f->wait_ticks_missile++;
            if (const UnitRangedAbility *ranged = ranged_ability_for(*f);
                ranged && f->wait_ticks_missile > ranged->cooldown) {
                map_point tile;
                if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->direction =
                        static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
                    f->wait_ticks_missile = 0;
                    int figure_id = f->id();
                    figure_create_missile(
                        figure_id, f->x, f->y, tile.x, tile.y, ranged->projectile_type);
                    sound_effect_play(SOUND_EFFECT_BALLISTA_SHOOT);
                } else {
                    f->action_state = FIGURE_ACTION_180_BALLISTA_CREATED;
                }
            }
            break;
    }
}

static void tower_sentry_pick_target(Figure *f)
{
    if (enemy_army_total_enemy_formations() <= 0) {
        return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK ||
        f->action_state == FIGURE_ACTION_149_CORPSE ||
        f->action_state == FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER) {
        return;
    }
    if (f->in_building_wait_ticks) {
        return;
    }
    const UnitRangedAbility *ranged = ranged_ability_for(*f);
    if (!ranged) {
        return;
    }

    f->wait_ticks_next_target++;
    if (f->wait_ticks_next_target >= 40) {
        f->wait_ticks_next_target = 0;
        map_point tile;
        if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
            f->action_state = FIGURE_ACTION_172_TOWER_SENTRY_FIRING;
            f->destination_x = f->x;
            f->destination_y = f->y;
        }
    }
}

static int tower_sentry_init_patrol(Building &tower, int *x_tile, int *y_tile)
{
    building *b = const_cast<building *>(tower.record());
    if (!b) {
        return 0;
    }
    int dir = b->figure_roam_direction;
    int x = b->x;
    int y = b->y;
    switch (dir) {
        case DIR_0_TOP: y -= 8; break;
        case DIR_2_RIGHT: x += 8; break;
        case DIR_4_BOTTOM: y += 8; break;
        case DIR_6_LEFT: x -= 8; break;
    }
    map_grid_bound(&x, &y);

    if (Route::findWallTileInRadius(x, y, 6, x_tile, y_tile)) {
        b->figure_roam_direction += 2;
        if (b->figure_roam_direction > 6) {
            b->figure_roam_direction = 0;
        }
        return 1;
    }
    for (int i = 0; i < 4; i++) {
        dir = b->figure_roam_direction;
        b->figure_roam_direction += 2;
        if (b->figure_roam_direction > 6) {
            b->figure_roam_direction = 0;
        }
        x = b->x;
        y = b->y;
        switch (dir) {
            case DIR_0_TOP: y -= 3; break;
            case DIR_2_RIGHT: x += 3; break;
            case DIR_4_BOTTOM: y += 3; break;
            case DIR_6_LEFT: x -= 3; break;
        }
        map_grid_bound(&x, &y);
        if (Route::findWallTileInRadius(x, y, 6, x_tile, y_tile)) {
            return 1;
        }
    }
    return 0;
}

static void figure_watchtower_archer_spawn(Building &tower)
{
    building *b = const_cast<building *>(tower.record());
    if (!b || b->figure_id4 || !tower.type || !tower.type->is_watchtower()) {
        return;
    }
    Figure *f = Figure::create(FIGURE_WATCHTOWER_ARCHER, b->x, b->y, DIR_0_TOP);
    f->set_home_building(&tower);
    f->action_state = FIGURE_ACTION_223_ARCHER_GUARDING;
    b->figure_id4 = f->id();
}

void figure_tower_sentry_set_image(Figure *f)
{
    int dir = figure_image_direction(f);
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_group(GROUP_FIGURE_TOWER_SENTRY) + 136);
    } else if (f->action_state == FIGURE_ACTION_172_TOWER_SENTRY_FIRING) {
        f->select_legacy_directional_frame_image(
            image_group(GROUP_FIGURE_TOWER_SENTRY) + 96,
            dir,
            figure_type_registry_impl::FigureGraphics::missile_launcher_frame_for(*f));
    } else if (f->action_state == FIGURE_ACTION_225_WATCHMAN_SHOOTING) {
        dir = figure_image_normalize_direction(f->attack_direction);
        f->select_legacy_directional_frame_image(
            image_group(GROUP_FIGURE_TOWER_SENTRY) + 96,
            dir,
            figure_type_registry_impl::FigureGraphics::missile_launcher_frame_for(*f));
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        int image_id = image_group(GROUP_FIGURE_TOWER_SENTRY);
        const int frame_offset = f->attack_image_offset < 16 ? 0 : (f->attack_image_offset - 16) / 2;
        f->select_legacy_directional_frame_image(image_id + 96, dir, frame_offset);
    } else {
        f->select_legacy_directional_frame_image(
            image_group(GROUP_FIGURE_TOWER_SENTRY),
            dir,
            f->image_offset);
    }
}

void figure_tower_sentry_action(Figure *f)
{
    Building *tower = f ? f->building : nullptr;
    building *b = tower ? const_cast<building *>(tower->record()) : nullptr;
    if (!b) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    f->terrain_usage = TERRAIN_USAGE_WALLS;
    f->use_cross_country = 0;
    f->is_ghost = 1;
    if (f->action_state != FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER) {
        f->height_adjusted_ticks = 10;
    }
    f->max_roam_length = 800;
    if (b->state != BUILDING_STATE_IN_USE || b->figure_id != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);

    tower_sentry_pick_target(f);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_170_TOWER_SENTRY_AT_REST:

            if (!tower->matches("tower")) {
                f->state = FIGURE_STATE_DEAD;
            }

            f->image_offset = 0;
            f->wait_ticks++;
            if (f->wait_ticks > 40) {
                f->wait_ticks = 0;
                int x_tile, y_tile;
                if (tower_sentry_init_patrol(*tower, &x_tile, &y_tile)) {
                    f->action_state = FIGURE_ACTION_171_TOWER_SENTRY_PATROLLING;
                    f->destination_x = static_cast<unsigned char>(x_tile);
                    f->destination_y = static_cast<unsigned char>(y_tile);
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
        case FIGURE_ACTION_171_TOWER_SENTRY_PATROLLING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_173_TOWER_SENTRY_RETURNING;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_170_TOWER_SENTRY_AT_REST;
            }
            break;
        case FIGURE_ACTION_172_TOWER_SENTRY_FIRING:
            figure_movement_move_ticks_tower_sentry(f, 1);
            f->wait_ticks_missile++;
            if (const UnitRangedAbility *ranged = ranged_ability_for(*f);
                ranged && f->wait_ticks_missile > ranged->cooldown) {
                map_point tile;
                if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->direction =
                        static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
                    f->wait_ticks_missile = 0;
                    int figure_id = f->id();
                    figure_create_missile(
                        f->id(), f->x, f->y, tile.x, tile.y, ranged->projectile_type);
                    f = Figure::get(figure_id);
                } else {
                    f->action_state = FIGURE_ACTION_173_TOWER_SENTRY_RETURNING;
                    f->destination_x = f->source_x;
                    f->destination_y = f->source_y;
                    Route::remove(f);
                }
            }
            break;
        case FIGURE_ACTION_173_TOWER_SENTRY_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_170_TOWER_SENTRY_AT_REST;
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER:
            f->terrain_usage = TERRAIN_USAGE_ROADS_HIGHWAY;
            if (config_get(CONFIG_GP_CH_TOWER_SENTRIES_GO_OFFROAD)) {
                f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
            }

            f->is_ghost = 0;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                if (tower->type && tower->type->is_watchtower()) {
                    figure_watchtower_archer_spawn(*tower);
                    Route::remove(f);
                    f->state = FIGURE_STATE_DEAD;
                } else { // if Tower
                    map_figure_delete(f);
                    f->source_x = f->x = b->x;
                    f->source_y = f->y = b->y;
                    f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
                    map_figure_add(f);
                    f->action_state = FIGURE_ACTION_170_TOWER_SENTRY_AT_REST;
                    Route::remove(f);
                }
            } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
    if (map_terrain_is(f->grid_offset, TERRAIN_WALL)) {
        f->current_height = 18;
    } else if (map_terrain_is(f->grid_offset, TERRAIN_GATEHOUSE)) {
        f->in_building_wait_ticks = 24;
    }
    if (f->in_building_wait_ticks) {
        f->in_building_wait_ticks--;
        f->height_adjusted_ticks = 0;
    }
    figure_tower_sentry_set_image(f);
}

void figure_tower_sentry_reroute(void)
{
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->type != FIGURE_TOWER_SENTRY || Route::wallIsPassable(f->grid_offset)) {
            continue;
        }
        if (f->action_state == FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER ||
            (f->action_state == FIGURE_ACTION_150_ATTACK && f->action_state_before_attack == FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER)) {
            continue;
        }
        // tower sentry got off wall due to rotation
        int x_tile, y_tile;
        if (Route::findWallTileInRadius(f->x, f->y, 2, &x_tile, &y_tile)) {
            Route::remove(f);
            f->progress_on_tile = 0;
            map_figure_delete(f);
            f->previous_tile_x = f->x = static_cast<unsigned char>(x_tile);
            f->previous_tile_y = f->y = static_cast<unsigned char>(y_tile);
    f->cross_country_x = figure_movement_tile_to_cross_country(x_tile);
    f->cross_country_y = figure_movement_tile_to_cross_country(y_tile);
            f->grid_offset = static_cast<short>(map_grid_offset(x_tile, y_tile));
            map_figure_add(f);
            f->action_state = FIGURE_ACTION_173_TOWER_SENTRY_RETURNING;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
        } else {
            // Teleport back to tower
            map_figure_delete(f);
            building *b = f->building ? const_cast<building *>(f->building->record()) : nullptr;
            if (!b) {
                f->state = FIGURE_STATE_DEAD;
                continue;
            }
            f->source_x = f->x = b->x;
            f->source_y = f->y = b->y;
            f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
            map_figure_add(f);
            f->action_state = FIGURE_ACTION_170_TOWER_SENTRY_AT_REST;
            Route::remove(f);
        }
    }
}

static void watchman_pick_target(Figure *f)
{
    if (f->action_state == FIGURE_ACTION_150_ATTACK ||
        f->action_state == FIGURE_ACTION_149_CORPSE) {
        return;
    }
    const UnitRangedAbility *ranged = ranged_ability_for(*f);
    if (!ranged) {
        return;
    }
    f->wait_ticks_next_target++;
    if (f->wait_ticks_next_target >= 40) {
        f->wait_ticks_next_target = 0;
        map_point tile;
        if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
            f->action_state = FIGURE_ACTION_225_WATCHMAN_SHOOTING;
        }
    }
}


void figure_watchman_action(Figure *f)
{
    Building *watchman_building = f ? f->building : nullptr;
    building *b = watchman_building ? const_cast<building *>(watchman_building->record()) : nullptr;
    if (!b) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }

    f->terrain_usage = TERRAIN_USAGE_ROADS;
    f->use_cross_country = 0;
    f->max_roam_length = 640;
    if (b->state != BUILDING_STATE_IN_USE || (b->figure_id != f->id() && b->figure_id2 != f->id())) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_image_increase_offset(f, 12);
    watchman_pick_target(f);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_220_WATCHMAN_PATROL_INITIATE:
            f->roam_length = 0;
            figure_movement_init_roaming(f);
            if (b->figure_id2 == f->id()) { // Skip one roaming cycle for the second Watchman, so they go in the opposite directions
                figure_movement_init_roaming(f);
            }
            f->action_state = FIGURE_ACTION_221_WATCHMAN_PATROLLING;
            break;
        case FIGURE_ACTION_221_WATCHMAN_PATROLLING:
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= f->max_roam_length) {
                int x_road, y_road;
                if (map_closest_road_within_radius_building(
                        *watchman_building, 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_222_WATCHMAN_RETURNING;
                    f->destination_x = static_cast<unsigned char>(x_road);
                    f->destination_y = static_cast<unsigned char>(y_road);
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            figure_movement_roam_ticks(f, 1);
            break;
        case FIGURE_ACTION_222_WATCHMAN_RETURNING:
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST || f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_225_WATCHMAN_SHOOTING:
            f->wait_ticks_missile++;
            if (const UnitRangedAbility *ranged = ranged_ability_for(*f);
                ranged && f->wait_ticks_missile > ranged->cooldown) {
                map_point tile;
                if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->attack_direction =
                        static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
                    f->wait_ticks_missile = 0;
                    int figure_id = f->id();
                    figure_create_missile(
                        f->id(), f->x, f->y, tile.x, tile.y, ranged->projectile_type);
                    f = Figure::get(figure_id);
                } else {
                    f->action_state = FIGURE_ACTION_221_WATCHMAN_PATROLLING;
                }
            }
            break;
    }
    figure_tower_sentry_set_image(f);
}

void figure_watchtower_archer_action(Figure *f)
{
    building *b = f->building ? const_cast<building *>(f->building->record()) : nullptr;
    if (!b) {
        f->state = FIGURE_STATE_DEAD;
        return;
    }
    if (b->state != BUILDING_STATE_IN_USE || b->figure_id4 != f->id()) {
        f->state = FIGURE_STATE_DEAD;
    }
    switch (f->action_state) {
        case FIGURE_ACTION_149_CORPSE:
            f->state = FIGURE_STATE_DEAD;
            break;
        case FIGURE_ACTION_223_ARCHER_GUARDING:
            f->wait_ticks++;
            if (f->wait_ticks > 20) {
                f->wait_ticks = 0;
                map_point tile;
                const UnitRangedAbility *ranged = ranged_ability_for(*f);
                if (ranged && figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->action_state = FIGURE_ACTION_224_ARCHER_SHOOTING;
                    f->wait_ticks_missile = static_cast<unsigned char>(ranged->cooldown);
                }
            }
            break;
        case FIGURE_ACTION_224_ARCHER_SHOOTING:
            f->wait_ticks_missile++;
            if (const UnitRangedAbility *ranged = ranged_ability_for(*f);
                ranged && f->wait_ticks_missile > ranged->cooldown) {
                map_point tile;
                if (figure_combat_get_missile_target_for_soldier(f, ranged->range, &tile)) {
                    f->direction =
                        static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
                    f->wait_ticks_missile = 0;
                    figure_create_missile(
                        f->id(), f->x, f->y, tile.x, tile.y, ranged->projectile_type);
                    sound_effect_play(SOUND_EFFECT_ARROW);
                } else {
                    f->action_state = FIGURE_ACTION_223_ARCHER_GUARDING;
                }
            }
            break;
    }
}


void figure_kill_tower_sentries_at(int x, int y)
{
    for (unsigned int i = 0; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (!f->is_dead() && f->type == FIGURE_TOWER_SENTRY) {
            if (calc_maximum_distance(f->x, f->y, x, y) <= 1) {
                f->state = FIGURE_STATE_DEAD;
            }
        }
    }
}

void figure_kill_tower_sentries_in_building(Building &building)
{
    for (unsigned int figure_id : Figure::ids_directly_referencing_building(building)) {
        Figure *f = Figure::get(figure_id);
        if (f && f->id() == figure_id && !f->is_dead() && f->type == FIGURE_TOWER_SENTRY &&
            f->home_building_id() == static_cast<unsigned int>(building.id)) {
            f->state = FIGURE_STATE_DEAD;
        }
    }
}
