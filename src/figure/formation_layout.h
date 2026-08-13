#pragma once

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace formation_layout_legacy {

// Stable scenario/save ids. Runtime formations bind FormationLayoutDef objects.
constexpr int COLUMN = 0;
constexpr int DOUBLE_LINE_1 = 1;
constexpr int DOUBLE_LINE_2 = 2;
constexpr int SINGLE_LINE_1 = 3;
constexpr int SINGLE_LINE_2 = 4;
constexpr int TORTOISE = 5;
constexpr int MOP_UP = 6;
constexpr int AT_REST = 7;
constexpr int ENEMY_MOB = 8;
constexpr int HERD = 9;
constexpr int ENEMY_DOUBLE_LINE = 10;
constexpr int ENEMY_WIDE_COLUMN = 11;
constexpr int ENEMY_12 = 12;
constexpr int COUNT = 13;

} // namespace formation_layout_legacy

struct FormationLayoutFootprint {
    int width;
    int height;
};

struct FormationLayoutPosition {
    int x;
    int y;
};

class FormationLayoutDef {
public:
    using ArmyOffsets = std::array<std::vector<FormationLayoutPosition>, 4>;

    FormationLayoutDef(
        std::string key,
        int legacy_id,
        std::vector<FormationLayoutPosition> positions,
        ArmyOffsets army_offsets,
        std::string army_offsets_reference);

    const char *key() const;
    bool matches_key(const char *key) const;
    int legacy_id() const;
    int authored_position_count() const;
    FormationLayoutPosition authored_position(int index) const;
    bool has_authored_army_offsets() const;
    const char *army_offsets_reference() const;
    void bind_army_offsets_reference(const FormationLayoutDef *layout);
    FormationLayoutPosition army_offset(int orientation, int formation_index) const;

private:
    std::string key_;
    int legacy_id_ = 0;
    std::vector<FormationLayoutPosition> positions_;
    ArmyOffsets army_offsets_;
    std::string army_offsets_reference_;
    const FormationLayoutDef *army_offsets_definition_ = nullptr;
};

namespace formation_layout_registry_impl {

const FormationLayoutDef *find_layout(const char *key);
const FormationLayoutDef *find_layout_by_legacy_id(int legacy_id);
const std::vector<std::unique_ptr<FormationLayoutDef>> &layouts();

} // namespace formation_layout_registry_impl

const char *formation_layout_registry_get_failure_reason(void);
const char *formation_layout_definition_source_path(const char *key);
int formation_layout_definition_is_suppressed(const char *key);
int formation_layout_registry_load(void);
void formation_layout_registry_reset(void);

// Legacy numeric values enter and leave runtime only through these bridges.
const FormationLayoutDef *formation_layout_from_legacy_id(int legacy_id);
int formation_layout_to_legacy_id(const FormationLayoutDef *layout);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
} formation_layout_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_legacy_id;
    int queried_position_count;
    int queried_source_layer;
} formation_layout_layer_test_result;

int formation_layout_layered_definition_buffers_are_valid_for_test(
    const formation_layout_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_layout_layer_test_result *result);
#endif

FormationLayoutFootprint formation_layout_legacy_footprint();

FormationLayoutPosition formation_layout_position(
    const FormationLayoutDef *layout,
    int index,
    int declared_capacity);

FormationLayoutPosition formation_layout_position(
    const FormationLayoutDef *layout,
    int index,
    int declared_capacity,
    FormationLayoutFootprint footprint);

std::vector<FormationLayoutPosition> formation_layout_positions(
    const FormationLayoutDef *layout,
    int live_slot_count,
    int declared_capacity);

std::vector<FormationLayoutPosition> formation_layout_positions(
    const FormationLayoutDef *layout,
    int live_slot_count,
    int declared_capacity,
    FormationLayoutFootprint footprint);
