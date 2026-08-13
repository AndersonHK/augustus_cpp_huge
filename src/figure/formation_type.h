#pragma once

#include "figure/formation_layout.h"
#include "figure/unit_type.h"

#include <memory>
#include <string>
#include <vector>

struct FormationTypeSlot {
    int x = 0;
    int y = 0;
    std::string unit_key;
    const UnitType *unit = nullptr;
};

class FormationType {
public:
    explicit FormationType(std::string key);

    const char *key() const;
    bool matches_key(const char *key) const;

    void set_grid(int width, int height);
    void set_recruit_capacity(int capacity);
    int capacity() const;
    int recruit_capacity() const;
    FormationLayoutFootprint layout_footprint() const;

    bool has_grid() const;
    bool has_slots() const;
    bool has_valid_slots() const;
    bool has_duplicate_slots() const;
    bool contains_row_range(int first_row, int last_row) const;

    bool add_slot(int x, int y, const char *unit_key);
    bool fill_slots(const char *unit_key);
    bool add_slots_in_row_range(int first_row, int last_row, const char *unit_key);
    const UnitType *primary_unit() const;
    figure_type primary_figure_type() const;
    int primary_recruit_type() const;
    bool primary_unit_requires_weapon() const;

private:
    bool contains(int x, int y) const;
    int slot_index(int x, int y) const;

    std::string key_;
    int width_ = 0;
    int height_ = 0;
    int recruit_capacity_ = 0;
    std::vector<FormationTypeSlot> slots_;
};

namespace formation_type_registry_impl {

const FormationType *find_formation_type(const char *key);
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
    int queried_source_layer;
} formation_type_layer_test_result;

int formation_type_layered_definition_buffers_are_valid_for_test(
    const formation_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_type_layer_test_result *result);
#endif
