#include "building/building_type.h"
#include "translation/translation.h"
#include "building/construction.h"
#include "building/data_transfer.h"
#include "../building/industry.h"
#include "building/rotation.h"
#include "building/variant.h"
#include "city/victory.h"
#include "city/warning.h"
#include "figure/roamer_preview.h"
#include "game/orientation.h"
#include "game/state.h"
#include "game/undo.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/weather.h"
#include "map/building.h"
#include "widget/city.h"
#include "widget/city_with_overlay.h"
#include "widget/sidebar/city.h"
#include "widget/sidebar/extra.h"
#include "widget/sidebar/military.h"
#include "widget/top_menu.h"
#include "window/building_info.h"
#include "window/empire.h"
#include "window/file_dialog.h"

#include "city.h"

#include "graphics/complex_button.h"
#include "window/message_list.h"
#include "window/overlay_menu.h"
#include "window/advisors.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"

#include "game/settings.h"

#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/message.h"
#include "city/view.h"
#include "core/config.h"
#include "core/string.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "game/time.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "map/bookmark.h"
#include "map/grid.h"
#include "map/property.h"
#include "map/terrain.h"
#include "scenario/allowed_building.h"
#include "scenario/criteria.h"
#include "scenario/custom_variable.h"


#define TOPLEFT_MESSAGES_X 5
#define TOPLEFT_MESSAGES_Y_SPACING 24

static void set_construction_building_type(const building_type_registry_impl::BuildingType *type, int rotation)
{
    if (scenario_allowed_building(type) && building_menu_is_enabled(type)) {
        building_construction_cancel();
        building_construction_set_type(type, rotation);
        window_request_refresh();
    }
}


static int time_left_label_shown;

static void create_roamer_preview_for_building_attr(const char *text_id)
{
    figure_roamer_preview_create_all_for_building_type(building_type_registry_impl::type_from_attr(text_id));
}

static void draw_topleft_label_with_fragments(int x, int y, const lang_fragment *fragments, int fragment_count, font_t font, color_t color_ver);

int window_city_simulated_weather(weather_type weather)
{
    switch (weather) {
        case WEATHER_RAIN:
            return config_get(CONFIG_UI_WT_PREVIEW_RAIN) || config_get(CONFIG_UI_WT_PREVIEW_HEAVY_RAIN);
        case WEATHER_SNOW:
            return config_get(CONFIG_UI_WT_ENABLE_SNOW_CENTRAL);
        case WEATHER_SAND:
            return config_get(CONFIG_UI_WT_PREVIEW_SANDSTORM);
        default:
            return 0;
    }
}

int window_city_is_window_cityview(void)
{
    int is_regular_cityview = ((window_get_id() >= WINDOW_CITY && window_get_id() <= WINDOW_RACE_BET)
        && window_get_id() != WINDOW_TOP_MENU);
    int is_cart_depo_window = 0; // placeholder
    return is_regular_cityview || is_cart_depo_window;
}

static void draw_background(void)
{
    if (window_city_is_window_cityview()) {
        widget_city_setup_routing_preview();
    }
    widget_sidebar_city_draw_background();
    widget_top_menu_draw(1);
}

static void draw_background_military(void)
{
    if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR)) {
        widget_sidebar_military_draw_background();
    } else {
        widget_sidebar_city_draw_background();
    }
    widget_top_menu_draw(1);
}

static int center_in_city(int element_width_pixels)
{
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    x = screen_pixel_to_ui(x);
    width = screen_pixel_to_ui(width);
    int margin = (width - element_width_pixels) / 2;
    return x + margin;
}

static int find_index(const uint8_t *string, char search)
{
    int index = 0;
    while (*string) {
        if (*string == search) {
            return index;
        }
        string++;
        index++;
    }
    return -1;
}

static int is_same_mapping(const hotkey_mapping *current, const hotkey_mapping *candidate)
{
    if (!candidate) {
        return current->key == KEY_TYPE_NONE;
    }
    return current->key == candidate->key && current->modifiers == candidate->modifiers;
}

static const uint8_t *get_paused_text(void)
{
    const uint8_t *pause_string = lang_get_string("main_strings.13.2");
    static uint8_t proper_hotkey_pause_string[300];
    static hotkey_mapping pause_key_mapping;
    const hotkey_mapping *new_mapping = hotkey_for_action(HOTKEY_TOGGLE_PAUSE, 0);
    if (*pause_string == *proper_hotkey_pause_string && is_same_mapping(&pause_key_mapping, new_mapping)) {
        return proper_hotkey_pause_string;
    }
    int parenthesis_index = find_index(pause_string, '(');
    if (parenthesis_index == -1) {
        return pause_string;
    }
    int p_key_index = find_index(pause_string + parenthesis_index, 'P');
    if (p_key_index == -1) {
        return pause_string;
    }
    uint8_t *cursor = string_copy(pause_string, proper_hotkey_pause_string, parenthesis_index + 1);
    pause_string += parenthesis_index;
    if (new_mapping) {
        pause_key_mapping.key = new_mapping->key;
        pause_key_mapping.modifiers = new_mapping->modifiers;
    } else {
        pause_key_mapping.key = KEY_TYPE_NONE;
        pause_key_mapping.modifiers = KEY_MOD_NONE;
    }
    if (pause_key_mapping.key == KEY_TYPE_NONE) {
        return proper_hotkey_pause_string;
    }
    cursor = string_copy(pause_string, cursor, p_key_index + 1);
    pause_string += p_key_index + 1;
    const uint8_t *keyname = key_combination_display_name(pause_key_mapping.key, pause_key_mapping.modifiers);
    cursor = string_copy(keyname, cursor, 300 - (cursor - proper_hotkey_pause_string));
    string_copy(pause_string, cursor, 300 - (cursor - proper_hotkey_pause_string));
    return proper_hotkey_pause_string;
}

static void draw_paused_banner(void)
{
    if (game_state_is_paused()) {
        int x_offset = center_in_city(448);
        outer_panel_draw(x_offset, 40, 28, 3);
        text_draw_centered(get_paused_text(), x_offset, 58, 448, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
}

void window_city_draw_custom_variables_text_display(void)
{
    if (!config_get(CONFIG_UI_SHOW_CUSTOM_VARIABLES)) {
        return;
    }
    int y = 25 + (time_left_label_shown ? (TOPLEFT_MESSAGES_Y_SPACING) : 0);
    const font_t font = FONT_NORMAL_WHITE;

    for (unsigned int i = 0; i < scenario_custom_variable_count(); i++) {
        if (!scenario_custom_variable_is_visible(i)) {
            continue;
        }
        const uint8_t *var_text_raw = scenario_custom_variable_get_text_display(i);
        uint8_t var_text_resolved[100];
        scenario_custom_variable_resolve_name(var_text_raw, var_text_resolved, 100);

        // Draw just the text since numbers are now baked into the text
        lang_fragment frags[1] = {
            {.type = LANG_FRAG_TEXT, .text = var_text_resolved }
        };
        int c_group = scenario_custom_variable_get_color_group(i);
        color_t color_ver = complex_button_basic_colors(c_group - 1); // color groups are 1-based
        draw_topleft_label_with_fragments(TOPLEFT_MESSAGES_X, y, frags, 1, font, color_ver);
        y += TOPLEFT_MESSAGES_Y_SPACING;
    }
}

static void draw_time_left(void)
{

    int fps_offset = config_get(CONFIG_UI_DISPLAY_FPS) * 2 * BLOCK_SIZE; // shift to the right if FPS is displayed
    time_left_label_shown = fps_offset > 0; // if fps is shown skip first row anyway
    if ((scenario_criteria_time_limit_enabled() || scenario_criteria_survival_enabled()) && !city_victory_has_won()) {
        time_left_label_shown = 1;

        int years;
        if (scenario_criteria_max_year() <= game_time_year() + 1) {
            years = 0;
        } else {
            years = scenario_criteria_max_year() - game_time_year() - 1;
        }

        int total_months = 12 - game_time_month() + 12 * years;
        int years_left = total_months / 12;
        int months_left = total_months % 12;

        const font_t font = FONT_NORMAL_WHITE;

        // Precompose the prefix text (translated): "Time left until defeat/victory: "
        const translation_key label_id = scenario_criteria_time_limit_enabled()
            ? "TR_CONDITION_TEXT_TIME_LEFT_UNTIL_DEFEAT"
            : "TR_CONDITION_TEXT_TIME_LEFT_UNTIL_VICTORY";
        const uint8_t *label_str = lang_get_string(label_id);

        lang_fragment frags[5] = {
            {.type = LANG_FRAG_TEXT, .text = label_str },
            {.type = LANG_FRAG_NUMBER, .number = years_left },
            {.type = LANG_FRAG_LABEL, .text_key = "TR_EDITOR_REPEAT_FREQUENCY_YEARS"},
            {.type = LANG_FRAG_NUMBER, .number = months_left},
            {.type = LANG_FRAG_LABEL, .text_key = "TR_EDITOR_REPEAT_FREQUENCY_MONTHS"}
        };
        draw_topleft_label_with_fragments(fps_offset + TOPLEFT_MESSAGES_X, 25, frags, 5, font, COLOR_MASK_NONE);
    }
}

void label_draw_masked(int x, int y, int width_blocks, int type, color_t color)
{
    int image_base = Image::group(GROUP_PANEL_BUTTON);
    for (int i = 0; i < width_blocks; i++) {
        int image_id;
        if (i == 0) {
            image_id = 3 * type + 40;
        } else if (i < width_blocks - 1) {
            image_id = 3 * type + 41;
        } else {
            image_id = 3 * type + 42;
        }
        Image::from_id(image_base + image_id).draw(x + BLOCK_SIZE * i, y, color, SCALE_NONE);
    }
}

static void draw_topleft_label_with_fragments(int x, int y, const lang_fragment *fragments, int fragment_count, font_t font, color_t color)
{
    // Measure total width using the new sequence width function
    int label_width = lang_text_get_sequence_width(fragments, fragment_count, font, screen_ui_to_pixel(font_definition_for(font)->line_height));

    int label_blocks = (label_width + 2 * BLOCK_SIZE) / BLOCK_SIZE;
    if (label_blocks < 1) label_blocks = 1;

    label_draw_masked(x, y, label_blocks, 1, color);

    // Draw the sequence using the new lang_fragment system
    lang_text_draw_sequence(fragments, fragment_count, x + 6, y + 4, font, screen_ui_to_pixel(font_definition_for(font)->line_height), COLOR_MASK_NONE);
}




static void draw_speedrun_info(void)
{
    if (config_get(CONFIG_UI_SHOW_SPEEDRUN_INFO)) {
        int s_height = screen_height();
        large_label_draw(0, s_height - 25, 10, 0);
        lang_text_draw_centered(current_string_key(153, setting_difficulty() + 1), 4, s_height - 18, 150, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }
}

static void draw_foreground(void)
{
    widget_top_menu_draw(0);
    window_city_draw();
    widget_sidebar_city_draw_foreground();
    draw_speedrun_info();
    if (window_city_is_window_cityview()) {
        draw_time_left();
        window_city_draw_custom_variables_text_display();
        widget_city_draw_construction_buttons();
        if (!mouse_get()->is_touch || sidebar_extra_is_information_displayed(SIDEBAR_EXTRA_DISPLAY_GAME_SPEED)) {
            draw_paused_banner();
        }
    }
    widget_city_draw_construction_cost_and_size();
    if (window_is(WINDOW_CITY)) {
        city_message_process_queue();
    }
}

static void draw_foreground_military(void)
{
    widget_top_menu_draw(0);
    window_city_draw();
    if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR)) {
        widget_sidebar_military_draw_foreground();
    } else {
        widget_sidebar_city_draw_foreground();
    }
    draw_time_left();
    widget_city_draw_construction_buttons();
    if (!mouse_get()->is_touch || sidebar_extra_is_information_displayed(SIDEBAR_EXTRA_DISPLAY_GAME_SPEED)) {
        draw_paused_banner();
    }
}

static void exit_military_command(void)
{
    if (window_is(WINDOW_CITY_MILITARY)) {
        window_city_show();
    }
}

static void show_roamers_for_overlay(int overlay)
{
    figure_roamer_preview_reset_building_types();

    switch (overlay) {
        case OVERLAY_FIRE:
        case OVERLAY_CRIME:
            create_roamer_preview_for_building_attr("prefecture");
            break;
        case OVERLAY_DAMAGE:
            create_roamer_preview_for_building_attr("engineers_post");
            break;
        case OVERLAY_TAVERN:
            create_roamer_preview_for_building_attr("tavern");
            break;
        case OVERLAY_THEATER:
            create_roamer_preview_for_building_attr("theater");
            break;
        case OVERLAY_AMPHITHEATER:
            create_roamer_preview_for_building_attr("amphitheater");
            break;
        case OVERLAY_ARENA:
            create_roamer_preview_for_building_attr("arena");
            break;
        case OVERLAY_COLOSSEUM:
            create_roamer_preview_for_building_attr("colosseum");
            break;
        case OVERLAY_HIPPODROME:
            create_roamer_preview_for_building_attr("hippodrome");
            break;
        case OVERLAY_SCHOOL:
            create_roamer_preview_for_building_attr("school");
            break;
        case OVERLAY_LIBRARY:
            create_roamer_preview_for_building_attr("library");
            break;
        case OVERLAY_ACADEMY:
            create_roamer_preview_for_building_attr("academy");
            break;
        case OVERLAY_BARBER:
            create_roamer_preview_for_building_attr("barber");
            break;
        case OVERLAY_BATHHOUSE:
            create_roamer_preview_for_building_attr("bathhouse");
            break;
        case OVERLAY_CLINIC:
            create_roamer_preview_for_building_attr("doctor");
            break;
        case OVERLAY_HOSPITAL:
            create_roamer_preview_for_building_attr("hospital");
            break;
        case OVERLAY_TAX_INCOME:
            create_roamer_preview_for_building_attr("forum");
            create_roamer_preview_for_building_attr("senate");
            break;
        case OVERLAY_FOOD_STOCKS:
            create_roamer_preview_for_building_attr("market");
            break;
        case OVERLAY_SICKNESS:
            create_roamer_preview_for_building_attr("doctor");
            create_roamer_preview_for_building_attr("hospital");
            break;
        case OVERLAY_NONE:
        default:
            break;
    }
    widget_city_clear_routing_grid_offset();
}

static void show_overlay(int overlay)
{
    exit_military_command();
    if (game_state_overlay() == overlay) {
        overlay = OVERLAY_NONE;
    }
    game_state_set_overlay(overlay);
    city_with_overlay_update();
    show_roamers_for_overlay(overlay);
    window_invalidate();
}

static int get_overlay_for_building_type(const building_type_registry_impl::BuildingType &type)
{
    const int is_dock = type.attr_is( "dock");
    int overlay = OVERLAY_NONE;

    if (type.is_well()) {
        overlay = OVERLAY_WATER;
    } else if (type.is_theater()) {
        overlay = OVERLAY_THEATER;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {
        "plaza", "road", "roadblock", "roofed_garden_wall_gate", "looped_garden_gate",
        "panelled_garden_gate", "highway"
    })) {
        overlay = OVERLAY_ROADS;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"aqueduct", "reservoir", "fountain"})) {
        overlay = OVERLAY_WATER;
    } else if (type.is_temple() ||
        building_type_registry_impl::type_attr_is_any(type.type(), {"lararium", "nymphaeum", "small_mausoleum", "large_mausoleum"})) {
        overlay = OVERLAY_RELIGION;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"prefecture", "burning_ruin"})) {
        overlay = OVERLAY_FIRE;
    } else if (type.is_architect_guild() || type.attr_is( "engineers_post")) {
        overlay = OVERLAY_DAMAGE;
    } else if (type.attr_is( "actor_colony")) {
        overlay = OVERLAY_THEATER;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"amphitheater", "gladiator_school"})) {
        overlay = OVERLAY_AMPHITHEATER;
    } else if (type.attr_is( "tavern")) {
        overlay = OVERLAY_TAVERN;
    } else if (type.attr_is( "arena")) {
        overlay = OVERLAY_ARENA;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"colosseum", "lion_house"})) {
        overlay = OVERLAY_COLOSSEUM;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"hippodrome", "chariot_maker"})) {
        overlay = OVERLAY_HIPPODROME;
    } else if (type.attr_is( "school")) {
        overlay = OVERLAY_SCHOOL;
    } else if (type.attr_is( "library")) {
        overlay = OVERLAY_LIBRARY;
    } else if (type.attr_is( "academy")) {
        overlay = OVERLAY_ACADEMY;
    } else if (type.attr_is( "barber")) {
        overlay = OVERLAY_BARBER;
    } else if (type.attr_is( "bathhouse")) {
        overlay = OVERLAY_BATHHOUSE;
    } else if (type.attr_is( "doctor")) {
        overlay = OVERLAY_CLINIC;
    } else if (type.attr_is( "hospital")) {
        overlay = OVERLAY_HOSPITAL;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"forum", "senate"})) {
        overlay = OVERLAY_TAX_INCOME;
    } else if (type.is_granary() || type.is_mess_hall() || type.is_caravanserai() ||
        resource_is_food(building_output_resource(type.type())) ||
        building_type_registry_impl::type_attr_is_any(type.type(), {
            "market", "oil_workshop", "wine_workshop", "wharf"
        })) {
        overlay = OVERLAY_FOOD_STOCKS;
    } else if (type.has_housing() ||
        building_type_registry_impl::type_attr_is_any(type.type(), {
            "gardens", "overgrown_gardens", "governors_house", "governors_villa", "governors_palace",
            "small_statue", "medium_statue", "large_statue", "triumphal_arch", "small_pond", "large_pond",
            "pine_tree", "fir_tree", "oak_tree", "elm_tree", "fig_tree", "plum_tree", "palm_tree", "date_tree",
            "pine_path", "fir_path", "oak_path", "elm_path", "fig_path", "plum_path", "palm_path", "date_path",
            "garden_path", "pavilion",
            "goddess_statue", "senator_statue", "obelisk", "horse_statue", "legion_statue", "gladiator_statue",
            "panelled_garden_wall"
        })) {
        overlay = OVERLAY_DESIRABILITY;
    } else if (building_type_registry_impl::type_attr_is_any(type.type(), {"mission_post", "native_hut", "native_hut_alt", "native_meeting"})) {
        overlay = OVERLAY_NATIVE;
    } else if (type.is_warehouse() || type.is_lighthouse() || type.is_armoury() ||
        is_dock || building_type_registry_impl::type_attr_is_any(type.type(), {"warehouse_space", "cart_depot"})) {
        overlay = OVERLAY_LOGISTICS;
    } else if (type.attr_is( "latrines")) {
        overlay = OVERLAY_HEALTH;
    }
    return overlay;
}

static int get_overlay_for_terrain(int terrain)
{
    if (terrain & TERRAIN_RUBBLE) {
        return OVERLAY_DAMAGE;
    }
    if (terrain & TERRAIN_AQUEDUCT) {
        return OVERLAY_WATER;
    }
    if (terrain & TERRAIN_GARDEN) {
        return OVERLAY_DESIRABILITY;
    }
    if (terrain & TERRAIN_ROAD) {
        return OVERLAY_ROADS;
    }
    if (terrain & TERRAIN_HIGHWAY) {
        return OVERLAY_ROADS;
    }
    return OVERLAY_NONE;
}

static void show_overlay_from_grid_offset(int grid_offset)
{
    int overlay = OVERLAY_NONE;
    const int terrain = map_terrain_get(grid_offset);
    const Building building = map_building_at(grid_offset).main();
    if (building.id && building.type) {
        overlay = get_overlay_for_building_type(*building.type);
    } else {
        overlay = get_overlay_for_terrain(terrain);
    }
    if (!(game_state_overlay() == OVERLAY_NONE && overlay == OVERLAY_NONE)) {
        show_overlay(overlay);
    }
}

static int has_storage_orders(const building_type_registry_impl::BuildingType &type)
{
    const int is_dock = type.attr_is( "dock");
    return type.has_native_storage() ||
        type.has_distribution() ||
        is_dock ||
        building_type_registry_impl::type_attr_is_any(type.type(), {
            "warehouse_space", "market", "tavern", "roadblock",
            "roofed_garden_wall_gate", "looped_garden_gate", "panelled_garden_gate",
            "hedge_gate_dark", "hedge_gate_light", "palisade_gate"
        }) ||
        ((type.attr_is( "small_temple_ceres") || type.attr_is( "large_temple_ceres")) &&
            building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD)) ||
        ((type.attr_is( "small_temple_venus") || type.attr_is( "large_temple_venus")) &&
            building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE));
}

static void toggle_mothball_building(Building *building)
{
    building->building_mothball_toggle();
    if (building->is_in_use()) {
        city_warning_show(WARNING_DATA_MOTHBALL_OFF, translation_for_key("TR_CITY_WARNING_DATA_MOTHBALL_OFF"));
    } else if (building->is_mothballed()) {
        city_warning_show(WARNING_DATA_MOTHBALL_ON, translation_for_key("TR_CITY_WARNING_DATA_MOTHBALL_ON"));
    }
}

static void copy_building_settings(Building &building)
{
    building_data_transfer_copy(&building, 0);
}

static void paste_building_settings(Building &building)
{
    building_data_transfer_paste(&building, 0);
}

static int tooltip_has_widget_payload(const tooltip_context *c)
{
    return c->type != TOOLTIP_NONE
        || c->text_id != 0
        || static_cast<bool>(c->translation_key)
        || c->precomposed_text != 0
        || c->has_numeric_prefix
        || c->num_extra_values > 0
        || c->num_extra_texts > 0;
}

static void cycle_legion(void)
{
    static int current_legion_id = 0;
    int next_legion_id = 0;
    for (int pass = 0; pass < 2 && next_legion_id == 0; pass++) {
        for (int i = 1; i < formation_count(); i++) {
            const formation *m = formation_get(i);
            if (!m || m->in_use != 1 || !m->is_legion || m->is_herd) {
                continue;
            }
            if ((pass == 0 && i > current_legion_id) || (pass == 1)) {
                next_legion_id = i;
                break;
            }
        }
    }
    if (next_legion_id > 0) {
        current_legion_id = next_legion_id;
        const formation *m = formation_get(current_legion_id);
        city_view_go_to_grid_offset(map_grid_offset(m->x_home, m->y_home));
        window_city_military_show(current_legion_id);
    }
}

static void toggle_pause(void)
{
    game_state_toggle_paused();
    city_warning_clear_all();
}

static void handle_hotkeys(const hotkeys *h)
{
    if (h->toggle_pause) {
        toggle_pause();
    }
    if (h->decrease_game_speed) {
        setting_decrease_game_speed();
    }
    if (h->increase_game_speed) {
        setting_increase_game_speed();
    }
    if (h->show_overlay) {
        show_overlay(h->show_overlay);
        window_overlay_menu_update();
    }
    if (h->show_overlay_relative) {
        show_overlay_from_grid_offset(widget_city_current_grid_offset());
        window_overlay_menu_update();
    }
    if (h->toggle_overlay) {
        exit_military_command();
        game_state_toggle_overlay();
        show_roamers_for_overlay(game_state_overlay());
        city_with_overlay_update();
        window_overlay_menu_update();
        window_invalidate();
    }
    if (h->show_advisor) {
        window_advisors_show_advisor(static_cast<advisor_type>(h->show_advisor));
    }
    if (h->cycle_legion) {
        cycle_legion();
    }
    if (h->rotate_map_left) {
        if (!building_construction_in_progress()) {
            game_orientation_apply(GameOrientationRequest::turn_quarter_steps(1));
            window_invalidate();
        }
    }
    if (h->rotate_map_right) {
        if (!building_construction_in_progress()) {
            game_orientation_apply(GameOrientationRequest::turn_quarter_steps(-1));
            window_invalidate();
        }
    }
    if (h->rotate_map_north) {
        if (!building_construction_in_progress()) {
            game_orientation_apply(GameOrientationRequest::face(DIR_0_TOP));
            window_invalidate();
        }
    }
    if (h->go_to_bookmark) {
        if (map_bookmark_go_to(h->go_to_bookmark - 1)) {
            window_invalidate();
        }
    }
    if (h->set_bookmark) {
        map_bookmark_save(h->set_bookmark - 1);
    }
    if (h->load_file) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_LOAD);
    }
    if (h->save_file) {
        window_file_dialog_show(FILE_TYPE_SAVED_GAME, FILE_DIALOG_SAVE);
    }
    if (h->rotate_building) {
        building_rotation_rotate_forward();
    }
    if (h->rotate_building_back) {
        building_rotation_rotate_backward();
    }
    if (h->building) {
        set_construction_building_type(
            building_type_registry_impl::definition_for_type(static_cast<building_type>(h->building)),
            0);
    }
    if (h->undo) {
        game_undo_perform();
        window_invalidate();
    }
    if (h->mothball_toggle) {
        Building *building = map_building_at(widget_city_current_grid_offset()).main();
        building ? toggle_mothball_building(building) : (void)0;
    }
    if (h->storage_order) {
        int grid_offset = widget_city_current_grid_offset();
        const Building building = map_building_at(grid_offset).main();
        if (building.id && building.type) {
            if (has_storage_orders(*building.type)) {
                window_building_info_show(grid_offset);
                window_building_info_show_storage_orders();
            }
        }
    }
    if (h->clone_building) {
        Building building = map_building_at(widget_city_current_grid_offset()).main();
        const building_type_registry_impl::BuildingType *type =
            building.og_type ? building.og_type : building.type;
        set_construction_building_type(type, building.Graphics().rotation());
    }
    if (h->copy_building_settings) {
        Building building = map_building_at(widget_city_current_grid_offset()).main();
        copy_building_settings(building);
    }
    if (h->paste_building_settings) {
        Building building = map_building_at(widget_city_current_grid_offset()).main();
        paste_building_settings(building);
    }
    if (h->show_empire_map) {
        if (!window_is(WINDOW_EMPIRE)) {
            window_empire_show_checked();
        }
    }
    if (h->show_messages) {
        window_message_list_show();
    }
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    const mouse *m_pixel = mouse_get_pixel();
    handle_hotkeys(h);
    if (!building_construction_in_progress()) {
        if (widget_top_menu_handle_input(m, h)) {
            return;
        }
        if (widget_sidebar_city_handle_mouse(m)) {
            return;
        }
    }
    widget_city_handle_input(m_pixel, h);
}

static void handle_input_military(const mouse *m, const hotkeys *h)
{
    const mouse *m_pixel = mouse_get_pixel();
    handle_hotkeys(h);
    if (widget_top_menu_handle_input(m, h)) {
        return;
    }
    if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR) && widget_sidebar_military_handle_input(m)) {
        return;
    }
    widget_city_handle_input_military(m_pixel, h, formation_get_selected());
}

static void get_tooltip(tooltip_context *c)
{
    int text_id = widget_top_menu_get_tooltip_text(c);
    if (!text_id && !tooltip_has_widget_payload(c)) {
        if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR) && formation_get_selected()) {
            text_id = widget_sidebar_military_get_tooltip_text(c);
        } else {
            text_id = widget_sidebar_city_get_tooltip_text(c);
        }
    }
    if (text_id || tooltip_has_widget_payload(c)) {
        if (c->type == TOOLTIP_NONE) {
            c->type = TOOLTIP_BUTTON;
        }
        if (text_id) {
            c->text_id = text_id;
        }
        return;
    }
    const mouse *m_pixel = mouse_get_pixel();
    c->mouse_x = m_pixel->x;
    c->mouse_y = m_pixel->y;
    screen_set_pixel_render_scale();
    widget_city_get_tooltip(c);
    if (c->type == TOOLTIP_NONE) {
        screen_set_ui_render_scale();
    }
}

int window_city_military_is_cursor_in_menu(void)
{
    if (!config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR) || !window_is(WINDOW_CITY_MILITARY)) {
        return 0;
    }
    const mouse *m = mouse_get_pixel();
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    y += 24;
    height += 24;
    return m->x < x || m->x >= width || m->y < y || m->y >= height;
}

void window_city_draw_all(void)
{
    screen_set_ui_render_scale();
    if (formation_get_selected() && config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR)) {
        draw_background_military();
        draw_foreground_military();
    } else {
        draw_background();
        draw_foreground();
    }
}

void window_city_draw_panels(void)
{
    screen_set_ui_render_scale();
    if (formation_get_selected() && config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR)) {
        draw_background_military();
    } else {
        draw_background();
    }
}

void window_city_draw(void)
{
    screen_set_pixel_render_scale();
    widget_city_draw();
    screen_set_ui_render_scale();
}

void window_city_show(void)
{
    show_roamers_for_overlay(game_state_overlay());
    if (formation_get_selected()) {
        formation_set_selected(0);
        if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR) && widget_sidebar_military_exit()) {
            return;
        }
    }
    window_type window = {
        WINDOW_CITY,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}

void window_city_military_show(int legion_formation_id)
{
    if (building_construction_type()) {
        building_construction_cancel();
        building_construction_clear_type();
    }
    formation_set_selected(legion_formation_id);
    if (config_get(CONFIG_UI_SHOW_MILITARY_SIDEBAR) && widget_sidebar_military_enter(legion_formation_id)) {
        return;
    }
    window_type window = {
        WINDOW_CITY_MILITARY,
        draw_background_military,
        draw_foreground_military,
        handle_input_military,
        get_tooltip
    };
    window_show(&window);
}

void window_city_return(void)
{
    int formation_id = formation_get_selected();
    if (formation_id) {
        window_city_military_show(formation_id);
    } else {
        window_city_show();
    }
}
