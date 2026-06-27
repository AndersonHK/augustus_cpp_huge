#pragma once

#include "building/building_fwd.h"
#include "core/buffer.h"
#include "figure/formation_type.h"
#include "figure/type.h"

#include <initializer_list>
#include <vector>
class Building;
class Figure;


#define MAX_LEGIONS 6
#define MAX_FORMATION_FIGURES 16

#define NATIVE_FORMATION 0

enum formation_attack_enum {
    FORMATION_ATTACK_FOOD_CHAIN = 0,
    FORMATION_ATTACK_GOLD_STORES = 1,
    FORMATION_ATTACK_BEST_BUILDINGS = 2,
    FORMATION_ATTACK_TROOPS = 3,
    FORMATION_ATTACK_RANDOM = 4
};

enum {
    FORMATION_COLUMN = 0,
    FORMATION_DOUBLE_LINE_1 = 1,
    FORMATION_DOUBLE_LINE_2 = 2,
    FORMATION_SINGLE_LINE_1 = 3,
    FORMATION_SINGLE_LINE_2 = 4,
    FORMATION_TORTOISE = 5,
    FORMATION_MOP_UP = 6,
    FORMATION_AT_REST = 7,
    FORMATION_ENEMY_MOB = 8,
    FORMATION_HERD = 9,
    FORMATION_ENEMY_DOUBLE_LINE = 10,
    FORMATION_ENEMY_WIDE_COLUMN = 11,
    FORMATION_ENEMY12 = 12,
    FORMATION_MAX = 13
};

struct formation_state {
    int duration_halt;
    int duration_advance;
    int duration_regroup;
};

/**
 * Formation data
 */
struct formation {
    unsigned int id; /**< ID of the formation */
    int faction_id; /**< 1 = player, 0 = everyone else */

    /* General variables */
    int in_use; /**< Flag whether this entry is in use */
    int is_herd; /**< Flag to indicate herd */
    int is_legion; /**< Flag to indicate (own) legion */
    int legion_id; /**< Legion ID > 0 for own troops */
    int layout;
    int direction;
    int orientation;

    int morale;
    int months_from_home;
    int months_low_morale;
    int months_very_low_morale;

    /* Figures */
    int figure_type; /**< Type of figure in this formation */
    int num_figures; /**< Current number of figures in the formation */
    int max_figures; /**< Maximum number of figures */
    const FormationType *formation_type_definition; /**< Runtime declaration; restored from the owning fort. */
    int figures[MAX_FORMATION_FIGURES]; /**< Figure IDs */
    int total_damage; /**< Total damage of all figures added */
    int max_total_damage; /**< Maximum total damage of all figures added */

    /* Position */
    int x; // legions - x position of the fort
    int y; // legions - y position of the fort
    int x_home; // legions - x position of the formation RIGHT NOW
    int y_home; // legions - y position of the formation RIGHT NOW
    int building_id; // legions - Building ID of home fort
    int standard_x; // legions - x position of the DESTINATION. Standard means 'flag' in this case
    int standard_y; // legions - y position of the DESTINATION. Standard means 'flag' in this case
    int standard_figure_id; // Figure id of the standard slag - position in the array of figures
    int destination_x; //for enemy and animals
    int destination_y; //for enemy and animals
    int destination_building_id;

    /* Movement */
    int wait_ticks;
    int is_halted;
    int is_moving;
    int is_charging; //state of moving for at least 4 tiles
    int recent_fight;
    int unknown_fired;
    int missile_fired;
    int missile_attack_timeout;
    int missile_attack_formation_id;
    int started_moving_from_grid_offset;
    int halted_at_grid_offset;
    int traveled_tiles;
    int halted_for_months;


    /* Legion-related */
    int empire_service; /**< Flag to indicate this legion is selected for empire service */
    int in_distant_battle; /**< Flag to indicate this legion is away in a distant battle */
    int cursed_by_mars; /**< Flag to indicate this legion is cursed */
    int has_military_training; /**< Flag to indicate this legion has had military training */
    int legion_recruit_type; /**< Recruit type: none if this legion is fully occupied */
    int is_at_fort; /**< Flag to indicate this legion is resting at the fort */
    int mess_hall_max_morale_modifier; /**< Mess hall bonus */
    int legion_flag_id; //image id connecting the legion to their unique symbol
    int legion_name_id; //text id connecting the legion to their unique name
    int legion_name_group; //text id for the group

    /* Enemy-related */
    int enemy_type;
    int enemy_legion_index;
    int attack_type;
    int invasion_id;
    int invasion_sequence;
    formation_state enemy_state;

    /* Herd-related */
    int herd_direction;
    int herd_wolf_spawn_delay;

    struct {
        int layout;
        int x_home;
        int y_home;
    } prev;

    unsigned int target_formation_id;

    void set_formation_type(const FormationType *definition)
    {
        formation_type_definition = definition;
        if (definition) {
            max_figures = definition->capacity();
        }
    }

    const FormationType *formation_type() const
    {
        return formation_type_definition;
    }

    void bind_legion_definition_from_fort(const Building &fort);
    void refresh_legion_definition_from_home();
    void initialize_legion_from_fort(const Building &fort, int assigned_legion_id);

    ::figure_type figure_type_id() const
    {
        return static_cast<::figure_type>(figure_type);
    }

    bool has_figure_type(::figure_type type) const
    {
        return figure_type_id() == type;
    }

    bool has_any_figure_type(std::initializer_list<::figure_type> types) const
    {
        for (::figure_type type : types) {
            if (has_figure_type(type)) {
                return true;
            }
        }
        return false;
    }

    int declared_capacity() const
    {
        if (formation_type_definition) {
            return formation_type_definition->capacity();
        }
        return max_figures > 0 ? max_figures : MAX_FORMATION_FIGURES;
    }

    int slot_capacity() const
    {
        const int capacity = declared_capacity();
        if (capacity <= 0 || capacity > MAX_FORMATION_FIGURES) {
            return MAX_FORMATION_FIGURES;
        }
        return capacity;
    }

    int figure_count() const
    {
        if (num_figures <= 0) {
            return 0;
        }
        const int capacity = slot_capacity();
        return num_figures < capacity ? num_figures : capacity;
    }

    int total_figure_count() const
    {
        return num_figures;
    }

    bool has_figures() const
    {
        return num_figures > 0;
    }

    int has_open_slot() const
    {
        return num_figures < slot_capacity();
    }

    int is_full() const
    {
        return num_figures >= slot_capacity();
    }

    int overflow_count() const
    {
        const int overflow = num_figures - slot_capacity();
        return overflow > 0 ? overflow : 0;
    }

    bool counts_for_legion_type(::figure_type type) const
    {
        return is_legion && (type == FIGURE_FORT_STANDARD || has_figure_type(type));
    }

    bool should_update_herd(bool include_empty_wolf_spawns) const
    {
        return in_use && is_herd && !is_legion && (has_figures() || include_empty_wolf_spawns);
    }

    int base_morale_limit() const;
    int declared_recruit_type() const;
    int legion_distant_battle_strength_factor() const;
    int legion_curse_weight() const;
    std::vector<int> layout_grid_offsets() const;
    int legacy_storage_slot_count() const;
    void write_legacy_figure_slots(buffer *buf) const;
    void read_legacy_figure_slots(buffer *buf);

    void clear_roster()
    {
        for (int &figure_id : figures) {
            figure_id = 0;
        }
        num_figures = 0;
        total_damage = 0;
        max_total_damage = 0;
    }

    int assign_figure_to_open_slot(int figure_id)
    {
        const int capacity = slot_capacity();
        for (int slot = 0; slot < capacity; slot++) {
            if (!figures[slot]) {
                figures[slot] = figure_id;
                return slot;
            }
        }
        // Large invasions can overflow the fixed legacy array; spread overflow soldiers across existing slots.
        return figure_id % capacity;
    }

    int add_figure_to_roster(int figure_id, int deployed, int damage, int max_damage)
    {
        num_figures++;
        total_damage += damage;
        max_total_damage += max_damage;
        if (deployed) {
            is_at_fort = 0;
        }
        return assign_figure_to_open_slot(figure_id);
    }

    int first_figure_id() const
    {
        const int capacity = slot_capacity();
        for (int slot = 0; slot < capacity; slot++) {
            if (figures[slot]) {
                return figures[slot];
            }
        }
        return 0;
    }

    Figure *first_figure() const;
    Figure *first_alive_figure() const;
    int count_alive_figures() const;
    int count_figures_in_action(int action_state) const;
    bool has_figure_in_action(int action_state) const;
    bool has_figure_attacking_live_legion() const;
    bool is_fully_in_city() const;
    void kill_figures() const;
    int kill_alive_figures(int limit) const;
    void set_all_figures_action(int action_state) const;
    void set_alive_figures_action(int action_state, bool mark_at_rest = false) const;
    void set_non_combat_figures_action(int action_state, bool remove_route = false) const;
    void reset_non_combat_figures_action(int action_state) const;
    void move_herd_animals(int attacking_animals) const;
    void mark_overflow_figures_returning_to_barracks() const;
    bool prepare_legion_to_move();
    void send_non_combat_figures_to_standard();
    void send_non_combat_figures_to_fort();

    template <typename Visitor>
    void for_each_figure_id(Visitor visitor) const
    {
        const int capacity = slot_capacity();
        for (int slot = 0; slot < capacity; slot++) {
            if (figures[slot]) {
                visitor(figures[slot], slot);
            }
        }
    }

    template <typename Visitor>
    void for_each_figure_id_reverse(Visitor visitor) const
    {
        for (int slot = slot_capacity() - 1; slot >= 0; slot--) {
            if (figures[slot]) {
                visitor(figures[slot], slot);
            }
        }
    }

    template <typename Predicate>
    bool any_figure_id(Predicate predicate) const
    {
        const int capacity = slot_capacity();
        for (int slot = 0; slot < capacity; slot++) {
            if (figures[slot] && predicate(figures[slot], slot)) {
                return true;
            }
        }
        return false;
    }

};

void formations_clear(void);

void formation_clear(int formation_id);

formation *formation_create_legion(const Building &fort);
int formation_create_herd(figure_type type, int x, int y, int num_animals);
int formation_create_enemy(figure_type type, int x, int y, int layout, int orientation,
                           int enemy_type, int attack_type, int invasion_id, int invasion_sequence);

formation *formation_get(int formation_id);
int formation_count(void);

unsigned int formation_get_selected(void);
void formation_set_selected(int formation_id);

int formation_update_halted_state(formation *m);
int formation_update_movement_state(formation *m);
int formation_update_charge_state(formation *m);

void formation_update_movement_all_states(formation *m);

int formation_is_halted(const formation *m);
int formation_is_moving(const formation *m);
int formation_is_charging(const formation *m);

void formation_toggle_empire_service(int formation_id);

void formation_record_missile_fired(formation *m);
void formation_record_missile_attack(formation *m, int from_formation_id);
void formation_record_fight(formation *m);

int formation_grid_offset_for_invasion(int invasion_sequence);

void formation_caesar_pause(void);

void formation_caesar_retreat(void);

int formation_get_num_legions_cached(void);
void formation_calculate_legion_totals(void);

int formation_get_num_legions(void);
int formation_get_max_legions(void);

int formation_for_legion(int legion_index);

void formation_change_morale(formation *m, int amount);
int formation_has_low_morale(formation *m);
void formation_update_morale_after_death(formation *m);

void formation_change_all_legions_morale(int amount);

void formation_update_monthly_morale_deployed(void);
void formation_update_monthly_morale_at_rest(void);
void formation_decrease_monthly_counters(formation *m);
void formation_clear_monthly_counters(formation *m);

void formation_set_destination(formation *m, int x, int y);
void formation_set_destination_building(formation *m, int x, int y, const Building *building);
void formation_set_home(formation *m, int x, int y);
void formation_retreat(formation *m);

int formation_legion_count_alive_soldiers(int formation_id);
int formation_legion_count_alive_soldiers_by_type(figure_type type);
void formation_move_herds_away(int x, int y);

void formation_calculate_figures(void);

void formation_update_all(int second_time);
void formation_refresh_runtime_definitions(void);

void formations_save_state(buffer *buf, buffer *totals);
void formations_load_state(buffer *buf, buffer *totals, int version);

