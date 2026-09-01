#include "missile.h"

#include "city/view.h"
#include "core/image.h"
#include "figure/formation.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "figure/properties.h"
#include "figure/sound.h"
#include "figure/unit_type.h"
#include "map/figure.h"
#include "map/point.h"
#include "sound/effect.h"

static const int CLOUD_TILE_OFFSETS[] = { 0, 0, 0, 1, 1, 2 };

static constexpr int CLOUD_CC_OFFSETS[] = {
    0,
    FIGURE_CROSS_COUNTRY_TILE_UNITS / 2,
    FIGURE_CROSS_COUNTRY_TILE_UNITS - 1,
    FIGURE_CROSS_COUNTRY_TILE_UNITS / 2,
    FIGURE_CROSS_COUNTRY_TILE_UNITS - 1,
    FIGURE_CROSS_COUNTRY_TILE_UNITS / 2
};

static const int CLOUD_SPEED[] = {
    1, 2, 1, 3, 2, 1, 3, 2, 1, 1, 2, 1, 2, 1, 3, 1
};

static const map_point CLOUD_DIRECTION[] = {
    {0, -6}, {-2, -5}, {-4, -4}, {-5, -2}, {-6, 0}, {-5, -2}, {-4, -4}, {-2, -5},
    {0, -6}, {-2, -5}, {-4, -4}, {-5, -2}, {-6, 0}, {-5, -2}, {-4, -4}, {-2, -5}
};

static const int CLOUD_IMAGE_OFFSETS[] = {
    0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2,
    2, 2, 2, 2, 3, 3, 3, 4, 4, 5, 6, 7
};

void figure_create_explosion_cloud(int x, int y, int size, int alt_sound)
{
    int tile_offset = CLOUD_TILE_OFFSETS[size];
    int cc_offset = CLOUD_CC_OFFSETS[size];
    for (int i = 0; i < 16; i++) {
        Figure *f = Figure::create(FIGURE_EXPLOSION,
            x + tile_offset, y + tile_offset, DIR_0_TOP);
        if (f->id()) {
    f->cross_country_x += cc_offset;
    f->cross_country_y += cc_offset;
            f->destination_x = static_cast<unsigned char>(f->destination_x + CLOUD_DIRECTION[i].x);
            f->destination_y = static_cast<unsigned char>(f->destination_y + CLOUD_DIRECTION[i].y);
            figure_movement_set_cross_country_direction(f,
                f->cross_country_x, f->cross_country_y,
                figure_movement_tile_to_cross_country(f->destination_x) + cc_offset,
                figure_movement_tile_to_cross_country(f->destination_y) + cc_offset, 0);
            f->speed_multiplier = static_cast<unsigned char>(CLOUD_SPEED[i]);
        }
    }
    sound_effect_play(alt_sound ? SOUND_EFFECT_BUILD : SOUND_EFFECT_EXPLOSION);
}

void figure_create_missile(int figure_id, int x, int y, int x_dst, int y_dst, figure_type type)
{
    Figure *f = Figure::create(type, x, y, DIR_0_TOP);
    Figure *launcher = Figure::get(figure_id);
    if (f->id()) {
        if (launcher->type == FIGURE_BALLISTA || launcher->type == FIGURE_WATCHTOWER_ARCHER) {
            f->missile_height = 60;
        } else {
            f->missile_height = 10;
        }
        f->set_last_destination_figure_id(static_cast<unsigned int>(figure_id));
        f->destination_x = static_cast<unsigned char>(x_dst);
        f->destination_y = static_cast<unsigned char>(y_dst);
        figure_movement_set_cross_country_direction(
            f, f->cross_country_x, f->cross_country_y,
            figure_movement_tile_to_cross_country(x_dst),
            figure_movement_tile_to_cross_country(y_dst), 1);
    }
}

static int is_citizen(Figure *f)
{
    const figure_properties *target_props = figure_properties_for_type(static_cast<figure_type>(f->type));
    const figure_category_mask category = target_props->category;
    if (f->action_state != FIGURE_ACTION_149_CORPSE) {
        if (category & FIGURE_CATEGORY_CITIZEN || category & FIGURE_CATEGORY_CRIMINAL) {
            return f->id();
        }
    }
    return 0;
}

static int get_citizen_on_tile(int grid_offset)
{
    return map_figure_foreach_until(grid_offset, is_citizen);
}

static int is_non_citizen(Figure *f)
{
    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        return 0;
    }
    if (f->is_enemy()) {
        return f->id();
    }
    if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
        return f->id();
    }
    if (f->type == FIGURE_WOLF || f->type == FIGURE_SHEEP || f->type == FIGURE_ZEBRA) {
        return f->id();
    }
    return 0;
}

static int get_non_citizen_on_tile(int grid_offset)
{
    return map_figure_foreach_until(grid_offset, is_non_citizen);
}

void figure_explosion_cloud_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 44) {
        f->state = FIGURE_STATE_DEAD;
    }
    figure_movement_move_ticks_cross_country(f, f->speed_multiplier);
    figure_explosion_cloud_update_graphics(f);
}

void figure_explosion_cloud_update_graphics(Figure *f)
{
    const int image_offset = f->progress_on_tile < 48 ? CLOUD_IMAGE_OFFSETS[f->progress_on_tile / 2] : 7;
    figure_runtime_graphics_select_default_entry_frame(f, "cloud", image_offset + 1);
}

static int missile_damage(const Figure &missile)
{
    int damage = figure_properties_for_type(
        static_cast<figure_type>(missile.type))->missile_attack_value;
    const Figure *launcher = Figure::get(missile.last_destination_id);
    const UnitType *launcher_unit = launcher ?
        unit_type_registry_impl::find_unit_type(static_cast<figure_type>(launcher->type)) : nullptr;
    const UnitRangedAbility *ranged = launcher_unit ? launcher_unit->ranged_ability() : nullptr;
    if (ranged && launcher) {
        const formation *launcher_formation = formation_get(launcher->formation_id);
        const UnitRangedAbility::Projectile projectile =
            ranged->projectile_for_enemy(launcher_formation->enemy_type);
        if (projectile.type == static_cast<figure_type>(missile.type)) {
            damage = projectile.damage;
        }
    }
    return damage;
}

static void missile_hit_target(Figure *f, int target_id)
{
    Figure *target = Figure::get(target_id);
    const figure_type target_type = static_cast<figure_type>(target->type);
    int max_damage = figure_damage_limit_for_type(target_type);
    int damage_inflicted = missile_damage(*f) - figure_missile_defense_for_type(target_type);
    formation *m = formation_get(target->formation_id);
    const UnitType *target_unit = unit_type_registry_impl::find_unit_type(target_type);
    const UnitMeleeAbility *target_melee = target_unit ? target_unit->melee_ability() : nullptr;
    if (target_melee &&
        (m->uses_layout("single_line_1") || m->uses_layout("single_line_2"))) {
        damage_inflicted -= target_melee->single_line_missile_defense_bonus;
    }
    if (damage_inflicted < 0) {
        damage_inflicted = 0;
    }
    if (m->is_halted && m->uses_layout("column")) {
        if (target_melee && target_melee->column_missile_damage > 0) {
            damage_inflicted = target_melee->column_missile_damage;
        }
    }
    int target_damage = damage_inflicted + target->damage;
    if (target_damage <= max_damage) {
        target->damage = static_cast<unsigned char>(target_damage);
    } else { // kill target
        target->damage = static_cast<unsigned char>(max_damage + 1);
        target->action_state = FIGURE_ACTION_149_CORPSE;
        target->wait_ticks = 0;
        figure_play_die_sound(target);
        formation_update_morale_after_death(m);
    }
    f->state = FIGURE_STATE_DEAD;
    // for missiles: last_destination_id contains the figure who shot it
    int missile_formation = Figure::get(f->last_destination_id)->formation_id;
    formation_record_missile_attack(m, missile_formation);
}

void figure_arrow_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(SOUND_EFFECT_ARROW_HIT);
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
    }
    int dir = (16 + f->direction - 2 * city_view_orientation()) % 16;
    f->select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE) + 16, dir);
}

void figure_spear_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(SOUND_EFFECT_JAVELIN);
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
    }
    int dir = (16 + f->direction - 2 * city_view_orientation()) % 16;
    f->select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE), dir);
}

void figure_friendly_arrow_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_non_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(SOUND_EFFECT_ARROW_HIT);
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
    }
    int dir = (16 + f->direction - 2 * city_view_orientation()) % 16;
    f->select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE) + 16, dir);
}


void figure_javelin_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_non_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(SOUND_EFFECT_JAVELIN);
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
    }
    int dir = (16 + f->direction - 2 * city_view_orientation()) % 16;
    f->select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE), dir);
}

void figure_bolt_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_non_citizen_on_tile(f->grid_offset);
    if (target_id) {
        Figure *target = Figure::get(target_id);
        const figure_type target_type = static_cast<figure_type>(target->type);
        int max_damage = figure_damage_limit_for_type(target_type);
        int damage_inflicted = missile_damage(*f) - figure_missile_defense_for_type(target_type);
        if (damage_inflicted < 0) {
            damage_inflicted = 0;
        }
        int target_damage = damage_inflicted + target->damage;
        if (target_damage <= max_damage) {
            target->damage = static_cast<unsigned char>(target_damage);
        } else { // kill target
            target->damage = static_cast<unsigned char>(max_damage + 1);
            target->action_state = FIGURE_ACTION_149_CORPSE;
            target->wait_ticks = 0;
            figure_play_die_sound(target);
            formation_update_morale_after_death(formation_get(target->formation_id));
        }
        sound_effect_play(SOUND_EFFECT_BALLISTA_HIT_PERSON);
        f->state = FIGURE_STATE_DEAD;
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
        sound_effect_play(SOUND_EFFECT_BALLISTA_HIT_GROUND);
    }
    int dir = (16 + f->direction - 2 * city_view_orientation()) % 16;
    f->select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE) + 32, dir);
}

void figure_catapult_missile_action(Figure *f)
{
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > 120) {
        f->state = FIGURE_STATE_DEAD;
    }
    int should_die = figure_movement_move_ticks_cross_country(f, 4);
    int target_id = get_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(SOUND_EFFECT_BALLISTA_HIT_GROUND);
    } else if (should_die) {
        f->state = FIGURE_STATE_DEAD;
    }
    f->clear_legacy_image();
}

