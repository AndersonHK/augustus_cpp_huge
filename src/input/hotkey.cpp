#include "window/hotkey_editor.h"
#include "window/popup_dialog.h"
#include "hotkey.h"

#include "building/building_type_registry_internal.h"
#include "city/constants.h"
#include "empire/editor.h"
#include "game/settings.h"
#include "game/state.h"
#include "game/system.h"
#include "graphics/screenshot.h"
#include "graphics/video.h"
#include "graphics/window.h"
#include "input/scroll.h"
#include "sound/music.h"

#include <stdlib.h>
#include <string.h>

typedef struct {
    int *action;
    const building_type_registry_impl::BuildingType **building_action;
    int value;
    const building_type_registry_impl::BuildingType *building_value;
    key_type key;
    key_modifier_type modifiers;
    int repeatable;
} hotkey_definition;

typedef struct {
    void (*action)(int is_down);
    key_type key;
} arrow_definition;

typedef struct {
    int center_screen;
    int toggle_fullscreen;
    int resize_to;
    int save_screenshot;
    int save_city_screenshot;
    int save_minimap_screenshot;
    int next_track;
} global_hotkeys;

typedef struct {
    key_type keys[10];
} build_menu_hotkeys;

static struct {
    global_hotkeys global_hotkey_state;
    hotkeys hotkey_state;
    hotkey_definition *definitions;
    int num_definitions;
    arrow_definition *arrows;
    int num_arrows;
    key_modifier_type modifiers;
    build_menu_hotkeys build_menu_hotkeys;
} data;

static int set_building_action(hotkey_definition *def, building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        def->building_action = nullptr;
        def->building_value = nullptr;
        return 0;
    }
    def->building_action = &data.hotkey_state.building;
    def->building_value = definition;
    return 1;
}

static int set_building_action_from_text(hotkey_definition *def, const char *text_id)
{
    return set_building_action(def, building_type_registry_impl::type_from_attr(text_id));
}

static void set_definition_for_action(hotkey_action action, hotkey_definition *def)
{
    def->action = nullptr;
    def->building_action = nullptr;
    def->building_value = nullptr;
    def->value = 1;
    def->repeatable = 0;
    if (const char *building_text_id = hotkey_building_text_id_for_action(action)) {
        set_building_action_from_text(def, building_text_id);
        return;
    }
    switch (action) {
        case HOTKEY_TOGGLE_PAUSE:
            def->action = &data.hotkey_state.toggle_pause;
            break;
        case HOTKEY_TOGGLE_OVERLAY:
            def->action = &data.hotkey_state.toggle_overlay;
            break;
        case HOTKEY_CYCLE_LEGION:
            def->action = &data.hotkey_state.cycle_legion;
            break;
        case HOTKEY_INCREASE_GAME_SPEED:
            def->action = &data.hotkey_state.increase_game_speed;
            def->repeatable = 1;
            break;
        case HOTKEY_DECREASE_GAME_SPEED:
            def->action = &data.hotkey_state.decrease_game_speed;
            def->repeatable = 1;
            break;
        case HOTKEY_ROTATE_MAP_LEFT:
            def->action = &data.hotkey_state.rotate_map_left;
            break;
        case HOTKEY_ROTATE_MAP_RIGHT:
            def->action = &data.hotkey_state.rotate_map_right;
            break;
        case HOTKEY_ZOOM_IN:
            def->action = &data.hotkey_state.zoom_in;
            break;
        case HOTKEY_ZOOM_OUT:
            def->action = &data.hotkey_state.zoom_out;
            break;
        case HOTKEY_RESET_ZOOM:
            def->action = &data.hotkey_state.reset_zoom;
            break;
        case HOTKEY_SHOW_ADVISOR_LABOR:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_LABOR;
            break;
        case HOTKEY_SHOW_ADVISOR_MILITARY:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_MILITARY;
            break;
        case HOTKEY_SHOW_ADVISOR_IMPERIAL:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_IMPERIAL;
            break;
        case HOTKEY_SHOW_ADVISOR_RATINGS:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_RATINGS;
            break;
        case HOTKEY_SHOW_ADVISOR_TRADE:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_TRADE;
            break;
        case HOTKEY_SHOW_ADVISOR_POPULATION:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_POPULATION;
            break;
        case HOTKEY_SHOW_ADVISOR_HEALTH:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_HEALTH;
            break;
        case HOTKEY_SHOW_ADVISOR_EDUCATION:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_EDUCATION;
            break;
        case HOTKEY_SHOW_ADVISOR_ENTERTAINMENT:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_ENTERTAINMENT;
            break;
        case HOTKEY_SHOW_ADVISOR_RELIGION:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_RELIGION;
            break;
        case HOTKEY_SHOW_ADVISOR_FINANCIAL:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_FINANCIAL;
            break;
        case HOTKEY_SHOW_ADVISOR_CHIEF:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_CHIEF;
            break;
        case HOTKEY_SHOW_ADVISOR_HOUSING:
            def->action = &data.hotkey_state.show_advisor;
            def->value = ADVISOR_HOUSING;
            break;
        case HOTKEY_SHOW_OVERLAY_RELATIVE:
            def->action = &data.hotkey_state.show_overlay_relative;
            break;
        case HOTKEY_SHOW_OVERLAY_WATER:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_WATER;
            break;
        case HOTKEY_SHOW_OVERLAY_FIRE:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_FIRE;
            break;
        case HOTKEY_SHOW_OVERLAY_DAMAGE:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_DAMAGE;
            break;
        case HOTKEY_SHOW_OVERLAY_CRIME:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_CRIME;
            break;
        case HOTKEY_SHOW_OVERLAY_PROBLEMS:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_PROBLEMS;
            break;
        case HOTKEY_EDITOR_TOGGLE_BATTLE_INFO:
            def->action = &data.hotkey_state.toggle_editor_battle_info;
            break;
        case HOTKEY_LOAD_FILE:
            def->action = &data.hotkey_state.load_file;
            break;
        case HOTKEY_SAVE_FILE:
            def->action = &data.hotkey_state.save_file;
            break;
        case HOTKEY_ROTATE_BUILDING:
            def->action = &data.hotkey_state.rotate_building;
            break;
        case HOTKEY_ROTATE_BUILDING_BACK:
            def->action = &data.hotkey_state.rotate_building_back;
            break;
        case HOTKEY_GO_TO_BOOKMARK_1:
            def->action = &data.hotkey_state.go_to_bookmark;
            def->value = 1;
            break;
        case HOTKEY_GO_TO_BOOKMARK_2:
            def->action = &data.hotkey_state.go_to_bookmark;
            def->value = 2;
            break;
        case HOTKEY_GO_TO_BOOKMARK_3:
            def->action = &data.hotkey_state.go_to_bookmark;
            def->value = 3;
            break;
        case HOTKEY_GO_TO_BOOKMARK_4:
            def->action = &data.hotkey_state.go_to_bookmark;
            def->value = 4;
            break;
        case HOTKEY_SET_BOOKMARK_1:
            def->action = &data.hotkey_state.set_bookmark;
            def->value = 1;
            break;
        case HOTKEY_SET_BOOKMARK_2:
            def->action = &data.hotkey_state.set_bookmark;
            def->value = 2;
            break;
        case HOTKEY_SET_BOOKMARK_3:
            def->action = &data.hotkey_state.set_bookmark;
            def->value = 3;
            break;
        case HOTKEY_SET_BOOKMARK_4:
            def->action = &data.hotkey_state.set_bookmark;
            def->value = 4;
            break;
        case HOTKEY_CENTER_WINDOW:
            def->action = &data.global_hotkey_state.center_screen;
            break;
        case HOTKEY_TOGGLE_FULLSCREEN:
            def->action = &data.global_hotkey_state.toggle_fullscreen;
            break;
        case HOTKEY_RESIZE_TO_640:
            def->action = &data.global_hotkey_state.resize_to;
            def->value = 640;
            break;
        case HOTKEY_RESIZE_TO_800:
            def->action = &data.global_hotkey_state.resize_to;
            def->value = 800;
            break;
        case HOTKEY_RESIZE_TO_1024:
            def->action = &data.global_hotkey_state.resize_to;
            def->value = 1024;
            break;
        case HOTKEY_SAVE_SCREENSHOT:
            def->action = &data.global_hotkey_state.save_screenshot;
            break;
        case HOTKEY_SAVE_CITY_SCREENSHOT:
            def->action = &data.global_hotkey_state.save_city_screenshot;
            break;
        case HOTKEY_SAVE_MINIMAP_SCREENSHOT:
            def->action = &data.global_hotkey_state.save_minimap_screenshot;
            break;
        case HOTKEY_BUILD_VACANT_HOUSE:
            set_building_action(def, building_type_registry_impl::vacant_lot_fill_type());
            break;
        case HOTKEY_BUILD_CLONE:
            def->action = &data.hotkey_state.clone_building;
            break;
        case HOTKEY_UNDO:
            def->action = &data.hotkey_state.undo;
            break;
        case HOTKEY_MOTHBALL_TOGGLE:
            def->action = &data.hotkey_state.mothball_toggle;
            break;
        case HOTKEY_STORAGE_ORDER:
            def->action = &data.hotkey_state.storage_order;
            break;
        case HOTKEY_COPY_BUILDING_SETTINGS:
            def->action = &data.hotkey_state.copy_building_settings;
            break;
        case HOTKEY_PASTE_BUILDING_SETTINGS:
            def->action = &data.hotkey_state.paste_building_settings;
            break;
        case HOTKEY_SHOW_OVERLAY_ENTERTAINMENT:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_ENTERTAINMENT;
            break;
        case HOTKEY_SHOW_OVERLAY_EDUCATION:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_EDUCATION;
            break;
        case HOTKEY_SHOW_OVERLAY_SCHOOL:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_SCHOOL;
            break;
        case HOTKEY_SHOW_OVERLAY_LIBRARY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_LIBRARY;
            break;
        case HOTKEY_SHOW_OVERLAY_ACADEMY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_ACADEMY;
            break;
        case HOTKEY_SHOW_OVERLAY_HEALTH:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_HEALTH;
            break;
        case HOTKEY_SHOW_OVERLAY_BARBER:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_BARBER;
            break;
        case HOTKEY_SHOW_OVERLAY_BATHHOUSE:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_BATHHOUSE;
            break;
        case HOTKEY_SHOW_OVERLAY_CLINIC:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_CLINIC;
            break;
        case HOTKEY_SHOW_OVERLAY_HOSPITAL:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_HOSPITAL;
            break;
        case HOTKEY_SHOW_OVERLAY_SICKNESS:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_SICKNESS;
            break;
        case HOTKEY_SHOW_OVERLAY_LOGISTICS:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_LOGISTICS;
            break;
        case HOTKEY_SHOW_OVERLAY_FOOD_STOCKS:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_FOOD_STOCKS;
            break;
        case HOTKEY_SHOW_OVERLAY_EFFICIENCY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_EFFICIENCY;
            break;
        case HOTKEY_SHOW_OVERLAY_MOTHBALL:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_MOTHBALL;
            break;
        case HOTKEY_SHOW_OVERLAY_TAX_INCOME:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_TAX_INCOME;
            break;
        case HOTKEY_SHOW_OVERLAY_LEVY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_LEVY;
            break;
        case HOTKEY_SHOW_OVERLAY_EMPLOYMENT:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_EMPLOYMENT;
            break;
        case HOTKEY_SHOW_OVERLAY_RELIGION:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_RELIGION;
            break;
        case HOTKEY_SHOW_OVERLAY_DESIRABILITY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_DESIRABILITY;
            break;
        case HOTKEY_SHOW_OVERLAY_SENTIMENT:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_SENTIMENT;
            break;
        case HOTKEY_SHOW_OVERLAY_ROADS:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_ROADS;
            break;
        case HOTKEY_ROTATE_MAP_NORTH:
            def->action = &data.hotkey_state.rotate_map_north;
            break;
        case HOTKEY_SHOW_EMPIRE_MAP:
            def->action = &data.hotkey_state.show_empire_map;
            break;
        case HOTKEY_SHOW_MESSAGES:
            def->action = &data.hotkey_state.show_messages;
            break;
        case HOTKEY_SHOW_OVERLAY_RISKS_NATIVE:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_NATIVE;
            break;
        case HOTKEY_SHOW_OVERLAY_ENEMY:
            def->action = &data.hotkey_state.show_overlay;
            def->value = OVERLAY_ENEMY;
            break;
        case HOTKEY_NEXT_TRACK:
            def->action = &data.global_hotkey_state.next_track;
            break;
        case HOTKEY_EDITOR_EMPIRE_DELETE_OBJECT:
            def->action = &data.hotkey_state.delete_empire_object;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_OUR_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_OUR_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_TRADE_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_TRADE_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_ROMAN_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_ROMAN_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_VULNERABLE_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_VULNERABLE_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_FUTURE_TRADE_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_FUTURE_TRADE_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_DISTANT_CITY:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_DISTANT_CITY + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_BORDER:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_BORDER + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_BATTLE:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_BATTLE + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_BABRIAN:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_DISTANT_BABARIAN + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_LEGION:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_DISTANT_LEGION + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_LAND_POINT:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_LAND_ROUTE + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_SEA_POINT:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_SEA_ROUTE + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_TOOL_SELECTION:
            def->action = &data.hotkey_state.empire_tool;
            def->value = EMPIRE_TOOL_MAX + 1;
            break;
        case HOTKEY_EDITOR_EMPIRE_PICK_TOOL:
            def->action = &data.hotkey_state.pick_empire_tool;
            break;
        default:
            def->action = 0;
    }
}

static void add_definition(const hotkey_mapping *mapping)
{
    hotkey_definition *def = &data.definitions[data.num_definitions];
    def->key = mapping->key;
    def->modifiers = mapping->modifiers;
    set_definition_for_action(mapping->action, def);
    if (mapping->building_text_id[0]) {
        set_building_action_from_text(def, mapping->building_text_id);
    }
    if (def->action || def->building_action) {
        data.num_definitions++;
    }
}

static void add_arrow(const hotkey_mapping *mapping)
{
    arrow_definition *arrow = &data.arrows[data.num_arrows];
    arrow->key = mapping->key;
    switch (mapping->action) {
        case HOTKEY_ARROW_UP:
            arrow->action = scroll_arrow_up;
            break;
        case HOTKEY_ARROW_DOWN:
            arrow->action = scroll_arrow_down;
            break;
        case HOTKEY_ARROW_LEFT:
            arrow->action = scroll_arrow_left;
            break;
        case HOTKEY_ARROW_RIGHT:
            arrow->action = scroll_arrow_right;
            break;
        default:
            arrow->action = 0;
            break;
    }
    if (arrow->action) {
        data.num_arrows++;
    }
}

static int allocate_mapping_memory(int total_definitions, int total_arrows)
{
    free(data.definitions);
    free(data.arrows);
    data.num_definitions = 0;
    data.num_arrows = 0;
    data.definitions = static_cast<hotkey_definition *>(malloc(sizeof(hotkey_definition) * total_definitions));
    data.arrows = static_cast<arrow_definition *>(malloc(sizeof(arrow_definition) * total_arrows));
    if (!data.definitions || !data.arrows) {
        free(data.definitions);
        free(data.arrows);
        return 0;
    }
    memset(data.definitions, 0, sizeof(hotkey_definition) * total_definitions);
    memset(data.arrows, 0, sizeof(arrow_definition) * total_arrows);
    return 1;
}

void hotkey_install_mapping(hotkey_mapping *mappings, int num_mappings)
{
    int total_definitions = 5; // Fixed keys: Enter, ESC, F5, Delete, Backspace
    int total_arrows = 0;
    for (int i = 0; i < num_mappings; i++) {
        hotkey_action action = mappings[i].action;
        if (action == HOTKEY_ARROW_UP || action == HOTKEY_ARROW_DOWN ||
            action == HOTKEY_ARROW_LEFT || action == HOTKEY_ARROW_RIGHT) {
            total_arrows++;
        } else {
            total_definitions++;
        }
    }
    if (!allocate_mapping_memory(total_definitions, total_arrows)) {
        return;
    }

    // Fixed keys: Enter, ESC, F5, Delete, Backspace -- yep they're still fixed even down here. crazy, i know
    data.definitions[0].action = &data.hotkey_state.enter_pressed;
    data.definitions[0].key = KEY_TYPE_ENTER;
    data.definitions[0].modifiers = KEY_MOD_NONE;
    data.definitions[0].repeatable = 0;
    data.definitions[0].value = 1;

    data.definitions[1].action = &data.hotkey_state.escape_pressed;
    data.definitions[1].key = KEY_TYPE_ESCAPE;
    data.definitions[1].modifiers = KEY_MOD_NONE;
    data.definitions[1].repeatable = 0;
    data.definitions[1].value = 1;

    data.definitions[2].action = &data.hotkey_state.f5_pressed;
    data.definitions[2].key = KEY_TYPE_F5;
    data.definitions[2].modifiers = KEY_MOD_NONE;
    data.definitions[2].repeatable = 0;
    data.definitions[2].value = 1;

    data.definitions[3].action = &data.hotkey_state.delete_pressed;
    data.definitions[3].key = KEY_TYPE_DELETE;
    data.definitions[3].modifiers = KEY_MOD_NONE;
    data.definitions[3].repeatable = 0;
    data.definitions[3].value = 1;

    data.definitions[4].action = &data.hotkey_state.backspace_pressed;
    data.definitions[4].key = KEY_TYPE_BACKSPACE;
    data.definitions[4].modifiers = KEY_MOD_NONE;
    data.definitions[4].repeatable = 0;
    data.definitions[4].value = 1;

    data.num_definitions = 5;

    for (int i = 0; i < num_mappings; i++) {
        hotkey_action action = mappings[i].action;
        if (action == HOTKEY_ARROW_UP || action == HOTKEY_ARROW_DOWN ||
            action == HOTKEY_ARROW_LEFT || action == HOTKEY_ARROW_RIGHT) {
            add_arrow(&mappings[i]);
        } else {
            add_definition(&mappings[i]);
        }
    }

    for (int i = 0; i < 10; i++) {
        key_type k = static_cast<key_type>(KEY_TYPE_1 + i);
        data.build_menu_hotkeys.keys[i] = k;
    }
}

const hotkeys *hotkey_state(void)
{
    return &data.hotkey_state;
}

void hotkey_reset_state(void)
{
    memset(&data.hotkey_state, 0, sizeof(data.hotkey_state));
    memset(&data.global_hotkey_state, 0, sizeof(data.global_hotkey_state));
}

void hotkey_key_pressed(key_type key, key_modifier_type modifiers, int repeat)
{
    data.modifiers = modifiers;

    if (window_is(WINDOW_HOTKEY_EDITOR)) {
        window_hotkey_editor_key_pressed(key, modifiers);
        return;
    }
    if (key == KEY_TYPE_NONE) {
        return;
    }
    int found_action = 0;
    for (int i = 0; i < data.num_definitions; i++) {
        hotkey_definition *def = &data.definitions[i];
        if ((window_is(WINDOW_ASSET_PREVIEWER) || window_is(WINDOW_EDITOR_EMPIRE)) &&
            key == KEY_TYPE_F5 && def->action != &data.hotkey_state.f5_pressed) {
            continue;
        }
        if (def->key == key && def->modifiers == modifiers && (!repeat || def->repeatable)) {
            if (def->action) {
                *(def->action) = def->value;
            } else if (def->building_action) {
                *(def->building_action) = def->building_value;
            }
            found_action = 1;
        }
    }
    for (int i = 0; i < 10; i++) {
        if (data.build_menu_hotkeys.keys[i] == key) {
            data.hotkey_state.build_menu_index_num = (key - KEY_TYPE_1) + 1;
            found_action = 1;
        }
    }
    if (found_action) {
        return;
    }
    for (int i = 0; i < data.num_arrows; i++) {
        arrow_definition *arrow = &data.arrows[i];
        if (arrow->key == key) {
            arrow->action(1);
        }
    }
}

void hotkey_key_released(key_type key, key_modifier_type modifiers)
{
    data.modifiers = modifiers;

    if (window_is(WINDOW_HOTKEY_EDITOR)) {
        window_hotkey_editor_key_released(key, modifiers);
        return;
    }
    if (key == KEY_TYPE_NONE) {
        return;
    }
    for (int i = 0; i < data.num_arrows; i++) {
        arrow_definition *arrow = &data.arrows[i];
        if (arrow->key == key) {
            arrow->action(0);
        }
    }
}

key_modifier_type hotkey_get_modifiers(void)
{
    return data.modifiers;
}

static void confirm_exit(int accepted, int checked)
{
    (void) checked;
    if (accepted) {
        system_exit();
    }
}

void hotkey_handle_escape(void)
{
    video_stop();
    window_popup_dialog_show(POPUP_DIALOG_QUIT, confirm_exit, 1);
}

void hotkey_handle_global_keys(void)
{
    if (data.global_hotkey_state.center_screen) {
        system_center();
    }
    if (data.global_hotkey_state.resize_to) {
        switch (data.global_hotkey_state.resize_to) {
            case 640: system_resize(640, 480); break;
            case 800: system_resize(800, 600); break;
            case 1024: system_resize(1024, 768); break;
        }
    }
    if (data.global_hotkey_state.toggle_fullscreen) {
        system_set_fullscreen(!setting_fullscreen());
    }
    if (data.global_hotkey_state.save_screenshot) {
        graphics_save_screenshot(SCREENSHOT_DISPLAY);
    }
    if (data.global_hotkey_state.save_city_screenshot) {
        graphics_save_screenshot(SCREENSHOT_FULL_CITY);
    }
    if (data.global_hotkey_state.save_minimap_screenshot) {
        graphics_save_screenshot(SCREENSHOT_MINIMAP);
    }
    if (data.global_hotkey_state.next_track) {
        sound_music_next_track();
    }
}

void hotkey_set_value_for_action(hotkey_action action, int value)
{
    hotkey_definition def = {};
    set_definition_for_action(action, &def);
    if (def.action) {
        *(def.action) = value ? def.value : 0;
    }
}
