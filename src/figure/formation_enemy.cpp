#include "building/building_record.h"
#include "formation_enemy.h"

#include "building/building.h"
#include "building/building_type_api.h"
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
#include "figure/formation_layout.h"
#include "figure/route.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/routing.h"
#include "map/routing_path.h"
#include "map/soldier_strength.h"
#include "map/terrain.h"

#define INFINITE 10000

enum class TargetKind {
    End,
    Type,
    HousingDescending,
    HousingAtOrAbove
};

struct TargetSpec {
    TargetKind kind;
    const char *text_id;
    int level;
};

#define TARGET_TYPE(id) { TargetKind::Type, id, 0 }
#define TARGET_HOUSING_DESCENDING() { TargetKind::HousingDescending, nullptr, 0 }
#define TARGET_HOUSING_AT_OR_ABOVE(min_level) { TargetKind::HousingAtOrAbove, nullptr, min_level }
#define TARGET_END() { TargetKind::End, nullptr, 0 }

static const TargetSpec ENEMY_ATTACK_PRIORITY[4][16] = {
    {
        TARGET_TYPE("granary"), TARGET_TYPE("warehouse"), TARGET_TYPE("market"),
        TARGET_TYPE("wheat_farm"), TARGET_TYPE("vegetable_farm"), TARGET_TYPE("fruit_farm"),
        TARGET_TYPE("olive_farm"), TARGET_TYPE("vines_farm"), TARGET_TYPE("pig_farm"), TARGET_END()
    },
    {
        TARGET_TYPE("senate"), TARGET_TYPE("senate_1_unused"),
        TARGET_TYPE("forum_2_unused"), TARGET_TYPE("forum"), TARGET_END()
    },
    {
        TARGET_TYPE("governors_palace"), TARGET_TYPE("governors_villa"), TARGET_TYPE("governors_house"),
        TARGET_HOUSING_DESCENDING(), TARGET_END()
    },
    {
        TARGET_TYPE("military_academy"), TARGET_TYPE("prefecture"), TARGET_END()
    }
};

static const TargetSpec RIOTER_ATTACK_PRIORITY[] = {
    TARGET_TYPE("governors_palace"),
    TARGET_TYPE("governors_villa"),
    TARGET_TYPE("governors_house"),
    TARGET_TYPE("amphitheater"),
    TARGET_TYPE("theater"),
    TARGET_TYPE("hospital"),
    TARGET_TYPE("academy"),
    TARGET_TYPE("bathhouse"),
    TARGET_TYPE("library"),
    TARGET_TYPE("school"),
    TARGET_TYPE("doctor"),
    TARGET_TYPE("gladiator_school"),
    TARGET_TYPE("actor_colony"),
    TARGET_TYPE("chariot_maker"),
    TARGET_TYPE("lion_house"),
    TARGET_TYPE("barber"),
    TARGET_TYPE("tavern"),
    TARGET_TYPE("arena"),
    TARGET_TYPE("horse_statue"),
    TARGET_TYPE("legion_statue"),
    TARGET_TYPE("large_statue"),
    TARGET_TYPE("medium_statue"),
    TARGET_TYPE("obelisk"),
    TARGET_HOUSING_AT_OR_ABOVE(HOUSE_LARGE_VILLA),
    TARGET_END()
};

#define NUM_LAYOUT_FORMATIONS 40
static const int LAYOUT_ORIENTATION_OFFSETS[13][4][NUM_LAYOUT_FORMATIONS] = {
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -6, 0, 6, 0, -6, 2, 6, 2, -2, 4, 4, 6, 0},
        {0, 0, 0, -6, 0, 6, 2, -6, 2, 6, 4, -2, 6, 4, 0},
        {0, 0, -6, 0, 6, 0, -6, -2, 6, -2, -4, -6, 4, -6, 0},
        {0, 0, 0, -6, 0, 6, -2, -6, -2, 6, -6, -4, -6, 4, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    },
    {
        {0, 0, -4, 0, 4, 0, -12, 0, 12, 0, -4, 12, 4, 12, 0},
        {0, 0, 0, -4, 0, 4, 0, -12, 0, 12, 12, -4, 12, 4, 0},
        {0, 0, -4, 0, 4, 0, -12, 0, 12, 0, -4, -12, 4, -12, 0},
        {0, 0, 0, -4, 0, 4, 0, -12, 0, 12, -12, -4, -12, 4, 0},
    },
    {
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, 8, 3, 8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, 8, -3, 8, 3, 0},
        {0, 0, -3, 0, 3, 0, -8, 0, 8, 0, -3, -8, 3, -8, 0},
        {0, 0, 0, -3, 0, 3, 0, -8, 0, 8, -8, -3, -8, 3, 0},
    }
};

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    building_type resolved = runtime_type(text_id);
    return resolved != BUILDING_NONE && type == resolved;
}

static int type_matches_any(building_type type, const char *const *text_ids, int count)
{
    for (int i = 0; i < count; i++) {
        if (type_matches(type, text_ids[i])) {
            return 1;
        }
    }
    return 0;
}

static building *first_active_building_of_type(building_type type)
{
    if (type <= BUILDING_NONE) {
        return nullptr;
    }
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE) {
            return b;
        }
    }
    return nullptr;
}

static building *first_active_housing_at_level(int level)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state == BUILDING_STATE_IN_USE &&
            building_type_registry_get_housing_level(b->type) == level) {
            return b;
        }
    }
    return nullptr;
}

static building *closest_active_building_of_type(building_type type, int x, int y, int *min_distance)
{
    building *best_building = nullptr;
    if (type <= BUILDING_NONE) {
        return nullptr;
    }
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        int distance = calc_maximum_distance(x, y, b->x, b->y);
        if (distance < *min_distance) {
            best_building = b;
            *min_distance = distance;
        }
    }
    return best_building;
}

static building *closest_active_housing_at_level(int level, int x, int y, int *min_distance)
{
    building *best_building = nullptr;
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE ||
            building_type_registry_get_housing_level(b->type) != level) {
            continue;
        }
        int distance = calc_maximum_distance(x, y, b->x, b->y);
        if (distance < *min_distance) {
            best_building = b;
            *min_distance = distance;
        }
    }
    return best_building;
}

static building *get_best_housing_descending(int min_level)
{
    for (int index = building_type_registry_get_housing_level_count() - 1; index >= 0; index--) {
        int level = building_type_registry_get_housing_level_at(index);
        if (level < min_level) {
            continue;
        }
        if (building *b = first_active_housing_at_level(level)) {
            return b;
        }
    }
    return nullptr;
}

static building *get_closest_housing_descending(int x, int y, int min_level)
{
    for (int index = building_type_registry_get_housing_level_count() - 1; index >= 0; index--) {
        int level = building_type_registry_get_housing_level_at(index);
        if (level < min_level) {
            continue;
        }
        int min_distance = INFINITE;
        if (building *b = closest_active_housing_at_level(level, x, y, &min_distance)) {
            return b;
        }
    }
    return nullptr;
}

static building *get_best_building(const TargetSpec *priority_order)
{
    for (int i = 0; priority_order[i].kind != TargetKind::End; i++) {
        const TargetSpec &spec = priority_order[i];
        if (spec.kind == TargetKind::HousingDescending) {
            if (building *b = get_best_housing_descending(0)) {
                return b;
            }
        } else if (spec.kind == TargetKind::HousingAtOrAbove) {
            if (building *b = get_best_housing_descending(spec.level)) {
                return b;
            }
        } else if (building *b = first_active_building_of_type(runtime_type(spec.text_id))) {
            return b;
        }
    }
    return nullptr;
}

static building *get_best_and_closest_building(int x, int y, const TargetSpec *priority_order)
{
    for (int i = 0; priority_order[i].kind != TargetKind::End; i++) {
        const TargetSpec &spec = priority_order[i];
        if (spec.kind == TargetKind::HousingDescending) {
            if (building *b = get_closest_housing_descending(x, y, 0)) {
                return b;
            }
        } else if (spec.kind == TargetKind::HousingAtOrAbove) {
            if (building *b = get_closest_housing_descending(x, y, spec.level)) {
                return b;
            }
        } else {
            int min_distance = INFINITE;
            if (building *b = closest_active_building_of_type(runtime_type(spec.text_id), x, y, &min_distance)) {
                return b;
            }
        }
    }
    return nullptr;
}

int formation_rioter_get_target_building(int *x_tile, int *y_tile)
{
    building *best_building = get_best_building(RIOTER_ATTACK_PRIORITY);
    if (!best_building) {
        return 0;
    }
    *x_tile = best_building->x;
    *y_tile = best_building->y;
    return best_building->id;
}

int formation_rioter_get_target_building_for_robbery(int x, int y, int *x_tile, int *y_tile)
{
    building *best_building = 0;
    int closest = INFINITE;

    static const char *const building_targets[] = { "senate", "forum" };
    for (int i = 0; i < 2; i++) {
        building_type type = runtime_type(building_targets[i]);
        if (type == BUILDING_NONE) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->state != BUILDING_STATE_IN_USE) {
                continue;
            }
            int distance = calc_maximum_distance(x, y, b->x, b->y);
            if (distance >= 150) {
                continue;
            }
            if (distance < closest) {
                closest = distance;
                best_building = b;
            }
        }
    }
    if (!best_building) {
        return 0;
    }

    *x_tile = best_building->road_access_x;
    *y_tile = best_building->road_access_y;

    return best_building->id;
}

static int set_enemy_target_building(formation *m)
{
    int attack = m->attack_type;
    if (attack == FORMATION_ATTACK_RANDOM) {
        attack = random_byte() & 3;
    }
    building *best_building = get_best_and_closest_building(m->x_home, m->y_home, ENEMY_ATTACK_PRIORITY[attack]);

    if (!best_building) {
        // no target buildings left: take rioter attack priority
        best_building = get_best_and_closest_building(m->x_home, m->y_home, RIOTER_ATTACK_PRIORITY);
    }
    if (best_building) {
        if (type_matches(best_building->type, "warehouse")) {
            formation_set_destination_building(m, best_building->x + 1, best_building->y, best_building->id + 1);
        } else {
            formation_set_destination_building(m, best_building->x, best_building->y, best_building->id);
        }
    }
    return best_building != 0;
}


static int native_target_can_anchor_search(const building *b)
{
    return b->state == BUILDING_STATE_IN_USE;
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
    int min_distance = INFINITE;

    for (int i = 0; i < sizeof(native_buildings) / sizeof(native_buildings[0]) && min_distance == INFINITE; i++) {
        building_type type = runtime_type(native_buildings[i]);
        if (type == BUILDING_NONE) {
            continue;
        }
        int size = building_type_registry_get_model_size(type);
        if (size <= 0) {
            size = building_properties_for_type(type)->size;
        }
        int radius = size * 3;
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (!native_target_can_anchor_search(b)) {
                continue;
            }
            int x_min, y_min, x_max, y_max;
            map_grid_get_area(b->x, b->y, size, radius, &x_min, &y_min, &x_max, &y_max);
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
        }
    }
    return min_distance < INFINITE;
}

static void set_native_target_building(formation *m)
{
    int meeting_x, meeting_y;
    city_buildings_main_native_meeting_center(&meeting_x, &meeting_y);
    building *min_building = 0;
    int min_distance = INFINITE;
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
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (b->state != BUILDING_STATE_IN_USE || type_matches(b->type, "gardens") ||
            type_matches_any(b->type, excluded_types, sizeof(excluded_types) / sizeof(excluded_types[0]))) {
            continue;
        }
        int distance = calc_maximum_distance(meeting_x, meeting_y, b->x, b->y);
        if (distance < min_distance) {
            min_building = b;
            min_distance = distance;
        }
    }
    if (min_building) {
        formation_set_destination_building(m, min_building->x, min_building->y, min_building->id);
    } else {
        int dst_x = 0;
        int dst_y = 0;
        int has_target = get_structures_on_native_land(&dst_x, &dst_y);
        if (has_target) {
            formation_set_destination_building(m, dst_x, dst_y, 0);
        } else {
            formation_retreat(m);
        }
    }
}

static void set_figures_to_initial(const formation *m)
{
    for (int i = 0; i < MAX_FORMATION_FIGURES; i++) {
        if (m->figures[i] > 0) {
            figure *f = figure_get(m->figures[i]);
            if (f->action_state != FIGURE_ACTION_149_CORPSE &&
                f->action_state != FIGURE_ACTION_150_ATTACK) {
                f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
                f->wait_ticks = 0;
            }
        }
    }
}

int formation_enemy_move_formation_to(const formation *m, int x, int y, int *x_tile, int *y_tile)
{
    int base_offset = map_grid_offset(
        formation_layout_position_x(m->layout, 0),
        formation_layout_position_y(m->layout, 0));
    int figure_offsets[50];
    figure_offsets[0] = 0;
    for (int i = 1; i < m->num_figures; i++) {
        figure_offsets[i] = map_grid_offset(
            formation_layout_position_x(m->layout, i),
            formation_layout_position_y(m->layout, i)) - base_offset;
    }
    map_routing_noncitizen_can_travel_over_land(x, y, -1, -1, 8, 0, 600);
    for (int r = 0; r <= 10; r++) {
        int x_min, y_min, x_max, y_max;
        map_grid_get_area(x, y, 1, r, &x_min, &y_min, &x_max, &y_max);
        for (int yy = y_min; yy <= y_max; yy++) {
            for (int xx = x_min; xx <= x_max; xx++) {
                int can_move = 1;
                for (int fig = 0; fig < m->num_figures; fig++) {
                    int grid_offset = map_grid_offset(xx, yy) + figure_offsets[fig];
                    if (!map_grid_is_valid_offset(grid_offset)) {
                        can_move = 0;
                        break;
                    }
                    if (map_terrain_is(grid_offset, TERRAIN_IMPASSABLE_ENEMY)) {
                        can_move = 0;
                        break;
                    }
                    if (map_routing_distance(grid_offset) <= 0) {
                        can_move = 0;
                        break;
                    }
                    if (map_has_figure_at(grid_offset) &&
                        figure_get(map_figure_at(grid_offset))->formation_id != m->id) {
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
    for (unsigned int i = 1; i < figure_count() && to_kill > 0; i++) {
        figure *f = figure_get(i);
        if (f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        if (figure_is_enemy(f) && f->type != FIGURE_ENEMY54_GLADIATOR) {
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
    int layout = army->layout;
    int legion_index_offset = (2 * m->enemy_legion_index) % NUM_LAYOUT_FORMATIONS;
    *x_offset = LAYOUT_ORIENTATION_OFFSETS[layout][m->orientation / 2][legion_index_offset];
    *y_offset = LAYOUT_ORIENTATION_OFFSETS[layout][m->orientation / 2][legion_index_offset + 1];
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
        if (army->layout == FORMATION_ENEMY_MOB || army->layout == FORMATION_ENEMY12) {
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
                set_figures_to_initial(m);
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
                set_figures_to_initial(m);
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
                set_figures_to_initial(m);
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
            if (target->num_figures > 0) {
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

static int formation_fully_in_city(const formation *m)
{
    for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
        figure *f = figure_get(m->figures[n]);
        if (f->state != FIGURE_STATE_DEAD && f->is_ghost) {
            return 0;
        }
    }
    return 1;
}

static void update_enemy_formation(formation *m, int *roman_distance)
{
    enemy_army *army = enemy_army_get_editable(m->invasion_id);
    if (enemy_army_is_stronger_than_legions()) {
        if (m->figure_type != FIGURE_FORT_JAVELIN) {
            army->ignore_roman_soldiers = 1;
        }
    }
    formation_decrease_monthly_counters(m);
    if (city_figures_soldiers() <= 0) {
        formation_clear_monthly_counters(m);
    }
    for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
        figure *f = figure_get(m->figures[n]);
        if (f->action_state == FIGURE_ACTION_150_ATTACK) {
            figure *opponent = figure_get(f->opponent_id);
            if (!figure_is_dead(opponent) && figure_is_legion(opponent)) {
                formation_record_fight(m);
            }
        }
    }
    if (formation_has_low_morale(m)) {
        for (int n = 0; n < MAX_FORMATION_FIGURES; n++) {
            figure *f = figure_get(m->figures[n]);
            if (f->action_state != FIGURE_ACTION_150_ATTACK &&
                f->action_state != FIGURE_ACTION_149_CORPSE &&
                f->action_state != FIGURE_ACTION_148_FLEEING) {
                f->action_state = FIGURE_ACTION_148_FLEEING;
                figure_route_remove(f);
            }
        }
        return;
    }
    if (m->figures[0]) {
        figure *f = figure_get(m->figures[0]);
        if (f->state == FIGURE_STATE_ALIVE) {
            formation_set_home(m, f->x, f->y);
        }
    }
    if (!army->formation_id) {
        army->formation_id = m->id;
        army->home_x = m->x_home;
        army->home_y = m->y_home;
        army->layout = m->layout;
        *roman_distance = 0;
        map_routing_noncitizen_can_travel_over_land(m->x_home, m->y_home, -1, -1, 8, 100000, 300);
        int x_tile, y_tile;
        if (map_soldier_strength_get_max(m->x_home, m->y_home, 16, &x_tile, &y_tile)) {
            *roman_distance = 1;
        } else if (map_soldier_strength_get_max(m->x_home, m->y_home, 32, &x_tile, &y_tile)) {
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
            if (!set_enemy_target_building(m) && !army->started_retreating && formation_fully_in_city(m)) {
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
        formation_set_destination_building(m, army->destination_x, army->destination_y, army->destination_building_id);
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
