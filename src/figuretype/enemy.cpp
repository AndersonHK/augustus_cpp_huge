#include "enemy.h"

#include "building/building.h"
#include "building/building_record.h"
#include "city/figures.h"
#include "city/sound.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/formation.h"
#include "figure/formation_enemy.h"
#include "figure/formation_layout.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/properties.h"
#include "figure/route.h"
#include "figuretype/missile.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "scenario/property.h"
#include "scenario/gladiator_revolt.h"
#include "sound/effect.h"
#include "sound/speech.h"
#include "window/building/common.h"

void figuretype::Enemy::draw(building_info_context *c)
{
    int big_people_image_id = Figure::big_people_image_id(static_cast<figure_type>(type));
    int enemy_type = formation_get(formation_id)->enemy_type;
    switch (type) {
        case FIGURE_ENEMY43_SPEAR:
            switch (enemy_type) {
                case ENEMY_5_PERGAMUM: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 44 - 1; break;
                case ENEMY_6_SELEUCID: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 46 - 1; break;
                case ENEMY_7_ETRUSCAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 32 - 1; break;
                case ENEMY_8_GREEK: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 36 - 1; break;
            }
            break;
        case FIGURE_ENEMY44_SWORD:
            switch (enemy_type) {
                case ENEMY_5_PERGAMUM: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 45 - 1; break;
                case ENEMY_6_SELEUCID: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 47 - 1; break;
                case ENEMY_9_EGYPTIAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 29 - 1; break;
            }
            break;
        case FIGURE_ENEMY45_SWORD:
            switch (enemy_type) {
                case ENEMY_7_ETRUSCAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 31 - 1; break;
                case ENEMY_8_GREEK: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 37 - 1; break;
                case ENEMY_10_CARTHAGINIAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 22 - 1; break;
            }
            break;
        case FIGURE_ENEMY49_FAST_SWORD:
            switch (enemy_type) {
                case ENEMY_0_BARBARIAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 21 - 1; break;
                case ENEMY_1_NUMIDIAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 20 - 1; break;
                case ENEMY_4_GOTH: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 35 - 1; break;
            }
            break;
        case FIGURE_ENEMY50_SWORD:
            switch (enemy_type) {
                case ENEMY_2_GAUL: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 40 - 1; break;
                case ENEMY_3_CELT: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 24 - 1; break;
            }
            break;
        case FIGURE_ENEMY51_SPEAR:
            switch (enemy_type) {
                case ENEMY_1_NUMIDIAN: big_people_image_id = Image::group(GROUP_BIG_PEOPLE) + 20 - 1; break;
            }
            break;
    }
    Image::from_id(big_people_image_id).draw(c->x_offset + 28, c->y_offset + 112);

    lang_text_draw(current_string_key(65, name), c->x_offset + 90, c->y_offset + 108,
        FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));
    lang_text_draw(current_string_key(37, scenario_property_enemy() + 20), c->x_offset + 92, c->y_offset + 149,
        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
}

static void enemy_initial(Figure *f, formation *m)
{
    map_figure_update(f);
    f->image_offset = 0;
    Route::remove(f);
    f->wait_ticks--;
    if (f->wait_ticks <= 0) {
        if (f->is_ghost && f->index_in_formation == 0 && static_cast<unsigned int>(m->roster_figure_id(0)) == f->id()) {
            if (m->layout == FORMATION_ENEMY_MOB) {
                sound_speech_play_file("wavs/drums.wav");
            } else if (m->layout == FORMATION_ENEMY12) {
                sound_speech_play_file("wavs/horn2.wav");
            } else {
                sound_speech_play_file("wavs/horn1.wav");
            }
        }
        f->is_ghost = 0;
        if (m->recent_fight) {
            f->action_state = FIGURE_ACTION_154_ENEMY_FIGHTING;
        } else {
            f->destination_x = static_cast<unsigned char>(m->destination_x + f->formation_position_x.enemy);
            f->destination_y = static_cast<unsigned char>(m->destination_y + f->formation_position_y.enemy);
            if (calc_general_direction(f->x, f->y, f->destination_x, f->destination_y) < 8) {
                f->action_state = FIGURE_ACTION_153_ENEMY_MARCHING;
            }
        }
    }
    if (f->type == FIGURE_ENEMY43_SPEAR || f->type == FIGURE_ENEMY46_CAMEL ||
        f->type == FIGURE_ENEMY51_SPEAR || f->type == FIGURE_ENEMY52_MOUNTED_ARCHER ||
        f->type == FIGURE_ENEMY_CATAPULT) {
        // missile throwers
        f->wait_ticks_missile++;
        map_point tile = { 0, 0 };
        if (f->wait_ticks_missile > figure_properties_for_type(static_cast<figure_type>(f->type))->missile_delay) {
            f->wait_ticks_missile = 0;
            if (figure_combat_get_missile_target_for_enemy(f, 10, city_figures_soldiers() < 4, &tile)) {
                f->attack_image_offset = 1;
                f->direction =
                    static_cast<signed char>(calc_missile_shooter_direction(f->x, f->y, tile.x, tile.y));
            } else {
                f->attack_image_offset = 0;
            }
        }
        if (f->attack_image_offset) {
            figure_type missile_type;
            switch (m->enemy_type) {
                case ENEMY_4_GOTH:
                case ENEMY_5_PERGAMUM:
                case ENEMY_9_EGYPTIAN:
                case ENEMY_10_CARTHAGINIAN:
                    missile_type = FIGURE_ARROW;
                    break;
                case FIGURE_ENEMY_CATAPULT:
                    missile_type = FIGURE_CATAPULT_MISSILE;
                    break;
                default:
                    missile_type = FIGURE_SPEAR;
                    break;
            }
            if (f->attack_image_offset == 1) {
                if (tile.x == -1 || tile.y == -1) {
                    map_point_get_last_result(&tile);
                }
                figure_create_missile(f->id(), f->x, f->y, tile.x, tile.y, missile_type);
                formation_record_missile_fired(m);
                if (missile_type == FIGURE_ARROW && city_sound_update_shoot_arrow()) {
                    sound_effect_play(SOUND_EFFECT_ARROW);
                }
            }
            f->attack_image_offset++;
            if (f->attack_image_offset > 100) {
                f->attack_image_offset = 0;
            }
        }
    }
}

static void enemy_marching(Figure *f, const formation *m)
{
    f->wait_ticks--;
    if (f->wait_ticks <= 0) {
        f->wait_ticks = 50;
        f->destination_x = static_cast<unsigned char>(m->destination_x + f->formation_position_x.enemy);
        f->destination_y = static_cast<unsigned char>(m->destination_y + f->formation_position_y.enemy);
        if (calc_general_direction(f->x, f->y, f->destination_x, f->destination_y) == DIR_FIGURE_AT_DESTINATION) {
            f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
            return;
        }
        Building *destination = nullptr;
        Building::for_each([&](Building *building) {
            if (!destination && building && building->id == static_cast<unsigned int>(m->destination_building_id)) {
                destination = building;
            }
        });
        f->destination_building = destination;
        Route::remove(f);
    }
    figure_movement_move_ticks(f, f->speed_multiplier);
    if (f->direction == DIR_FIGURE_AT_DESTINATION ||
        f->direction == DIR_FIGURE_REROUTE ||
        f->direction == DIR_FIGURE_LOST) {
        f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
    }
}

static void enemy_fighting(Figure *f, const formation *m)
{
    if (!m->recent_fight) {
        f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
    }
    if (f->type != FIGURE_ENEMY46_CAMEL && f->type != FIGURE_ENEMY47_ELEPHANT) {
        if (f->type == FIGURE_ENEMY48_CHARIOT || f->type == FIGURE_ENEMY52_MOUNTED_ARCHER) {
            if (city_sound_update_march_horse()) {
                sound_effect_play(SOUND_EFFECT_HORSE_MOVING);
            }
        } else {
            if (city_sound_update_march_enemy()) {
                sound_effect_play(SOUND_EFFECT_MARCHING);
            }
        }
    }
    int target_id = f->target_figure.save_id();
    if (target_id && Figure::get(target_id)->is_dead()) {
        f->target_figure.clear();
        target_id = 0;
    }
    if (target_id <= 0) {
        target_id = figure_combat_get_target_for_enemy(f->x, f->y);
        if (target_id) {
            Figure *target = Figure::get(target_id);
            f->destination_x = target->x;
            f->destination_y = target->y;
            f->target_figure.retarget(*target);
            f->target_figure_created_sequence = target->created_sequence;
            target->targeted_by_figure.retarget(*f);
            Route::remove(f);
        }
    }
    if (target_id > 0) {
        figure_movement_move_ticks(f, f->speed_multiplier);
        if (f->direction == DIR_FIGURE_AT_DESTINATION) {
            Figure *target = &f->target_figure.get();
            f->destination_x = target->x;
            f->destination_y = target->y;
            Route::remove(f);
        } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
            f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
            f->target_figure.clear();
        }
    } else {
        f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
        f->wait_ticks = 50;
    }
}

static void enemy_action(Figure *f, formation *m)
{
    city_figures_add_enemy();
    f->terrain_usage = TERRAIN_USAGE_ENEMY;
    const FormationLayoutPosition position =
        formation_layout_position(m->layout, f->index_in_formation, m->declared_capacity());
    f->formation_position_x.enemy = static_cast<signed char>(position.x);
    f->formation_position_y.enemy = static_cast<signed char>(position.y);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_148_FLEEING:
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
            figure_movement_move_ticks(f, f->speed_multiplier);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE ||
                f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_151_ENEMY_INITIAL:
            enemy_initial(f, m);
            break;
        case FIGURE_ACTION_152_ENEMY_WAITING:
            map_figure_update(f);
            break;
        case FIGURE_ACTION_153_ENEMY_MARCHING:
            enemy_marching(f, m);
            break;
        case FIGURE_ACTION_154_ENEMY_FIGHTING:
            enemy_fighting(f, m);
            break;
    }
}

static int get_direction(Figure *f)
{
    int dir;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        dir = f->attack_direction;
    } else if (f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(dir);
}

static int get_missile_direction(Figure *f, const formation *m)
{
    int dir;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        dir = f->attack_direction;
    } else if (m->missile_fired || f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(dir);
}

void figure_enemy43_spear_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_missile_direction(f, m);

    f->is_enemy_image = 1;

    switch (m->enemy_type) {
        case ENEMY_5_PERGAMUM:
        case ENEMY_6_SELEUCID:
        case ENEMY_7_ETRUSCAN:
        case ENEMY_8_GREEK:
            break;
        default:
            return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(745, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL) {
        f->select_legacy_directional_frame_image(697, dir, figure_image_missile_launcher_offset(f));
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(793);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(745, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy44_sword_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    switch (m->enemy_type) {
        case ENEMY_5_PERGAMUM:
        case ENEMY_6_SELEUCID:
        case ENEMY_9_EGYPTIAN:
            break;
        default:
            return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(545, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(593);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(545, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(449, dir, f->image_offset);
    }
}

void figure_enemy45_sword_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    switch (m->enemy_type) {
        case ENEMY_7_ETRUSCAN:
        case ENEMY_8_GREEK:
        case ENEMY_10_CARTHAGINIAN:
            break;
        default:
            return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(545, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(593);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(545, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(449, dir, f->image_offset);
    }
}

void figure_enemy_camel_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_missile_direction(f, m);

    f->is_enemy_image = 1;

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(745);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK && f->direction != DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(601, dir, 0);
    } else if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL && f->direction != DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(697, dir, figure_image_missile_launcher_offset(f));
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy_elephant_action(Figure *f)
{
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, formation_get(f->formation_id));

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(705);
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy_chariot_action(Figure *f)
{
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 3;
    enemy_action(f, formation_get(f->formation_id));

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(745);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK || f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(697, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy49_fast_sword_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 2;
    enemy_action(f, m);

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    int attack_id, corpse_id, normal_id;
    if (m->enemy_type == ENEMY_0_BARBARIAN) {
        attack_id = 393;
        corpse_id = 441;
        normal_id = 297;
    } else if (m->enemy_type == ENEMY_1_NUMIDIAN) {
        attack_id = 593;
        corpse_id = 641;
        normal_id = 449;
    } else if (m->enemy_type == ENEMY_4_GOTH) {
        attack_id = 545;
        corpse_id = 593;
        normal_id = 449;
    } else {
        return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(attack_id, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(corpse_id);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(attack_id, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(normal_id, dir, f->image_offset);
    }
}

void figure_enemy50_sword_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    if (m->enemy_type != ENEMY_2_GAUL && m->enemy_type != ENEMY_3_CELT) {
        return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(545, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(593);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(545, dir, f->image_offset / 2);
    } else {
        if (m->enemy_type == ENEMY_3_CELT) {
            // Celt swordsmen walking sprites don't match
            // direction convention - adjust for that
            dir = figure_image_offset_direction(dir, -1);
        }
        f->select_legacy_directional_frame_image(449, dir, f->image_offset);
    }
}

void figure_enemy51_spear_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 2;
    enemy_action(f, m);

    int dir = get_missile_direction(f, m);

    f->is_enemy_image = 1;

    if (m->enemy_type != ENEMY_1_NUMIDIAN) {
        return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(593, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL) {
        f->select_legacy_directional_frame_image(545, dir, figure_image_missile_launcher_offset(f));
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(641);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(593, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(449, dir, f->image_offset);
    }
}

void figure_enemy52_mounted_archer_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 3;
    enemy_action(f, m);

    int dir = get_missile_direction(f, m);

    f->is_enemy_image = 1;
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(745);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK && f->direction != DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(601, dir, 0);
    } else if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL && f->direction != DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(697, dir, figure_image_missile_launcher_offset(f));
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy53_axe_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_direction(f);

    f->is_enemy_image = 1;

    if (m->enemy_type != ENEMY_2_GAUL) {
        return;
    }
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
        f->select_legacy_directional_frame_image(697, dir, frame_offset);
    } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(745);
    } else if (f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(697, dir, f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(601, dir, f->image_offset);
    }
}

void figure_enemy_gladiator_action(Figure *f)
{
    f->terrain_usage = TERRAIN_USAGE_ANY;
    f->use_cross_country = 0;
    f->is_ghost = 0;
    figure_image_increase_offset(f, 12);
    if (scenario_gladiator_revolt_is_finished()) {
        // end of gladiator revolt: kill gladiators
        if (f->action_state != FIGURE_ACTION_149_CORPSE) {
            f->action_state = FIGURE_ACTION_149_CORPSE;
            f->wait_ticks = 0;
            f->direction = 0;
        }
    }
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            figure_image_increase_offset(f, 16);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_158_NATIVE_CREATED:
            f->image_offset = 0;
            f->wait_ticks++;
            if ((unsigned int) f->wait_ticks > 10 + (f->id() & 3)) {
                f->wait_ticks = 0;
                f->action_state = FIGURE_ACTION_159_NATIVE_ATTACKING;
                int x_tile, y_tile;
                int building_id = formation_rioter_get_target_building(&x_tile, &y_tile);
                if (building_id) {
                    f->destination_x = static_cast<unsigned char>(x_tile);
                    f->destination_y = static_cast<unsigned char>(y_tile);
                    const int grid_offset = map_grid_offset(x_tile, y_tile);
                    f->destination_building = map_building_exists_at(grid_offset) ? &map_building_at(grid_offset).main() : nullptr;
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            break;
        case FIGURE_ACTION_159_NATIVE_ATTACKING:
            city_figures_set_gladiator_revolt();
            f->terrain_usage = TERRAIN_USAGE_ENEMY;
            figure_movement_move_ticks(f, 1);
            if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                f->direction == DIR_FIGURE_REROUTE ||
                f->direction == DIR_FIGURE_LOST) {
                f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            }
            break;
    }
    int dir;
    if (f->action_state == FIGURE_ACTION_150_ATTACK || f->direction == DIR_FIGURE_ATTACK) {
        dir = f->attack_direction;
    } else if (f->direction < 8) {
        dir = f->direction;
    } else {
        dir = f->previous_tile_direction;
    }
    dir = figure_image_normalize_direction(dir);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_group(GROUP_FIGURE_GLADIATOR) + 96);
    } else if (f->action_state == FIGURE_ACTION_150_ATTACK || f->direction == DIR_FIGURE_ATTACK) {
        f->select_legacy_directional_frame_image(
            image_group(GROUP_FIGURE_GLADIATOR) + 104,
            dir,
            f->image_offset / 2);
    } else {
        f->select_legacy_directional_frame_image(image_group(GROUP_FIGURE_GLADIATOR), dir, f->image_offset);
    }
}

void figure_enemy_caesar_legionary_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    city_figures_add_imperial_soldier();
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    f->speed_multiplier = 1;
    enemy_action(f, m);

    int dir = get_direction(f);

    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
        {
            const int frame_offset = f->attack_image_offset >= 12 ? (f->attack_image_offset - 12) / 2 : 0;
            f->select_legacy_directional_frame_image(
                image_group(GROUP_FIGURE_CAESAR_LEGIONARY),
                dir,
                frame_offset);
            break;
        }
        case FIGURE_ACTION_149_CORPSE:
            f->select_legacy_corpse_image(image_group(GROUP_FIGURE_CAESAR_LEGIONARY) + 152);
            break;
        case FIGURE_ACTION_84_SOLDIER_AT_STANDARD:
            if (m->is_halted && m->layout == FORMATION_COLUMN && m->missile_attack_timeout) {
                f->select_legacy_directional_frame_image(
                    image_group(GROUP_BUILDING_FORT_LEGIONARY) + 144,
                    dir,
                    0);
            } else {
                f->select_legacy_directional_frame_image(image_group(GROUP_BUILDING_FORT_LEGIONARY), dir, 0);
            }
            break;
        default:
            if (f->direction == DIR_FIGURE_ATTACK) {
                f->select_legacy_directional_frame_image(
                    image_group(GROUP_FIGURE_CAESAR_LEGIONARY),
                    dir,
                    f->image_offset / 2);
            } else {
                f->select_legacy_directional_frame_image(
                    image_group(GROUP_FIGURE_CAESAR_LEGIONARY) + 48,
                    dir,
                    f->image_offset);
            }
            break;
    }
}

void figure_enemy_catapult_action(Figure *f)
{
    formation *m = formation_get(f->formation_id);
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    enemy_action(f, m);
    f->clear_legacy_image();
}
