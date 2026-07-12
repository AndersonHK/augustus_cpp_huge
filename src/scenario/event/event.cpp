#include "event.h"

#include "core/encoding.h"
#include "core/log.h"
#include "core/random.h"
#include "game/save_version.h"
#include "scenario/event/action_handler.h"
#include "scenario/event/condition_handler.h"
#include "scenario/event/controller.h"

#include <algorithm>
#include <utility>

namespace {

bool valid_condition_group_type(int type)
{
    return type == FULFILLMENT_TYPE_ALL || type == FULFILLMENT_TYPE_ANY;
}

bool action_in_use(const scenario_action_t &action)
{
    return action.type != ACTION_TYPE_UNDEFINED;
}

bool condition_in_use(const scenario_condition_t &condition)
{
    return condition.type != CONDITION_TYPE_UNDEFINED;
}

void discard_condition_group(scenario_condition_group_t *group)
{
    if (group) {
        group->conditions.clear();
    }
}

bool valid_action_type(int type)
{
    return type >= ACTION_TYPE_UNDEFINED && type < ACTION_TYPE_MAX;
}

scenario_action_t *append_action(scenario_action_array_t &actions)
{
    actions.push_back({});
    return &actions.back();
}

scenario_action_t *create_action(scenario_action_array_t &actions)
{
    for (scenario_action_t &action : actions) {
        if (!action_in_use(action)) {
            action = {};
            return &action;
        }
    }
    return append_action(actions);
}

scenario_condition_t *append_condition(scenario_condition_array_t &conditions)
{
    conditions.push_back({});
    return &conditions.back();
}

scenario_condition_t *create_condition(scenario_condition_array_t &conditions)
{
    for (scenario_condition_t &condition : conditions) {
        if (!condition_in_use(condition)) {
            condition = {};
            return &condition;
        }
    }
    return append_condition(conditions);
}

scenario_condition_group_t *append_condition_group(scenario_condition_group_array_t &groups)
{
    groups.push_back({});
    scenario_condition_group_t *group = &groups.back();
    scenario_condition_group_new(group, static_cast<unsigned int>(groups.size() - 1));
    return group;
}

scenario_condition_group_t *create_condition_group(scenario_condition_group_array_t &groups)
{
    for (size_t i = 0; i < groups.size(); ++i) {
        scenario_condition_group_t *group = &groups[i];
        if (!scenario_condition_group_in_use(group)) {
            scenario_condition_group_new(group, static_cast<unsigned int>(i));
            return group;
        }
    }
    return append_condition_group(groups);
}

scenario_condition_group_t *create_condition_group_after(scenario_condition_group_array_t &groups, unsigned int index)
{
    while (index > groups.size()) {
        append_condition_group(groups);
    }
    for (size_t i = index; i < groups.size(); ++i) {
        scenario_condition_group_t *group = &groups[i];
        if (!scenario_condition_group_in_use(group)) {
            scenario_condition_group_new(group, static_cast<unsigned int>(i));
            return group;
        }
    }
    return append_condition_group(groups);
}

int link_condition_group_to_event(scenario_event_t *event, scenario_condition_group_t *group);
int link_action_to_event(scenario_event_t *event, scenario_action_t *action);

class ScenarioEventLinker {
public:
    static int link_condition_group(int event_id, scenario_condition_group_t *group)
    {
        scenario_event_t *event = scenario_event_get(event_id);
        if (!event) {
            log_error("Ignoring scenario condition group linked to invalid event.", 0, event_id);
            discard_condition_group(group);
            return 0;
        }
        if (!link_condition_group_to_event(event, group)) {
            discard_condition_group(group);
            return 0;
        }
        return 1;
    }

    static int link_action(int event_id, scenario_action_t *action)
    {
        scenario_event_t *event = scenario_event_get(event_id);
        if (!event) {
            log_error("Ignoring scenario action linked to invalid event.", 0, event_id);
            return 0;
        }
        return link_action_to_event(event, action);
    }
};

} // namespace

void scenario_event_new(scenario_event_t *event, unsigned int position)
{
    event->id = position;
    event->actions.clear();
    event->condition_groups.clear();
}

int scenario_event_is_active(const scenario_event_t *event)
{
    return event->state != EVENT_STATE_UNDEFINED;
}

void scenario_event_init(scenario_event_t *event)
{
    event->state = EVENT_STATE_ACTIVE;
    unsigned int event_id = event->id;
    for (scenario_condition_group_t &group : event->condition_groups) {
        for (scenario_condition_t &condition : group.conditions) {
            scenario_condition_type_init(&condition);
            condition.parent_event_id = event_id;
        }
    }
}

void scenario_event_release_contents(scenario_event_t *event)
{
    if (!event) {
        return;
    }
    *event = {};
    event->state = EVENT_STATE_UNDEFINED;
}

void scenario_event_save_state(buffer *buf, scenario_event_t *event)
{
    if (event->state == EVENT_STATE_UNDEFINED) {
        event->state = EVENT_STATE_DELETED; // We need a different state than undefined to avoid array overrides on load which breaks the linking by id.
    }

    buffer_write_i32(buf, event->id);
    buffer_write_i16(buf, static_cast<int16_t>(event->state));
    buffer_write_i32(buf, event->repeat_days_min);
    buffer_write_i32(buf, event->repeat_days_max);
    buffer_write_u8(buf, event->repeat_interval);
    buffer_write_i32(buf, event->days_until_active);
    buffer_write_i32(buf, event->max_number_of_repeats);
    buffer_write_i32(buf, event->execution_count);
    buffer_write_u16(buf, static_cast<uint16_t>(event->actions.size()));
    buffer_write_u16(buf, static_cast<uint16_t>(event->condition_groups.size()));
    char name_utf8[EVENT_NAME_LENGTH * 2] = { 0 };
    encoding_to_utf8(event->name, name_utf8, EVENT_NAME_LENGTH * 2, 0);
    buffer_write_raw(buf, name_utf8, EVENT_NAME_LENGTH * 2);
}

void scenario_event_load_state(buffer *buf, scenario_event_t *event, int scenario_version)
{
    int saved_id = buffer_read_i32(buf);
    event->state = static_cast<event_state>(buffer_read_i16(buf));
    event->repeat_days_min = buffer_read_i32(buf);
    event->repeat_days_max = buffer_read_i32(buf);
    if (scenario_version <= SCENARIO_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        event->repeat_interval = 2; // months
        event->repeat_days_min *= 16;
        event->repeat_days_max *= 16;
    } else {
        event->repeat_interval = buffer_read_u8(buf);
    }
    event->days_until_active = buffer_read_i32(buf);
    event->max_number_of_repeats = buffer_read_i32(buf);
    event->execution_count = buffer_read_i32(buf);
    if (!(scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA)) {
        buffer_skip(buf, 2);
    }
    unsigned int actions_count = buffer_read_u16(buf);
    unsigned int condition_groups_count = 0;
    if (scenario_version > SCENARIO_LAST_STATIC_ORIGINAL_DATA) {
        condition_groups_count = buffer_read_u16(buf);
        char name_utf8[EVENT_NAME_LENGTH * 2];
        buffer_read_raw(buf, name_utf8, EVENT_NAME_LENGTH * 2);
        encoding_from_utf8(name_utf8, event->name, EVENT_NAME_LENGTH);
    }

    event->actions.clear();
    event->actions.reserve(actions_count);
    event->condition_groups.clear();
    event->condition_groups.reserve(condition_groups_count ? condition_groups_count : 1);
    if (condition_groups_count == 0) {
        append_condition_group(event->condition_groups);
    }
    if (event->id != (unsigned int) saved_id) {
        log_error("Loaded event id does not match what it was saved with. The game will likely crash. event->id: ",
            0, event->id);
    }
}

scenario_condition_t *scenario_event_condition_create(scenario_condition_group_t *group, int type)
{
    if (!group) {
        return 0;
    }
    scenario_condition_t *condition = create_condition(group->conditions);
    condition->type = static_cast<condition_types>(type);

    return condition;
}

namespace {

int link_condition_group_to_event(scenario_event_t *event, scenario_condition_group_t *group)
{
    if (!event || !group) {
        log_error("Unable to link scenario condition group to missing event.", 0, 0);
        return 0;
    }
    if (!valid_condition_group_type(group->type)) {
        log_error("Unable to link scenario condition group with invalid fulfillment type.", 0, group->type);
        return 0;
    }
    scenario_condition_group_t *new_group = create_condition_group(event->condition_groups);
    if (!new_group) {
        log_error("Unable to create scenario condition group link.", 0, 0);
        return 0;
    }
    new_group->type = group->type;
    new_group->conditions = std::move(group->conditions);
    for (scenario_condition_t &condition : new_group->conditions) {
        condition.parent_event_id = event->id;
    }
    return 1;
}

} // namespace

int scenario_event_link_condition_group_by_id(int event_id, scenario_condition_group_t *group)
{
    return ScenarioEventLinker::link_condition_group(event_id, group);
}

scenario_action_t *scenario_event_action_create(scenario_event_t *event, int type)
{
    if (!event) {
        return 0;
    }
    scenario_action_t *action = create_action(event->actions);
    action->type = static_cast<action_types>(type);

    return action;
}

namespace {

int link_action_to_event(scenario_event_t *event, scenario_action_t *action)
{
    if (!event || !action) {
        log_error("Unable to link scenario action to missing event.", 0, 0);
        return 0;
    }
    if (!valid_action_type(action->type)) {
        log_error("Unable to link scenario action with invalid type.", 0, action->type);
        return 0;
    }
    scenario_action_t *new_action = create_action(event->actions);
    if (!new_action) {
        log_error("Unable to create scenario action link.", 0, 0);
        return 0;
    }

    new_action->type = action->type;
    new_action->parameter1 = action->parameter1;
    new_action->parameter2 = action->parameter2;
    new_action->parameter3 = action->parameter3;
    new_action->parameter4 = action->parameter4;
    new_action->parameter5 = action->parameter5;
    new_action->parent_event_id = event->id;
    return 1;
}

} // namespace

int scenario_event_link_action_by_id(int event_id, scenario_action_t *action)
{
    return ScenarioEventLinker::link_action(event_id, action);
}

unsigned int scenario_event_condition_group_count(const scenario_event_t *event)
{
    return event ? static_cast<unsigned int>(event->condition_groups.size()) : 0;
}

scenario_condition_group_t *scenario_event_condition_group_get(scenario_event_t *event, unsigned int index)
{
    if (!event || index >= event->condition_groups.size()) {
        return 0;
    }
    return &event->condition_groups[index];
}

const scenario_condition_group_t *scenario_event_condition_group_get_const(const scenario_event_t *event, unsigned int index)
{
    if (!event || index >= event->condition_groups.size()) {
        return 0;
    }
    return &event->condition_groups[index];
}

scenario_condition_group_t *scenario_event_condition_group_add(scenario_event_t *event)
{
    return event ? append_condition_group(event->condition_groups) : 0;
}

scenario_condition_group_t *scenario_event_condition_group_add_after(scenario_event_t *event, unsigned int index)
{
    return event ? create_condition_group_after(event->condition_groups, index) : 0;
}

void scenario_event_condition_groups_pack(scenario_event_t *event)
{
    if (event) {
        event->condition_groups.erase(
            std::remove_if(event->condition_groups.begin(), event->condition_groups.end(),
                [](const scenario_condition_group_t &group) { return !scenario_condition_group_in_use(&group); }),
            event->condition_groups.end());
    }
}

unsigned int scenario_condition_group_condition_count(const scenario_condition_group_t *group)
{
    return group ? static_cast<unsigned int>(group->conditions.size()) : 0;
}

scenario_condition_t *scenario_condition_group_condition_get(scenario_condition_group_t *group, unsigned int index)
{
    if (!group || index >= group->conditions.size()) {
        return 0;
    }
    return &group->conditions[index];
}

const scenario_condition_t *scenario_condition_group_condition_get_const(const scenario_condition_group_t *group, unsigned int index)
{
    if (!group || index >= group->conditions.size()) {
        return 0;
    }
    return &group->conditions[index];
}

scenario_condition_t *scenario_condition_group_condition_add(scenario_condition_group_t *group)
{
    return group ? append_condition(group->conditions) : 0;
}

void scenario_condition_group_conditions_clear(scenario_condition_group_t *group)
{
    if (group) {
        group->conditions.clear();
    }
}

void scenario_condition_group_conditions_pack(scenario_condition_group_t *group)
{
    if (group) {
        group->conditions.erase(
            std::remove_if(group->conditions.begin(), group->conditions.end(),
                [](const scenario_condition_t &condition) { return condition.type == CONDITION_TYPE_UNDEFINED; }),
            group->conditions.end());
    }
}

unsigned int scenario_event_action_count(const scenario_event_t *event)
{
    return event ? static_cast<unsigned int>(event->actions.size()) : 0;
}

scenario_action_t *scenario_event_action_get(scenario_event_t *event, unsigned int index)
{
    if (!event || index >= event->actions.size()) {
        return 0;
    }
    return &event->actions[index];
}

const scenario_action_t *scenario_event_action_get_const(const scenario_event_t *event, unsigned int index)
{
    if (!event || index >= event->actions.size()) {
        return 0;
    }
    return &event->actions[index];
}

void scenario_event_actions_pack(scenario_event_t *event)
{
    if (event) {
        event->actions.erase(
            std::remove_if(event->actions.begin(), event->actions.end(),
                [](const scenario_action_t &action) { return action.type == ACTION_TYPE_UNDEFINED; }),
            event->actions.end());
    }
}

int scenario_event_can_repeat(scenario_event_t *event)
{
    return (event->repeat_days_min > 0) && (event->repeat_days_max >= event->repeat_days_min) &&
        ((event->execution_count < event->max_number_of_repeats) || (event->max_number_of_repeats <= 0));
}

static int conditions_fulfilled(scenario_event_t *event)
{
    if (event->state != EVENT_STATE_ACTIVE) {
        return 0;
    }
    if (event->actions.empty()) {
        return 0;
    }

    for (scenario_condition_group_t &group : event->condition_groups) {
        int group_fulfilled = 0;
        for (scenario_condition_t &condition : group.conditions) {
            if (group.type == FULFILLMENT_TYPE_ALL && !scenario_condition_type_is_met(&condition)) {
                return 0;
            }
            if (group.type == FULFILLMENT_TYPE_ANY && scenario_condition_type_is_met(&condition)) {
                group_fulfilled = 1;
                break;
            }
        }
        if (group.type == FULFILLMENT_TYPE_ANY && !group.conditions.empty() && !group_fulfilled) {
            return 0;
        }
    }

    return 1;
}

int scenario_event_decrease_pause_time(scenario_event_t *event, int days_passed)
{
    if (event->state != EVENT_STATE_PAUSED) {
        return 0;
    }

    if (event->days_until_active > 0) {
        event->days_until_active -= days_passed;
    }
    if (event->days_until_active < 0) {
        event->days_until_active = 0;
    }
    if (event->days_until_active == 0) {
        event->state = EVENT_STATE_ACTIVE;
    }
    return 1;
}

int scenario_event_count_conditions(const scenario_event_t *event)
{
    int total_conditions = 0;
    if (!event) {
        return total_conditions;
    }
    for (const scenario_condition_group_t &group : event->condition_groups) {
        total_conditions += static_cast<int>(group.conditions.size());
    }
    return total_conditions;
}

int scenario_event_conditional_execute(scenario_event_t *event)
{
    if (conditions_fulfilled(event)) {
        int result = scenario_event_execute(event);
        event->execution_count++;
        if (scenario_event_can_repeat(event)) {
            scenario_event_init(event);
            event->state = EVENT_STATE_PAUSED;
            event->days_until_active = random_between_from_stdlib(event->repeat_days_min, event->repeat_days_max);
        } else {
            event->state = EVENT_STATE_DISABLED;
        }
        return result;
    }
    return 0;
}

int scenario_event_execute(scenario_event_t *event)
{
    int actioned = 1;

    for (scenario_action_t &current : event->actions) {
        int action_result = scenario_action_type_execute(&current);
        actioned &= action_result;
    }

    return actioned;
}

int scenario_event_uses_custom_variable(const scenario_event_t *event, int custom_variable_id)
{
    for (const scenario_condition_group_t &group : event->condition_groups) {
        for (const scenario_condition_t &condition : group.conditions) {
            if (scenario_condition_uses_custom_variable(&condition, custom_variable_id)) {
                return 1;
            }
        }
    }

    for (const scenario_action_t &action : event->actions) {
        if (scenario_action_uses_custom_variable(&action, custom_variable_id)) {
            return 1;
        }
    }

    return 0;
}
