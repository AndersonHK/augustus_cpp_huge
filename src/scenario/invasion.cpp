#include "invasion.h"

#include "building/destruction.h"
#include "city/emperor.h"
#include "city/message.h"
#include "core/calc.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/object.h"
#include "figure/enemy_army.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/name.h"
#include "game/campaign.h"
#include "game/cheats.h"
#include "game/difficulty.h"
#include "game/time.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/data.h"
#include "scenario/map.h"
#include "scenario/property.h"

#include <vector>

#define MAX_ORIGINAL_INVASION_WARNINGS 101

#define INVASIONS_STRUCT_SIZE_CURRENT (5 * sizeof(int16_t) + 4 * sizeof(uint16_t) + 1 * sizeof(uint8_t))
#define WARNINGS_STRUCT_SIZE_CURRENT (1 * sizeof(int32_t) + 6 * sizeof(int16_t) + 5 * sizeof(uint8_t))

#define BARBARIAN_ENEMY_TYPE_MAX 4

#define CHEATED_ARMY_ID 23
#define CAESAR_ATTACK_ARMY_ID 24
#define ACTION_ARMY_ID_START 25

static const enemy_type_t ENEMY_ID_TO_ENEMY_TYPE[20] = {
    ENEMY_0_BARBARIAN,
    ENEMY_7_ETRUSCAN,
    ENEMY_7_ETRUSCAN,
    ENEMY_10_CARTHAGINIAN,
    ENEMY_8_GREEK,
    ENEMY_8_GREEK,
    ENEMY_9_EGYPTIAN,
    ENEMY_5_PERGAMUM,
    ENEMY_6_SELEUCID,
    ENEMY_3_CELT,
    ENEMY_3_CELT,
    ENEMY_3_CELT,
    ENEMY_2_GAUL,
    ENEMY_2_GAUL,
    ENEMY_4_GOTH,
    ENEMY_4_GOTH,
    ENEMY_4_GOTH,
    ENEMY_6_SELEUCID,
    ENEMY_1_NUMIDIAN,
    ENEMY_6_SELEUCID
};

static const int LOCAL_UPRISING_NUM_ENEMIES[20] = {
    0, 0, 0, 0, 0, 3, 3, 3, 0, 6, 6, 6, 6, 6, 9, 9, 9, 9, 9, 9
};

static const struct {
    int pct_type1;
    int pct_type2;
    int pct_type3;
    int figure_types[3];
    int formation_layout;
} ENEMY_PROPERTIES[12] = {
    {100, 0, 0, {FIGURE_ENEMY49_FAST_SWORD, 0, 0}, FORMATION_ENEMY_MOB}, // barbarian
    {40, 60, 0, {FIGURE_ENEMY49_FAST_SWORD, FIGURE_ENEMY51_SPEAR, 0}, FORMATION_ENEMY_MOB}, // numidian
    {50, 50, 0, {FIGURE_ENEMY50_SWORD, FIGURE_ENEMY53_AXE, 0}, FORMATION_ENEMY_MOB}, // gaul
    {80, 20, 0, {FIGURE_ENEMY50_SWORD, FIGURE_ENEMY48_CHARIOT, 0}, FORMATION_ENEMY_MOB}, // celt
    {50, 50, 0, {FIGURE_ENEMY49_FAST_SWORD, FIGURE_ENEMY52_MOUNTED_ARCHER, 0}, FORMATION_ENEMY_MOB}, // goth
    {30, 70, 0, {FIGURE_ENEMY44_SWORD, FIGURE_ENEMY43_SPEAR, 0}, FORMATION_COLUMN}, // pergamum
    {50, 50, 0, {FIGURE_ENEMY44_SWORD, FIGURE_ENEMY43_SPEAR, 0}, FORMATION_ENEMY_DOUBLE_LINE}, // seleucid
    {50, 50, 0, {FIGURE_ENEMY45_SWORD, FIGURE_ENEMY43_SPEAR, 0}, FORMATION_ENEMY_DOUBLE_LINE}, // etruscan
    {80, 20, 0, {FIGURE_ENEMY45_SWORD, FIGURE_ENEMY43_SPEAR, 0}, FORMATION_ENEMY_DOUBLE_LINE}, // greek
    {80, 20, 0, {FIGURE_ENEMY44_SWORD, FIGURE_ENEMY46_CAMEL, 0}, FORMATION_ENEMY_WIDE_COLUMN}, // egyptian
    {90, 10, 0, {FIGURE_ENEMY45_SWORD, FIGURE_ENEMY47_ELEPHANT, 0}, FORMATION_ENEMY_WIDE_COLUMN}, // carthaginian
    {100, 0, 0, {FIGURE_ENEMY_CAESAR_LEGIONARY, 0, 0}, FORMATION_COLUMN} // caesar
};

typedef struct {
    unsigned int id;
    int in_use;
    int handled;
    int invasion_path_id;
    int warning_years;
    int x;
    int y;
    int image_id;
    int empire_object_id;
    int year_notified;
    int month_notified;
    int months_to_go;
    unsigned int invasion_id;
} invasion_warning;

static struct {
    int last_internal_invasion_id;
    int last_action_army_id;
    std::vector<invasion_t> invasions;
    std::vector<invasion_warning> warnings;
} data;


static invasion_t new_invasion(unsigned int index)
{
    invasion_t invasion = {};
    invasion.id = index;
    invasion.year = INVASION_DEFAULT_START_YEAR;
    invasion.amount.min = INVASION_DEFAULT_AMOUNT_MIN;
    invasion.amount.max = INVASION_DEFAULT_AMOUNT_MAX;
    invasion.type = INVASION_DEFAULT_TYPE;
    invasion.from = INVASION_DEFAULT_FROM;
    invasion.attack_type = INVASION_DEFAULT_ATTACK_TYPE;
    return invasion;
}

static invasion_t inactive_invasion(unsigned int index)
{
    invasion_t invasion = {};
    invasion.id = index;
    return invasion;
}

static bool invasion_in_use(const invasion_t &invasion)
{
    return invasion.type != INVASION_TYPE_NONE;
}

static invasion_t *invasion_slot(int id)
{
    return id >= 0 && static_cast<size_t>(id) < data.invasions.size() ? &data.invasions[id] : nullptr;
}

static invasion_t *append_invasion()
{
    data.invasions.push_back(new_invasion(static_cast<unsigned int>(data.invasions.size())));
    return &data.invasions.back();
}

static void resize_invasions(size_t size)
{
    data.invasions.clear();
    data.invasions.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_invasion();
    }
}

static invasion_t *create_invasion()
{
    for (invasion_t &invasion : data.invasions) {
        if (!invasion_in_use(invasion)) {
            invasion = new_invasion(invasion.id);
            return &invasion;
        }
    }
    return append_invasion();
}

static void trim_invasions()
{
    while (data.invasions.size() > 1 && !invasion_in_use(data.invasions.back())) {
        data.invasions.pop_back();
    }
}

static void reindex_invasions()
{
    for (size_t i = 0; i < data.invasions.size(); ++i) {
        data.invasions[i].id = static_cast<unsigned int>(i);
    }
}

static void remove_first_invasion_if_inactive()
{
    if (!data.invasions.empty() && !invasion_in_use(data.invasions.front())) {
        data.invasions.erase(data.invasions.begin());
        reindex_invasions();
    }
}

static invasion_warning new_warning(unsigned int index)
{
    invasion_warning warning = {};
    warning.id = index;
    return warning;
}

static bool warning_in_use(const invasion_warning &warning)
{
    return warning.in_use != 0;
}

static invasion_warning *append_warning()
{
    data.warnings.push_back(new_warning(static_cast<unsigned int>(data.warnings.size())));
    return &data.warnings.back();
}

static void resize_warnings(size_t size)
{
    data.warnings.clear();
    data.warnings.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_warning();
    }
}

static void trim_warnings()
{
    while (data.warnings.size() > 1 && !warning_in_use(data.warnings.back())) {
        data.warnings.pop_back();
    }
}

static void clear_warnings(void)
{
    data.warnings.clear();
}

void scenario_invasion_clear(void)
{
    data.invasions.clear();
    clear_warnings();
}

static void init_warnings(void)
{
    clear_warnings();
    append_warning();
    int path_current = 1;
    int path_max = empire_object_get_max_invasion_path();
    if (path_max == 0) {
        return;
    }
    for (const invasion_t &invasion : data.invasions) {
        if (!invasion.type) {
            continue;
        }
        if (invasion.type == INVASION_TYPE_LOCAL_UPRISING ||
            invasion.type == INVASION_TYPE_DISTANT_BATTLE) {
            continue;
        }
        for (int year = 1; year < 8; year++) {
            const empire_object *obj = empire_object_get_battle(path_current, year);
            if (!obj) {
                continue;
            }
            invasion_warning *warning = append_warning();
            warning->in_use = 1;
            warning->invasion_path_id = obj->invasion_path_id;
            warning->warning_years = obj->invasion_years;
            warning->x = obj->x;
            warning->y = obj->y;
            warning->image_id = obj->image_id;
            warning->invasion_id = invasion.id;
            warning->empire_object_id = obj->id;
            warning->month_notified = 0;
            warning->year_notified = 0;
            warning->months_to_go = 12 * invasion.year;
            warning->months_to_go += invasion.month;
            warning->months_to_go -= 12 * year;
        }
        path_current++;
        if (path_current > path_max) {
            path_current = 1;
        }
    }
}

void scenario_invasion_init(void)
{
    for (invasion_t &invasion : data.invasions) {
        random_generate_next();
        if (!invasion.type) {
            continue;
        }
        invasion.month = 2 + (random_byte() & 7);
    }
    init_warnings();
}

int scenario_invasion_new(void)
{
    invasion_t *invasion = create_invasion();
    return invasion ? invasion->id : -1;
}

const invasion_t *scenario_invasion_get(int id)
{
    return invasion_slot(id);
}

void scenario_invasion_update(const invasion_t *invasion)
{
    invasion_t *base_invasion = invasion_slot(invasion->id);
    if (base_invasion) {
        *base_invasion = *invasion;
        trim_invasions();
    }
}

void scenario_invasion_delete(int id)
{
    invasion_t *invasion = invasion_slot(id);
    if (invasion) {
        *invasion = inactive_invasion(invasion->id);
        trim_invasions();
    }
}

int scenario_invasion_exists_upcoming(void)
{
    for (const invasion_warning &warning : data.warnings) {
        if (warning.in_use && !warning.handled) {
            return 1;
        }
    }
    return 0;
}

int scenario_invasion_get_years_remaining(void)
{
    int years_until_invasion = 4;
    for (const invasion_warning &warning : data.warnings) {
        if (warning.in_use && warning.handled && warning.warning_years < years_until_invasion) {
            years_until_invasion = warning.warning_years;
        }
    }
    return years_until_invasion != 4 ? years_until_invasion : 0;
}

void scenario_invasion_foreach_warning(void (*callback)(int x, int y, int image_id))
{
    for (const invasion_warning &warning : data.warnings) {
        if (warning.in_use && warning.handled) {
            callback(warning.x, warning.y, warning.image_id);
        }
    }
}

int scenario_invasion_count_total(void)
{
    return static_cast<int>(data.invasions.size());
}

int scenario_invasion_count_active(void)
{
    int num_invasions = 0;
    for (const invasion_t &invasion : data.invasions) {
        if (invasion.type) {
            num_invasions++;
        }
    }
    return num_invasions;
}

static void determine_formations(int num_soldiers, int *num_formations, int soldiers_per_formation[])
{
    if (num_soldiers > 0) {
        if (num_soldiers <= 16) {
            *num_formations = 1;
            soldiers_per_formation[0] = num_soldiers;
        } else if (num_soldiers <= 32) {
            *num_formations = 2;
            soldiers_per_formation[1] = num_soldiers / 2;
            soldiers_per_formation[0] = num_soldiers - num_soldiers / 2;
        } else {
            *num_formations = 3;
            soldiers_per_formation[2] = num_soldiers / 3;
            soldiers_per_formation[1] = num_soldiers / 3;
            soldiers_per_formation[0] = num_soldiers - 2 * (num_soldiers / 3);
        }
    }
}

static int start_invasion(enemy_type_t enemy_type, int amount, int invasion_point, formation_attack_enum attack_type, int invasion_id)
{
    if (game_cheat_disabled_invasions()) { // invasions disabled 
        return -1;
    }
    if (amount <= 0) {
        return -1;
    }

    enemy_army *army = enemy_army_get_editable(invasion_id);
    if (army) {
        army->started_retreating = 0;
    }

    int formations_per_type[3];
    int soldiers_per_formation[3][4];
    int x, y;
    int orientation;

    amount = difficulty_adjust_enemies(amount);
    if (amount >= 150) {
        amount = 150;
    }
    data.last_internal_invasion_id++;
    if (data.last_internal_invasion_id > 32000) {
        data.last_internal_invasion_id = 1;
    }
    // calculate soldiers per type
    int num_type1 = calc_adjust_with_percentage(amount, ENEMY_PROPERTIES[enemy_type].pct_type1);
    int num_type2 = calc_adjust_with_percentage(amount, ENEMY_PROPERTIES[enemy_type].pct_type2);
    int num_type3 = calc_adjust_with_percentage(amount, ENEMY_PROPERTIES[enemy_type].pct_type3);
    num_type1 += amount - (num_type1 + num_type2 + num_type3); // assign leftovers to type1

    for (int t = 0; t < 3; t++) {
        formations_per_type[t] = 0;
        for (int f = 0; f < 4; f++) {
            soldiers_per_formation[t][f] = 0;
        }
    }

    // calculate number of formations
    determine_formations(num_type1, &formations_per_type[0], soldiers_per_formation[0]);
    determine_formations(num_type2, &formations_per_type[1], soldiers_per_formation[1]);
    determine_formations(num_type3, &formations_per_type[2], soldiers_per_formation[2]);

    // determine invasion point
    if (enemy_type == ENEMY_11_CAESAR) {
        map_point entry_point = scenario_map_entry();
        x = entry_point.x;
        y = entry_point.y;
    } else {
        // Determinate maximum number of valid invasion points.
        // Strip out invalid invasion points from the end of the list.
        int num_points = MAX_INVASION_POINTS;
        for (int i = MAX_INVASION_POINTS - 1; i >= 0; i--) {
            if (scenario.invasion_points[i].x == -1) {
                num_points--;
            } else if (num_points != MAX_INVASION_POINTS) {
                break;
            }
        }
        if (invasion_point == MAX_INVASION_POINTS) { // random
            if (num_points <= 2) {
                invasion_point = random_byte() & 1;
            } else if (num_points <= 4) {
                invasion_point = random_byte() & 3;
            } else {
                invasion_point = random_byte() & 7;
            }
        }
        if (num_points > 0) {
            while (scenario.invasion_points[invasion_point].x == -1) {
                invasion_point++;
                if (invasion_point >= MAX_INVASION_POINTS) {
                    invasion_point = 0;
                }
            }
        }
        x = scenario.invasion_points[invasion_point].x;
        y = scenario.invasion_points[invasion_point].y;
    }
    if (x == -1 || y == -1) {
        map_point exit_point = scenario_map_exit();
        x = exit_point.x;
        y = exit_point.y;
    }
    // determine orientation
    if (y == 0) {
        orientation = DIR_4_BOTTOM;
    } else if (y >= scenario.map.height - 1) {
        orientation = DIR_0_TOP;
    } else if (x == 0) {
        orientation = DIR_2_RIGHT;
    } else if (x >= scenario.map.width - 1) {
        orientation = DIR_6_LEFT;
    } else {
        orientation = DIR_4_BOTTOM;
    }
    // check terrain
    int grid_offset = map_grid_offset(x, y);
    if (map_terrain_is(grid_offset, TERRAIN_ELEVATION | TERRAIN_ROCK | TERRAIN_TREE)) {
        return -1;
    }
    if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
        if (!map_terrain_is(grid_offset, TERRAIN_ROAD)) {
            // bridge - any changes to bridge behaviour will need to ensure that invasion doesnt target it 
            return -1;
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_BUILDING | TERRAIN_AQUEDUCT | TERRAIN_GATEHOUSE | TERRAIN_WALL)) {
        building_destroy_by_enemy(grid_offset);
    }
    // spawn the lot!
    int seq = 0;
    for (int type_id = 0; type_id < 3; type_id++) {
        if (formations_per_type[type_id] <= 0) {
            continue;
        }
        figure_type type = static_cast<figure_type>(ENEMY_PROPERTIES[enemy_type].figure_types[type_id]);
        for (int i = 0; i < formations_per_type[type_id]; i++) {
            int formation_id = formation_create_enemy(
                type, x, y, ENEMY_PROPERTIES[enemy_type].formation_layout, orientation,
                enemy_type, attack_type, invasion_id, data.last_internal_invasion_id
            );
            if (formation_id <= 0) {
                continue;
            }
            for (int fig = 0; fig < soldiers_per_formation[type_id][i]; fig++) {
                Figure *f = Figure::create(type, x, y, static_cast<direction_type>(orientation));
                f->faction_id = 0;
                f->is_friendly = 0;
                f->action_state = FIGURE_ACTION_151_ENEMY_INITIAL;
                // TODO: should we adjust wait ticks to make enemy camping harder?
                f->wait_ticks = static_cast<short>(200 * seq + 10 * fig + 10);
                f->formation_id = formation_id;
                f->name = static_cast<short>(figure_name_get(type, enemy_type));
                f->is_ghost = 1;
            }
            seq++;
        }
    }
    return grid_offset;
}

void repeat_invasion_without_warnings(invasion_t *invasion)
{
    if (invasion->repeat.times != INVASIONS_REPEAT_INFINITE) {
        invasion->repeat.times--;
    }
    int years = random_between_from_stdlib(invasion->repeat.interval.min, invasion->repeat.interval.max);

    // If enemies retreated in previous repeating attack, reset this behavior
    int invasion_id = invasion->id;
    enemy_army *army = enemy_army_get_editable(invasion_id);
    if (army) {
        army->started_retreating = 0;
    }

    invasion->year += years;
    invasion->month = 2 + (random_from_stdlib() & 7);
}

static void repeat_invasion_with_warnings(invasion_t *invasion)
{
    // Calls a function that repeats the invasion without issuing any warnings
    repeat_invasion_without_warnings(invasion);

    // Gets the maximum number of available invasion paths from the empire
    // If there are no paths available, exit the function early
    int path_max = empire_object_get_max_invasion_path();
    if (path_max == 0) {
        return;
    }

    // Determining the current intrusion path number
    // Start with the first invasion path
    int path_current = 1;
    // Iterate through all invasions stored in the data.invasions array
    for (const invasion_t &inv_it : data.invasions) {
        // Skip if the invasion has no type, or is a local uprising, or a distant battle
        if (!inv_it.type ||
            inv_it.type == INVASION_TYPE_LOCAL_UPRISING ||
            inv_it.type == INVASION_TYPE_DISTANT_BATTLE) {
            continue;
        } // Stop the loop when we reach the current invasion (used to count how many valid invasions were before it)
        if (&inv_it == invasion) {
            break;
        } // Cycle the path number if it exceeds the maximum — loop back to 1.
        path_current++;
        if (path_current > path_max) {
            path_current = 1;
        }
    }

    // Clear old warning
    // Clear existing warnings related to this invasion: mark as not in use and not handled.
    for (invasion_warning &w : data.warnings) {
        if (w.invasion_id == invasion->id) {
            w.in_use = 0;
            w.handled = 0;
        }
    }

    // Calculating the invasion date and the current date
    // Get current game time in months
    const int game_month = game_time_year() * 12 + game_time_month();
    // Get the scheduled month of the invasion (in months since year 0).
    const int invasion_month = (scenario.start_year + invasion->year) * 12 + invasion->month;

    // Iterate from year 1 to 7 (inclusive) to schedule warnings for up to 7 years ahead
    for (int year = 1; year < 8; year++) {
        // Get the empire object (e.g., icon or location) for this path and year. Skip if none found.
        const empire_object *obj = empire_object_get_battle(path_current, year);
        if (!obj) {
            continue;
        }
        // Allocate a new warning slot in the warnings array. If memory fails, log an error and exit
        invasion_warning *warning = append_warning();
        // Mark the warning as active and unhandled
        warning->in_use = 1;
        warning->handled = 0;

        // Set up all data for the warning: coordinates, image, path info, invasion ID, etc
        warning->invasion_path_id = obj->invasion_path_id;
        warning->warning_years = obj->invasion_years;
        warning->x = obj->x;
        warning->y = obj->y;
        warning->image_id = obj->image_id;
        warning->invasion_id = invasion->id;
        warning->empire_object_id = obj->id;
        warning->month_notified = 0;
        warning->year_notified = 0;

        // Calculate how many months remain until this warning should be triggered.
        // Adjust the months if the warning is for an invasion more than one year away.
        // Sometimes 1 is added to move the warning a little later.
        int months_to_go = invasion_month - obj->invasion_years * 12 - game_month;
        if (obj->invasion_years > 1) {
            months_to_go++;
        }
        // Set final delay value. If it's negative, set to 0 (trigger immediately).
        warning->months_to_go = months_to_go < 0 ? 0 : months_to_go;
    }
}

void scenario_invasion_process(void)
{
    if (game_cheat_disabled_invasions()) { // invasions disabled 
        return;
    }
    int enemy_id = scenario.enemy_id;
    for (size_t i = 0; i < data.warnings.size(); ++i) {
        invasion_warning *warning = &data.warnings[i];
        if (!warning->in_use) {
            continue;
        }
        // update warnings
        warning->months_to_go--;
        if (warning->months_to_go <= 0) {
            if (warning->handled != 1) {
                warning->handled = 1;
                warning->year_notified = game_time_year();
                warning->month_notified = game_time_month();
                if (warning->warning_years > 2) {
                    city_message_post(0, MESSAGE_DISTANT_BATTLE, 0, 0);
                } else if (warning->warning_years > 1) {
                    city_message_post(0, MESSAGE_ENEMIES_CLOSING, 0, 0);
                } else {
                    city_message_post(0, MESSAGE_ENEMIES_AT_THE_DOOR, 0, 0);
                }
            }
        }
        invasion_t *invasion = invasion_slot(warning->invasion_id);
        if (!invasion) {
            warning->in_use = 0;
            continue;
        }
        if (game_time_year() >= scenario.start_year + invasion->year &&
            game_time_month() >= invasion->month) {
            // invasion attack time has passed
            warning->in_use = 0;
            if (warning->warning_years > 1) {
                continue;
            }
            int random_amount = random_between_from_stdlib(invasion->amount.min, invasion->amount.max);
            // enemy invasions
            if (invasion->type == INVASION_TYPE_ENEMY_ARMY) {
                int grid_offset = start_invasion(ENEMY_ID_TO_ENEMY_TYPE[enemy_id],
                    random_amount, invasion->from, static_cast<formation_attack_enum>(invasion->attack_type), warning->invasion_id);
                if (grid_offset > 0) {
                    if (ENEMY_ID_TO_ENEMY_TYPE[enemy_id] > BARBARIAN_ENEMY_TYPE_MAX) {
                        city_message_post(1, MESSAGE_ENEMY_ARMY_ATTACK, data.last_internal_invasion_id, grid_offset);
                    } else {
                        city_message_post(1, MESSAGE_BARBARIAN_ATTACK, data.last_internal_invasion_id, grid_offset);
                    }
                }
            }
            if (invasion->type == INVASION_TYPE_CAESAR) {
                city_emperor_force_attack(random_amount);
            }
            if (invasion->repeat.times != 0) {
                repeat_invasion_with_warnings(invasion);
            }
        }
    }
    // local uprisings
    for (invasion_t &invasion : data.invasions) {
        if (invasion.type == INVASION_TYPE_LOCAL_UPRISING) {
            if (game_time_year() == scenario.start_year + invasion.year && game_time_month() == invasion.month) {
                int grid_offset = start_invasion(ENEMY_0_BARBARIAN,
                    random_between_from_stdlib(invasion.amount.min, invasion.amount.max),
                    invasion.from, static_cast<formation_attack_enum>(invasion.attack_type), invasion.id);
                if (grid_offset > 0) {
                    city_message_post(1, MESSAGE_LOCAL_UPRISING, data.last_internal_invasion_id, grid_offset);
                }
                if (invasion.repeat.times != 0) {
                    repeat_invasion_without_warnings(&invasion);
                }
            }
        }
    }

}

int scenario_invasion_start_from_mars(void)
{
    int mission = scenario_campaign_mission();
    int amount;
    if (game_campaign_is_original() && 0 <= mission && mission <= 19) {
        amount = LOCAL_UPRISING_NUM_ENEMIES[mission];
    } else if (scenario_invasion_count_total() > 0) {
        amount = random_between_from_stdlib(3, 9);
    } else {
        amount = 0;
    }
    if (amount <= 0) {
        return 0;
    }
    int grid_offset = start_invasion(ENEMY_0_BARBARIAN, amount, 8, FORMATION_ATTACK_FOOD_CHAIN, CHEATED_ARMY_ID);
    if (grid_offset) {
        city_message_post(1, MESSAGE_LOCAL_UPRISING_MARS, data.last_internal_invasion_id, grid_offset);
    }
    return 1;
}

int scenario_invasion_start_from_caesar(int size)
{
    if (game_cheat_disabled_invasions()) { // invasions disabled 
        return 0;
    }
    int grid_offset = start_invasion(ENEMY_11_CAESAR, size, 0, FORMATION_ATTACK_BEST_BUILDINGS, CAESAR_ATTACK_ARMY_ID);
    if (grid_offset > 0) {
        city_message_post(1, MESSAGE_CAESAR_ARMY_ATTACK, data.last_internal_invasion_id, grid_offset);
        return 1;
    }
    return 0;
}

void scenario_invasion_start_from_cheat(void)
{
    // leaving this one out of the disabled_invasions check - no reason for cheat to stop cheats
    int enemy_id = scenario.enemy_id;
    int grid_offset = start_invasion(ENEMY_ID_TO_ENEMY_TYPE[enemy_id], 150, 8,
        FORMATION_ATTACK_FOOD_CHAIN, CHEATED_ARMY_ID);
    if (grid_offset) {
        if (ENEMY_ID_TO_ENEMY_TYPE[enemy_id] > BARBARIAN_ENEMY_TYPE_MAX) {
            city_message_post(1, MESSAGE_ENEMY_ARMY_ATTACK, data.last_internal_invasion_id, grid_offset);
        } else {
            city_message_post(1, MESSAGE_BARBARIAN_ATTACK, data.last_internal_invasion_id, grid_offset);
        }
    }
}

void scenario_invasion_start_from_action(invasion_type_enum invasion_type, int size, int invasion_point,
    formation_attack_enum attack_type, enemy_type_t enemy_id)
{
    if (game_cheat_disabled_invasions()) { // invasions disabled 
        return;
    }
    if (attack_type > FORMATION_ATTACK_RANDOM) {
        attack_type = FORMATION_ATTACK_RANDOM;
    }

    data.last_action_army_id++;
    if (data.last_action_army_id < ACTION_ARMY_ID_START || data.last_action_army_id >= MAX_ENEMY_ARMIES) {
        data.last_action_army_id = ACTION_ARMY_ID_START;
    }
    switch (invasion_type) {
        case INVASION_TYPE_ENEMY_ARMY:
        {
            if (enemy_id <= ENEMY_UNDEFINED) {
                enemy_id = static_cast<enemy_type_t>(scenario.enemy_id);
            }
            enemy_army_clear(data.last_action_army_id);
            int grid_offset = start_invasion(ENEMY_ID_TO_ENEMY_TYPE[enemy_id], size,
                invasion_point, attack_type, data.last_action_army_id);
            if (grid_offset) {
                if (ENEMY_ID_TO_ENEMY_TYPE[enemy_id] > BARBARIAN_ENEMY_TYPE_MAX) {
                    city_message_post(1, MESSAGE_ENEMY_ARMY_ATTACK, data.last_internal_invasion_id, grid_offset);
                } else {
                    city_message_post(1, MESSAGE_BARBARIAN_ATTACK, data.last_internal_invasion_id, grid_offset);
                }
            }
            break;
        }
        case INVASION_TYPE_CAESAR:
        {
            city_emperor_force_attack(size);
            break;
        }
        case INVASION_TYPE_LOCAL_UPRISING:
        {
            if (enemy_id <= ENEMY_UNDEFINED) {
                enemy_id = ENEMY_0_BARBARIAN;
            }
            enemy_army_clear(data.last_action_army_id);
            int grid_offset = start_invasion(ENEMY_ID_TO_ENEMY_TYPE[enemy_id], size,
                invasion_point, attack_type, data.last_action_army_id);
            if (grid_offset) {
                city_message_post(1, MESSAGE_LOCAL_UPRISING, data.last_internal_invasion_id, grid_offset);
            }
            break;
        }
        case INVASION_TYPE_MARS_NATIVES:
        {
            enemy_army_clear(data.last_action_army_id);
            int grid_offset = start_invasion(ENEMY_0_BARBARIAN, size, 8, attack_type, data.last_action_army_id);
            if (grid_offset) {
                city_message_post(1, MESSAGE_LOCAL_UPRISING_MARS, data.last_internal_invasion_id, grid_offset);
            }
            break;
        }
        default:
            break;
    }
}

void scenario_invasion_start_from_console(invasion_type_enum invasion_type, int size, int invasion_point)
{
    formation_attack_enum attack_type = FORMATION_ATTACK_RANDOM;
    enemy_type_t enemy_id = static_cast<enemy_type_t>(scenario.enemy_id);
    switch (invasion_type) {
        case INVASION_TYPE_ENEMY_ARMY:
            attack_type = FORMATION_ATTACK_RANDOM;
            break;
        case INVASION_TYPE_CAESAR:
            attack_type = FORMATION_ATTACK_BEST_BUILDINGS;
            break;
        case INVASION_TYPE_LOCAL_UPRISING:
        case INVASION_TYPE_MARS_NATIVES:
            enemy_id = ENEMY_0_BARBARIAN;
            attack_type = FORMATION_ATTACK_FOOD_CHAIN;
            break;
        default:
            break;
    }
    scenario_invasion_start_from_action(invasion_type, size, invasion_point, attack_type, enemy_id);
}

void scenario_invasion_warning_save_state(buffer *invasion_id, buffer *warnings)
{
    buffer_write_u16(invasion_id, static_cast<uint16_t>(data.last_internal_invasion_id));

    buffer_init_dynamic_array(warnings, data.warnings.size(), WARNINGS_STRUCT_SIZE_CURRENT);

    for (const invasion_warning &w : data.warnings) {
        buffer_write_u8(warnings, static_cast<uint8_t>(w.in_use));
        buffer_write_u8(warnings, static_cast<uint8_t>(w.handled));
        buffer_write_u8(warnings, static_cast<uint8_t>(w.invasion_path_id));
        buffer_write_u8(warnings, static_cast<uint8_t>(w.warning_years));
        buffer_write_i16(warnings, static_cast<int16_t>(w.x));
        buffer_write_i16(warnings, static_cast<int16_t>(w.y));
        buffer_write_i16(warnings, static_cast<int16_t>(w.image_id));
        buffer_write_i16(warnings, static_cast<int16_t>(w.empire_object_id));
        buffer_write_i16(warnings, static_cast<int16_t>(w.month_notified));
        buffer_write_i16(warnings, static_cast<int16_t>(w.year_notified));
        buffer_write_i32(warnings, w.months_to_go);
        buffer_write_u8(warnings, static_cast<uint8_t>(w.invasion_id));
    }
}

void scenario_invasion_warning_load_state(buffer *invasion_id, buffer *warnings, int has_dynamic_warnings)
{
    data.last_internal_invasion_id = buffer_read_u16(invasion_id);

    size_t size = has_dynamic_warnings ? buffer_load_dynamic_array(warnings) : MAX_ORIGINAL_INVASION_WARNINGS;

    resize_warnings(size);

    for (size_t i = 0; i < size; i++) {
        invasion_warning *w = &data.warnings[i];
        w->in_use = buffer_read_u8(warnings);
        w->handled = buffer_read_u8(warnings);
        w->invasion_path_id = buffer_read_u8(warnings);
        w->warning_years = buffer_read_u8(warnings);
        w->x = buffer_read_i16(warnings);
        w->y = buffer_read_i16(warnings);
        w->image_id = buffer_read_i16(warnings);
        w->empire_object_id = buffer_read_i16(warnings);
        w->month_notified = buffer_read_i16(warnings);
        w->year_notified = buffer_read_i16(warnings);
        w->months_to_go = buffer_read_i32(warnings);
        w->invasion_id = buffer_read_u8(warnings);
        if (!has_dynamic_warnings) {
            buffer_skip(warnings, 11);
        }
    }
    trim_warnings();
}

void scenario_invasion_save_state(buffer *buf)
{
    buffer_init_dynamic_array(buf, data.invasions.size(), INVASIONS_STRUCT_SIZE_CURRENT);

    for (const invasion_t &invasion : data.invasions) {
        buffer_write_i16(buf, static_cast<int16_t>(invasion.type));
        buffer_write_i16(buf, static_cast<int16_t>(invasion.year));
        buffer_write_u16(buf, static_cast<uint16_t>(invasion.amount.min));
        buffer_write_u16(buf, static_cast<uint16_t>(invasion.amount.max));
        buffer_write_i16(buf, static_cast<int16_t>(invasion.from));
        buffer_write_i16(buf, static_cast<int16_t>(invasion.attack_type));
        buffer_write_u8(buf, static_cast<uint8_t>(invasion.month));
        buffer_write_i16(buf, static_cast<int16_t>(invasion.repeat.times));
        buffer_write_u16(buf, static_cast<uint16_t>(invasion.repeat.interval.min));
        buffer_write_u16(buf, static_cast<uint16_t>(invasion.repeat.interval.max));
    }
}

void scenario_invasion_load_state(buffer *buf)
{
    size_t size = buffer_load_dynamic_array(buf);

    resize_invasions(size);

    for (size_t i = 0; i < size; i++) {
        invasion_t *invasion = &data.invasions[i];
        invasion->type = buffer_read_i16(buf);
        invasion->year = buffer_read_i16(buf);
        invasion->amount.min = buffer_read_u16(buf);
        invasion->amount.max = buffer_read_u16(buf);
        invasion->from = buffer_read_i16(buf);
        invasion->attack_type = buffer_read_i16(buf);
        invasion->month = buffer_read_u8(buf);
        invasion->repeat.times = buffer_read_i16(buf);
        invasion->repeat.interval.min = buffer_read_u16(buf);
        invasion->repeat.interval.max = buffer_read_u16(buf);
    }
    trim_invasions();
    remove_first_invasion_if_inactive();
}

int scenario_invasion_count_active_from_buffer(buffer *buf)
{
    size_t size = buffer_load_dynamic_array(buf);

    int num_invasions = 0;

    for (size_t i = 0; i < size; i++) {
        if (buffer_read_i16(buf) != INVASION_TYPE_NONE) {
            num_invasions++;
        }
        buffer_skip(buf, INVASIONS_STRUCT_SIZE_CURRENT - sizeof(int16_t));
    }

    return num_invasions;
}

void scenario_invasion_load_state_old_version(buffer *buf, invasion_old_state_sections section)
{
    if (section == INVASION_OLD_STATE_FIRST_SECTION) {
        resize_invasions(MAX_ORIGINAL_INVASIONS);
        for (invasion_t &invasion : data.invasions) {
            invasion.year = buffer_read_i16(buf);
        }
        for (invasion_t &invasion : data.invasions) {
            invasion.type = buffer_read_i16(buf);
        }
        for (invasion_t &invasion : data.invasions) {
            invasion.amount.min = buffer_read_i16(buf);
            invasion.amount.max = invasion.amount.min;
        }
        for (invasion_t &invasion : data.invasions) {
            invasion.from = buffer_read_i16(buf);
        }
        for (invasion_t &invasion : data.invasions) {
            invasion.attack_type = buffer_read_i16(buf);
        }
    } else if (section == INVASION_OLD_STATE_LAST_SECTION) {
        for (invasion_t &invasion : data.invasions) {
            invasion.month = buffer_read_u8(buf);
        }
        trim_invasions();
        remove_first_invasion_if_inactive();
    }
}
