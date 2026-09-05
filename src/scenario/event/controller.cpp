#include "controller.h"

#include "core/log.h"
#include "core/string.h"   
#include "empire/city.h"
#include "game/save_version.h"
#include "map/grid.h"
#include "scenario/custom_variable.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/condition_handler.h"
#include "scenario/event/event.h"
#include "scenario/event/formula.h"
#include "scenario/event/parameter_data.h"
#include "scenario/scenario.h"
#include "widget/map_editor.h"

#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <vector>

#define SCENARIO_EVENTS_SIZE_STEP 50
#define SCENARIO_FORMULAS_SIZE_STEP 500
#define SCENARIO_ACTION_STRUCT_SIZE ((2 * sizeof(int16_t)) + (6 * sizeof(int32_t)))
#define SCENARIO_FORMULA_STRUCT_SIZE (sizeof(uint32_t) + MAX_FORMULA_LENGTH + sizeof(int32_t) + (sizeof(uint8_t) * 2) + (sizeof(int32_t) * 2))

static std::vector<scenario_event_t> scenario_events;
static std::vector<scenario_formula_t> scenario_formulas;

static void formulas_save_state(buffer *buf);
static void formulas_load_state(buffer *buf, int allow_legacy_id_repair);

static int load_dynamic_array_header(buffer *buf, const char *label, size_t *array_size, size_t *element_size)
{
    if (!buf || !buf->data || buf->size < 16 || !array_size || !element_size) {
        log_error("Malformed dynamic scenario array in save.", label, 0);
        return 0;
    }

    buffer_set(buf, 0);
    uint32_t stored_size = buffer_read_u32(buf);
    buffer_skip(buf, 4);
    *array_size = buffer_read_u32(buf);
    *element_size = buffer_read_u32(buf);

    if (buf->overflow || stored_size < 16 || stored_size > buf->size || !*element_size ||
        *array_size > (stored_size - 16) / *element_size) {
        log_error("Malformed dynamic scenario array header in save.", label, (int) *array_size);
        return 0;
    }
    return 1;
}

static int load_dynamic_payload_header(buffer *buf, const char *label)
{
    if (!buf || !buf->data || buf->size < sizeof(uint32_t)) {
        log_error("Malformed dynamic scenario payload in save.", label, 0);
        return 0;
    }

    buffer_set(buf, 0);
    uint32_t stored_size = buffer_read_u32(buf);
    if (buf->overflow || stored_size < sizeof(uint32_t) || stored_size > buf->size) {
        log_error("Malformed dynamic scenario payload header in save.", label, (int) stored_size);
        return 0;
    }
    return 1;
}

void scenario_events_init(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_init(&event);
    }
}

static void new_formula(scenario_formula_t *formula, unsigned int id)
{
    formula->id = id;
}

static scenario_formula_t *add_formula_slot()
{
    scenario_formulas.emplace_back();
    scenario_formula_t *formula = &scenario_formulas.back();
    memset(formula, 0, sizeof(scenario_formula_t));
    new_formula(formula, static_cast<unsigned int>(scenario_formulas.size() - 1));
    return formula;
}

static void trim_scenario_events()
{
    while (scenario_events.size() > 1 && !scenario_event_is_active(&scenario_events.back())) {
        scenario_event_release_contents(&scenario_events.back());
        scenario_events.pop_back();
    }
}

static scenario_event_t *add_scenario_event_slot()
{
    for (size_t i = 0; i < scenario_events.size(); ++i) {
        scenario_event_t *event = &scenario_events[i];
        if (!scenario_event_is_active(event)) {
            scenario_event_release_contents(event);
            scenario_event_new(event, static_cast<unsigned int>(i));
            return event;
        }
    }
    scenario_events.emplace_back();
    scenario_event_t *event = &scenario_events.back();
    memset(event, 0, sizeof(scenario_event_t));
    scenario_event_new(event, static_cast<unsigned int>(scenario_events.size() - 1));
    return event;
}

unsigned int scenario_formula_add(const uint8_t *formatted_calculation, int min_limit, int max_limit)
{
    scenario_formula_t *calculation = add_formula_slot();

    if (!calculation) {
        log_error("Unable to allocate memory for a new formula. The game will now crash.", 0, 0);
        return 0;
    }
    calculation->min_evaluation = min_limit;
    calculation->max_evaluation = max_limit;
    calculation->evaluation = 0;
    strncpy((char *) calculation->formatted_calculation, (const char *) formatted_calculation, MAX_FORMULA_LENGTH - 1);
    scenario_event_formula_check(calculation);
    // null termination on last char-  treating as string 
    return calculation->id;
}

void scenario_formula_change(unsigned int id, const uint8_t *formatted_calculation, int min_eval, int max_eval)
{
    if (id == 0 || id >= scenario_formulas.size()) {
        log_error("Invalid formula ID.", 0, 0);
        return;
    }
    scenario_formula_t *formula = &scenario_formulas[id];
    strncpy((char *) formula->formatted_calculation,
        (const char *) formatted_calculation, MAX_FORMULA_LENGTH - 1);
    formula->min_evaluation = min_eval; //update limits too
    formula->max_evaluation = max_eval;
    if (!scenario_event_formula_check(formula)) {
        formula->evaluation = 0;
        formula->is_error = 1;
        formula->is_static = 0;
    }
}

const uint8_t *scenario_formula_get_string(unsigned int id)
{
    if (id == 0 || id >= scenario_formulas.size()) {
        log_error("Invalid formula index.", 0, 0);
        return NULL;
    }
    scenario_formula_t *formula = &scenario_formulas[id];
    if (formula->is_error) {
        return (const uint8_t *) "Error";
    } else if (formula->is_static) {
        static uint8_t buffer[16];
        snprintf((char *) buffer, sizeof(buffer), "%d", formula->evaluation);
        return buffer;
    }
    return formula->formatted_calculation;
}

scenario_formula_t *scenario_formula_get(unsigned int id)
{
    if (id == 0 || id >= scenario_formulas.size()) {
        log_error("Invalid formula index.", 0, 0);
        return NULL;
    }
    return &scenario_formulas[id];
}

int scenario_formula_evaluate_formula(unsigned int id)
{
    if (id == 0 || id >= scenario_formulas.size()) {
        log_error("Invalid formula index.", 0, 0);
        return 0;
    }
    int evaluation = scenario_event_formula_evaluate(&scenario_formulas[id]);
    return evaluation;
}

void scenario_events_clear(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_release_contents(&event);
    }
    scenario_events.clear();
    scenario_events.reserve(SCENARIO_EVENTS_SIZE_STEP);

    scenario_formulas.clear();
    scenario_formulas.reserve(SCENARIO_FORMULAS_SIZE_STEP);
    add_formula_slot(); // Reserve ID 0 as invalid
}

scenario_event_t *scenario_event_get(int event_id)
{
    if (event_id < 0 || static_cast<size_t>(event_id) >= scenario_events.size()) {
        return 0;
    }
    return &scenario_events[event_id];
}

scenario_event_t *scenario_event_create(int repeat_min, int repeat_max, int max_repeats)
{
    if (repeat_min < 0) {
        log_error("Event minimum repeat is less than 0.", 0, 0);
        return 0;
    }
    if (repeat_max < 0) {
        log_error("Event maximum repeat is less than 0.", 0, 0);
        return 0;
    }

    if (repeat_max < repeat_min) {
        log_info("Event maximum repeat is less than its minimum. Swapping the two values.", 0, 0);
        int temp = repeat_min;
        repeat_min = repeat_max;
        repeat_max = temp;
    }

    scenario_event_t *event = add_scenario_event_slot();
    if (!event) {
        return 0;
    }
    event->state = EVENT_STATE_ACTIVE;
    event->repeat_days_min = repeat_min;
    event->repeat_days_max = repeat_max;
    event->max_number_of_repeats = max_repeats;
    event->repeat_interval = 1; // Default to every day

    return event;
}

void scenario_event_delete(scenario_event_t *event)
{
    scenario_event_release_contents(event);
    trim_scenario_events();
}

int scenario_events_get_count(void)
{
    return static_cast<int>(scenario_events.size());
}

static void info_save_state(buffer *buf)
{
    uint32_t array_size = static_cast<uint32_t>(scenario_events.size());
    uint32_t struct_size =
        (6 * sizeof(int32_t)) + // repeat_days_min, repeat_days_max, days_until_active, max_repeats, exec_count + id
        (1 * sizeof(int16_t)) + // state
        (1 * sizeof(uint8_t)) + // repeat_interval
        (2 * sizeof(uint16_t)) + // actions.size, condition_groups.size
        (EVENT_NAME_LENGTH * 2 * sizeof(char)); // name_utf8

    buffer_init_dynamic_array(buf, array_size, struct_size);

    for (scenario_event_t &event : scenario_events) {
        scenario_event_save_state(buf, &event);
    }
}

static void conditions_save_state(buffer *buf)
{
    unsigned int total_groups = 0;
    unsigned int total_conditions = 0;

    const scenario_event_t *event;
    const scenario_condition_group_t *group;

    for (const scenario_event_t &current_event : scenario_events) {
        event = &current_event;
        total_groups += scenario_event_condition_group_count(event);
        for (unsigned int j = 0; j < scenario_event_condition_group_count(event); j++) {
            group = scenario_event_condition_group_get_const(event, j);
            total_conditions += scenario_condition_group_condition_count(group);
        }
    }

    unsigned int size = total_groups * CONDITION_GROUP_STRUCT_SIZE + total_conditions * CONDITION_STRUCT_SIZE;

    buffer_init_dynamic(buf, size);

    for (const scenario_event_t &current_event : scenario_events) {
        event = &current_event;
        for (unsigned int j = 0; j < scenario_event_condition_group_count(event); j++) {
            group = scenario_event_condition_group_get_const(event, j);
            scenario_condition_group_save_state(buf, group, LINK_TYPE_SCENARIO_EVENT, event->id);
        }
    }
}

static void actions_save_state(buffer *buf)
{
    int32_t array_size = 0;
    scenario_event_t *current_event;
    for (scenario_event_t &event : scenario_events) {
        current_event = &event;
        array_size += scenario_event_action_count(current_event);
    }

    int struct_size = (2 * sizeof(int16_t)) + (6 * sizeof(int32_t));
    buffer_init_dynamic_array(buf, array_size, struct_size);

    for (unsigned int i = 0; i < scenario_events.size(); i++) {
        current_event = &scenario_events[i];

        for (unsigned int j = 0; j < scenario_event_action_count(current_event); j++) {
            scenario_action_t *current_action = scenario_event_action_get(current_event, j);
            scenario_action_type_save_state(buf, current_action, LINK_TYPE_SCENARIO_EVENT, current_event->id);
        }
    }

}

void scenario_events_save_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas)
{
    info_save_state(buf_events);
    conditions_save_state(buf_conditions);
    actions_save_state(buf_actions);
    formulas_save_state(buf_formulas);
}

static void info_load_state(buffer *buf, int scenario_version)
{
    size_t array_size = 0;
    size_t element_size = 0;
    if (!load_dynamic_array_header(buf, "scenario_events", &array_size, &element_size)) {
        return;
    }

    for (size_t i = 0; i < array_size; i++) {
        scenario_event_t *event = scenario_event_create(0, 0, 0);
        if (!event) {
            log_error("Unable to create scenario event during load.", 0, (int) i);
            return;
        }
        scenario_event_load_state(buf, event, scenario_version);
        if (buf->overflow) {
            log_error("Malformed scenario event data in save.", 0, (int) i);
            return;
        }
    }
}

static void conditions_load_state_old_version(buffer *buf)
{
    size_t total_conditions = 0;
    size_t element_size = 0;
    if (!load_dynamic_array_header(buf, "scenario_conditions", &total_conditions, &element_size)) {
        return;
    }

    for (size_t i = 0; i < total_conditions; i++) {
        buffer_skip(buf, 2); // Skip the link type
        int event_id = buffer_read_i32(buf);
        scenario_event_t *event = scenario_event_get(event_id);
        if (!event || !scenario_event_condition_group_count(event)) {
            log_error("Ignoring scenario condition linked to invalid event.", 0, event_id);
            buffer_skip(buf, CONDITION_STRUCT_SIZE);
            continue;
        }
        scenario_condition_group_t *group = scenario_event_condition_group_get(event, 0);
        scenario_condition_t *condition = scenario_condition_group_condition_add(group);
        if (!condition) {
            log_error("Unable to create legacy scenario condition during load.", 0, (int) i);
            return;
        }
        scenario_condition_load_state(buf, group, condition);
        if (buf->overflow) {
            log_error("Malformed legacy scenario condition data in save.", 0, (int) i);
            return;
        }
    }
}

static void load_link_condition_group(scenario_condition_group_t *condition_group, int link_type, int32_t link_id)
{
    switch (link_type) {
        case LINK_TYPE_SCENARIO_EVENT:
            scenario_event_link_condition_group_by_id(link_id, condition_group);
            break;
        default:
            log_error("Ignoring scenario condition group with invalid link type.", 0, link_type);
            scenario_condition_group_conditions_clear(condition_group);
            break;
    }
}

static void conditions_load_state(buffer *buf)
{
    if (!load_dynamic_payload_header(buf, "scenario_conditions")) {
        return;
    }

    int link_type = 0;
    int32_t link_id = 0;

    // Using `buffer_at_end` is a hackish way to load all the condition groups
    // However, we never stored the total condition group count anywhere and, except for some sort of corruption,
    // this should work. Regardless, this is not a good practice.
    while (!buffer_at_end(buf)) {
        scenario_condition_group_t condition_group = { 0 };
        if (!scenario_condition_group_load_state(buf, &condition_group, &link_type, &link_id)) {
            scenario_condition_group_conditions_clear(&condition_group);
            return;
        }
        load_link_condition_group(&condition_group, link_type, link_id);
    }
}

static void load_link_action(scenario_action_t *action, int link_type, int32_t link_id)
{
    switch (link_type) {
        case LINK_TYPE_SCENARIO_EVENT:
            scenario_event_link_action_by_id(link_id, action);
            break;
        default:
            log_error("Ignoring scenario action with invalid link type.", 0, link_type);
            break;
    }
}

static void actions_load_state(buffer *buf, int is_new_version)
{
    size_t array_size = 0;
    size_t element_size = 0;
    if (!load_dynamic_array_header(buf, "scenario_actions", &array_size, &element_size)) {
        return;
    }
    if (element_size < SCENARIO_ACTION_STRUCT_SIZE) {
        log_error("Malformed scenario action element size in save.", 0, (int) element_size);
        return;
    }

    int link_type = 0;
    int32_t link_id = 0;
    for (unsigned int i = 0; i < array_size; i++) {
        size_t record_start = buf->index;
        size_t record_end = record_start + element_size;
        if (record_end > buf->size) {
            log_error("Malformed scenario action record bounds in save.", 0, (int) i);
            return;
        }
        scenario_action_t action = { 0 };
        int original_id = scenario_action_type_load_state(buf, &action, &link_type, &link_id, is_new_version);
        if (buf->overflow) {
            log_error("Malformed scenario action data in save.", 0, (int) i);
            return;
        }
        load_link_action(&action, link_type, link_id);
        if (original_id) {
            unsigned int index = 1;
            while (index) {
                index = scenario_action_type_load_allowed_building(&action, original_id, index);
                load_link_action(&action, link_type, link_id);
            }
        }
        if (buf->index < record_end) {
            buffer_set(buf, record_end);
        }
    }
}

static void formulas_save_state(buffer *buf)
{
    int struct_size =
        sizeof(uint32_t)              // id
        + MAX_FORMULA_LENGTH          // formatted_calculation
        + sizeof(int32_t)             // evaluation
        + sizeof(uint8_t) * 2         // is_static + is_error
        + sizeof(int32_t) * 2;        // min_evaluation + max_evaluation
    unsigned int formula_count = scenario_formulas.empty() ? 0 : static_cast<unsigned int>(scenario_formulas.size() - 1);
    buffer_init_dynamic_array(buf, formula_count, struct_size);

    for (size_t i = 1; i < scenario_formulas.size(); ++i) {
        scenario_formula_t *formula = &scenario_formulas[i];
        buffer_write_u32(buf, formula->id);
        buffer_write_raw(buf, formula->formatted_calculation, MAX_FORMULA_LENGTH);
        buffer_write_i32(buf, formula->evaluation);
        buffer_write_u8(buf, formula->is_static);
        buffer_write_u8(buf, formula->is_error);
        buffer_write_i32(buf, formula->min_evaluation);
        buffer_write_i32(buf, formula->max_evaluation);
    }
}

static void formulas_load_state(buffer *buf, int allow_legacy_id_repair)
{
    size_t array_size = 0;
    size_t element_size = 0;
    if (!load_dynamic_array_header(buf, "scenario_formulas", &array_size, &element_size)) {
        return;
    }
    if (element_size < SCENARIO_FORMULA_STRUCT_SIZE) {
        log_error("Malformed scenario formula element size in save.", 0, (int) element_size);
        return;
    }
    if (array_size > UINT_MAX) {
        log_error("Scenario formula array is too large to load.", 0, 0);
        return;
    }

    scenario_formulas.clear();
    scenario_formulas.reserve(static_cast<size_t>(array_size) + 1);

    add_formula_slot(); // Advance once to skip index 0, which is reserved for invalid formulas
    std::vector<uint8_t> loaded_ids(1, 1);

    for (size_t i = 0; i < array_size; ++i) {
        size_t record_start = buf->index;
        size_t record_end = record_start + element_size;
        if (record_end > buf->size) {
            log_error("Malformed scenario formula record bounds in save.", 0, (int) i);
            return;
        }

        unsigned int id = buffer_read_u32(buf);
        const unsigned int expected_id = static_cast<unsigned int>(i + 1);
        if (id != expected_id) {
            char detail[128];
            snprintf(detail, sizeof(detail), "record=%u saved_id=%u expected_id=%u", static_cast<unsigned int>(i), id, expected_id);
            if (!allow_legacy_id_repair) {
                log_error("Formula ID mismatch during loading", detail, 0);
                return;
            }
            log_warning("Repairing legacy scenario formula ID layout", detail, 0);
            // Formula references historically addressed the record's stable
            // array position. Some old writers persisted an uninitialized id
            // field, so the position is authoritative during this bridge.
            id = expected_id;
        }
        if (id > 1000000) {
            log_error("Scenario formula ID is too large to load", 0, static_cast<int>(id));
            return;
        }
        while (scenario_formulas.size() <= id) {
            add_formula_slot();
        }
        if (loaded_ids.size() <= id) {
            loaded_ids.resize(static_cast<size_t>(id) + 1, 0);
        }
        if (loaded_ids[id]) {
            if (!allow_legacy_id_repair) {
                log_error("Duplicate scenario formula ID during loading", 0, static_cast<int>(id));
                return;
            }
            char detail[96];
            snprintf(detail, sizeof(detail), "record=%u duplicate_id=%u", static_cast<unsigned int>(i), id);
            log_warning("Discarding duplicate legacy scenario formula record", detail, 0);
            buffer_set(buf, record_end);
            continue;
        }
        loaded_ids[id] = 1;
        scenario_formula_t *formula = &scenario_formulas[id];
        memset(formula, 0, sizeof(*formula));
        new_formula(formula, id);

        buffer_read_raw(buf, formula->formatted_calculation, MAX_FORMULA_LENGTH);
        formula->formatted_calculation[MAX_FORMULA_LENGTH - 1] = '\0'; // ensure safety
        formula->evaluation = buffer_read_i32(buf);
        formula->is_static = buffer_read_u8(buf);
        formula->is_error = buffer_read_u8(buf);
        formula->min_evaluation = buffer_read_i32(buf);
        formula->max_evaluation = buffer_read_i32(buf);
        if (buf->index < record_end) {
            buffer_set(buf, record_end);
        }
    }
}

void scenario_events_load_state(buffer *buf_events, buffer *buf_conditions, buffer *buf_actions, buffer *buf_formulas,
     int scenario_version)
{
    scenario_events_clear();
    info_load_state(buf_events, scenario_version);
    if (scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA) {
        conditions_load_state(buf_conditions);
    } else {
        conditions_load_state_old_version(buf_conditions);
    }
    actions_load_state(buf_actions, scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA);
    if (scenario_version > SCENARIO_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        formulas_load_state(buf_formulas, scenario_version < SCENARIO_CURRENT_VERSION);
    }

    for (scenario_event_t &event : scenario_events) {
        if (event.state == EVENT_STATE_DELETED) {
            event.state = EVENT_STATE_UNDEFINED;
        }
    }
}

void scenario_events_process_all(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_conditional_execute(&event);
    }
}

scenario_event_t *scenario_events_get_using_custom_variable(int custom_variable_id)
{
    for (scenario_event_t &event : scenario_events) {
        if (scenario_event_uses_custom_variable(&event, custom_variable_id)) {
            return &event;
        }
    }
    return 0;
}

void scenario_events_progress_paused(int days_passed)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_decrease_pause_time(&event, days_passed);
    }
}

static void migrate_parameters_action(scenario_action_t *action)
{
    // migration for older actions (pre-formulas)
    int min_limit = 0, max_limit = 0;
    parameter_type p_type;
    action_types action_type = action->type;
    if (action_type == ACTION_TYPE_ADJUST_CITY_HEALTH || action_type == ACTION_TYPE_ADJUST_ROME_WAGES ||
        action_type == ACTION_TYPE_ADJUST_MONEY || action_type == ACTION_TYPE_ADJUST_SAVINGS) {
        return;
    }
    int *params[] = {    // Collect addresses of the fields
        &action->parameter1,
        &action->parameter2,
        &action->parameter3,
        &action->parameter4,
        &action->parameter5
    };
    for (int i = 1; i <= 5; ++i) {
        int *param_value = params[i - 1];
        p_type = scenario_events_parameter_data_get_action_parameter_type(
            action_type, i, &min_limit, &max_limit);
        if ((p_type == PARAMETER_TYPE_FORMULA || p_type == PARAMETER_TYPE_GRID_SLICE) && param_value != NULL) {
            char buffer[16];  // Make sure buffer is large enough
            memset(buffer, 0, sizeof(buffer));
            string_from_int((uint8_t *) buffer, *param_value, 0);
            unsigned int id = scenario_formula_add((const uint8_t *) buffer, min_limit, max_limit);
            switch (i) {
                case 1: action->parameter1 = id; break;
                case 2: action->parameter2 = id; break;
                case 3: action->parameter3 = id; break;
                case 4: action->parameter4 = id; break;
                case 5: action->parameter5 = id; break;
            }
        }
    }
}

static void migrate_parameters_condition(scenario_condition_t *condition)
{
    // migration for older conditions (pre-formulas)
    int min_limit = 0, max_limit = 0;
    parameter_type p_type;
    condition_types condition_type = condition->type;

    int *params[] = {    // Collect addresses of the fields
        &condition->parameter1,
        &condition->parameter2,
        &condition->parameter3,
        &condition->parameter4,
        &condition->parameter5
    };

    for (int i = 1; i <= 5; ++i) {
        int *param_value = params[i - 1];
        p_type = scenario_events_parameter_data_get_condition_parameter_type(
            condition_type, i, &min_limit, &max_limit);
        if ((p_type == PARAMETER_TYPE_FORMULA || p_type == PARAMETER_TYPE_GRID_SLICE) && param_value != NULL) {
            uint8_t buffer[16];  // Make sure buffer is large enough
            memset(buffer, 0, sizeof(buffer));
            string_from_int(buffer, *param_value, 0);
            unsigned int id = scenario_formula_add((const uint8_t *) buffer, min_limit, max_limit);
            switch (i) {
                case 1: condition->parameter1 = id; break;
                case 2: condition->parameter2 = id; break;
                case 3: condition->parameter3 = id; break;
                case 4: condition->parameter4 = id; break;
                case 5: condition->parameter5 = id; break;
            }
        }
    }
}

void scenario_events_migrate_to_resolved_display_names(void)
{
    for (unsigned int i = 0; i < scenario_custom_variable_count(); i++) {
        const uint8_t *name = scenario_custom_variable_get_text_display(i);
        uint8_t new_name[CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH];
        // Format: "<original name> [id]"
        snprintf((char *) new_name, sizeof(new_name), "%s [%u]", (const char *) name, i);
        scenario_custom_variable_set_text_display(i, new_name);
    }
}

void scenario_events_migrate_to_formulas(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            migrate_parameters_action(action); //migrate parameters if needed
        }
        scenario_condition_group_t *group;
        scenario_condition_t *condition;
        for (unsigned int j = 0; j < scenario_event_condition_group_count(current); j++) {
            group = scenario_event_condition_group_get(current, j);
            for (unsigned int k = 0; k < scenario_condition_group_condition_count(group); k++) {
                condition = scenario_condition_group_condition_get(group, k);
                migrate_parameters_condition(condition); //migrate parameters if needed
            }
        }
    }
}

void scenario_events_min_max_migrate_to_formulas(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            action_types type = action->type;
            if (type != ACTION_TYPE_ADJUST_CITY_HEALTH && type != ACTION_TYPE_ADJUST_ROME_WAGES &&
                type != ACTION_TYPE_ADJUST_MONEY && type != ACTION_TYPE_ADJUST_SAVINGS) {
                continue;
            }
            int min_limit = 0, max_limit = 0;
            uint8_t buffer[32];
            memset(buffer, 0, sizeof(buffer));
            if (type == ACTION_TYPE_ADJUST_CITY_HEALTH) {
                min_limit = -100;
                max_limit = 100;
            } else if (type == ACTION_TYPE_ADJUST_ROME_WAGES) {
                min_limit = -10000;
                max_limit = 10000;
            } else {
                min_limit = -1000000000;
                max_limit = 1000000000;
            }
            sprintf((char *)buffer, "{%i,%i}", action->parameter1, action->parameter2);
            if (type == ACTION_TYPE_ADJUST_CITY_HEALTH || type == ACTION_TYPE_ADJUST_ROME_WAGES) {
                action->parameter2 = action->parameter3;
            }
            unsigned int id = scenario_formula_add((const uint8_t *) buffer, min_limit, max_limit);
            action->parameter1 = id;
        }
    }
}

void scenario_events_migrate_to_buys_sells(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            action_types type = action->type;
            if (type != ACTION_TYPE_TRADE_ADJUST_ROUTE_AMOUNT) {
                continue;
            }
            int city_id = empire_city_get_for_trade_route(action->parameter1);
            if (city_id < 0) {
                action->parameter5 = 1;
                continue;
            }
            action->parameter5 = empire_city_get(city_id)->buys_resource[action->parameter2];
        }
    }
}

void scenario_events_assign_parent_event_ids(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        int event_id = current->id;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            action->parent_event_id = event_id;
        }
        scenario_condition_group_t *group;
        scenario_condition_t *condition;
        for (unsigned int j = 0; j < scenario_event_condition_group_count(current); j++) {
            group = scenario_event_condition_group_get(current, j);
            for (unsigned int k = 0; k < scenario_condition_group_condition_count(group); k++) {
                condition = scenario_condition_group_condition_get(group, k);
                condition->parent_event_id = event_id;
            }
        }
    }
}

void scenario_events_fetch_event_tiles_to_editor(void)
{
    widget_map_editor_clear_draw_context_event_tiles();
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        int event_id = current->id;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            if (action->type == ACTION_TYPE_BUILDING_FORCE_COLLAPSE ||
                action->type == ACTION_TYPE_CHANGE_TERRAIN) {
                int grid_offset1 = action->parameter1;
                int grid_offset2 = action->parameter2;
                grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(grid_offset1, grid_offset2);
                for (int i = 0; i < slice->size; i++) {
                    widget_map_editor_add_draw_context_event_tile(slice->grid_offsets[i], event_id);
                }

            }
        }
        scenario_condition_group_t *group;
        scenario_condition_t *condition;
        for (unsigned int j = 0; j < scenario_event_condition_group_count(current); j++) {
            group = scenario_event_condition_group_get(current, j);
            for (unsigned int k = 0; k < scenario_condition_group_condition_count(group); k++) {
                condition = scenario_condition_group_condition_get(group, k);
                if (condition->type == CONDITION_TYPE_BUILDING_COUNT_AREA ||
                    condition->type == CONDITION_TYPE_TERRAIN_IN_AREA) {
                    int grid_offset1 = condition->parameter1;
                    int grid_offset2 = condition->parameter2;
                    grid_slice *slice = map_grid_get_grid_slice_from_corner_offsets(grid_offset1, grid_offset2);
                    for (int i = 0; i < slice->size; i++) {
                        widget_map_editor_add_draw_context_event_tile(slice->grid_offsets[i], event_id);
                    }

                }
            }
        }
    }
}
void scenario_events_migrate_to_grid_slices(void)
{
    for (scenario_event_t &event : scenario_events) {
        scenario_event_t *current = &event;
        scenario_action_t *action;
        for (unsigned int j = 0; j < scenario_event_action_count(current); j++) {
            action = scenario_event_action_get(current, j);
            if (action->type == ACTION_TYPE_BUILDING_FORCE_COLLAPSE ||
                action->type == ACTION_TYPE_CHANGE_TERRAIN) {
                // For these action types, we need to convert grid offset and radius into grid offset corners
                // into two GRID_SLICE parameters (corner1 and corner2)
                int old_grid_offset = scenario_formula_evaluate_formula(action->parameter1);
                int radius = scenario_formula_evaluate_formula(action->parameter2);
                int corner1 = 0, corner2 = 0;
                grid_slice *slice = map_grid_get_grid_slice_from_center(old_grid_offset, radius);
                map_grid_get_corner_offsets_from_grid_slice(slice, &corner1, &corner2);
                action->parameter1 = corner1; // test on value not formula
                action->parameter2 = corner2;
            }
        }
        scenario_condition_group_t *group;
        scenario_condition_t *condition;
        for (unsigned int j = 0; j < scenario_event_condition_group_count(current); j++) {
            group = scenario_event_condition_group_get(current, j);
            for (unsigned int k = 0; k < scenario_condition_group_condition_count(group); k++) {
                condition = scenario_condition_group_condition_get(group, k);
                if (condition->type == CONDITION_TYPE_BUILDING_COUNT_AREA) {
                    // CONDITION_TYPE_TERRAIN_IN_AREA introduced with the new parameters from start
                    int old_grid_offset = scenario_formula_evaluate_formula(condition->parameter1);
                    int radius = scenario_formula_evaluate_formula(condition->parameter2);
                    int corner1 = 0, corner2 = 0;
                    grid_slice *slice = map_grid_get_grid_slice_from_center(old_grid_offset, radius);
                    map_grid_get_corner_offsets_from_grid_slice(slice, &corner1, &corner2);
                    condition->parameter1 = corner1;
                    condition->parameter2 = corner2;
                }
            }
        }
    }
}
