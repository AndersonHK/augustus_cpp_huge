#pragma once

#include "building/building_fwd.h"
#include "core/buffer.h"
#include "core/relationship.h"
#include "figure/formation_member_movement_plan.h"
#include "figure/FormationDestination.h"
#include "figure/formation_type.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/type.h"

#include <algorithm>
#include <functional>
#include <vector>
class Building;
class Figure;
struct formation;

class FormationRelation {
public:
    FormationRelation(formation *owner, const char *role);
    FormationRelation(const FormationRelation &) = delete;
    FormationRelation &operator=(const FormationRelation &other);
    formation *get_ptr() const;
    void retarget(formation &target);
    void clear();
    unsigned int save_id() const;

private:
    formation *owner_ = nullptr;
    ObjectRelationship<formation, formation> relationship_;
};


#define MAX_LEGIONS 6

enum class FormationFortLinkFailure {
    None,
    MissingFormation,
    MissingFort,
    InactiveFort,
    InactiveFormation,
    NotLegion,
    MissingFormationType,
    ConflictingFormationId,
    ConflictingFortId
};

struct FormationFortLinkState {
    unsigned int formation_id = 0;
    unsigned int fort_id = 0;
    int fort_is_live = 0;
    int formation_in_use = 0;
    int formation_is_legion = 0;
    int has_resolved_formation_type = 0;
    int saved_fort_formation_id = 0;
    int saved_formation_fort_id = 0;
};

inline FormationFortLinkFailure validate_formation_fort_link(const FormationFortLinkState &state)
{
    if (!state.formation_id) {
        return FormationFortLinkFailure::MissingFormation;
    }
    if (!state.fort_id) {
        return FormationFortLinkFailure::MissingFort;
    }
    if (!state.fort_is_live) {
        return FormationFortLinkFailure::InactiveFort;
    }
    if (!state.formation_in_use) {
        return FormationFortLinkFailure::InactiveFormation;
    }
    if (!state.formation_is_legion) {
        return FormationFortLinkFailure::NotLegion;
    }
    if (!state.has_resolved_formation_type) {
        return FormationFortLinkFailure::MissingFormationType;
    }
    if (state.saved_fort_formation_id != 0 &&
        static_cast<unsigned int>(state.saved_fort_formation_id) != state.formation_id) {
        return FormationFortLinkFailure::ConflictingFormationId;
    }
    if (state.saved_formation_fort_id != 0 &&
        static_cast<unsigned int>(state.saved_formation_fort_id) != state.fort_id) {
        return FormationFortLinkFailure::ConflictingFortId;
    }
    return FormationFortLinkFailure::None;
}

inline const char *formation_fort_link_failure_text(FormationFortLinkFailure failure)
{
    switch (failure) {
        case FormationFortLinkFailure::None: return "none";
        case FormationFortLinkFailure::MissingFormation: return "formation has no runtime id";
        case FormationFortLinkFailure::MissingFort: return "fort has no runtime id";
        case FormationFortLinkFailure::InactiveFort: return "fort is not live";
        case FormationFortLinkFailure::InactiveFormation: return "formation is inactive";
        case FormationFortLinkFailure::NotLegion: return "formation is not a legion";
        case FormationFortLinkFailure::MissingFormationType: return "fort has no resolved FormationType";
        case FormationFortLinkFailure::ConflictingFormationId: return "fort references a different formation";
        case FormationFortLinkFailure::ConflictingFortId: return "formation references a different fort";
    }
    return "unknown formation-fort link failure";
}

#define NATIVE_FORMATION 0

enum formation_attack_enum {
    FORMATION_ATTACK_FOOD_CHAIN = 0,
    FORMATION_ATTACK_GOLD_STORES = 1,
    FORMATION_ATTACK_BEST_BUILDINGS = 2,
    FORMATION_ATTACK_TROOPS = 3,
    FORMATION_ATTACK_RANDOM = 4
};

struct formation_state {
    int duration_halt;
    int duration_advance;
    int duration_regroup;
};

/**
 * Formation data
 */
struct formation : public RelationshipEndpoint {
    unsigned int id; /**< ID of the formation */
    int faction_id; /**< 1 = player, 0 = everyone else */

    /* General variables */
    int in_use; /**< Flag whether this entry is in use */
    int is_herd; /**< Flag to indicate herd */
    int is_legion; /**< Flag to indicate (own) legion */
    int legion_id; /**< Legion ID > 0 for own troops */
    const FormationLayoutDef *layout_definition; /**< Runtime layout; legacy numeric ids exist only at bridges. */
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
    std::vector<int> figures; /**< Figure IDs, indexed by resolved formation slot. */
    FormationMemberMovementPlan member_movement_plan;
    bool roster_compaction_pending = true; /**< Derived from roster changes; never serialized. */
    int total_damage; /**< Total damage of all figures added */
    int max_total_damage; /**< Maximum total damage of all figures added */

    /* Position */
    int x; // legions - x position of the fort
    int y; // legions - y position of the fort
    int x_home; // legions - x position of the formation RIGHT NOW
    int y_home; // legions - y position of the formation RIGHT NOW
    int building_id; // legions - Building ID of home fort
    int standard_x; // legions - x origin of the authoritative commanded station plan
    int standard_y; // legions - y origin of the authoritative commanded station plan
    FormationDestination standard{ this }; // Ephemeral, non-physical presentation of the commanded destination.
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
    int herd_spawn_delay;

    struct {
        const FormationLayoutDef *layout_definition;
        int x_home;
        int y_home;
    } prev;

    FormationRelation target_formation{ this, "formation.target" };

    void bind_definition(const FormationType &definition)
    {
        formation_type_definition = &definition;
        is_herd = definition.spawn.role == FormationSpawnRole::Herd;
        max_figures = definition.capacity();
        ensure_roster_capacity(declared_capacity());
        invalidate_member_movement_plan();
    }

    FormationLayoutPosition layout_position(int index, const FormationLayoutDef *layout_override = nullptr) const;
    FormationMemberMovementResult move_member_to_slot(
        Figure &figure, FormationMemberDestination destination, int num_ticks, int tick_percentage);
    void rebuild_member_movement_plan(FormationMemberDestination destination, const FormationLayoutDef &layout,
        int origin_x, int origin_y, int area_width, int area_height);
    void resolve_member_movement_station(Figure &figure);
    int member_station_route_distance(
        const Route::DistanceQuery &route, const FigureMovementDestination &candidate) const;
    FormationMemberMovementResult settle_member(Figure &figure, FormationMemberDestination destination,
        const FormationLayoutDef &layout, FormationMemberMovementResult result) const;
    void invalidate_member_movement_plan() { member_movement_plan.invalidate(); }
    void compact_idle_roster();
    bool has_incoming_recruit() const;
    bool is_commanded_home() const { return !in_distant_battle && standard_x == x && standard_y == y; }
    bool can_receive_recruit() const;
    int available_layout_count() const
    {
        return (figure_type == FIGURE_FORT_LEGIONARY || figure_type == FIGURE_FORT_INFANTRY) && !has_military_training ? 3 : 5;
    }

    bool set_layout(const char *key)
    {
        const FormationLayoutDef *definition = formation_layout_registry_impl::find_layout(key);
        if (!definition) {
            return false;
        }
        if (layout_definition != definition) {
            layout_definition = definition;
            roster_compaction_pending = true;
            invalidate_member_movement_plan();
        }
        return true;
    }

    bool set_layout_from_legacy_id(int legacy_id)
    {
        const FormationLayoutDef *definition = formation_layout_from_legacy_id(legacy_id);
        layout_definition = definition;
        roster_compaction_pending = true;
        invalidate_member_movement_plan();
        return definition != nullptr;
    }

    bool uses_layout(const char *key) const
    {
        return layout_definition && layout_definition->matches_key(key);
    }

    bool bind_to_fort(Building &fort, const char **failure_reason = nullptr);
    void unbind_from_fort();
    void remove();
    Building *fort() const;
    bool refresh_legion_definition_from_home();
    bool reconcile_loaded_legion_command();
    bool initialize_legion_from_fort(Building &fort, int assigned_legion_id);
    bool owns_figure(const Figure &figure) const;

    ::figure_type figure_type_id() const
    {
        return static_cast<::figure_type>(figure_type);
    }

    bool has_figure_type(::figure_type type) const
    {
        return figure_type_id() == type;
    }

    int declared_capacity() const
    {
        require_definition("declared_capacity");
        return formation_type_definition->capacity();
    }

    bool has_figures() const
    {
        return num_figures > 0;
    }

    int has_open_slot() const
    {
        const int capacity = declared_capacity();
        return capacity <= 0 || num_figures < capacity;
    }

    bool matches_player_troop_query(::figure_type type) const
    {
        return is_legion && (type == FIGURE_FORT_STANDARD || has_figure_type(type));
    }

    bool is_roster_member(const Figure &figure) const;

    int modified_combat_value(FormationCombatStat stat, int base_value) const;
    void update_movement_states();
    void update_direction(const Figure &first_figure);
    void update_herd(bool reproduction_allowed);
    void update_herd_member(Figure &member) const;
    void update_herd_member_graphics(Figure &member) const;
    bool is_aggressive_herd() const;
    bool find_herd_roaming_destination(const FormationHerdBehavior &behavior, int *x_tile, int *y_tile) const;
    void change_morale(int amount);
    void update_morale_after_death();
    void decrease_monthly_counters();
    void clear_monthly_counters();
    void set_destination(int tile_x, int tile_y);
    void set_destination(int tile_x, int tile_y, const Building *building);
    void set_station_origin(int tile_x, int tile_y);
    void set_distant_battle(int value);
    void record_missile_attack(int from_formation_id);
    void pursue_target();
    void toggle_empire_service() { empire_service ^= 1; }
    void record_missile_fired() { missile_fired = 6; }
    void record_fight() { recent_fight = 6; }
    bool has_low_morale() const { return months_low_morale || months_very_low_morale; }
    void set_home(int tile_x, int tile_y) { x_home = tile_x; y_home = tile_y; }
    void retreat() { months_low_morale = 1; }
    bool recruit_requires_weapon() const;
    int base_morale_limit() const;
    int barracks_recruit_capacity() const;
    int barracks_recruit_overflow_count() const;
    int declared_recruit_type() const;
    int distant_battle_strength() const;
    int curse_weight() const;
    std::vector<int> layout_grid_offsets() const;
    void ensure_roster_capacity(int capacity);
    int roster_figure_id(int slot) const;
    void write_legacy_figure_slots(buffer *buf) const;
    void read_legacy_figure_slots(buffer *buf);
    void write_extended_figure_slots(buffer *buf) const;
    void read_extended_figure_slots(buffer *buf, int formation_buf_size);
    void require_definition(const char *operation) const;
    void prepare_roster_refresh();
    int publish_figure(Figure &figure);
    void refresh_published_figure(Figure &figure);
    void unpublish_figure(Figure &figure);

    int first_figure_id() const
    {
        for (int slot = 0; slot < static_cast<int>(figures.size()); slot++) {
            const int figure_id = roster_figure_id(slot);
            if (figure_id) {
                return figure_id;
            }
        }
        return 0;
    }

    Figure *first_figure() const;
    Figure *first_alive_figure() const;
    void for_each_figure(const std::function<void(Figure &, int)> &visitor) const;
    void for_each_alive_figure(const std::function<void(Figure &, int)> &visitor) const;
    int count_alive_figures() const;
    int count_figures_in_action(int action_state) const;
    bool has_figure_in_action(int action_state) const;
    bool has_figure_attacking_live_legion() const;
    bool is_fully_in_city() const;
    void kill_figures() const;
    int kill_alive_figures(int limit) const;
    void set_all_figures_action(int action_state) const;
    void set_alive_figures_action(int action_state) const;
    void set_non_combat_figures_action(int action_state, bool remove_route = false) const;
    void reset_non_combat_figures_action(int action_state) const;
    void move_herd_animals(int attacking_animals) const;
    void mark_barracks_recruit_overflow_returning() const;
    bool prepare_legion_to_move();
    void send_non_combat_figures_to_standard();
    void send_non_combat_figures_to_fort();

    template <typename Visitor>
    void for_each_figure_id(Visitor visitor) const
    {
        for (int slot = 0; slot < static_cast<int>(figures.size()); slot++) {
            const int figure_id = roster_figure_id(slot);
            if (figure_id) {
                visitor(figure_id, slot);
            }
        }
    }

    template <typename Visitor>
    void for_each_figure_id_reverse(Visitor visitor) const
    {
        for (int slot = static_cast<int>(figures.size()) - 1; slot >= 0; slot--) {
            const int figure_id = roster_figure_id(slot);
            if (figure_id) {
                visitor(figure_id, slot);
            }
        }
    }

    template <typename Predicate>
    bool any_figure_id(Predicate predicate) const
    {
        for (int slot = 0; slot < static_cast<int>(figures.size()); slot++) {
            const int figure_id = roster_figure_id(slot);
            if (figure_id && predicate(figure_id, slot)) {
                return true;
            }
        }
        return false;
    }

};

inline FormationRelation::FormationRelation(formation *owner, const char *role) : owner_(owner), relationship_(role) {}
inline FormationRelation &FormationRelation::operator=(const FormationRelation &)
{
    if (owner_) relationship_.clear(*owner_, RelationshipDisconnectReason::EndpointReassigned);
    return *this;
}
inline formation *FormationRelation::get_ptr() const { return relationship_.get_ptr(); }
inline void FormationRelation::retarget(formation &target) { relationship_.retarget(*owner_, &target); }
inline void FormationRelation::clear() { relationship_.clear(*owner_); }
inline unsigned int FormationRelation::save_id() const { return relationship_ ? relationship_->id : 0; }

void formations_clear(void);

formation *formation_create_legion(Building &fort);
int formation_create_herd(const FormationType &definition, int x, int y);
int formation_create_enemy(figure_type type, int x, int y, int layout, int orientation,
                           int enemy_type, int attack_type, int invasion_id, int invasion_sequence);

formation *formation_get(int formation_id);
int formation_count(void);

unsigned int formation_get_selected(void);
void formation_set_selected(int formation_id);

int formation_grid_offset_for_invasion(int invasion_sequence);

void formation_caesar_pause(void);

void formation_caesar_retreat(void);

int formation_get_num_legions_cached(void);
void formation_calculate_legion_totals(void);

int formation_get_num_legions(void);
int formation_get_max_legions(void);

int formation_for_legion(int legion_index);

void formation_update_monthly_morale_deployed(void);
void formation_update_monthly_morale_at_rest(void);

int formation_legion_count_alive_soldiers_by_type(figure_type type);
void formation_move_herds_away(int x, int y);

void formation_calculate_figures(void);

void formation_update_all(int second_time);
int formation_refresh_runtime_definitions(void);
int formation_finish_load_bridge(void);

void formations_save_state(buffer *buf, buffer *totals);
void formations_load_state(buffer *buf, buffer *totals, int version);
