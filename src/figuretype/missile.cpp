#include "missile.h"

#include "city/view.h"
#include "core/image.h"
#include "core/log.h"
#include "figure/formation.h"
#include "figure/figure_runtime_api.h"
#include "figure/movement.h"
#include "figure/properties.h"
#include "figure/sound.h"
#include "figure/unit_type.h"
#include "map/figure.h"
#include "map/point.h"
#include "sound/effect.h"

#include <exception>

static void select_projectile_graphics(Figure &projectile)
{
    int group_offset = 0;
    switch (projectile.type) {
        case FIGURE_ARROW:
        case FIGURE_FRIENDLY_ARROW:
            group_offset = 16;
            break;
        case FIGURE_BOLT:
            group_offset = 32;
            break;
        case FIGURE_CATAPULT_MISSILE:
            projectile.clear_legacy_image();
            return;
        case FIGURE_JAVELIN:
        case FIGURE_SPEAR:
            break;
        default:
            log_error("Cannot select projectile graphics for a non-projectile figure", 0, projectile.type);
            std::terminate();
    }
    const int direction = (16 + projectile.direction - 2 * city_view_orientation()) % 16;
    projectile.select_legacy_frame_image(image_group(GROUP_FIGURE_MISSILE) + group_offset, direction);
}

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
        select_projectile_graphics(*f);
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
    if (f->is_herd()) {
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
    const formation *launcher_formation = launcher ? formation_get(launcher->formation_id) : nullptr;
    const bool launcher_has_formation = launcher && launcher_formation && launcher_formation->owns_figure(*launcher);
    if (ranged) {
        const UnitRangedAbility::Projectile projectile =
            ranged->projectile_for_enemy(launcher_has_formation ? launcher_formation->enemy_type : ENEMY_UNDEFINED);
        if (projectile.type == static_cast<figure_type>(missile.type)) {
            damage = projectile.damage;
        }
    }
    return launcher_has_formation ?
        launcher_formation->modified_combat_value(FormationCombatStat::MissileDamage, damage) :
        damage;
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
    const bool target_has_formation = m && m->owns_figure(*target);
    if (target_has_formation && target_melee &&
        (m->uses_layout("single_line_1") || m->uses_layout("single_line_2"))) {
        damage_inflicted -= target_melee->single_line_missile_defense_bonus;
    }
    if (damage_inflicted < 0) {
        damage_inflicted = 0;
    }
    if (target_has_formation && m->is_halted && m->uses_layout("column")) {
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
        if (target_has_formation) {
            m->update_morale_after_death();
        }
    }
    f->state = FIGURE_STATE_DEAD;
    // for missiles: last_destination_id contains the figure who shot it
    Figure *launcher = Figure::get(f->last_destination_id);
    formation *launcher_formation = formation_get(launcher->formation_id);
    const int missile_formation = launcher_formation && launcher_formation->owns_figure(*launcher) ?
        launcher_formation->id : 0;
    if (target_has_formation) {
        m->record_missile_attack(missile_formation);
    }
}

static bool projectile_targets_citizens(figure_type type)
{
    switch (type) {
        case FIGURE_ARROW:
        case FIGURE_SPEAR:
        case FIGURE_CATAPULT_MISSILE:
            return true;
        case FIGURE_FRIENDLY_ARROW:
        case FIGURE_JAVELIN:
        case FIGURE_BOLT:
            return false;
        default:
            log_error("Cannot choose targets for a non-projectile figure", 0, type);
            std::terminate();
    }
}

static sound_effect_type projectile_impact_sound(figure_type type)
{
    switch (type) {
        case FIGURE_ARROW:
        case FIGURE_FRIENDLY_ARROW:
            return SOUND_EFFECT_ARROW_HIT;
        case FIGURE_JAVELIN:
        case FIGURE_SPEAR:
            return SOUND_EFFECT_JAVELIN;
        case FIGURE_BOLT:
            return SOUND_EFFECT_BALLISTA_HIT_PERSON;
        case FIGURE_CATAPULT_MISSILE:
            return SOUND_EFFECT_BALLISTA_HIT_GROUND;
        default:
            log_error("Cannot choose impact sound for a non-projectile figure", 0, type);
            std::terminate();
    }
}

void figure_projectile_action(Figure *f)
{
    constexpr int kLifetimeActions = 120;
    constexpr int kMovementTicksPerAction = 4;
    const figure_type type = static_cast<figure_type>(f->type);
    f->use_cross_country = 1;
    f->progress_on_tile++;
    if (f->progress_on_tile > kLifetimeActions) {
        f->state = FIGURE_STATE_DEAD;
    }
    const bool arrived = figure_movement_move_ticks_cross_country(f, kMovementTicksPerAction) != 0;
    const int target_id = projectile_targets_citizens(type) ?
        get_citizen_on_tile(f->grid_offset) : get_non_citizen_on_tile(f->grid_offset);
    if (target_id) {
        missile_hit_target(f, target_id);
        sound_effect_play(projectile_impact_sound(type));
    } else if (arrived) {
        f->state = FIGURE_STATE_DEAD;
        if (type == FIGURE_BOLT) sound_effect_play(SOUND_EFFECT_BALLISTA_HIT_GROUND);
    }
    select_projectile_graphics(*f);
}

