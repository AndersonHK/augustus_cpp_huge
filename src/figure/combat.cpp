#include "combat.h"

#include "building/monument.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/formation.h"
#include "figure/movement.h"
#include "figure/properties.h"
#include "figure/route.h"
#include "figure/sound.h"
#include "game/difficulty.h"
#include "map/figure.h"
#include "sound/effect.h"

static figure_type type_of(const Figure &f)
{
    return static_cast<figure_type>(f.type);
}

static const figure_properties *properties_for(const Figure &f)
{
    return figure_properties_for_type(type_of(f));
}

static figure_category_mask category_for(const Figure &f)
{
    return properties_for(f)->category;
}

static int is_attacking_native(const Figure *f)
{
    return f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING;
}

void figure_combat_handle_corpse(Figure *f)
{
    if (f->wait_ticks < 0) {
        f->wait_ticks = 0;
    }
    f->wait_ticks++;
    if (f->wait_ticks >= 128) {
        f->wait_ticks = 127;
        f->state = FIGURE_STATE_DEAD;
    }
}

static int attack_is_same_direction(int dir1, int dir2)
{
    if (dir1 == dir2) {
        return 1;
    }
    int dir2_off = dir2 <= 0 ? 7 : dir2 - 1;
    if (dir1 == dir2_off) {
        return 1;
    }
    dir2_off = dir2 >= 7 ? 0 : dir2 + 1;
    if (dir1 == dir2_off) {
        return 1;
    }
    return 0;
}

static void resume_activity_after_attack(Figure *f)
{
    f->num_attackers = 0;
    f->action_state = f->action_state_before_attack;
    f->opponent.clear();
    f->attacker1.clear();
    f->attacker2.clear();
    Route::remove(f);
}

static void hit_opponent(Figure *f)
{
    const formation *m = formation_get(f->formation_id);
    Figure *opponent = &f->opponent.get();
    formation *opponent_formation = formation_get(opponent->formation_id);

    const figure_properties *props = properties_for(*f);
    const figure_properties *opponent_props = properties_for(*opponent);
    const figure_category_mask cat = opponent_props->category;
    if (cat & FIGURE_CATEGORY_CITIZEN || cat & FIGURE_CATEGORY_CRIMINAL) {
        f->attack_image_offset = 12;
    } else {
        f->attack_image_offset = 0;
    }
    int figure_attack = props->attack_value;
    int opponent_defense = opponent_props->defense_value;

    // attack modifiers
    if (f->type == FIGURE_WOLF) {
        figure_attack = difficulty_adjust_wolf_attack(figure_attack);
    }
    if (opponent->opponent.save_id() != f->id() && m->figure_type != FIGURE_FORT_LEGIONARY &&
            attack_is_same_direction(f->attack_direction, opponent->attack_direction)) {
        figure_attack += 4; // attack opponent on the (exposed) back
        sound_effect_play(SOUND_EFFECT_SWORD_SWING);
    }
    if (m->is_halted && m->figure_type == FIGURE_FORT_LEGIONARY &&
            attack_is_same_direction(f->attack_direction, m->direction)) {
        figure_attack += 4; // coordinated formation attack bonus
    }
    if (m->is_halted && m->figure_type == FIGURE_FORT_INFANTRY &&
        attack_is_same_direction(f->attack_direction, m->direction)) {
        figure_attack += 2; // coordinated formation attack bonus
    }
    if (m->is_charging && m->figure_type == FIGURE_FORT_MOUNTED) {
        figure_attack += 4; // charging bonus for mounted units
    }
    if (m->is_charging && m->figure_type == FIGURE_FORT_INFANTRY) {
        figure_attack += 2; // charging bonus for sword infantry
    }

    // defense modifiers
    if (opponent_formation->is_halted &&
            (opponent_formation->figure_type == FIGURE_FORT_LEGIONARY ||
                opponent_formation->figure_type == FIGURE_ENEMY_CAESAR_LEGIONARY)) {
        if (!attack_is_same_direction(opponent->attack_direction, opponent_formation->direction)) {
            opponent_defense -= 4; // opponent not attacking in coordinated formation
        } else if (opponent_formation->layout == FORMATION_COLUMN) {
            opponent_defense += 5;
        } else if (opponent_formation->layout == FORMATION_DOUBLE_LINE_1 ||
                   opponent_formation->layout == FORMATION_DOUBLE_LINE_2) {
            opponent_defense += 2;
        }
    }

    // defense modifiers
    if (opponent_formation->is_halted &&
            (opponent_formation->figure_type == FIGURE_FORT_INFANTRY)) {
        if (!attack_is_same_direction(opponent->attack_direction, opponent_formation->direction)) {
            opponent_defense -= 2; // opponent not attacking in coordinated formation
        } else if (opponent_formation->layout == FORMATION_COLUMN) {
            opponent_defense += 3;
        } else if (opponent_formation->layout == FORMATION_DOUBLE_LINE_1 ||
                   opponent_formation->layout == FORMATION_DOUBLE_LINE_2) {
            opponent_defense += 1;
        }
    }


    int max_damage = opponent_props->max_damage;
    int net_attack = figure_attack - opponent_defense;
    if (net_attack < 0) {
        net_attack = 0;
    }
    opponent->damage = static_cast<unsigned char>(opponent->damage + net_attack);
    if (opponent->damage <= max_damage) {
        figure_play_hit_sound(type_of(*f));
    } else {
        opponent->action_state = FIGURE_ACTION_149_CORPSE;
        opponent->wait_ticks = 0;
        figure_play_die_sound(opponent);
        formation_update_morale_after_death(opponent_formation);
    }
}

void figure_combat_handle_attack(Figure *f)
{
    figure_movement_advance_attack(f);
    if (f->num_attackers == 0) {
        resume_activity_after_attack(f);
        return;
    }
    if (f->num_attackers == 1) {
        Figure *target = &f->opponent.get();
        if (target->is_dead()) {
            resume_activity_after_attack(f);
            return;
        }
    } else if (f->num_attackers == 2) {
        if (f->opponent.get().is_dead()) {
            if (f->opponent.save_id() == f->attacker1.save_id()) {
                f->opponent.retarget(f->attacker2.get());
            } else if (f->opponent.save_id() == f->attacker2.save_id()) {
                f->opponent.retarget(f->attacker1.get());
            }
            if (f->opponent.get().is_dead()) {
                resume_activity_after_attack(f);
                return;
            }
            f->num_attackers = 1;
            f->attacker1.retarget(f->opponent.get());
            f->attacker2.clear();
        }
    }
    f->attack_image_offset++;
    if (f->attack_image_offset >= 24) {
        hit_opponent(f);
    }
}

int figure_combat_get_target_for_soldier(int x, int y, int max_distance)
{
    int min_figure_id = 0;
    int min_distance = 10000;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead() || f->is_ghost) {
            // Do not allow to target dead and enemies located outside of the map
            continue;
        }
        if (f->is_enemy() || f->type == FIGURE_RIOTER || is_attacking_native(f)) {
            int distance = calc_maximum_distance(x, y, f->x, f->y);
            if (distance <= max_distance) {
                if (f->targeted_by_figure.save_id()) {
                    distance *= 2; // penalty
                }
                if (distance < min_distance) {
                    min_distance = distance;
                    min_figure_id = i;
                }
            }
        }
    }
    if (min_figure_id) {
        return min_figure_id;
    }
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead()) {
            continue;
        }
        if (f->is_enemy() || f->type == FIGURE_RIOTER || is_attacking_native(f)) {
            return i;
        }
    }
    return 0;
}

int figure_combat_get_target_for_wolf(int x, int y, int max_distance)
{
    int min_figure_id = 0;
    int min_distance = 10000;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead() || !f->type) {
            continue;
        }
        switch (f->type) {
            case FIGURE_EXPLOSION:
            case FIGURE_FORT_STANDARD:
            case FIGURE_TRADE_SHIP:
            case FIGURE_FISHING_BOAT:
            case FIGURE_MAP_FLAG:
            case FIGURE_FLOTSAM:
            case FIGURE_SHIPWRECK:
            case FIGURE_INDIGENOUS_NATIVE:
            case FIGURE_TOWER_SENTRY:
            case FIGURE_NATIVE_TRADER:
            case FIGURE_ARROW:
            case FIGURE_JAVELIN:
            case FIGURE_BOLT:
            case FIGURE_BALLISTA:
            case FIGURE_CATAPULT_MISSILE:
            case FIGURE_FRIENDLY_ARROW:
            case FIGURE_WATCHTOWER_ARCHER:
            case FIGURE_CREATURE:
                continue;
        }
        if (f->is_herd()) {
            continue;
        }
        if (f->is_legion() && f->action_state == FIGURE_ACTION_80_SOLDIER_AT_REST) {
            continue;
        }
        int distance = calc_maximum_distance(x, y, f->x, f->y);
        if (f->targeted_by_figure.save_id()) {
            distance *= 2;
        }
        if (distance < min_distance) {
            min_distance = distance;
            min_figure_id = i;
        }
    }
    if (min_distance <= max_distance && min_figure_id) {
        return min_figure_id;
    }
    return 0;
}

int figure_combat_get_target_for_enemy(int x, int y)
{
    int min_figure_id = 0;
    int min_distance = 10000;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead()) {
            continue;
        }
        if (!f->targeted_by_figure.save_id() && f->is_legion()) {
            int distance = calc_maximum_distance(x, y, f->x, f->y);
            if (distance < min_distance) {
                min_distance = distance;
                min_figure_id = i;
            }
        }
    }
    if (min_figure_id) {
        return min_figure_id;
    }
    // no 'free' soldier found, take first one
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead()) {
            continue;
        }
        if (f->is_legion()) {
            return i;
        }
    }
    return 0;
}

static int is_valid_missile_target(Figure *f, formation *l)
{
    if (f->is_enemy() || is_attacking_native(f)) {
        return 1;
    }
    if (!f->is_herd()) {
        return 0;
    }
    if (f->type == FIGURE_WOLF || config_get(CONFIG_GP_CH_AUTO_KILL_ANIMALS)) {
        return 1;
    }
    if (l->target_formation_id && l->target_formation_id == f->formation_id) {
        return 1;
    }
    return 0;
}

int figure_combat_get_missile_target_for_soldier(Figure *shooter, int max_distance, map_point *tile)
{
    int x = shooter->x;
    int y = shooter->y;

    int min_distance = max_distance;
    Figure *min_figure = nullptr;
    formation *l = formation_get(shooter->formation_id);
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead() || f->is_ghost) {
            // Do not allow to target dead and enemies located outside of the map
            continue;
        }
        if (is_valid_missile_target(f, l)) {
            int distance = calc_maximum_distance(x, y, f->x, f->y);
            if (distance < min_distance && figure_movement_can_launch_cross_country_missile(x, y, f->x, f->y)) {
                min_distance = distance;
                min_figure = f;
            }
        }
    }
    if (min_figure) {
        map_point_store_result(min_figure->x, min_figure->y, tile);
        return min_figure->id();
    }
    return 0;
}

int figure_combat_get_missile_target_for_enemy(Figure *enemy, int max_distance, int attack_citizens,
                                               map_point *tile)
{
    if (enemy->is_ghost) {
        // Do not allow enemies to attack from outside of the map
        return 0;
    }
    int x = enemy->x;
    int y = enemy->y;

    Figure *min_figure = nullptr;
    int min_distance = max_distance;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->is_dead() || !f->type) {
            continue;
        }
        switch (f->type) {
            case FIGURE_EXPLOSION:
            case FIGURE_FORT_STANDARD:
            case FIGURE_MAP_FLAG:
            case FIGURE_FLOTSAM:
            case FIGURE_INDIGENOUS_NATIVE:
            case FIGURE_NATIVE_TRADER:
            case FIGURE_ARROW:
            case FIGURE_JAVELIN:
            case FIGURE_BOLT:
            case FIGURE_BALLISTA:
            case FIGURE_FRIENDLY_ARROW:
            case FIGURE_CATAPULT_MISSILE:
            case FIGURE_WATCHTOWER_ARCHER:
            case FIGURE_CREATURE:
            case FIGURE_FISH_GULLS:
            case FIGURE_SHIPWRECK:
            case FIGURE_SHEEP:
            case FIGURE_WOLF:
            case FIGURE_ZEBRA:
            case FIGURE_SPEAR:
                continue;
        }
        int distance;
        if (f->is_legion()) {
            distance = calc_maximum_distance(x, y, f->x, f->y);
        } else if (attack_citizens && f->is_friendly) {
            distance = calc_maximum_distance(x, y, f->x, f->y) + 5;
        } else {
            continue;
        }
        if (distance < min_distance && figure_movement_can_launch_cross_country_missile(x, y, f->x, f->y)) {
            min_distance = distance;
            min_figure = f;
        }
    }
    if (min_figure) {
        map_point_store_result(min_figure->x, min_figure->y, tile);
        return min_figure->id();
    }
    return 0;
}

static int can_attack_animal(figure_category_mask category, figure_category_mask opponent_category, formation *l, Figure *opponent)
{
    if (!(category & FIGURE_CATEGORY_ARMED || category & FIGURE_CATEGORY_HOSTILE) || !(opponent_category & FIGURE_CATEGORY_ANIMAL)) {
        return 0;
    }
    if (category & FIGURE_CATEGORY_HOSTILE) {
        return 1;
    }
    if (config_get(CONFIG_GP_CH_AUTO_KILL_ANIMALS)) {
        return 1;
    }
    if ((l->target_formation_id && l->target_formation_id == opponent->formation_id) ||
        (opponent_category & FIGURE_CATEGORY_AGGRESSIVE_ANIMAL)) {
        return 1;
    }
    return 0;
}

void figure_combat_attack_figure_at(Figure *f, int grid_offset)
{
    const figure_category_mask category = category_for(*f);
    if (category <= FIGURE_CATEGORY_INACTIVE ||
        (category >= FIGURE_CATEGORY_CRIMINAL && category <= FIGURE_CATEGORY_ANIMAL) ||
        f->action_state == FIGURE_ACTION_150_ATTACK) {
        return;
    }
    formation *l = formation_get(f->formation_id);
    unsigned int guard = 0;
    unsigned int opponent_id = map_figure_at(grid_offset);
    while (1) {
        if (++guard >= Figure::count() || opponent_id <= 0) {
            break;
        }
        Figure *opponent = Figure::get(opponent_id);
        if (opponent_id == f->id() || opponent->is_ghost) {
            // Do not allow troops to attack themselves or enemies located outside of the map
            opponent_id = opponent->next_figure_id_on_same_tile;
            continue;
        }

        const figure_category_mask opponent_category = category_for(*opponent);
        int attack = 0;
        if (opponent->state != FIGURE_STATE_ALIVE) {
            attack = 0;
        } else if (opponent->action_state == FIGURE_ACTION_149_CORPSE) {
            attack = 0;
        } else if (category & FIGURE_CATEGORY_ARMED && opponent_category & FIGURE_CATEGORY_NATIVE) {
            if (opponent->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
                attack = 1;
            }
        } else if (category & FIGURE_CATEGORY_ARMED && opponent_category & FIGURE_CATEGORY_HOSTILE) {
            attack = 1;
        } else if (category & FIGURE_CATEGORY_HOSTILE && opponent_category & FIGURE_CATEGORY_CITIZEN) {
            attack = 1;
        } else if (category & FIGURE_CATEGORY_HOSTILE && opponent_category & FIGURE_CATEGORY_CRIMINAL) {
            attack = 1;
        } else if (category & FIGURE_CATEGORY_AGGRESSIVE_ANIMAL && opponent_category & FIGURE_CATEGORY_CITIZEN) {
            attack = 1;
        } else if (category & FIGURE_CATEGORY_AGGRESSIVE_ANIMAL && opponent_category & FIGURE_CATEGORY_ARMED) {
            attack = 1;
        } else if (category & FIGURE_CATEGORY_AGGRESSIVE_ANIMAL && opponent_category & FIGURE_CATEGORY_HOSTILE) {
            attack = 1;
        } else if (can_attack_animal(category, opponent_category, l, opponent)) {
            attack = 1;
        }
        if (attack && opponent->action_state == FIGURE_ACTION_150_ATTACK && opponent->num_attackers >= 2) {
            attack = 0;
        }
        if (attack) {
            f->action_state_before_attack = f->action_state;
            f->action_state = FIGURE_ACTION_150_ATTACK;
            f->opponent.retarget(*opponent);
            f->attacker1.retarget(*opponent);
            f->num_attackers = 1;
            f->attack_image_offset = 12;
            if (opponent->x != opponent->destination_x || opponent->y != opponent->destination_y) {
                f->attack_direction = static_cast<signed char>(calc_general_direction(f->previous_tile_x, f->previous_tile_y,
                    opponent->previous_tile_x, opponent->previous_tile_y));
            } else {
                f->attack_direction = static_cast<signed char>(calc_general_direction(f->previous_tile_x, f->previous_tile_y,
                    opponent->x, opponent->y));
            }
            if (f->attack_direction >= 8) {
                f->attack_direction = 0;
            }
            if (opponent->action_state != FIGURE_ACTION_150_ATTACK) {
                opponent->action_state_before_attack = opponent->action_state;
                opponent->action_state = FIGURE_ACTION_150_ATTACK;
                opponent->attack_image_offset = 0;
                opponent->attack_direction = (f->attack_direction + 4) % 8;
            }
            if (opponent->num_attackers == 0) {
                opponent->attacker1.retarget(*f);
                opponent->opponent.retarget(*f);
                opponent->num_attackers = 1;
            } else if (opponent->num_attackers == 1) {
                opponent->attacker2.retarget(*f);
                opponent->num_attackers = 2;
            }
            return;
        }
        opponent_id = opponent->next_figure_id_on_same_tile;
    }
}
