#pragma once

#include "core/buffer.h"
#include "scenario/event/data.h"
#ifdef __cplusplus
extern "C" {
#endif


void scenario_event_new(scenario_event_t *event, unsigned int position);
int scenario_event_is_active(const scenario_event_t *event);

void scenario_event_init(scenario_event_t *event);
void scenario_event_release_contents(scenario_event_t *event);

void scenario_event_save_state(buffer *buf, scenario_event_t *event);
void scenario_event_load_state(buffer *buf, scenario_event_t *event, int is_new_version);
scenario_condition_t *scenario_event_condition_create(scenario_condition_group_t *group, int type);
int scenario_event_link_condition_group_by_id(int event_id, scenario_condition_group_t *group);
scenario_action_t *scenario_event_action_create(scenario_event_t *event, int type);
int scenario_event_link_action_by_id(int event_id, scenario_action_t *action);

unsigned int scenario_event_condition_group_count(const scenario_event_t *event);
scenario_condition_group_t *scenario_event_condition_group_get(scenario_event_t *event, unsigned int index);
const scenario_condition_group_t *scenario_event_condition_group_get_const(const scenario_event_t *event, unsigned int index);
scenario_condition_group_t *scenario_event_condition_group_add(scenario_event_t *event);
scenario_condition_group_t *scenario_event_condition_group_add_after(scenario_event_t *event, unsigned int index);
void scenario_event_condition_groups_pack(scenario_event_t *event);

unsigned int scenario_condition_group_condition_count(const scenario_condition_group_t *group);
scenario_condition_t *scenario_condition_group_condition_get(scenario_condition_group_t *group, unsigned int index);
const scenario_condition_t *scenario_condition_group_condition_get_const(const scenario_condition_group_t *group, unsigned int index);
scenario_condition_t *scenario_condition_group_condition_add(scenario_condition_group_t *group);
void scenario_condition_group_conditions_clear(scenario_condition_group_t *group);
void scenario_condition_group_conditions_pack(scenario_condition_group_t *group);

unsigned int scenario_event_action_count(const scenario_event_t *event);
scenario_action_t *scenario_event_action_get(scenario_event_t *event, unsigned int index);
const scenario_action_t *scenario_event_action_get_const(const scenario_event_t *event, unsigned int index);
void scenario_event_actions_pack(scenario_event_t *event);

int scenario_event_count_conditions(const scenario_event_t *event);

int scenario_event_can_repeat(scenario_event_t *event);

int scenario_event_decrease_pause_time(scenario_event_t *event, int days_passed);
int scenario_event_conditional_execute(scenario_event_t *event);
int scenario_event_execute(scenario_event_t *event);
int scenario_event_uses_custom_variable(const scenario_event_t *event, int custom_variable_id);

#ifdef __cplusplus
}
#endif
