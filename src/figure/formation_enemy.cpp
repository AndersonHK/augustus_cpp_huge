#include "formation_enemy.h"

#include "building/building.h"
#include "building/HousingProfileDef.h"
#include "building/housing_profile_registry.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/figures.h"
#include "city/god.h"
#include "city/message.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/random.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/route.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/point.h"
#include "map/soldier_strength.h"
#include "map/terrain.h"

#include <optional>
#include <vector>

constexpr int INFINITE_DISTANCE = 10000;

enum class TargetKind {
    End,
    Type,
    Farm,
    Housing
};

struct TargetSpec {
    TargetKind kind;
    const char *text_id;
    int level;
};

constexpr TargetSpec target_type(const char *id)
{
    return {TargetKind::Type, id, 0};
}

constexpr TargetSpec target_farm()
{
    return {TargetKind::Farm, nullptr, 0};
}

constexpr TargetSpec target_housing_descending(int min_level = 0)
{
    return {TargetKind::Housing, nullptr, min_level};
}

constexpr TargetSpec target_end()
{
    return {TargetKind::End, nullptr, 0};
}

static const TargetSpec ENEMY_ATTACK_PRIORITY[4][16] = {
    {
        target_type("granary"), target_type("warehouse"), target_type("market"),
        target_farm(), target_end()
    },
    {
        target_type("senate"), target_type("senate_1_unused"),
        target_type("forum_2_unused"), target_type("forum"), target_end()
    },
    {
        target_type("governors_palace"), target_type("governors_villa"), target_type("governors_house"),
        target_housing_descending(), target_end()
    },
    {
        target_type("military_academy"), target_type("prefecture"), target_end()
    }
};

static const TargetSpec RIOTER_ATTACK_PRIORITY[] = {
    target_type("governors_palace"),
    target_type("governors_villa"),
    target_type("governors_house"),
    target_type("amphitheater"),
    target_type("theater"),
    target_type("hospital"),
    target_type("academy"),
    target_type("bathhouse"),
    target_type("library"),
    target_type("school"),
    target_type("doctor"),
    target_type("gladiator_school"),
    target_type("actor_colony"),
    target_type("chariot_maker"),
    target_type("lion_house"),
    target_type("barber"),
    target_type("tavern"),
    target_type("arena"),
    target_type("horse_statue"),
    target_type("legion_statue"),
    target_type("large_statue"),
    target_type("medium_statue"),
    target_type("obelisk"),
    target_housing_descending(HOUSE_LARGE_VILLA),
    target_end()
};

static int find_reachable_soldier_strength(
    const Route::TerrainQuery &route,
    int x,
    int y,
    int radius,
    int *out_x,
    int *out_y)
{
    int x_min, y_min, x_max, y_max;
    map_grid_get_area(x, y, 1, radius, &x_min, &y_min, &x_max, &y_max);

    int max_value = 0;
    int max_tile_x = 0;
    int max_tile_y = 0;
    for (int yy = y_min; yy <= y_max; yy++) {
        for (int xx = x_min; xx <= x_max; xx++) {
            const int grid_offset = map_grid_offset(xx, yy);
            const int strength = map_soldier_strength_get(grid_offset);
            if (route.canReach(grid_offset) && strength > max_value) {
                max_value = strength;
                max_tile_x = xx;
                max_tile_y = yy;
            }
        }
    }
    if (max_value > 0) {
        *out_x = max_tile_x;
        *out_y = max_tile_y;
        return 1;
    }
    return 0;
}

static bool matches_target(const Building &candidate, const TargetSpec &target)
{
    return target.kind == TargetKind::Farm ?
        candidate.type && candidate.type->is_farm() :
        candidate.matches(target.text_id);
}

static Building *select_active_building(
    const TargetSpec &target,
    const std::optional<map_point> &origin)
{
    Building *selected = nullptr;
    int selected_distance = INFINITE_DISTANCE;
    Building::for_each([&](Building *candidate) {
        if (!candidate || !candidate->is_in_use() || !matches_target(*candidate, target) ||
            (selected && !origin)) {
            return;
        }
        if (!origin) {
            selected = candidate;
            return;
        }
        const int distance = calc_maximum_distance(origin->x, origin->y, candidate->x(), candidate->y());
        if (distance < selected_distance) {
            selected = candidate;
            selected_distance = distance;
        }
    });
    return selected;
}

static Building *select_active_housing(
    int level,
    const std::optional<map_point> &origin)
{
    Building *selected = nullptr;
    int selected_distance = INFINITE_DISTANCE;
    Building::for_each(BuildingRuntimeList::Housing, [&](Building *candidate) {
        const building_type_registry_impl::HousingProfileDef *profile =
            candidate && candidate->Housing ? candidate->Housing->definition().profile : nullptr;
        if (!candidate || !candidate->is_in_use() || !profile ||
            profile->compatibility_level != level || (selected && !origin)) {
            return;
        }
        if (!origin) {
            selected = candidate;
            return;
        }
        const int distance = calc_maximum_distance(origin->x, origin->y, candidate->x(), candidate->y());
        if (distance < selected_distance) {
            selected = candidate;
            selected_distance = distance;
        }
    });
    return selected;
}

static Building *select_housing_descending(
    int min_level,
    const std::optional<map_point> &origin)
{
    for (int index = building_type_registry_impl::housing_profile_compatibility_level_count() - 1;
        index >= 0;
        --index) {
        const int level = building_type_registry_impl::housing_profile_compatibility_level_at(index);
        if (level >= min_level) {
            if (Building *candidate = select_active_housing(level, origin)) {
                return candidate;
            }
        }
    }
    return nullptr;
}

static Building *select_priority_target(
    const TargetSpec *priority_order,
    const std::optional<map_point> &origin)
{
    for (int index = 0; priority_order[index].kind != TargetKind::End; ++index) {
        const TargetSpec &target = priority_order[index];
        Building *candidate = nullptr;
        if (target.kind == TargetKind::Housing) {
            candidate = select_housing_descending(target.level, origin);
        } else {
            candidate = select_active_building(target, origin);
        }
        if (candidate) {
            return candidate;
        }
    }
    return nullptr;
}

int formation_rioter_get_target_building(int *x_tile, int *y_tile)
{
    Building *best_building = select_priority_target(RIOTER_ATTACK_PRIORITY, std::nullopt);
    if (!best_building) {
        return 0;
    }
    *x_tile = best_building->x();
    *y_tile = best_building->y();
    return best_building->id;
}

int formation_rioter_get_target_building_for_robbery(int x, int y, int *x_tile, int *y_tile)
{
    Building *best_building = nullptr;
    int closest = INFINITE_DISTANCE;

    static const char *const building_targets[] = { "senate", "forum" };
    for (int i = 0; i < 2; i++) {
        Building::for_each([&](Building *candidate) {
            if (!candidate || !candidate->is_in_use() || !candidate->matches(building_targets[i])) {
                return;
            }
            const int distance = calc_maximum_distance(x, y, candidate->x(), candidate->y());
            if (distance >= 150) {
                return;
            }
            if (distance < closest) {
                closest = distance;
                best_building = candidate;
            }
        });
    }
    if (!best_building) {
        return 0;
    }

    *x_tile = best_building->road_access_x();
    *y_tile = best_building->road_access_y();

    return best_building->id;
}

static int set_enemy_target_building(formation *m)
{
    int attack = m->attack_type;
    if (attack == FORMATION_ATTACK_RANDOM) {
        attack = random_byte() & 3;
    }
    const map_point origin = {m->x_home, m->y_home};
    Building *best_building = select_priority_target(ENEMY_ATTACK_PRIORITY[attack], origin);

    if (!best_building) {
        // no target buildings left: take rioter attack priority
        best_building = select_priority_target(RIOTER_ATTACK_PRIORITY, origin);
    }
    if (best_building) {
        Building *best_object = best_building->Composition ?
            best_building->Composition->owner() : best_building;
        if (!best_object) {
            return 0;
        }
        if (best_object->matches("warehouse")) {
            if (!best_object->Composition || !best_object->Composition->is_owner() ||
                !best_object->Composition->complete() || best_object->Composition->children().empty()) {
                return 0;
            }
            Building *destination = best_object->Composition->children().front()->building();
            formation_set_destination_building(m, best_building->x() + 1, best_building->y(), destination);
        } else {
            formation_set_destination_building(m, best_building->x(), best_building->y(), best_object);
        }
    }
    return best_building != nullptr;
}


static int get_structures_on_native_land(int *dst_x, int *dst_y)
{
    int meeting_x, meeting_y;
    city_buildings_main_native_meeting_center(&meeting_x, &meeting_y);

    const char *const native_buildings[] = {
        "native_meeting",
        "native_watchtower",
        "native_hut",
        "native_hut_alt"
    };
    int min_distance = INFINITE_DISTANCE;

    for (int i = 0;
        i < sizeof(native_buildings) / sizeof(native_buildings[0]) && min_distance == INFINITE_DISTANCE;
        ++i) {
        Building::for_each([&](Building *candidate) {
            if (!candidate || !candidate->is_in_use() || !candidate->matches(native_buildings[i])) {
                return;
            }
            const building_type_registry_impl::BuildingType *definition = candidate->type;
            int width = definition ? definition->placement_width(candidate->orientation()) : 0;
            int height = definition ? definition->placement_height(candidate->orientation()) : 0;
            int size = width > height ? width : height;
            if (size <= 0) {
                size = 1;
            }
            int radius = size * 3;
            int x_min, y_min, x_max, y_max;
            map_grid_get_area(candidate->x(), candidate->y(), size, radius, &x_min, &y_min, &x_max, &y_max);
            for (int yy = y_min; yy <= y_max; yy++) {
                for (int xx = x_min; xx <= x_max; xx++) {
                    if (map_terrain_is(map_grid_offset(xx, yy), TERRAIN_AQUEDUCT | TERRAIN_WALL | TERRAIN_GARDEN)) {
                        int distance = calc_maximum_distance(meeting_x, meeting_y, xx, yy);
                        if (distance < min_distance) {
                            min_distance = distance;
                            *dst_x = xx;
                            *dst_y = yy;
                        }
                    }
                }
            }
        });
    }
    return min_distance < INFINITE_DISTANCE;
}

static void set_native_target_building(formation *m)
{
    int meeting_x, meeting_y;
    city_buildings_main_native_meeting_center(&meeting_x, &meeting_y);
    Building *min_building = nullptr;
    int min_distance = INFINITE_DISTANCE;
    static const char *const excluded_types[] = {
        "mission_post",
        "native_hut",
        "native_hut_alt",
        "native_crops",
        "native_meeting",
        "native_monument",
        "native_watchtower",
        "native_decor",
        "warehouse",
        "fort_archers",
        "fort_legionaries",
        "fort_javelin",
        "fort_mounted",
        "fort_swords",
        "fort_ground",
        "roadblock",
        "garden_wall_gate",
        "panelled_garden_wall_gate",
        "looped_garden_wall_gate",
        "hedge_gate_dark",
        "hedge_gate_light",
        "low_bridge",
        "ship_bridge"
    };
    Building::for_each([&](Building *target) {
        if (!target || !target->is_in_use() || target->matches("gardens")) {
            return;
        }
        for (const char *excluded_type : excluded_types) {
            if (target->matches(excluded_type)) {
                return;
            }
        }
        const int distance = calc_maximum_distance(meeting_x, meeting_y, target->x(), target->y());
        if (distance < min_distance) {
            min_building = target;
            min_distance = distance;
        }
    });
    if (min_building) {
        formation_set_destination_building(m, min_building->x(), min_building->y(), min_building);
    } else {
        int dst_x = 0;
        int dst_y = 0;
        int has_target = get_structures_on_native_land(&dst_x, &dst_y);
        if (has_target) {
            formation_set_destination_building(m, dst_x, dst_y, nullptr);
        } else {
            formation_retreat(m);
        }
    }
}

int formation_enemy_move_formation_to(const formation *m, int x, int y, int *x_tile, int *y_tile)
{
    const std::vector<int> figure_offsets = m->layout_grid_offsets();
    const int figure_count = static_cast<int>(figure_offsets.size());
    if (figure_count <= 0) {
        return 0;
    }
    const Route::TerrainQuery route = Route::TerrainQuery::enemyLandFrom({ x, y }, 600);
    for (int r = 0; r <= 10; r++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(x, y, 1, r, &x_min, &y_min, &x_max, &y_max);
        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int can_move = 1;
                for (int fig = 0; fig < figure_count; fig++) {
                    int grid_offset = map_grid_offset(xx, yy) + figure_offsets[fig];
                    if (!map_grid_is_valid_offset(grid_offset)) {
                        can_move = 0;
                        break;
                    }
                    if (map_terrain_is(grid_offset, TERRAIN_IMPASSABLE_ENEMY)) {
                        can_move = 0;
                        break;
                    }
                    if (!route.canReach(grid_offset)) {
                        can_move = 0;
                        break;
                    }
                    if (map_has_figure_at(grid_offset) &&
                        Figure::get(map_figure_at(grid_offset))->formation_id != m->id) {
                        can_move = 0;
                        break;
                    }
                }
                if (can_move) {
                    *x_tile = xx;
                    *y_tile = yy;
                    return 1;
                }
            }
        }
    }
    return 0;
}

static void mars_kill_enemies(void)
{
    int to_kill = city_god_spirit_of_mars_power();
    if (to_kill <= 0) {
        return;
    }
    int grid_offset = 0;
    for (unsigned int i = 1; i < Figure::count() && to_kill > 0; i++) {
        Figure *f = Figure::get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (f->is_enemy() && f->type != FIGURE_ENEMY54_GLADIATOR) {
            f->action_state = FIGURE_ACTION_149_CORPSE;
            to_kill--;
            if (!grid_offset) {
                grid_offset = f->grid_offset;
            }
        }
    }
    city_god_spirit_of_mars_mark_used();
    city_message_post(1, MESSAGE_SPIRIT_OF_MARS, 0, grid_offset);
}

static void get_layout_orientation_offset(const enemy_army *army, const formation *m, int *x_offset, int *y_offset)
{
    const FormationLayoutDef *layout = army->layout_definition ?
        army->layout_definition : formation_layout_registry_impl::find_layout("column");
    const FormationLayoutPosition offset = layout ?
        layout->army_offset(m->orientation / 2, m->enemy_legion_index) : FormationLayoutPosition{0, 0};
    *x_offset = offset.x;
    *y_offset = offset.y;
}

static void update_enemy_movement(formation *m, int roman_distance)
{
    const enemy_army *army = enemy_army_get(m->invasion_id);
    formation_state *state = &m->enemy_state;
    int regroup = 0;
    int halt = 0;
    int pursue_target = 0;
    int advance = 0;
    int target_formation_id = 0;
    if (m->missile_fired) {
        halt = 1;
    } else if (m->missile_attack_timeout) {
        pursue_target = 1;
        target_formation_id = m->missile_attack_formation_id;
    } else if (m->wait_ticks < 32) {
        regroup = 1;
        state->duration_advance = 4;
    } else if (army->ignore_roman_soldiers) {
        halt = 0;
        regroup = 0;
        advance = 1;
    } else {
        int halt_duration, advance_duration, regroup_duration;
        if (army->layout_definition &&
            (army->layout_definition->matches_key("enemy_mob") ||
                army->layout_definition->matches_key("enemy_12"))) {
            switch (m->enemy_legion_index) {
                case 0:
                case 1:
                    regroup_duration = 2;
                    advance_duration = 4;
                    halt_duration = 2;
                    break;
                case 2:
                case 3:
                    regroup_duration = 2;
                    advance_duration = 5;
                    halt_duration = 3;
                    break;
                default:
                    regroup_duration = 2;
                    advance_duration = 6;
                    halt_duration = 4;
                    break;
            }
            if (!roman_distance) {
                advance_duration += 6;
                halt_duration--;
                regroup_duration--;
            }
        } else {
            if (roman_distance) {
                regroup_duration = 6;
                advance_duration = 4;
                halt_duration = 2;
            } else {
                regroup_duration = 1;
                advance_duration = 12;
                halt_duration = 1;
            }
        }
        if (state->duration_halt) {
            state->duration_advance = 0;
            state->duration_regroup = 0;
            halt = 1;
            state->duration_halt--;
            if (state->duration_halt <= 0) {
                state->duration_regroup = regroup_duration;
                m->reset_non_combat_figures_action(FIGURE_ACTION_151_ENEMY_INITIAL);
                regroup = 0;
                halt = 1;
            }
        } else if (state->duration_regroup) {
            state->duration_advance = 0;
            state->duration_halt = 0;
            regroup = 1;
            state->duration_regroup--;
            if (state->duration_regroup <= 0) {
                state->duration_advance = advance_duration;
                m->reset_non_combat_figures_action(FIGURE_ACTION_151_ENEMY_INITIAL);
                advance = 1;
                regroup = 0;
            }
        } else {
            state->duration_regroup = 0;
            state->duration_halt = 0;
            advance = 1;
            state->duration_advance--;
            if (state->duration_advance <= 0) {
                state->duration_halt = halt_duration;
                m->reset_non_combat_figures_action(FIGURE_ACTION_151_ENEMY_INITIAL);
                halt = 1;
                advance = 0;
            }
        }
    }

    if (m->wait_ticks > 32) {
        mars_kill_enemies();
    }
    if (halt) {
        formation_set_destination(m, m->x_home, m->y_home);
    } else if (pursue_target) {
        if (target_formation_id > 0) {
            const formation *target = formation_get(target_formation_id);
            if (target->has_figures()) {
                formation_set_destination(m, target->x_home, target->y_home);
            }
        } else {
            formation_set_destination(m, army->destination_x, army->destination_y);
        }
    } else if (regroup) {
        int x_offset, y_offset;
        get_layout_orientation_offset(army, m, &x_offset, &y_offset);
        x_offset += army->home_x;
        y_offset += army->home_y;
        map_grid_bound(&x_offset, &y_offset);
        int x_tile, y_tile;
        if (formation_enemy_move_formation_to(m, x_offset, y_offset, &x_tile, &y_tile)) {
            formation_set_destination(m, x_tile, y_tile);
        }
    } else if (advance) {
        int x_offset, y_offset;
        get_layout_orientation_offset(army, m, &x_offset, &y_offset);
        x_offset += army->destination_x;
        y_offset += army->destination_y;
        map_grid_bound(&x_offset, &y_offset);
        int x_tile, y_tile;
        if (formation_enemy_move_formation_to(m, x_offset, y_offset, &x_tile, &y_tile)) {
            formation_set_destination(m, x_tile, y_tile);
        }
    }
}

static void update_enemy_formation(formation *m, int *roman_distance)
{
    enemy_army *army = enemy_army_get_editable(m->invasion_id);
    if (enemy_army_is_stronger_than_legions()) {
        if (!m->has_figure_type(FIGURE_FORT_JAVELIN)) {
            army->ignore_roman_soldiers = 1;
        }
    }
    formation_decrease_monthly_counters(m);
    if (city_figures_soldiers() <= 0) {
        formation_clear_monthly_counters(m);
    }
    if (m->has_figure_attacking_live_legion()) {
        formation_record_fight(m);
    }
    if (formation_has_low_morale(m)) {
        m->set_non_combat_figures_action(FIGURE_ACTION_148_FLEEING, true);
        return;
    }
    if (Figure *f = m->first_alive_figure()) {
        formation_set_home(m, f->x, f->y);
    }
    if (!army->formation_id) {
        army->formation_id = m->id;
        army->home_x = m->x_home;
        army->home_y = m->y_home;
        army->layout_definition = m->layout_type();
        *roman_distance = 0;
        const Route::TerrainQuery enemy_route =
            Route::TerrainQuery::enemyLandFrom({ m->x_home, m->y_home }, 300, 100000);
        int x_tile, y_tile;
        if (find_reachable_soldier_strength(enemy_route, m->x_home, m->y_home, 16, &x_tile, &y_tile)) {
            *roman_distance = 1;
        } else if (find_reachable_soldier_strength(enemy_route, m->x_home, m->y_home, 32, &x_tile, &y_tile)) {
            *roman_distance = 2;
        }
        if (army->ignore_roman_soldiers) {
            *roman_distance = 0;
        }
        if (*roman_distance == 1) {
            // attack roman legion
            army->destination_x = x_tile;
            army->destination_y = y_tile;
            army->destination_building_id = 0;
        } else {
            if (!set_enemy_target_building(m) && !army->started_retreating && m->is_fully_in_city()) {
                city_message_post(1, MESSAGE_ENEMIES_LEAVING, 0, 0);
                army->started_retreating = 1;
            }
            army->destination_x = m->destination_x;
            army->destination_y = m->destination_y;
            army->destination_building_id = m->destination_building_id;
        }
    }
    m->enemy_legion_index = army->num_legions++;
    m->wait_ticks++;
    if (!army->started_retreating) {
        if (army->destination_building_id) {
            Building *destination = Building::get(static_cast<unsigned int>(army->destination_building_id));
            formation_set_destination_building(m, army->destination_x, army->destination_y, destination);
        } else {
            formation_set_destination_building(m, army->destination_x, army->destination_y, nullptr);
        }
    } else {
        formation_retreat(m);
    }

    update_enemy_movement(m, *roman_distance);
}

void formation_enemy_update(void)
{
    if (enemy_army_total_enemy_formations() <= 0) {
        enemy_armies_clear_ignore_roman_soldiers();
    } else {
        enemy_army_calculate_roman_influence();
        enemy_armies_clear_formations();
        int roman_distance = 0;
        for (int i = 1; i < formation_count(); i++) {
            formation *m = formation_get(i);
            if (m->in_use && !m->is_herd && !m->is_legion) {
                update_enemy_formation(m, &roman_distance);
            }
        }
    }
    if (city_military_is_native_attack_active()) {
        set_native_target_building(formation_get(NATIVE_FORMATION));
    }
}
