#include "formation_herd.h"

#include "city/figures.h"
#include "city/sound.h"
#include "core/config.h"
#include "core/log.h"
#include "core/random.h"
#include "figure/combat.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/formation.h"
#include "figure/formation_enemy.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "map/desirability.h"
#include "map/grid.h"
#include "map/soldier_strength.h"
#include "map/terrain.h"
#include "sound/effect.h"

#include <exception>

#define FREE_TILE_SEARCH_RADIUS 4
// Look for a free tile, in the neighborhood of (x,y)
static int get_free_tile(int x, int y, int allow_negative_desirability, int *x_tile, int *y_tile)
{
    int disallowed_terrain = ~(TERRAIN_ACCESS_RAMP | TERRAIN_MEADOW);
    int tile_found = 0;
    int x_found = 0, y_found = 0;

    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, FREE_TILE_SEARCH_RADIUS, &x_min, &y_min, &x_max, &y_max);

    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            int grid_offset = map_grid_offset(xx, yy);
            if (!map_terrain_is(grid_offset, disallowed_terrain)) {
                if (map_soldier_strength_get(grid_offset)) {
                    return 0;
                }
                int desirability = map_desirability_get(grid_offset);
                if (allow_negative_desirability) {
                    if (desirability > 1) {
                        return 0;
                    }
                } else if (desirability) {
                    return 0;
                }
                tile_found = 1;
                x_found = xx;
                y_found = yy;
            }
        }
    }
    *x_tile = x_found;
    *y_tile = y_found;
    return tile_found;
}

// Try to find an open destination point (x_tile,y_tile) in the general direction, at the given distance
bool formation::find_herd_roaming_destination(const FormationHerdBehavior &behavior, int *x_tile, int *y_tile) const
{
    int allow_negative_desirability = behavior.allow_negative_desirability;
    int target_direction = (id + random_byte()) & 7;
    if (herd_direction >= DIR_0_TOP && herd_direction < DIR_8_NONE) {
        target_direction = herd_direction;
        allow_negative_desirability = 1;
    }
    // Do up to 4 checks to find a place to walk to
    for (int i = 0; i < 4; i++) {
        int x_target, y_target;
        switch (target_direction) {
            case DIR_0_TOP:
                x_target = x_home;
                y_target = y_home - behavior.roam_distance;
                break;
            case DIR_1_TOP_RIGHT:
                x_target = x_home + behavior.roam_distance;
                y_target = y_home - behavior.roam_distance;
                break;
            case DIR_2_RIGHT:
                x_target = x_home + behavior.roam_distance;
                y_target = y_home;
                break;
            case DIR_3_BOTTOM_RIGHT:
                x_target = x_home + behavior.roam_distance;
                y_target = y_home + behavior.roam_distance;
                break;
            case DIR_4_BOTTOM:
                x_target = x_home;
                y_target = y_home + behavior.roam_distance;
                break;
            case DIR_5_BOTTOM_LEFT:
                x_target = x_home - behavior.roam_distance;
                y_target = y_home + behavior.roam_distance;
                break;
            case DIR_6_LEFT:
                x_target = x_home - behavior.roam_distance;
                y_target = y_home;
                break;
            case DIR_7_TOP_LEFT:
                x_target = x_home - behavior.roam_distance;
                y_target = y_home - behavior.roam_distance;
                break;
            default:
                continue;
        }
        if (x_target <= 0) {
            x_target = 1;
        } else if (y_target <= 0) {
            y_target = 1;
        } else if (x_target >= map_grid_width() - 1) {
            x_target = map_grid_width() - 2;
        } else if (y_target >= map_grid_height() - 1) {
            y_target = map_grid_height() - 2;
        }
        // If we can find a free tile in this direction, return 1
        if (get_free_tile(x_target, y_target, allow_negative_desirability, x_tile, y_tile)) {
            return 1;
        }
        // ...otherwise turn right and try again
        target_direction = (target_direction + 2) % 8;
    }
    return 0;
}

bool formation::is_aggressive_herd() const
{
    require_definition("is_aggressive_herd");
    if (!in_use || !is_herd || formation_type_definition->spawn.role != FormationSpawnRole::Herd) {
        log_error("Formation is not a data-defined herd", formation_type_definition->key(), static_cast<int>(id));
        std::terminate();
    }
    return formation_type_definition->spawn.herd.aggressive;
}

void formation::update_herd_member(Figure &member) const
{
    if (!owns_figure(member) || !is_herd) {
        log_error("Figure is not owned by its data-defined herd", 0, static_cast<int>(member.id()));
        std::terminate();
    }
    const FormationHerdBehavior &behavior = formation_type_definition->spawn.herd;
    member.terrain_usage = TERRAIN_USAGE_ANIMAL;
    member.use_cross_country = 0;
    member.is_ghost = 0;
    city_figures_add_animal();
    figure_image_increase_offset(&member, behavior.member_animation_frames);

    auto rest = [&]() {
        member.direction = member.previous_tile_direction;
        member.action_state = FIGURE_ACTION_196_HERD_ANIMAL_AT_REST;
        member.wait_ticks = member.id() & 0x1f;
    };

    switch (member.action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(&member);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(&member);
            break;
        case FIGURE_ACTION_196_HERD_ANIMAL_AT_REST:
            member.wait_ticks++;
            if (member.wait_ticks > behavior.member_rest_delay) {
                const FormationLayoutPosition position = layout_position(member.index_in_formation);
                int member_destination_x = destination_x + position.x;
                int member_destination_y = destination_y + position.y;
                map_grid_bound(&member_destination_x, &member_destination_y);
                member.destination_x = static_cast<uint8_t>(member_destination_x);
                member.destination_y = static_cast<uint8_t>(member_destination_y);
                member.action_state = FIGURE_ACTION_197_HERD_ANIMAL_MOVING;
                member.wait_ticks = member.id() & 0x1f;
                member.roam_length = 0;
            }
            break;
        case FIGURE_ACTION_197_HERD_ANIMAL_MOVING:
            figure_movement_move_ticks(&member, behavior.member_move_speed);
            if (member.direction == DIR_FIGURE_AT_DESTINATION || member.direction == DIR_FIGURE_LOST) {
                rest();
            } else if (member.direction == DIR_FIGURE_REROUTE) {
                Route::remove(&member);
            }
            break;
        case FIGURE_ACTION_199_WOLF_ATTACKING:
            if (!behavior.aggressive) {
                log_error("Passive herd member entered the aggressive movement state", formation_type_definition->key(), static_cast<int>(member.id()));
                std::terminate();
            }
            figure_movement_move_ticks(&member, behavior.member_move_speed);
            if (member.direction == DIR_FIGURE_AT_DESTINATION) {
                const int target_id = figure_combat_get_target_for_aggressive_herd(member.x, member.y, 6);
                if (target_id) {
                    Figure *target = Figure::get(target_id);
                    member.destination_x = target->x;
                    member.destination_y = target->y;
                    member.target_figure.retarget(*target);
                    target->targeted_by_figure.retarget(member);
                    member.target_figure_created_sequence = target->created_sequence;
                    Route::remove(&member);
                } else {
                    rest();
                }
            } else if (member.direction == DIR_FIGURE_REROUTE) {
                Route::remove(&member);
            } else if (member.direction == DIR_FIGURE_LOST) {
                rest();
            }
            break;
    }
    update_herd_member_graphics(member);
}

void formation::update_herd_member_graphics(Figure &member) const
{
    if (!owns_figure(member) || !is_herd) {
        log_error("Figure graphics are not owned by a data-defined herd", 0, static_cast<int>(member.id()));
        std::terminate();
    }
    const FormationHerdBehavior &behavior = formation_type_definition->spawn.herd;
    const int graphics_direction = figure_image_direction(&member);
    if (member.action_state == FIGURE_ACTION_149_CORPSE) {
        figure_runtime_graphics_select_corpse_entry(&member, "corpse");
    } else if (member.action_state == FIGURE_ACTION_150_ATTACK) {
        const char *entry = nullptr;
        switch (behavior.combat_animation) {
            case FormationHerdCombatAnimation::Move: entry = "move"; break;
            case FormationHerdCombatAnimation::Attack: entry = "attack"; break;
            case FormationHerdCombatAnimation::Rest: entry = "rest"; break;
            default:
                log_error("Herd member has no data-defined combat animation", formation_type_definition->key(), static_cast<int>(member.id()));
                std::terminate();
        }
        figure_runtime_graphics_select_directional_entry_frame(&member, entry, graphics_direction, member.attack_image_offset / 4 + 1);
    } else if (member.action_state == FIGURE_ACTION_196_HERD_ANIMAL_AT_REST) {
        if (behavior.alternate_rest_animation && !(member.id() & 3)) {
            figure_runtime_graphics_select_directional_entry_frame(&member, "alternate_rest", graphics_direction, 0);
        } else {
            const int frame = behavior.alternate_rest_animation ? (member.wait_ticks & 0x3f) + 1 : 0;
            figure_runtime_graphics_select_directional_entry_frame(&member, "rest", graphics_direction, frame);
        }
    } else {
        figure_runtime_graphics_select_directional_entry_frame(&member, "move", graphics_direction, member.image_offset + 1);
    }
}

void formation::update_herd(bool reproduction_allowed)
{
    if (!in_use || !is_herd || is_legion) {
        return;
    }
    require_definition("update_herd");
    const FormationHerdBehavior &behavior = formation_type_definition->spawn.herd;
    const bool reproduction_enabled = reproduction_allowed && behavior.reproduction_delay > 0;
    if (!has_figures() && !reproduction_enabled) {
        return;
    }

    if (reproduction_enabled && has_open_slot() && ++herd_spawn_delay > behavior.reproduction_delay) {
        herd_spawn_delay = 0;
        if (!map_terrain_is(map_grid_offset(x, y), TERRAIN_IMPASSABLE_HERD)) {
            Figure *animal = Figure::create(figure_type_id(), x, y, DIR_0_TOP);
            animal->action_state = FIGURE_ACTION_196_HERD_ANIMAL_AT_REST;
            animal->formation_id = id;
            animal->wait_ticks = animal->id() & 0x1f;
        }
    }

    if (Figure *f = first_alive_figure()) {
        set_home(f->x, f->y);
    }

    int attacking_animals = 0;
    if (behavior.aggressive) {
        attacking_animals = count_figures_in_action(FIGURE_ACTION_150_ATTACK);
        if (missile_attack_timeout) attacking_animals = 1;
    }
    wait_ticks++;
    if (wait_ticks > behavior.roam_delay || attacking_animals) {
        wait_ticks = 0;
        if (attacking_animals) {
            set_destination(x_home, y_home);
            move_herd_animals(attacking_animals);
        } else {
            int x_tile, y_tile;
            if (find_herd_roaming_destination(behavior, &x_tile, &y_tile)) {
                herd_direction = DIR_8_NONE;
                if (formation_enemy_move_formation_to(this, x_tile, y_tile, &x_tile, &y_tile)) {
                    set_destination(x_tile, y_tile);
                    if (behavior.movement_sound == FormationHerdMovementSound::WolfHowl &&
                        city_sound_update_herd_movement()) {
                        sound_effect_play(SOUND_EFFECT_WOLF_HOWL);
                    }
                    move_herd_animals(0);
                }
            }
        }
    }
}

void formation_herd_update(void)
{
    const bool reproduction_allowed = !config_get(CONFIG_GP_CH_DISABLE_INFINITE_WOLVES_SPAWNING);
    for (int i = 1; i < formation_count(); i++) {
        formation_get(i)->update_herd(reproduction_allowed);
    }
}
