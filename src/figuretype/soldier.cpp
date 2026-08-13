#include "soldier.h"

#include "city/figures.h"
#include "city/games.h"
#include "city/map.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/FigureGraphics.h"
#include "figure/formation.h"
#include "figure/formation_layout.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/unit_type.h"
#include "figuretype/missile.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/point.h"

void map_figure_add(Figure *f);
void map_figure_update(Figure *f);
void map_figure_delete(Figure *f);

static const map_point ALTERNATIVE_POINTS[] = { {-1, -6},
    {0, -1}, {1, -1}, {1, 0}, {1, 1}, {0, 1}, {-1, 1}, {-1, 0}, {-1, -1},
    {0, -2}, {1, -2}, {2, -2}, {2, -1}, {2, 0}, {2, 1}, {2, 2}, {1, 2},
    {0, 2}, {-1, 2}, {-2, 2}, {-2, 1}, {-2, 0}, {-2, -1}, {-2, -2}, {-1, -2},
    {0, -3}, {1, -3}, {2, -3}, {3, -3}, {3, -2}, {3, -1}, {3, 0}, {3, 1},
    {3, 2}, {3, 3}, {2, 3}, {1, 3}, {0, 3}, {-1, 3}, {-2, 3}, {-3, 3},
    {-3, 2}, {-3, 1}, {-3, 0}, {-3, -1}, {-3, -2}, {-3, -3}, {-2, -3}, {-1, -3},
    {0, -4}, {1, -4}, {2, -4}, {3, -4}, {4, -4}, {4, -3}, {4, -2}, {4, -1},
    {4, 0}, {4, 1}, {4, 2}, {4, 3}, {4, 4}, {3, 4}, {2, 4}, {1, 4},
    {0, 4}, {-1, 4}, {-2, 4}, {-3, 4}, {-4, 4}, {-4, 3}, {-4, 2}, {-4, 1},
    {-4, 0}, {-4, -1}, {-4, -2}, {-4, -3}, {-4, -4}, {-3, -4}, {-2, -4}, {-1, -4},
    {0, -5}, {1, -5}, {2, -5}, {3, -5}, {4, -5}, {5, -5}, {5, -4}, {5, -3},
    {5, -2}, {5, -1}, {5, 0}, {5, 1}, {5, 2}, {5, 3}, {5, 4}, {5, 5},
    {4, 5}, {3, 5}, {2, 5}, {1, 5}, {0, 5}, {-1, 5}, {-2, 5}, {-3, 5},
    {-4, 5}, {-5, 5}, {-5, 4}, {-5, 3}, {-5, 2}, {-5, 1}, {-5, 0}, {-5, -1},
    {-5, -2}, {-5, -3}, {-5, -4}, {-5, -5}, {-4, -5}, {-3, -5}, {-2, -5}, {-1, -5},
    {0, -6}, {1, -6}, {2, -6}, {3, -6}, {4, -6}, {5, -6}, {6, -6}, {6, -5},
    {6, -4}, {6, -3}, {6, -2}, {6, -1}, {6, 0}, {6, 1}, {6, 2}, {6, 3},
    {6, 4}, {6, 5}, {6, 6}, {5, 6}, {4, 6}, {3, 6}, {2, 6}, {1, 6},
    {0, 6}, {-1, 6}, {-2, 6}, {-3, 6}, {-4, 6}, {-5, 6}, {-6, 6}, {-6, 5},
    {-6, 4}, {-6, 3}, {-6, 2}, {-6, 1}, {-6, 0}, {-6, -1}, {-6, -2}, {-6, -3},
    {-6, -4}, {-6, -5}, {-6, -6}, {-5, -6}, {-4, -6}, {-3, -6}, {-2, -6}, {-1, -6},
};



void figure_military_standard_action(Figure *f)
{
    const formation *m = formation_get(f->formation_id);

    f->terrain_usage = TERRAIN_USAGE_ANY;
    figure_image_increase_offset(f, 16);
    map_figure_delete(f);
    if (m->is_at_fort) {
        f->x = static_cast<unsigned char>(m->x);
        f->y = static_cast<unsigned char>(m->y);
    } else {
        f->x = static_cast<unsigned char>(m->standard_x);
        f->y = static_cast<unsigned char>(m->standard_y);
    }
    f->grid_offset = static_cast<short>(map_grid_offset(f->x, f->y));
    f->cross_country_x = static_cast<short>(figure_movement_tile_center_cross_country(f->x));
    f->cross_country_y = static_cast<short>(figure_movement_tile_center_cross_country(f->y));
    map_figure_add(f);

    int pole_offset = 20 - m->morale / 5;
    if (pole_offset < 0) {
        pole_offset = 0;
    }
    f->select_legacy_directional_frame_image(
        image_group(GROUP_FIGURE_FORT_STANDARD_POLE),
        0,
        pole_offset,
        1);

    f->clear_legacy_cart_overlay_image();
}

static const UnitType *unit_type_for(const Figure &f)
{
    return unit_type_registry_impl::find_unit_type(static_cast<figure_type>(f.type));
}

static bool ranged_formation_allows(const UnitRangedAbility &ability, const formation &formation)
{
    return !ability.requires_double_line ||
        formation.uses_layout("double_line_1") ||
        formation.uses_layout("double_line_2");
}

static Figure *soldier_launch_missile(Figure *f)
{
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        return f;
    }
    const UnitType *unit = unit_type_for(*f);
    const UnitRangedAbility *ability = unit ? unit->ranged_ability() : nullptr;
    if (!ability) {
        return f;
    }

    map_point tile = { -1, -1 };
    f->wait_ticks_missile++;
    if (f->wait_ticks_missile > ability->cooldown) {
        f->wait_ticks_missile = 0;
        if (figure_combat_get_missile_target_for_soldier(f, ability->range, &tile)) {
            f->attack_image_offset = 1;
            f->direction =
                static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
        } else {
            f->attack_image_offset = 0;
        }
    }
    if (f->attack_image_offset) {
        if (f->attack_image_offset == ability->launch_frame) {
            if (ability->launch_frame > 1) {
                // Adjust the target in case of long delay
                if (figure_combat_get_missile_target_for_soldier(f, ability->range, &tile)) {
                    f->direction =
                        static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
                }
            }
            if (tile.x == -1 || tile.y == -1) {
                map_point_get_last_result(&tile);
            }
            int soldier_id = f->id();
            figure_create_missile(
                soldier_id, f->x, f->y, tile.x, tile.y, ability->projectile_type);
            f = Figure::get(f->id());
            formation_record_missile_fired(formation_get(f->formation_id));
        }
        f->attack_image_offset++;
        if (f->attack_image_offset > 100) {
            f->attack_image_offset = 0;
        }
    }
    return f;
}

static void attack_adjacent_enemy(Figure *f)
{
    for (int i = 0; i < 8 && f->action_state != FIGURE_ACTION_150_ATTACK; i++) {
        figure_combat_attack_figure_at(f, f->grid_offset + map_grid_direction_delta(i));
    }
}

static int find_mop_up_target(Figure *f)
{
    int target_id = f->target_figure.save_id();
    if (Figure::get(target_id)->is_dead()) {
        f->target_figure.clear();
        target_id = 0;
    }
    if (target_id <= 0) {
        target_id = figure_combat_get_target_for_soldier(f->x, f->y, 20);
        if (target_id) {
            Figure *target = Figure::get(target_id);
            f->destination_x = target->x;
            f->destination_y = target->y;
            f->target_figure.retarget(*target);
            target->targeted_by_figure.retarget(*f);
            f->target_figure_created_sequence = target->created_sequence;
        } else {
            f->action_state = FIGURE_ACTION_84_SOLDIER_AT_STANDARD;
            f->image_offset = 0;
        }
        Route::remove(f);
    }
    return target_id;
}

static void update_image_javelin(Figure *f, int dir)
{
    int image_id = image_group(GROUP_BUILDING_FORT_JAVELIN);
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset < 12 ? 0 : (f->attack_image_offset - 12) / 2;
        f->select_legacy_directional_frame_image(image_id + 96, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_id + 144);
    } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
        f->select_legacy_directional_frame_image(
            image_id + 96,
            dir,
            figure_type_registry_impl::FigureGraphics::missile_launcher_frame_for(*f));
    } else {
        f->select_legacy_directional_frame_image(image_id, dir, f->image_offset);
    }
}

static void update_image_mounted(Figure *f, int dir)
{
    int image_id = image_group(GROUP_FIGURE_FORT_MOUNTED);
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset < 12 ? 0 : (f->attack_image_offset - 12) / 2;
        f->select_legacy_directional_frame_image(image_id + 96, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_id + 144);
    } else {
        f->select_legacy_directional_frame_image(image_id, dir, f->image_offset);
    }
}

static bool unit_can_launch_ranged(const Figure &f, const formation &m)
{
    const UnitType *unit = unit_type_for(f);
    const UnitRangedAbility *ability = unit ? unit->ranged_ability() : nullptr;
    return ability && ranged_formation_allows(*ability, m);
}

static void update_image_legionary(Figure *f, const formation *m, int dir)
{
    int image_id = image_group(GROUP_BUILDING_FORT_LEGIONARY);
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset < 12 ? 0 : (f->attack_image_offset - 12) / 2;
        f->select_legacy_directional_frame_image(image_id + 96, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_id + 152);
    } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
        const int missile_offset =
            figure_type_registry_impl::FigureGraphics::missile_launcher_frame_for(*f);
        if (m->is_halted && m->uses_layout("column") && m->missile_attack_timeout) {
            f->select_legacy_directional_frame_image(image_id + 144, dir, 0);
        } else if (unit_can_launch_ranged(*f, *m) && missile_offset >= 0 && dir < DIR_8_NONE) {
            f->select_legacy_directional_frame_image(image_id + 144, dir, 0);
        } else {
            f->select_legacy_directional_frame_image(image_id, dir, 0);
        }
    } else {
        f->select_legacy_directional_frame_image(image_id, dir, f->image_offset);
    }
}

static void update_image(Figure *f, const formation *m)
{
    int dir;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        dir = f->attack_direction;
    } else if (m->missile_fired) {
        dir = f->direction;
    } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
        dir = m->direction;
    } else if (f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    dir = figure_image_normalize_direction(dir);
    if (f->type == FIGURE_FORT_JAVELIN) {
        update_image_javelin(f, dir);
    } else if (f->type == FIGURE_FORT_MOUNTED) {
        update_image_mounted(f, dir);
    } else if (f->type == FIGURE_FORT_LEGIONARY) {
        update_image_legionary(f, m, dir);
    } else if (f->type == FIGURE_FORT_INFANTRY || f->type == FIGURE_FORT_ARCHER) {
        f->clear_legacy_image();
    }
}

static int soldier_percentage_speed(figure_type type)
{
    if (city_games_naval_battle_active()) {
        switch (type) {
        case FIGURE_FORT_LEGIONARY:
            return 25;
        case FIGURE_FORT_JAVELIN:
            return 50;
        case FIGURE_FORT_MOUNTED:
            return 75;
        case FIGURE_FORT_INFANTRY:
            return 85;
        case FIGURE_FORT_ARCHER:
            return 60;
            break;
        default:
            break;
        }
    }

    if (type == FIGURE_FORT_INFANTRY) {
        return 50;
    } else if (type == FIGURE_FORT_ARCHER) {
        return 25;
    }
    return 0;
}

void figure_soldier_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    city_figures_add_soldier();
    f->terrain_usage = TERRAIN_USAGE_ANY;
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    if (m->in_use != 1) {
        f->action_state = FIGURE_ACTION_149_CORPSE;
    }
    int speed_factor;
    int speed_factor_percentage = soldier_percentage_speed(static_cast<figure_type>(f->type));
    if (f->type == FIGURE_FORT_MOUNTED) {
        speed_factor = 3;
    } else if (f->type == FIGURE_FORT_JAVELIN) {
        speed_factor = 2;
    } else {
        speed_factor = 1;
    }
    const FormationLayoutDef *layout = m->layout_type();
    if (f->formation_at_rest || f->action_state == FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT) {
        layout = formation_layout_registry_impl::find_layout("at_rest");
    }
    FormationLayoutPosition position =
        formation_layout_position(layout, f->index_in_formation, m->declared_capacity());
    f->formation_position_x.soldier = static_cast<unsigned char>(m->x + position.x);
    f->formation_position_y.soldier = static_cast<unsigned char>(m->y + position.y);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_80_SOLDIER_AT_REST:
            map_figure_update(f);
            f->wait_ticks = 0;
            f->formation_at_rest = 1;
            f->image_offset = 0;
            f->attack_image_offset = 0;
            if (f->x != f->formation_position_x.soldier || f->y != f->formation_position_y.soldier) {
                f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            }
            break;
        case FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT:
        case FIGURE_ACTION_148_FLEEING:
            f->wait_ticks = 0;
            f->formation_at_rest = 1;
            f->destination_x = f->formation_position_x.soldier;
            f->destination_y = f->formation_position_y.soldier;
            f->destination_grid_offset = static_cast<short>(map_grid_offset(f->destination_x, f->destination_y));
            figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_80_SOLDIER_AT_REST;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_82_SOLDIER_RETURNING_TO_BARRACKS:
            f->formation_at_rest = 1;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
            figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            }
            break;
        case FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD:
            f->attack_image_offset = 0;
            f->formation_at_rest = 0;
        position = formation_layout_position(m->layout_type(), f->index_in_formation, m->declared_capacity());
            f->destination_x = static_cast<unsigned char>(m->standard_x + position.x);
            f->destination_y = static_cast<unsigned char>(m->standard_y + position.y);
            if (f->alternative_location_index) {
                f->destination_x = static_cast<unsigned char>(
                    f->destination_x + ALTERNATIVE_POINTS[f->alternative_location_index].x);
                f->destination_y = static_cast<unsigned char>(
                    f->destination_y + ALTERNATIVE_POINTS[f->alternative_location_index].y);
            }
            f->destination_grid_offset = static_cast<short>(map_grid_offset(f->destination_x, f->destination_y));
            figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_84_SOLDIER_AT_STANDARD;
                f->image_offset = 0;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->alternative_location_index++;
                if (f->alternative_location_index > 168) {
                    f->state = FIGURE_STATE_DEAD;
                }
                f->image_offset = 0;
            }
            break;
        case FIGURE_ACTION_84_SOLDIER_AT_STANDARD:
            f->formation_at_rest = 0;
            f->image_offset = 0;
            map_figure_update(f);
        position = formation_layout_position(m->layout_type(), f->index_in_formation, m->declared_capacity());
            f->destination_x = static_cast<unsigned char>(m->standard_x + position.x);
            f->destination_y = static_cast<unsigned char>(m->standard_y + position.y);
            if (f->alternative_location_index) {
                f->destination_x = static_cast<unsigned char>(
                    f->destination_x + ALTERNATIVE_POINTS[f->alternative_location_index].x);
                f->destination_y = static_cast<unsigned char>(
                    f->destination_y + ALTERNATIVE_POINTS[f->alternative_location_index].y);
            }
            if (f->x != f->destination_x || f->y != f->destination_y) {
                if (m->missile_fired <= 0 && m->recent_fight <= 0 && m->missile_attack_timeout <= 0) {
                    f->action_state = FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD;
                    f->alternative_location_index = 0;
                }
            }
            if (f->action_state != FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD) {
                const UnitType *unit = unit_type_for(*f);
                const UnitMeleeAbility *melee = unit ? unit->melee_ability() : nullptr;
                const UnitRangedAbility *ranged = unit ? unit->ranged_ability() : nullptr;
                if (melee && melee->engages_adjacent) {
                    attack_adjacent_enemy(f);
                }
                if (ranged && ranged_formation_allows(*ranged, *m)) {
                    f = soldier_launch_missile(f);
                }
            }
            break;
        case FIGURE_ACTION_85_SOLDIER_GOING_TO_MILITARY_ACADEMY:
            m->has_military_training = 1;
            f->formation_at_rest = 1;
            figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_86_SOLDIER_MOPPING_UP:
            f->formation_at_rest = 0;
            if (find_mop_up_target(f)) {
                figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    Figure &target = f->target_figure.get();
                    f->destination_x = target.x;
                    f->destination_y = target.y;
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->action_state = FIGURE_ACTION_84_SOLDIER_AT_STANDARD;
                    f->target_figure.clear();
                    f->image_offset = 0;
                }
            }
            break;
        case FIGURE_ACTION_87_SOLDIER_GOING_TO_DISTANT_BATTLE:
            {
                const map_tile *exit = city_map_exit_point();
                f->formation_at_rest = 0;
                f->destination_x = static_cast<unsigned char>(exit->x);
                f->destination_y = static_cast<unsigned char>(exit->y);
                figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_89_SOLDIER_AT_DISTANT_BATTLE;
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            }
        case FIGURE_ACTION_88_SOLDIER_RETURNING_FROM_DISTANT_BATTLE:
            f->is_ghost = 0;
            f->wait_ticks = 0;
            f->formation_at_rest = 1;
            f->destination_x = f->formation_position_x.soldier;
            f->destination_y = f->formation_position_y.soldier;
            f->destination_grid_offset = static_cast<short>(map_grid_offset(f->destination_x, f->destination_y));
            figure_movement_move_ticks_with_percentage(f, speed_factor, speed_factor_percentage);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_80_SOLDIER_AT_REST;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_89_SOLDIER_AT_DISTANT_BATTLE:
            f->is_ghost = 1;
            f->formation_at_rest = 1;
            break;
    }

    update_image(f, m);
}
