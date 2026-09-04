#pragma once

#include "figure/formation_layout.h"
#include "figure/unit_type.h"

#include <memory>
#include <string>
#include <vector>

struct FormationTypeSlot {
    int x = 0;
    int y = 0;
    const UnitType *unit = nullptr;
};

struct FormationSlotOffset {
    int x = 0;
    int y = 0;
};

enum class FormationStationAlignment {
    Unspecified,
    ScaledTileAnchor,
    TileAnchor
};

struct FormationCombatModifiers {
    int melee_attack = 0;
    int melee_defense = 0;
    int morale = 0;
    int missile_damage = 0;
    int distant_battle_strength = -1;
    int trained_distant_battle_bonus = -1;
    int curse_weight_per_figure = -1;
};

class FormationType;

enum class FormationSpawnRole {
    None,
    Herd,
    Enemy
};

enum class FormationHerdMovementSound {
    None,
    WolfHowl
};

enum class FormationHerdCombatAnimation {
    None,
    Move,
    Attack,
    Rest
};

struct FormationHerdBehavior {
    int roam_distance = 0;
    int roam_delay = 0;
    int reproduction_delay = 0;
    int member_rest_delay = 0;
    int member_move_speed = 0;
    int member_animation_frames = 0;
    bool aggressive = false;
    bool allow_negative_desirability = false;
    bool alternate_rest_animation = false;
    FormationHerdMovementSound movement_sound = FormationHerdMovementSound::None;
    FormationHerdCombatAnimation combat_animation = FormationHerdCombatAnimation::None;
};

struct FormationSpawn {
    FormationSpawnRole role = FormationSpawnRole::None;
    int climate = -1;
    int initial_count = 0;
    FormationHerdBehavior herd;
};

enum class FormationCombatStat {
    MeleeAttack,
    MeleeDefense,
    Morale,
    MissileDamage
};

class FormationType {
public:
    explicit FormationType(std::string key);

    FormationSpawn spawn;

    const char *key() const;
    bool matches_key(const char *key) const;

    void set_grid(int width, int height, FormationStationAlignment station_alignment);
    void set_recruit_capacity(int capacity);
    void set_combat_modifiers(const FormationCombatModifiers &modifiers);
    int capacity() const;
    int recruit_capacity() const;
    bool layout_positions(const FormationLayoutDef &layout, int live_slot_count, int declared_capacity, std::vector<FormationLayoutPosition> *positions) const;
    bool resolve_station_offset(const FormationLayoutDef &layout, int index, int declared_capacity,
        int area_width, int area_height, FormationSlotOffset *offset) const;

    int modified_combat_value(FormationCombatStat stat, int base_value) const;
    int distant_battle_strength(bool trained) const;
    int curse_weight(int figures) const;

    bool has_grid() const;
    bool has_slots() const;
    bool has_valid_slots() const;
    bool has_duplicate_slots() const;
    bool contains_row_range(int first_row, int last_row) const;
    bool can_spawn_figures_directly() const;

    bool add_slot(int x, int y, const char *unit_key);
    bool fill_slots(const char *unit_key);
    bool add_slots_in_row_range(int first_row, int last_row, const char *unit_key);
    const UnitType *primary_unit() const;

private:
    friend struct formation;

    bool try_layout_position(const FormationLayoutDef &layout, int index, int declared_capacity, FormationLayoutPosition *position) const;
    bool contains(int x, int y) const;
    int slot_index(int x, int y) const;

    std::string key_;
    int width_ = 0;
    int height_ = 0;
    int recruit_capacity_ = 0;
    FormationStationAlignment station_alignment_ = FormationStationAlignment::Unspecified;
    FormationCombatModifiers combat_modifiers_;
    std::vector<FormationTypeSlot> slots_;
};

namespace formation_type_registry_impl {

const FormationType *find_formation_type(const char *key);
const FormationType *find_spawn_formation(figure_type type);
const FormationType *find_herd_spawn_for_location(int climate, int x, int y);
const std::vector<std::unique_ptr<FormationType>> &formation_types();

} // namespace formation_type_registry_impl

const char *formation_type_registry_get_failure_reason(void);
const char *formation_type_definition_source_path(const char *key);
int formation_type_definition_is_suppressed(const char *key);
int formation_type_registry_load(void);
void formation_type_registry_reset(void);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
} formation_type_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_capacity;
    int queried_recruit_capacity;
    int queried_melee_attack_modifier;
    int queried_melee_defense_modifier;
    int queried_morale_modifier;
    int queried_missile_damage_modifier;
    int queried_spawn_role;
    int queried_spawn_climate;
    int queried_spawn_count;
    int queried_herd_roam_distance;
    int queried_herd_roam_delay;
    int queried_herd_reproduction_delay;
    int queried_herd_member_rest_delay;
    int queried_herd_member_move_speed;
    int queried_herd_member_animation_frames;
    int queried_herd_aggressive;
    int queried_herd_allow_negative_desirability;
    int queried_herd_alternate_rest_animation;
    int queried_herd_movement_sound;
    int queried_herd_combat_animation;
    int queried_source_layer;
} formation_type_layer_test_result;

int formation_type_layered_definition_buffers_are_valid_for_test(
    const formation_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_type_layer_test_result *result);
#endif
