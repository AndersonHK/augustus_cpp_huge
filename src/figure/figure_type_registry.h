#pragma once


const char *figure_type_registry_get_failure_reason(void);
const char *figure_type_definition_source_path(const char *type_attr);
int figure_type_definition_is_suppressed(const char *type_attr);
int figure_type_registry_load(void);
int figure_type_registry_resolve_building_references(void);
void figure_type_registry_reset(void);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
} figure_type_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_disabled;
    int queried_max_roam_length;
    int queried_source_layer;
    int queried_overlay_count;
    int queried_overlay_image_group;
    int queried_overlay_follows_direction;
    int queried_overlay_follows_frame;
    int queried_overlay_action_count;
    int queried_overlay_resource_stride;
    int queried_overlay_direction_stride;
    int queried_overlay_hide_on_corpse;
    int queried_overlay_sample_image_offset;
    int queried_overlay_sample_x_offset;
    int queried_overlay_sample_y_offset;
    int queried_standard_enabled;
    int queried_standard_flag_count;
    int queried_standard_moving_frame_divisor;
    int queried_standard_sample_moving_offset;
    int queried_standard_sample_halted_offset;
    int queried_state_layer_count;
    int queried_bucket_going_image_group;
    int queried_bucket_going_sample_offset;
    int queried_bucket_at_fire_image_group;
    int queried_bucket_at_fire_sample_offset;
    int queried_ballista_idle_image_group;
    int queried_ballista_idle_sample_offset;
    int queried_ballista_firing_image_group;
    int queried_ballista_firing_uses_missile_launcher;
    int queried_ballista_firing_sample_offset;
    int queried_map_flag_enabled;
    int queried_map_flag_marker_count;
    int queried_map_flag_sample_base_offset;
    int queried_map_flag_invasion_image_group;
    int queried_map_flag_invasion_image_offset;
    int queried_map_flag_invasion_number;
    int queried_map_flag_fishing_image_group;
    int queried_map_flag_fishing_number;
    int queried_map_flag_herd_image_offset;
    int queried_map_flag_number_x;
    int queried_map_flag_number_y;
    int queried_hippodrome_enabled;
    int queried_hippodrome_team_count;
    int queried_hippodrome_schedule_count;
    int queried_hippodrome_sample_horse_offset;
    int queried_hippodrome_sample_cart_offset;
    int queried_hippodrome_sample_cart_x;
    int queried_hippodrome_sample_cart_y;
    int queried_hippodrome_destination_horse_offset;
    int queried_hippodrome_destination_cart_offset;
    int queried_hippodrome_team0_horse_group;
    int queried_hippodrome_team1_cart_group;
    int queried_hippodrome_top_x;
    int queried_hippodrome_top_y;
    int queried_hippodrome_right_x;
    int queried_hippodrome_bottom_x;
    int queried_hippodrome_left_y;
    int queried_missile_launcher_enabled;
    int queried_missile_launcher_uses_attack_cursor;
    int queried_missile_launcher_uses_wait_cursor;
    int queried_missile_launcher_frame_divisor;
    int queried_missile_launcher_frame_count;
    int queried_missile_launcher_after_frame;
    int queried_missile_launcher_sample_start;
    int queried_missile_launcher_sample_middle;
    int queried_missile_launcher_sample_last;
    int queried_missile_launcher_sample_after;
    int queried_resource_cart_enabled;
    int queried_resource_cart_empty_image_group;
    int queried_resource_cart_collecting_item_source;
    int queried_resource_cart_resource_action;
    int queried_resource_cart_fixed_one_load;
    int queried_resource_cart_carried_or_one_load;
    int queried_resource_cart_runtime_state;
    int queried_resource_cart_suppress_body_when_hidden;
    int queried_resource_cart_lift_food_at_loads;
    int queried_resource_cart_lift_food_y_adjust;
    int queried_resource_cart_hide_on_corpse;
    int queried_resource_cart_sample_x;
    int queried_resource_cart_sample_y;
    int queried_directional_enabled;
    int queried_directional_image_group;
    int queried_directional_pose_count;
    int queried_directional_default_sample_offset;
    int queried_directional_pose_sample_offset;
} figure_type_layer_test_result;

int figure_type_layered_definition_buffers_are_valid_for_test(
    const figure_type_layer_test_input *inputs,
    int input_count,
    const char *query_attr,
    figure_type_layer_test_result *result);
#endif

