#include "building/industry.h"
#include "translation/translation.h"
#include "building/storage.h"
#include "game/resource_graphics.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"
#include "window/building_info.h"

#include "building/distribution.h"

#include "distribution.h"
#include "window/option_popup.h"
#include "building/building.h"
#include "building/caravanserai.h"
#include "building/dock.h"
#include "building/lighthouse.h"
#include "building/market.h"

extern "C" {
#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/building_record.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "core/dir.h"
#include "city/finance.h"
#include "city/military.h"
#include "city/resource.h"
#include "city/trade_policy.h"
#include "core/string.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/trade_route.h"
#include "figure/figure.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/image_button.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "sound/speech.h"

}

#include <cstring>
#include <math.h>

static const resource_list *stored_resources_for_type(const building_type_registry_impl::BuildingType &type)
{
    return type.is_granary() ? city_resource_get_potential_foods() : city_resource_get_potential();
}

static int is_tavern_distribution(const building_type_registry_impl::BuildingType &type)
{
    return type.has_distribution() && !type.has_market()
        && !type.is_venus_temple() && !type.is_ceres_temple() && !type.is_temple()
        && !type.is_caravanserai() && !type.is_mess_hall() && !type.is_lighthouse();
}

static int is_monument_working(building *b)
{
    return b && b->monument.phase == MONUMENT_FINISHED
        && b->state == BUILDING_STATE_IN_USE
        && !building_monument_has_labour_problems(b);
}

static int is_granary(const Building &building)
{
    return building.type().is_granary();
}

static int is_warehouse(const Building &building)
{
    return building.type().is_warehouse();
}

static int storage_strings_for_building(const Building &building)
{
    return is_granary(building) ? 98 : 99;
}

static void draw_resource_icon_centered(resource_type resource, int x, int y, int width = 25, int height = 25)
{
    const ImageGroupEntryRef &icon = resource_graphics(resource).panel_icon();
    icon.draw(x + (width - icon.width()) / 2, y + (height - icon.height()) / 2);
}
#include <stdio.h>

static const building_type_registry_impl::Distribution *distribution_for(const Building &building)
{
    return building.type().distribution();
}

static int distribution_accepts_nothing(const Building &building)
{
    const building_type_registry_impl::Distribution *distribution = distribution_for(building);
    return distribution ? distribution->accepts_nothing(building) : 1;
}

static void set_distribution_acceptance(Building &building, int value)
{
    if (const building_type_registry_impl::Distribution *distribution = distribution_for(building)) {
        distribution->set_acceptance(building, value);
    }
}

static void go_to_orders(const generic_button *button);
static void toggle_partial_resource_state(const generic_button *button, int reverse_order);
static void toggle_partial_resource_state_reverse(const generic_button *button);
static void toggle_partial_resource_state_forward(const generic_button *button);
static void resource_state_forward(const generic_button *button);
static void resource_state_back(const generic_button *button);
static void toggle_resource_state(const generic_button *button, int reverse_order);
static void dock_toggle_route(const generic_button *button);
static void storage_empty_all(const generic_button *button);
static void storage_toggle_all_states(int param1, int param2);
static void market_orders(const generic_button *button);
static void storage_toggle_permissions(const generic_button *button);
static void button_stockpiling(const generic_button *button);
static void toggle_mantain(int param1, int param2);
static void toggle_permissions_all(int param1, int param2);
static void roadblock_orders(int param1, int param2);
static void init_dock_permission_buttons(void);
static void draw_dock_permission_buttons(int x_offset, int y_offset, int dock_id);
static void on_scroll(void);


static void button_caravanserai_policy(const generic_button *button);
static void button_lighthouse_policy(const generic_button *button);

typedef struct {
    building_storage_permission_states permission;
    int x;
    int y;
    int width;
    int height;
} permission_button_info;
// Maximum number of permission buttons that can be displayed
#define MAX_PERMISSION_BUTTONS 16
#define WIDTH_WINDOW_BORDER 4
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define SMALL_ICON_SIDE 24
// Dynamic permission button system
static generic_button permission_buttons[MAX_PERMISSION_BUTTONS];
static int active_permissions_count = 0;

static building_storage_permission_states permission_from_button(const generic_button *button)
{
    return static_cast<building_storage_permission_states>(button->parameter1);
}

static resource_type resource_from_int(int resource)
{
    return static_cast<resource_type>(resource);
}

static generic_button make_permission_button(int x, int y, int size, building_storage_permission_states permission)
{
    generic_button button = {};
    button.x = static_cast<short>(x);
    button.y = static_cast<short>(y);
    button.width = static_cast<short>(size);
    button.height = static_cast<short>(size);
    button.left_click_handler = storage_toggle_permissions;
    button.parameter1 = static_cast<int>(permission);
    return button;
}

static building_storage_permission_states granary_permissions_buttons[] = {
    BUILDING_STORAGE_PERMISSION_MARKET,
    BUILDING_STORAGE_PERMISSION_TRADERS,
    BUILDING_STORAGE_PERMISSION_DOCK,
    BUILDING_STORAGE_PERMISSION_BARKEEP,
    BUILDING_STORAGE_PERMISSION_QUARTERMASTER,
    BUILDING_STORAGE_PERMISSION_CARAVANSERAI,
    BUILDING_STORAGE_PERMISSION_NATIVES,
    BUILDING_STORAGE_PERMISSION_CAESAR,
};
static building_storage_permission_states warehouse_permissions_buttons[] = {
    BUILDING_STORAGE_PERMISSION_MARKET,
    BUILDING_STORAGE_PERMISSION_TRADERS,
    BUILDING_STORAGE_PERMISSION_DOCK,
    BUILDING_STORAGE_PERMISSION_BARKEEP,
    BUILDING_STORAGE_PERMISSION_WORKCAMP,
    BUILDING_STORAGE_PERMISSION_ARMOURY,
    BUILDING_STORAGE_PERMISSION_LIGHTHOUSE,
    BUILDING_STORAGE_PERMISSION_NATIVES,
    BUILDING_STORAGE_PERMISSION_CAESAR,
};
static generic_button orders_resource_buttons[] = {
    {0, 0, 210, 22, resource_state_forward, resource_state_back, 1},
    {0, 22, 210, 22, resource_state_forward, resource_state_back, 2},
    {0, 44, 210, 22, resource_state_forward, resource_state_back, 3},
    {0, 66, 210, 22, resource_state_forward, resource_state_back, 4},
    {0, 88, 210, 22, resource_state_forward, resource_state_back, 5},
    {0, 110, 210, 22, resource_state_forward, resource_state_back, 6},
    {0, 132, 210, 22, resource_state_forward, resource_state_back, 7},
    {0, 154, 210, 22, resource_state_forward, resource_state_back, 8},
    {0, 176, 210, 22, resource_state_forward, resource_state_back, 9},
    {0, 198, 210, 22, resource_state_forward, resource_state_back, 10},
    {0, 220, 210, 22, resource_state_forward, resource_state_back, 11},
    {0, 242, 210, 22, resource_state_forward, resource_state_back, 12},
    {0, 264, 210, 22, resource_state_forward, resource_state_back, 13},
    {0, 286, 210, 22, resource_state_forward, resource_state_back, 14},
    {0, 308, 210, 22, resource_state_forward, resource_state_back, 15},
    {0, 330, 210, 22, resource_state_forward, resource_state_back, 16},
};

static generic_button orders_partial_resource_buttons[] = {
    {210, 0, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 1,0},
    {210, 22, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 2,0},
    {210, 44, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 3,0},
    {210, 66, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 4,0},
    {210, 88, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 5,0},
    {210, 110, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 6,0},
    {210, 132, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 7,0},
    {210, 154, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 8,0},
    {210, 176, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 9,0},
    {210, 198, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 10,0},
    {210, 220, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 11,0},
    {210, 242, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 12,0},
    {210, 264, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 13,0},
    {210, 286, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 14,0},
    {210, 308, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 15,0},
    {210, 330, 28, 22, toggle_partial_resource_state_forward, toggle_partial_resource_state_reverse, 16,0},
};
static generic_button dock_distribution_permissions_buttons[20];

static unsigned int dock_distribution_permissions_buttons_count;

static scrollbar_type scrollbar = { .on_scroll_callback = on_scroll };

static generic_button go_to_orders_button[] = {
    {0, 0, 304, 20, go_to_orders}
};
static generic_button go_to_storage_orders_button[] = {
    {0, 0, 0, 0, go_to_orders} //storage orders button is separate to be set dynamically
};
static generic_button go_to_lighthouse_action_button[] = {
    {0, 0, 400, 100, button_lighthouse_policy}
};
static generic_button go_to_caravanserai_action_button[] = {
    {0, 0, 110, 95, button_caravanserai_policy}
};
static generic_button storage_empty_all_button[] = {
    {0, 0, 0, 20, storage_empty_all},
};
static generic_button market_order_buttons[] = {
    {314, 0, 20, 20, market_orders},
};

static image_button storage_image_buttons[] = {
    {0, 0, 30, 19, IB_NORMAL, 0, 0, toggle_mantain, button_none, 0, 0, 1, "UI", "Maintain_1"},
    {0, 0, 30, 19, IB_NORMAL, 0, 0, toggle_mantain, button_none, 0, 0, 1, "UI", "Stop_Maintain_1"},
    {0, 0, 20, 20, IB_NORMAL, 0, 0, toggle_permissions_all, button_none, 1, 0, 1, "UI", "Denied_Walker_Checkmark"},
    {0, 0, 20, 20, IB_NORMAL, 0, 0, toggle_permissions_all, button_none, 0, 0, 1, "UI", "Selection_Checkmark"},
    {0, 0, 24, 24, IB_NORMAL, 0, 0, roadblock_orders, button_none, 0, 0, 1, "UI", "Roadblock_1"},
};

static image_button distribution_orders_buttons[] = {
    {0, 0, 20, 20, IB_NORMAL, 0, 0, storage_toggle_all_states, button_none, 1, 0, 1, "UI", "Denied_Walker_Checkmark"},
    {0, 0, 20, 20, IB_NORMAL, 0, 0, storage_toggle_all_states, button_none, 0, 0, 1, "UI", "Selection_Checkmark"},
}; //fix functions and positioning

static struct {
    translation_key title;
    translation_key subtitle;
    const char *base_image_name;
    option_menu_item items[4];
    const char *wav_file;
} land_trade_policy = {
    "TR_BUILDING_CARAVANSERAI_POLICY_TITLE",
    "TR_BUILDING_CARAVANSERAI_POLICY_TEXT",
    "Trade Policy",
    {
        { "TR_BUILDING_CARAVANSERAI_NO_POLICY" },
        { "TR_BUILDING_CARAVANSERAI_POLICY_1_TITLE", "TR_BUILDING_CARAVANSERAI_POLICY_1" },
        { "TR_BUILDING_CARAVANSERAI_POLICY_2_TITLE", "TR_BUILDING_CARAVANSERAI_POLICY_2" },
        { "TR_BUILDING_CARAVANSERAI_POLICY_3_TITLE", "TR_BUILDING_CARAVANSERAI_POLICY_3" }
    },
    "wavs/market4.wav"
};
static struct {
    translation_key title;
    translation_key subtitle;
    const char *base_image_name;
    option_menu_item items[4];
    const char *wav_file;
} sea_trade_policy = {
    "TR_BUILDING_LIGHTHOUSE_POLICY_TITLE",
    "TR_BUILDING_LIGHTHOUSE_POLICY_TEXT",
    "Sea Trade Policy",
    {
        { "TR_BUILDING_LIGHTHOUSE_NO_POLICY" },
        { "TR_BUILDING_LIGHTHOUSE_POLICY_1_TITLE", "TR_BUILDING_LIGHTHOUSE_POLICY_1" },
        { "TR_BUILDING_LIGHTHOUSE_POLICY_2_TITLE", "TR_BUILDING_LIGHTHOUSE_POLICY_2" },
        { "TR_BUILDING_LIGHTHOUSE_POLICY_3_TITLE", "TR_BUILDING_LIGHTHOUSE_POLICY_3" }
    },
    "wavs/dock1.wav"
};

static generic_button primary_product_producer_button_stockpiling[] = {
    {0, 0, 30, 30, button_stockpiling, 0, 0, 0}
};

static struct {
    unsigned int focus_button_id;
    unsigned int orders_focus_button_id;
    unsigned int resource_focus_button_id;
    unsigned int permission_focus_button_id;
    int building_id;
    unsigned int partial_resource_focus_button_id;
    int tooltip_id;
    unsigned int dock_max_cities_visible;
    unsigned int caravanserai_focus_button_id;
    unsigned int lighthouse_focus_button_id;
    unsigned int primary_product_stockpiling_id;
    unsigned int image_button_focus_id;
    int showing_special_orders;
    int caravanserai_button_y_offset;
    int y_permission_buttons;
    resource_list stored_resources;
} data;

uint8_t quantity_full_button_text[] = "32";

typedef enum {
    REJECT_ALL = 0,
    ACCEPT_ALL = 1,
} affect_all_button_current_state;

int get_storage_permission_image(building_storage_permission_states permission)
{
    switch (permission) {
        case BUILDING_STORAGE_PERMISSION_MARKET:
            return assets_get_image_id("Walkers", "marketbuyer_sw_01");
        case BUILDING_STORAGE_PERMISSION_TRADERS:
            return Image::group(GROUP_FIGURE_TRADE_CARAVAN) + 4;
        case BUILDING_STORAGE_PERMISSION_DOCK:
            return Image::group(GROUP_EMPIRE_TRADE_ROUTE_TYPE);
        case BUILDING_STORAGE_PERMISSION_BARKEEP:
            return assets_get_image_id("Walkers", "Barkeep SW 01");
        case BUILDING_STORAGE_PERMISSION_QUARTERMASTER:
            return assets_get_image_id("Walkers", "quartermaster_sw_01");
        case BUILDING_STORAGE_PERMISSION_WORKER:
            return assets_get_image_id("Walkers", "overseer_sw_01");
        case BUILDING_STORAGE_PERMISSION_CARAVANSERAI:
            return assets_get_image_id("Walkers", "caravanserai_overseer_sw_01");
        case BUILDING_STORAGE_PERMISSION_LIGHTHOUSE:
            return Image::group(GROUP_FIGURE_CARTPUSHER_CART) + 80;
        case BUILDING_STORAGE_PERMISSION_ARMOURY:
            return Image::group(GROUP_FIGURE_CARTPUSHER_CART) + 104;
        case BUILDING_STORAGE_PERMISSION_WORKCAMP:
            return assets_get_image_id("Walkers", "overseer_sw_01");
        case BUILDING_STORAGE_PERMISSION_NATIVES:
            return Image::group(GROUP_FIGURE_CARTPUSHER_CART) + 136;
        case BUILDING_STORAGE_PERMISSION_CAESAR:
            return assets_get_image_id("Walkers", "caesar_static_sw_02");
        default:
            return -1;
    }
}

static int affect_all_button_distribution_state(void)
{
    building *b = building_get(data.building_id);
    Building storage_building(b);
    if (distribution_accepts_nothing(storage_building)) {
        return ACCEPT_ALL;
    } else {
        return REJECT_ALL;
    }
}

static int affect_all_button_storage_state(void)
{
    int storage_id = building_get(data.building_id)->storage_id;
    if (building_storage_check_if_accepts_nothing(storage_id)) {
        return ACCEPT_ALL;
    } else {
        return REJECT_ALL;
    }
}

static void storage_permissions_for_type(const building_type_registry_impl::BuildingType &type,
    const building_storage_permission_states *&permissions, int &number_of_permissions)
{
    if (type.is_warehouse()) {
        permissions = warehouse_permissions_buttons;
        number_of_permissions = sizeof(warehouse_permissions_buttons) / sizeof(warehouse_permissions_buttons[0]);
    } else {
        permissions = granary_permissions_buttons;
        number_of_permissions = sizeof(granary_permissions_buttons) / sizeof(granary_permissions_buttons[0]);
    }
}

static void draw_accept_none_button(int x, int y, int focused, affect_all_button_current_state state)
{
    button_border_draw(x, y, 20, 20, focused ? 1 : 0);
    if (state == ACCEPT_ALL) {
        Image::from_id(assets_get_image_id("UI", "Selection_Checkmark")).draw(x + 4, y + 4);
    } else {
        Image::from_id(assets_get_image_id("UI", "Denied_Walker_Checkmark")).draw(x + 4, y + 4);
    }
}
static void toggle_permissions_all(int param1, int param2)
{
    const building_storage_permission_states *building_permissions;
    int number_of_permissions;
    int accept_all = param1;
    building *b = building_get(data.building_id);
    Building storage_building(b);
    const auto &type = storage_building.type();
    storage_permissions_for_type(type, building_permissions, number_of_permissions);

    for (int i = 0; i < number_of_permissions; i++) { //do it via loop instead of bit check due to unused permissions
        building_storage_permission_states permission = building_permissions[i];
        building_storage_set_permission(permission, storage_building, accept_all);
    }
    window_invalidate();
}

static int get_permissions_all_none_button_state(building_info_context *c)
{
    const building_storage_permission_states *building_permissions;
    int number_of_permissions;
    building *b = building_get(c->building_id);
    const Building storage_building(b);
    const auto &type = storage_building.type();
    storage_permissions_for_type(type, building_permissions, number_of_permissions);
    int rejects_all = 1; // Assume it rejects all permissions
    for (int i = 0; i < number_of_permissions; i++) { //do it via loop instead of bit check due to unused permissions
        building_storage_permission_states permission = building_permissions[i];
        if (!building_storage_get_permission(permission, storage_building)) {
            // If it accepts at least one resource, it's not rejecting all
            rejects_all = 0;
            break;
        }
    }
    return rejects_all;
}

static void draw_permissions_buttons(int x, int y, building_info_context *c)
{
    int image_offset_x, image_offset_y;
    const building_storage_permission_states *building_permissions;
    int number_of_permissions;
    active_permissions_count = 0;
    building *b = building_get(data.building_id);
    const Building storage_building(b);
    const auto &type = storage_building.type();
    storage_permissions_for_type(type, building_permissions, number_of_permissions);

    int default_margin = 16;
    x += default_margin;
    int available_width = c->width_blocks * BLOCK_SIZE - 2 * default_margin;

    // Calculate button width to fill available space
    int total_button_width = available_width - (number_of_permissions - 1); // Reserve 1px per gap minimum
    int button_width = total_button_width / number_of_permissions;

    // Apply button size constraints
    button_width = MAX(button_width, 38); // Minimum 38px
    button_width = MIN(button_width, 52); // Maximum 52px

    int raw_scaling_factor = (button_width * 100) / 52;
    raw_scaling_factor = raw_scaling_factor == 90 ? 91 : raw_scaling_factor; // Avoid 90% scaling due to visual issues
    float original_scaling_factor = raw_scaling_factor / 100.0f;

    // Calculate how much space is used by buttons
    int total_width_used_by_buttons = button_width * number_of_permissions;

    // Distribute remaining space as gaps between buttons
    int total_gap_space = available_width - total_width_used_by_buttons;
    int base_gap = total_gap_space / (number_of_permissions - 1);
    int gap_remainder = total_gap_space % (number_of_permissions - 1);

    // Track cumulative extra pixels for gap distribution
    int pixel_carry = 0;
    active_permissions_count = number_of_permissions;

    for (unsigned int i = 0; i < number_of_permissions; i++) {
        building_storage_permission_states permission = building_permissions[i];
        int is_sea_trade_route = (permission == BUILDING_STORAGE_PERMISSION_DOCK);
        int permission_state = building_storage_get_permission(permission, storage_building);

        // Calculate extra pixel for this gap
        pixel_carry += gap_remainder;
        int extra_gap_pixel = pixel_carry / (number_of_permissions - 1);
        pixel_carry %= (number_of_permissions - 1);

        // Calculate actual gap for this button (only add extra pixels between buttons, not after last)
        int actual_gap = (i < number_of_permissions - 1) ? base_gap + extra_gap_pixel : 0;

        if (!permission_state) {
            graphics_set_clip_rectangle(x, y, button_width - 2, button_width);
            inner_panel_draw(x + 2, y + 2, (button_width) / BLOCK_SIZE + 1, (button_width) / BLOCK_SIZE + 1);
            graphics_reset_clip_rectangle();
        }

        image_offset_x = (int) (is_sea_trade_route ? 12 * original_scaling_factor : 7 * original_scaling_factor);
        image_offset_y = (int) (is_sea_trade_route ? 16 * original_scaling_factor : 7 * original_scaling_factor);
        int scale = (int) (original_scaling_factor * 100);

        Image::from_id(get_storage_permission_image(permission)).draw_scaled_centered(x + image_offset_x, y + image_offset_y, COLOR_MASK_NONE, scale);

        if (!permission_state) {
            Image::from_id(assets_get_image_id("UI", "Large_Widget_Cross")).draw_scaled_centered(x + (int) (15 * original_scaling_factor), y + (int) (15 * original_scaling_factor), COLOR_MASK_NONE, scale);
        }

        button_border_draw(x, y, button_width, button_width,
            data.permission_focus_button_id == i + 1 || !permission_state);

        permission_buttons[i] = make_permission_button(x, y, button_width, permission);
        // Move to next button position (button width + gap with distributed remainder)
        x += button_width + actual_gap;
    }
}

static void init_dock_permission_buttons(void)
{
    dock_distribution_permissions_buttons_count = 0;
    for (int route_id = 0; route_id < trade_route_count(); route_id++) {
        int city_id = -1;
        if (empire_object_is_sea_trade_route(route_id) && empire_city_is_trade_route_open(route_id)) {
            city_id = empire_city_get_for_trade_route(route_id);
            if (city_id != -1) {
                generic_button button = { 0, 0, 210, 22, dock_toggle_route, 0, route_id, city_id };
                dock_distribution_permissions_buttons[dock_distribution_permissions_buttons_count] = button;
                dock_distribution_permissions_buttons_count++;
            }
        }
    }
}

static void draw_dock_permission_buttons(int x_offset, int y_offset, int dock_id)
{
    for (unsigned int i = 0; i < dock_distribution_permissions_buttons_count; i++) {
        if (i < scrollbar.scroll_position || i - scrollbar.scroll_position >= data.dock_max_cities_visible) {
            continue;
        }
        generic_button *button = &dock_distribution_permissions_buttons[i];
        int scrollbar_shown = dock_distribution_permissions_buttons_count > data.dock_max_cities_visible;
        button->x = scrollbar_shown ? 160 : 190;
        button->y = 22 * (i - scrollbar.scroll_position);
        button_border_draw(x_offset + button->x, y_offset + button->y, button->width, button->height,
            data.permission_focus_button_id == i + 1 ? 1 : 0);
        int state = building_dock_can_trade_with_route(dock_distribution_permissions_buttons[i].parameter1, dock_id);
        if (state) {
            lang_text_draw_centered("main_strings.99.7", x_offset + button->x, y_offset + button->y + 5, button->width, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
        } else {
            lang_text_draw_centered("main_strings.99.8", x_offset + button->x, y_offset + button->y + 5, button->width, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        }
        empire_city *city = empire_city_get(button->parameter2);
        const uint8_t *city_name = empire_city_get_name(city);
        int x = x_offset + 16;
        int y = y_offset + 4 + button->y;
        text_draw(city_name, x, y, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }
}

void window_building_draw_dock(building_info_context *c)
{
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = 83;

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.101.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);

    if (b->has_plague) {
        window_building_play_sound(c, "wavs/clinic.wav");
        if (b->sickness_doctor_cure == 99) {
            window_building_draw_description(c, "TR_BUILDING_FUMIGATION_DESC");
        } else {
            window_building_draw_description(c, "TR_BUILDING_DOCK_PLAGUE_DESC");
        }
    } else {
        window_building_play_sound(c, "wavs/dock.wav");
        if (!c->has_road_access) {
            window_building_draw_description(c, 69, 25);
        } else if (b->data.dock.trade_ship_id) {
            if (c->worker_percentage <= 0) {
                window_building_draw_description(c, 101, 2);
            } else if (c->worker_percentage < 50) {
                window_building_draw_description(c, 101, 3);
            } else if (c->worker_percentage < 75) {
                window_building_draw_description(c, 101, 4);
            } else {
                window_building_draw_description(c, 101, 5);
            }
        } else {
            if (c->worker_percentage <= 0) {
                window_building_draw_description(c, 101, 6);
            } else if (c->worker_percentage < 50) {
                window_building_draw_description(c, 101, 7);
            } else if (c->worker_percentage < 75) {
                window_building_draw_description(c, 101, 8);
            } else {
                window_building_draw_description(c, 101, 9);
            }
        }
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, 101, 1);
    init_dock_permission_buttons();
    text_draw_centered(translation_for_key("TR_BUILDING_DOCK_CITIES_CONFIG_DESC"), c->x_offset, c->y_offset + 215,
        BLOCK_SIZE * c->width_blocks, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    int panel_height = c->height_blocks - 23;
    data.dock_max_cities_visible = panel_height * BLOCK_SIZE / 22;
    int scrollbar_shown = dock_distribution_permissions_buttons_count > data.dock_max_cities_visible;
    int panel_width;
    if (scrollbar_shown) {
        panel_width = c->width_blocks - 5;
    } else {
        panel_width = c->width_blocks - 2;
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 240, panel_width, panel_height);
    if (data.showing_special_orders || data.building_id != c->building_id) {
        scrollbar.x = c->x_offset + (c->width_blocks - 4) * BLOCK_SIZE;
        scrollbar.y = c->y_offset + 240;
        scrollbar.height = panel_height * BLOCK_SIZE;
        scrollbar.scrollable_width = (c->width_blocks - 5) * BLOCK_SIZE;
        scrollbar.elements_in_view = data.dock_max_cities_visible;
        scrollbar_init(&scrollbar, 0, dock_distribution_permissions_buttons_count);
        data.showing_special_orders = 0;
    }
    if (!dock_distribution_permissions_buttons_count) {
        text_draw_centered(translation_for_key("TR_BUILDING_DOCK_CITIES_NO_ROUTES"), c->x_offset + 16,
            c->y_offset + 240 + panel_height * BLOCK_SIZE / 2 - 7, panel_width * BLOCK_SIZE, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
}

void window_building_draw_dock_foreground(building_info_context *c)
{
    button_border_draw(c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        BLOCK_SIZE * (c->width_blocks - 10), 20, data.focus_button_id == 1 ? 1 : 0);
    lang_text_draw_centered("main_strings.98.5", c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 30, BLOCK_SIZE * (c->width_blocks - 10), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    draw_dock_permission_buttons(c->x_offset + 16, c->y_offset + 245, c->building_id);
    scrollbar_draw(&scrollbar);
}

int window_building_handle_mouse_dock(const mouse *m, building_info_context *c)
{
    data.building_id = c->building_id;
    data.permission_focus_button_id = 0;
    data.focus_button_id = 0;
    GenericButtonList permission_buttons(
        dock_distribution_permissions_buttons,
        dock_distribution_permissions_buttons_count);
    return scrollbar_handle_mouse(&scrollbar, m, 1) ||
        permission_buttons.handle_mouse(
            *m,
            c->x_offset + 16,
            c->y_offset + 240 + 5,
            &data.permission_focus_button_id
        ) ||
        GenericButtonList(go_to_orders_button, 1).handle_mouse(
            *m,
            c->x_offset + 80,
            c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
            &data.focus_button_id
        );
}

static int count_food_types_in_stock(building *b)
{
    int count = 0;
    const resource_list *list = city_resource_get_potential_foods();

    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];
        if (resource_is_food(r) && b->resources[r] > 0) {
            count++;
        }
    }
    return count;
}

static void draw_caravanserai_food_per_month(building_info_context *c, int y_offset)
{
    int x_offset = c->x_offset + 32;

    y_offset = c->y_offset + y_offset;
    font_t font = FONT_NORMAL_BLACK;
    x_offset += lang_text_draw("TR_BUILDING_INFO_CARAVANSERAI_MONTHLY_CONSUMPTION", x_offset, y_offset, font, screen_ui_to_pixel(font_definition_for(font)->line_height));
    int food_required_monthly = building_caravanserai_food_required_monthly();
    x_offset += text_draw_number(food_required_monthly, '\0', "\0", x_offset, y_offset, font, screen_ui_to_pixel(font_definition_for(font)->line_height), COLOR_MASK_NONE);
    x_offset += lang_text_draw("TR_EDITOR_UNITS", x_offset, y_offset, font, screen_ui_to_pixel(font_definition_for(font)->line_height));
}

static void draw_food_stocks(building_info_context *c, building *b, int y_offset)
{
    int x_offset = 32;
    const resource_list *list = city_resource_get_potential_foods();
    int food_type_index = 0;
    const Building building(b);

    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];
        if (!resource_is_food(r) || b->resources[r] <= 0) {
            continue;
        }
        if (food_type_index > 0 && (food_type_index % 5) == 0) {
            y_offset += BLOCK_SIZE * 2;
            x_offset = 32;
        }
        font_t font = building.accepts_good(r) ? FONT_NORMAL_BLACK : FONT_NORMAL_RED;
        draw_resource_icon_centered(r, c->x_offset + x_offset, c->y_offset + y_offset);
        text_draw_number(b->resources[r], '@', " ",
            c->x_offset + x_offset + 25, c->y_offset + y_offset + 7, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
        x_offset += 85;
        food_type_index++;
    }
}

static void draw_good_stocks(building_info_context *c, building *b, int y_offset)
{
    int x_offset = 32;
    const Building building(b);
    for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
        resource_type r = resource_from_int(resource_id);
        if (!resource_is_inventory_good(r)) {
            continue;
        }
        font_t font = building.accepts_good(r) ? FONT_NORMAL_BLACK : FONT_NORMAL_RED;
        draw_resource_icon_centered(r, c->x_offset + x_offset, c->y_offset + y_offset);
        text_draw_number(b->resources[r], '@', " ",
            c->x_offset + x_offset + 25, c->y_offset + y_offset + 7, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
        x_offset += 85;
    }
}

void window_building_draw_market(building_info_context *c)
{
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = 2;
    data.showing_special_orders = 0;

    int food_types = 0;
    int y_offset = 0;
    building *b = building_get(c->building_id);

    if (c->has_road_access && b->num_workers > 0) {
        food_types = count_food_types_in_stock(b);
        //y_offset = ((food_types - 1) / 4) * BLOCK_SIZE * 2;
        //c->height_blocks = 22;
    }

    window_building_play_sound(c, "wavs/market.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.97.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, 97, 2);
    } else {
        if (food_types > 0) {
            draw_food_stocks(c, b, 50);
        } else {
            window_building_draw_description_at(c, 50, 97, 4);
        }
        draw_good_stocks(c, b, 94 + y_offset);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136 + y_offset, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140 + y_offset);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144 + y_offset);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 106, 97, 1);
}

void window_building_distributor_draw_foreground(building_info_context *c)
{
    button_border_draw(c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        BLOCK_SIZE * (c->width_blocks - 10), 20, data.focus_button_id == 1 ? 1 : 0);
    lang_text_draw_centered("main_strings.98.5", c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 30, BLOCK_SIZE * (c->width_blocks - 10), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

static void set_distributed_resources(const Building &building)
{
    for (unsigned int i = 0; i < data.stored_resources.size; i++) {
        data.stored_resources.items[i] = RESOURCE_NONE;
    }
    data.stored_resources.size = 0;
    const building_type_registry_impl::Distribution *distribution = distribution_for(building);
    if (!distribution) {
        return;
    }
    const resource_list *list = city_resource_get_potential();
    for (unsigned int i = 0; i < list->size; i++) {
        if (distribution->handles_resource(list->items[i])) {
            data.stored_resources.items[data.stored_resources.size++] = list->items[i];
        }
    }
}

void window_building_draw_distributor_orders(building_info_context *c, const uint8_t *title)
{
    Building building(building_get(c->building_id));
    const auto *definition = building.type_definition();
    c->help_id = definition && std::strcmp(definition->attr(), "dock") == 0 ? 83 : 3;
    int y_offset = window_building_get_vertical_offset(c, 28);
    outer_panel_draw(c->x_offset, y_offset, 29, 28);
    text_draw_centered(title, c->x_offset, y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);

    if (!data.showing_special_orders || data.building_id != c->building_id) {
        set_distributed_resources(building);

        scrollbar.x = c->x_offset + (c->width_blocks - 3) * BLOCK_SIZE;
        scrollbar.y = y_offset + 42;
        scrollbar.height = 21 * BLOCK_SIZE;
        scrollbar.scrollable_width = (c->width_blocks - 2) * BLOCK_SIZE;
        scrollbar.elements_in_view = 21 * BLOCK_SIZE / 22;
        scrollbar_init(&scrollbar, 0, data.stored_resources.size);

        data.showing_special_orders = 1;
    }

    int scrollbar_shown = scrollbar.max_scroll_position > 0;

    inner_panel_draw(c->x_offset + 16, y_offset + 42, c->width_blocks - (scrollbar_shown ? 4 : 2), 21);
}

void window_building_draw_distributor_orders_foreground(building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);
    //TODO: get buttons into the global and stop this monkey business with static declarations and hardcoded values
    affect_all_button_current_state button_state =
        static_cast<affect_all_button_current_state>(affect_all_button_distribution_state());

    draw_accept_none_button(c->x_offset + 394, y_offset + 404, data.orders_focus_button_id == 1, button_state);
    building *b = building_get(c->building_id);
    const Building storage_building(b);
    const auto &type = storage_building.type();
    int lang_group = 0;
    int lang_active_id = 0;
    int lang_inactive_id = 0;
    translation_key active_key;
    translation_key inactive_key;
    if (type.has_market()) {
        active_key = "TR_MARKET_TRADING";
        inactive_key = "TR_MARKET_NOT_TRADING";
    } else if (type.is_venus_temple() || type.is_ceres_temple()) {
        active_key = "TR_TEMPLE_DISTRIBUTING";
        inactive_key = "TR_TEMPLE_NOT_DISTRIBUTING";
    } else if (is_tavern_distribution(type)) {
        active_key = "TR_TAVERN_FETCHING";
        inactive_key = "TR_TAVERN_NOT_FETCHING";
    } else if (type.is_caravanserai() || type.is_mess_hall()) {
        lang_group = 99;
        lang_active_id = 10;
        lang_inactive_id = 8;
    } else {
        lang_group = 99;
        lang_active_id = 7;
        lang_inactive_id = 8;
    }

    scrollbar_draw(&scrollbar);

    int scrollbar_shown = scrollbar.max_scroll_position > 0;

    for (unsigned int i = 0; i < scrollbar.elements_in_view && i < data.stored_resources.size; i++) {
        resource_type resource = data.stored_resources.items[i + scrollbar.scroll_position];

        draw_resource_icon_centered(resource, c->x_offset + 32, y_offset + 43 + 22 * i);
        if (!scrollbar_shown) {
            draw_resource_icon_centered(resource, c->x_offset + 408, y_offset + 43 + 22 * i);
        }
        text_draw(resource_get_data(resource)->text, c->x_offset + 72, y_offset + 50 + 22 * i,
            FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), COLOR_MASK_NONE);
        button_border_draw(c->x_offset + 180, y_offset + 46 + 22 * i, 210, 22,
            data.resource_focus_button_id == i + 1);
        if (storage_building.accepts_good(resource)) {
            if (active_key) {
                lang_text_draw_centered(active_key,
                    c->x_offset + 180, y_offset + 51 + 22 * i, 210, FONT_NORMAL_WHITE,
                    screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            } else {
                lang_text_draw_centered(current_string_key(lang_group, lang_active_id), c->x_offset + 180, y_offset + 51 + 22 * i, 210, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            }
        } else {
            if (inactive_key) {
                lang_text_draw_centered(inactive_key,
                    c->x_offset + 180, y_offset + 51 + 22 * i, 210, FONT_NORMAL_RED,
                    screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
            } else {
                lang_text_draw_centered(current_string_key(lang_group, lang_inactive_id), c->x_offset + 180, y_offset + 51 + 22 * i, 210, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
            }
        }
    }
}

int window_building_handle_mouse_distributor(const mouse *m, building_info_context *c)
{
    return GenericButtonList(go_to_orders_button, 1).handle_mouse(
        *m,
        c->x_offset + 80,
        c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        &data.focus_button_id
    );
}

int window_building_handle_mouse_distributor_orders(const mouse *m, building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);

    data.building_id = c->building_id;

    int buttons_to_show = data.stored_resources.size < scrollbar.elements_in_view ?
        data.stored_resources.size : scrollbar.elements_in_view;

    return scrollbar_handle_mouse(&scrollbar, m, 1) ||
        GenericButtonList(orders_resource_buttons, buttons_to_show).handle_mouse(
            *m,
            c->x_offset + 180,
            y_offset + 46,
            &data.resource_focus_button_id
        ) ||
        GenericButtonList(market_order_buttons, 1).handle_mouse(
            *m,
            c->x_offset + 80,
            y_offset + 404,
            &data.orders_focus_button_id
        );
}

void window_building_get_tooltip_distribution_orders(int *group_id, int *text_id, translation_key *translation)
{
    if (data.orders_focus_button_id == 1) {
        if (affect_all_button_distribution_state() == ACCEPT_ALL) {
            *translation = "TR_TOOLTIP_BUTTON_STORAGE_ORDER_ACCEPT_ALL";
        } else {
            *translation = "TR_TOOLTIP_BUTTON_STORAGE_ORDER_REJECT_ALL";
        }
    }
}
int window_building_handle_mouse_primary_product_producer(const mouse *m, building_info_context *c)
{
    data.building_id = c->building_id;
    return GenericButtonList(primary_product_producer_button_stockpiling, 1).handle_mouse(
        *m,
        c->x_offset + BLOCK_SIZE * c->width_blocks - 40,
        c->y_offset + 10,
        &data.primary_product_stockpiling_id
    );
}

void window_building_draw_primary_product_stockpiling(building_info_context *c)
{
    int x = c->x_offset + primary_product_producer_button_stockpiling->x + BLOCK_SIZE * c->width_blocks - 40;
    int y = c->y_offset + primary_product_producer_button_stockpiling->y + 10;
    button_border_draw(x, y, 30, 30, data.primary_product_stockpiling_id);
        Image::from_id(assets_get_image_id("UI", "Stockpile_Sprite")).draw(x + 7, y + 6, building_stockpiling_enabled(building_get(c->building_id)) ? 0xfff5a46b : COLOR_MASK_NONE);
}

static void draw_button_from_state(resource_storage_entry entry, int x, int y, resource_type resource)
{
    // Draw storage state label or icon
    char text[4];
    int image_width, text_width, start_x;
    snprintf(text, sizeof(text), "%d", BUILDING_STORAGE_QUANTITY_MAX);
    switch (entry.state) {
        case BUILDING_STORAGE_STATE_GETTING:
        {
            image_width = image_get(Image::group(GROUP_CONTEXT_ICONS) + 12)->width + 10;
            int group_number = resource_is_food(resource) ? 10 : 9;  // 10 = "getting food", 9 = "getting goods"
            text_width = lang_text_get_width(current_string_key(99, group_number), FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            start_x = x + (210 - image_width - text_width) / 2;
            Image::from_id(Image::group(GROUP_CONTEXT_ICONS) + 12).draw(start_x, y - 2);
            lang_text_draw(current_string_key(99, group_number), start_x + image_width, y, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            snprintf(text, sizeof(text), "%d", entry.quantity);
            break;
        }
        case BUILDING_STORAGE_STATE_NOT_ACCEPTING:
            lang_text_draw_centered("main_strings.99.8", x, y, 210, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height)); // lang ID 8 = "Not accepting"
            break;
        case BUILDING_STORAGE_STATE_MAINTAINING:
        {
            int maintain_goods_icon_id = assets_get_image_id("UI", "Maintain_Goods_Icon_2");
            image_width = image_get(maintain_goods_icon_id)->width + 10;
            text_width = lang_text_get_width("TR_WINDOW_BUILDING_DISTRIBUTION_MAINTAINING",
                 FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            start_x = x + (210 - image_width - text_width) / 2;
            Image::from_id(maintain_goods_icon_id).draw(start_x, y - 2, COLOR_MAINTAIN_ICON, SCALE_NONE);
            // this icon needs scaling to be similar to getting goods - draw at 80% scale
            lang_text_draw("TR_WINDOW_BUILDING_DISTRIBUTION_MAINTAINING",
                start_x + image_width, y, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
            snprintf(text, sizeof(text), "%d", entry.quantity);
            break;
        }
        default: // ACCEPTING
            lang_text_draw_centered("main_strings.99.7", x, y, 210, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height)); // lang ID 7 = "Accepting"
            snprintf(text, sizeof(text), "%d", entry.quantity);
            break;
    }

    // Determined capacity text
    text_draw_centered((uint8_t *) text, x + 214, y, 20, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

}

static void draw_resource_orders_buttons(int x, int y, const resource_list *list, const building_storage *storage)
{
    int scrollbar_shown = scrollbar.max_scroll_position > 0;

    for (unsigned int i = 0; i < scrollbar.elements_in_view && i < list->size - scrollbar.scroll_position; i++) {
        resource_type resource = list->items[i + scrollbar.scroll_position];

        int y_offset = y + 22 * i;
        draw_resource_icon_centered(resource, x, y_offset - 2);
        if (!scrollbar_shown) {
            draw_resource_icon_centered(resource, x + 390, y_offset - 2);
            text_draw(resource_get_data(resource)->text, x + 30, y_offset + 4, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), COLOR_MASK_NONE);
            button_border_draw(x + 148, y_offset, 210, 22, data.resource_focus_button_id == i + 1);
            button_border_draw(x + 358, y_offset, 28, 22, data.partial_resource_focus_button_id == i + 1);

            draw_button_from_state(storage->resource_state[resource], x + 148, y_offset + 5, resource);
        }
        if (scrollbar_shown) {
            draw_resource_icon_centered(resource, x + 360, y_offset - 2);
            text_draw(resource_get_data(resource)->text, x + 30, y_offset + 4, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), COLOR_MASK_NONE);
            button_border_draw(x + 118, y_offset, 210, 22, data.resource_focus_button_id == i + 1);
            button_border_draw(x + 328, y_offset, 28, 22, data.partial_resource_focus_button_id == i + 1);

            draw_button_from_state(storage->resource_state[resource], x + 118, y_offset + 5, resource);
        }

    }
}

void window_building_get_tooltip_storage_orders(int *group_id, int *text_id, translation_key *translation)
{
    if (distribution_orders_buttons[0].focused || distribution_orders_buttons[1].focused) {
        if (affect_all_button_storage_state() == ACCEPT_ALL) {
            *translation = "TR_TOOLTIP_BUTTON_STORAGE_ORDER_ACCEPT_ALL";
        } else {
            *translation = "TR_TOOLTIP_BUTTON_STORAGE_ORDER_REJECT_ALL";
        }
    } else if (data.resource_focus_button_id || data.partial_resource_focus_button_id) {
        int building_id = data.building_id;
        building *b = building_get(building_id);
        const building_storage *s = building_storage_get(b->storage_id);
        // Convert 1-based focus ID to 0-based and apply scroll offset
        unsigned int index = data.resource_focus_button_id ?
            (data.resource_focus_button_id - 1 + scrollbar.scroll_position) :
            (data.partial_resource_focus_button_id - 1 + scrollbar.scroll_position);
        // Choose the correct resource list based on building type
        const Building building(b);
        const resource_list *list = stored_resources_for_type(building.type());

        // Ensure valid index
        if (index < list->size) {
            resource_type resource = list->items[index];
            const resource_storage_entry *entry = &s->resource_state[resource];

            if (data.partial_resource_focus_button_id) { //quantity button tooltip
                if (entry->state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
                    *translation = "TR_TOOLTIP_RIGHT_CLICK_TO_DECREASE";
                }
            } else if (data.resource_focus_button_id) {
                if (entry->state == BUILDING_STORAGE_STATE_MAINTAINING) {
                    *translation = "TR_TOOLTIP_BUILDING_DISTRIBUTION_MAINTAINING";
                }
            }
        }
    }
}

const uint8_t *window_building_dock_get_tooltip(building_info_context *c)
{
    int x_offset = c->x_offset + 16;
    int y_offset = c->y_offset + 240;
    const building *dock = building_get(c->building_id);
    Building dock_object(const_cast<building *>(dock));
    const auto *definition = dock_object.type_definition();
    if (!definition || std::strcmp(definition->attr(), "dock") != 0) {
        return 0;
    }
    int width = dock_distribution_permissions_buttons_count > data.dock_max_cities_visible ? 140 : 170;
    int height = 20;
    const mouse *m = mouse_get();

    for (unsigned int i = 0; i < dock_distribution_permissions_buttons_count; i++) {
        if (i < scrollbar.scroll_position || i - scrollbar.scroll_position >= data.dock_max_cities_visible) {
            continue;
        }
        int y_pos = y_offset + 22 * (i - scrollbar.scroll_position);
        if (m->x < x_offset || m->y < y_pos || m->x > x_offset + width || m->y > y_pos + height) {
            continue;
        }
        empire_city *city = empire_city_get(dock_distribution_permissions_buttons[i].parameter2);
        if (!city) {
            return 0;
        }
        static uint8_t text[400];
        uint8_t *cursor = text;
        cursor = string_copy(lang_get_string("main_strings.47.5"), cursor, 400 - (int) (cursor - text));
        cursor = string_copy(string_from_ascii(": "), cursor, 400 - (int) (cursor - text));
        int traded = 0;
        for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
            resource_type resource = resource_from_int(resource_id);
            if (!city->sells_resource[resource]) {
                continue;
            }
            if (traded > 0) {
                cursor = string_copy(string_from_ascii(", "), cursor, 400 - (int) (cursor - text));
            }
            traded++;
            cursor = string_copy(resource_get_data(resource)->text, cursor, 400 - (int) (cursor - text));
        }
        if (traded == 0) {
            cursor = string_copy(lang_get_string("main_strings.23.0"), cursor, 400 - (int) (cursor - text));
        }
        cursor = string_copy(string_from_ascii("\n"), cursor, 400 - (int) (cursor - text));
        cursor = string_copy(lang_get_string("main_strings.47.4"), cursor, 400 - (int) (cursor - text));
        cursor = string_copy(string_from_ascii(": "), cursor, 400 - (int) (cursor - text));
        traded = 0;
        for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
            resource_type resource = resource_from_int(resource_id);
            if (!city->buys_resource[resource]) {
                continue;
            }
            if (traded > 0) {
                cursor = string_copy(string_from_ascii(", "), cursor, 400 - (int) (cursor - text));
            }
            traded++;
            cursor = string_copy(resource_get_data(resource)->text, cursor, 400 - (int) (cursor - text));
        }
        if (traded == 0) {
            cursor = string_copy(lang_get_string("main_strings.23.0"), cursor, 400 - (int) (cursor - text));
        }
        return text;
    }
    return 0;
}

// ====================================================================================================================
// =====================================================STORAGE DRAWING================================================
// ====================================================================================================================
void window_building_draw_storage(building_info_context *c)
{
    building *b = building_get(c->building_id);
    Building storage_building(b);
    const auto &type = storage_building.type();
    building_warehouse_recount_resources(storage_building);
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = type.is_granary() ? 3 : 4;
    data.building_id = c->building_id;
    data.showing_special_orders = 0;
    int y = c->y_offset + 25;
    // height adjustment shouldn't be handled at this level, so was moved to building_info.c
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_label_and_number_centered(lang_get_building_type_string(b->type), b->storage_id, "",
        c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);

    const char *sound = b->has_plague ? "wavs/clinic.wav"
        : type.is_granary() ? "wavs/granary.wav"
        : "wavs/warehouse.wav";
    window_building_play_sound(c, sound);
    if (b->has_plague) {
        if (b->sickness_doctor_cure == 99) {
            window_building_draw_description(c, "TR_BUILDING_FUMIGATION_DESC");
        } else {
            window_building_draw_description(c,
                is_granary(storage_building) ? "TR_BUILDING_GRANARY_PLAGUE_DESC" : "TR_BUILDING_WAREHOUSE_PLAGUE_DESC");
        }
    } else if (!c->has_road_access) {
        y += 4 + window_building_draw_description_at(c, 56, 69, 25);
    } else if (type.is_granary() && scenario_property_rome_supplies_wheat()) {
        y += window_building_draw_description_at(c, 56, 98, 4);
    } else {
        int total_stored = 0;
        int x;
        int offset = 0;
        int stored_types = building_storage_count_stored_resource_types(b->id);
        if (!stored_types) {
            translation_key msg = type.is_granary() ? "TR_BUILDING_GRANARY_NO_FOOD" : "TR_BUILDING_WAREHOUSE_NO_GOODS";
            lang_text_draw_centered(msg, c->x_offset, c->y_offset + 56,
                BLOCK_SIZE * c->width_blocks, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
            y += 32;
        } else {
            for (int resource_id = RESOURCE_NONE + 1; resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
                resource_type r = resource_from_int(resource_id);
                int amount = b->resources[r];
                if (amount <= 0) {
                    continue;
                }
                if (offset & 1) {
                    x = c->x_offset + 240;
                } else {
                    x = c->x_offset + 32;
                    y += BLOCK_SIZE + 13;
                }
                offset++;
                total_stored += amount;

                draw_resource_icon_centered(r, x, y + 5);

                int width = text_draw_number(amount, '@', " ", x + 32, y + 12, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), COLOR_MASK_NONE);
                text_draw(resource_get_data(r)->text, x + 32 + width, y + 12, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), COLOR_MASK_NONE);
            }

            int width = lang_text_draw("main_strings.98.2", c->x_offset + 16, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
            lang_text_draw_amount(total_stored == 1 ? "TR_BUILDING_INFO_CARTLOAD" : "TR_BUILDING_INFO_CARTLOADS", total_stored,
                c->x_offset + 16 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

            width = lang_text_draw("main_strings.98.3", c->x_offset + 220, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
            int max = type.is_granary() ? b->resources[RESOURCE_NONE] : 32 - total_stored;
            lang_text_draw_amount(max == 1 ? "TR_BUILDING_INFO_CARTLOAD" : "TR_BUILDING_INFO_CARTLOADS", max,
                c->x_offset + 220 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        }
    }
    y += 26; // TODO: simplify these values later, hardcoding and multiple vars are not necessary
    int y_offset = y - c->y_offset;

    inner_panel_draw(c->x_offset + 16, c->y_offset + y_offset + 8, c->width_blocks - 2, 6);
    data.y_permission_buttons = c->y_offset + y_offset + 8 + 6 * BLOCK_SIZE + 10;
    window_building_draw_employment(c, y_offset + 12);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 16 + y_offset);
    lang_text_draw_multiline(current_string_key(storage_strings_for_building(storage_building), 1), c->x_offset + 32, c->y_offset + y_offset + 180, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    // cartpusher state
    figure *f = figure_get(b->figure_id);
    if (b->figure_id && f && f->state == FIGURE_STATE_ALIVE) {
        resource_type resource = resource_from_int(f->resource_id);
        resource_graphics(resource != RESOURCE_NONE ? resource : resource_from_int(f->collecting_item_id)).panel_icon().draw(c->x_offset + 32, c->y_offset + y_offset + 60);

        if (resource != RESOURCE_NONE) {
            if (f->action_state == FIGURE_ACTION_51_WAREHOUSEMAN_DELIVERING_RESOURCE) {
                lang_text_draw_multiline(current_string_key(storage_strings_for_building(storage_building), type.is_granary() ? 9 : 16), c->x_offset + 64, c->y_offset + y_offset + 63, BLOCK_SIZE * (c->width_blocks - 5), FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            } else if (f->loads_sold_or_carrying) {
                text_draw_multiline(translation_for_key("TR_WINDOW_BUILDING_DISTRIBUTION_CART_PUSHER_RETURNING_WITH"),
                    c->x_offset + 64, c->y_offset + y_offset + 63,
                    BLOCK_SIZE * (c->width_blocks - 5), 0, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            } else {
                lang_text_draw_multiline("main_strings.99.17", c->x_offset + 64, c->y_offset + y_offset + 63, BLOCK_SIZE * (c->width_blocks - 5), FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            }
        } else {
            text_draw_multiline(
                translation_for(type.is_granary()
                    ? "TR_WINDOW_BUILDING_DISTRIBUTION_GRANARY_CART_PUSHER_GETTING"
                    : "TR_WINDOW_BUILDING_DISTRIBUTION_CART_PUSHER_GETTING"),
                c->x_offset + 64, c->y_offset + y_offset + 63,
                BLOCK_SIZE * (c->width_blocks - 5), 0, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        }
    } else if (b->num_workers) {
        lang_text_draw_multiline("main_strings.99.15", c->x_offset + 32, c->y_offset + y_offset + 63, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
}

static void storage_buttons_init(building_info_context *c)
{
    if (c->show_special_orders == SPECIAL_ORDERS_GENERIC || c->show_special_orders == SPECIAL_ORDERS_STORAGE) {
        //could also be done with data.showing_special_orders, but this is more explicit
        int y_offset_special_base = window_building_get_vertical_offset(c, 28) - c->y_offset;
        // at least for now, the storage orders is 28 blocks high
        int y_offset_special = y_offset_special_base + 28 * BLOCK_SIZE - 20 - 18; // 20px height + 18px from bottom edge
        storage_empty_all_button->y = c->y_offset + y_offset_special; //empty storage
        storage_empty_all_button->width = c->width_blocks * BLOCK_SIZE / 2 + 2 * BLOCK_SIZE; // 50% width + padding
        storage_empty_all_button->x = c->x_offset + c->width_blocks * BLOCK_SIZE / 4 - BLOCK_SIZE; // 25% start, 50% width
        for (int i = 0; i < 2; i++) {
            distribution_orders_buttons[i].y_offset = y_offset_special + 4;//accept all none
            distribution_orders_buttons[i].x_offset = c->width_blocks * BLOCK_SIZE - 12 - 73 + 4; // 73px from right edge
            distribution_orders_buttons[i].dont_draw = 0;
            distribution_orders_buttons[i].static_image = 1;
        }
        return;
    }
    go_to_storage_orders_button->width = c->width_blocks * BLOCK_SIZE / 2 + 2 * BLOCK_SIZE; // 50% width + padding
    go_to_storage_orders_button->height = 20; // 20px height
    go_to_storage_orders_button->x = c->x_offset + c->width_blocks * BLOCK_SIZE / 4 - BLOCK_SIZE; // 25% start, 50% width
    go_to_storage_orders_button->y =
        c->y_offset + c->height_blocks * BLOCK_SIZE - go_to_storage_orders_button->height - 14; // 14px from bottom edge

    //generic button is not handled in the array, therefore needs precise coordinates, not relative to c->xy_offset
    for (int i = 0; i < 5; i++) {
        storage_image_buttons[i].dont_draw = 0;
    }
    for (int i = 0; i < 2; i++) { // hold deliveries buttons
        storage_image_buttons[i].x_offset = c->width_blocks * BLOCK_SIZE - storage_image_buttons[0].width - 12;
        storage_image_buttons[i].y_offset = 10; //10px frop top edge, 12px from right edge
    }
    for (int i = 2; i < 4; i++) { //permissions all and none buttons
        storage_image_buttons[i].static_image = 1; // dont handle focus or press natively
        storage_image_buttons[i].x_offset = c->width_blocks * BLOCK_SIZE - 12 - 77; // 77px from right edge
        storage_image_buttons[i].y_offset = c->height_blocks * BLOCK_SIZE - 12 - 21; // 21px from bottom edge
    }
    //roadblock orders button
    storage_image_buttons[4].x_offset = 62; // 62px from left edge
    storage_image_buttons[4].y_offset = c->height_blocks * BLOCK_SIZE - SMALL_ICON_SIDE - 13; // 13px from bottom edge
}

void window_building_draw_storage_foreground(building_info_context *c)
{
    Building storage_building = Building(building_get(c->building_id));
    storage_buttons_init(c);
    draw_permissions_buttons(c->x_offset, data.y_permission_buttons, c);
    // Special orders button
    generic_button *button = go_to_storage_orders_button;
    button_border_draw(button->x, button->y, button->width, button->height, data.focus_button_id == 1 ? 1 : 0);
    lang_text_draw_centered("main_strings.99.2", button->x, button->y + 5, button->width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    int rejects_all = get_permissions_all_none_button_state(c);
    storage_image_buttons[2 + rejects_all].dont_draw = 1; // hide the irrelevant button

    if (is_warehouse(storage_building)) {
        if (building_storage_get_permission(BUILDING_STORAGE_PERMISSION_WORKER, storage_building)) {
            storage_image_buttons[0].dont_draw = 1;
        } else {
            storage_image_buttons[1].dont_draw = 1;
        }
    } else {
        storage_image_buttons[0].dont_draw = 1;
        storage_image_buttons[1].dont_draw = 1;
    }
    int x_border = c->x_offset + storage_image_buttons[2].x_offset;
    int y_border = c->y_offset + storage_image_buttons[2].y_offset;
    int index = data.image_button_focus_id - 1; // convert to 0-based index
    int focused = index >= 2 && index <= 3;
    button_border_draw(x_border, y_border, 20, 20, focused);
    for (int i = 0; i < 5; i++) {
        if (storage_image_buttons[i].dont_draw) {
            continue;
        }
        int offset = (i == 2 || i == 3) ? 4 : 0;
        image_buttons_draw(c->x_offset + offset, c->y_offset + offset, &storage_image_buttons[i], 1);
    }
}

void window_building_draw_storage_orders(building_info_context *c)
{
    Building building = Building(building_get(c->building_id));
    int group_id = storage_strings_for_building(building);
    int storage_id = building_get(c->building_id)->storage_id;
    lang_fragment instructions_header[] = {
        { LANG_FRAG_LABEL, group_id, 0, 0, 0, nullptr },
        { LANG_FRAG_NUMBER, 0, 0, storage_id, 0, nullptr },
        { LANG_FRAG_LABEL, 0, 0, 0, 0, nullptr, "TR_BUILDING_INFO_INSTRUCTIONS" },
    };
    int y_offset = window_building_get_vertical_offset(c, 28);
    c->help_id = storage_strings_for_building(building) == 98 ? 3 : 4;
    outer_panel_draw(c->x_offset, y_offset, 29, 28);
    lang_text_draw_sequence_centered(instructions_header, 3, c->x_offset, y_offset + 10,
         BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), COLOR_MASK_NONE);
    if (!data.showing_special_orders || data.building_id != c->building_id) {
        const resource_list *list = stored_resources_for_type(building.type());

        scrollbar.x = c->x_offset + (c->width_blocks - 3) * BLOCK_SIZE;
        scrollbar.y = y_offset + 42;
        scrollbar.height = 21 * BLOCK_SIZE;
        scrollbar.scrollable_width = (c->width_blocks - 2) * BLOCK_SIZE;
        scrollbar.elements_in_view = 21 * BLOCK_SIZE / 22;
        scrollbar_init(&scrollbar, 0, list->size);

        data.showing_special_orders = 1;
    }

    int scrollbar_shown = scrollbar.max_scroll_position > 0;
    inner_panel_draw(c->x_offset + 16, y_offset + 42,
        c->width_blocks - (scrollbar_shown ? 4 : 2), 21);
}

void window_building_draw_storage_orders_foreground(building_info_context *c)
{
    Building building = Building(building_get(c->building_id));
    int y_offset = window_building_get_vertical_offset(c, 28);
    const building_storage *storage = building_storage_get(building_get(c->building_id)->storage_id);

    // Empty-all button
    int label_id = storage_strings_for_building(building);
    storage_buttons_init(c); // context informs initialisation of the special orders buttons, not regular ones

    button_border_draw(storage_empty_all_button->x, storage_empty_all_button->y, storage_empty_all_button->width,
        storage_empty_all_button->height, data.orders_focus_button_id == 1 ? 1 : 0);

    if (storage->empty_all) {
        lang_text_draw_centered(current_string_key(label_id, is_granary(building) ? 8 : 5), //button text
            storage_empty_all_button->x, storage_empty_all_button->y + 5, storage_empty_all_button->width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

        if (is_granary(building)) {
            lang_text_draw_centered("main_strings.98.9", c->x_offset, storage_empty_all_button->y - 28, // 'trying to send elsewhere'
                BLOCK_SIZE * c->width_blocks, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        } else {
            lang_text_draw_centered("main_strings.99.6", c->x_offset, storage_empty_all_button->y - 28, BLOCK_SIZE * c->width_blocks, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));
        }
    } else {
        lang_text_draw_centered(current_string_key(label_id, is_granary(building) ? 7 : 4), storage_empty_all_button->x, storage_empty_all_button->y + 5, storage_empty_all_button->width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }

    // Accept-none button
    int button_state = affect_all_button_storage_state();
    image_button *btn = &distribution_orders_buttons[button_state];
    distribution_orders_buttons[!button_state].dont_draw = 1; // hide the irrelevant button
    int focused = distribution_orders_buttons[!button_state].focused || distribution_orders_buttons[button_state].focused;
    button_border_draw(c->x_offset + btn->x_offset - 4, c->y_offset + btn->y_offset - 4, btn->width, btn->height, focused);
    image_buttons_draw(c->x_offset, c->y_offset, distribution_orders_buttons, 2);

    scrollbar_draw(&scrollbar);

    const resource_list *list = stored_resources_for_type(building.type());

    draw_resource_orders_buttons(
        c->x_offset + 24, y_offset + 46, list, storage
    );
}

int window_building_handle_mouse_storage(const mouse *m, building_info_context *c)
{
    for (int i = 0; i < 5; i++) {
        storage_image_buttons[i].focused = 0;
    }
    data.image_button_focus_id = 0;
    data.building_id = c->building_id;

    int result = 0;
    result += GenericButtonList(permission_buttons, active_permissions_count).handle_mouse(
        *m,
        0,
        0,
        &data.permission_focus_button_id
    );
    result += image_buttons_handle_mouse(m, c->x_offset, c->y_offset,
        storage_image_buttons, 5, &data.image_button_focus_id);
    if (data.image_button_focus_id) {
        int index = data.image_button_focus_id - 1; //convert to 0-based index
        storage_image_buttons[index].focused = 1;
    }

    result += GenericButtonList(go_to_storage_orders_button, 1).handle_mouse(
        *m,
        0,
        0,
        &data.focus_button_id
    );
    return result > 0 ? 1 : 0;
}

static translation_key storage_permission_tooltip(building_storage_permission_states permission, int reject_action)
{
    static const translation_key tooltips[][2] = {
        { "TR_TOOLTIP_BUTTON_ACCEPT_MARKET_LADIES", "TR_TOOLTIP_BUTTON_REJECT_MARKET_LADIES" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_TRADE_CARAVAN", "TR_TOOLTIP_BUTTON_REJECT_TRADE_CARAVAN" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_TRADE_SHIPS", "TR_TOOLTIP_BUTTON_REJECT_TRADE_SHIPS" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_QUARTERMASTER", "TR_TOOLTIP_BUTTON_REJECT_QUARTERMASTER" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_WORKERS", "TR_TOOLTIP_BUTTON_REJECT_WORKERS" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_BARKEEP", "TR_TOOLTIP_BUTTON_REJECT_BARKEEP" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_CARAVANSERAI", "TR_TOOLTIP_BUTTON_REJECT_CARAVANSERAI" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_LIGHTHOUSE", "TR_TOOLTIP_BUTTON_REJECT_LIGHTHOUSE" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_ARMOURY", "TR_TOOLTIP_BUTTON_REJECT_ARMOURY" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_WORKCAMP", "TR_TOOLTIP_BUTTON_REJECT_WORKCAMP" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_NATIVES", "TR_TOOLTIP_BUTTON_REJECT_NATIVES" },
        { "TR_TOOLTIP_BUTTON_ACCEPT_CAESAR", "TR_TOOLTIP_BUTTON_REJECT_CAESAR" }
    };

    int permission_index = static_cast<int>(permission);
    if (permission_index < 0 || permission_index >= static_cast<int>(sizeof(tooltips) / sizeof(tooltips[0]))) {
        return {};
    }
    return tooltips[permission_index][reject_action ? 1 : 0];
}

void window_building_storage_get_tooltip_distribution_permissions(translation_key *translation)
{
    building *b = building_get(data.building_id);
    const Building storage_building(b);
    int is_warehouse_building = is_warehouse(storage_building);

    if (data.permission_focus_button_id) {
        building_storage_permission_states permission =
            permission_from_button(&permission_buttons[data.permission_focus_button_id - 1]);
        int show_reject_tooltip = building_storage_get_permission(permission, storage_building);
        *translation = storage_permission_tooltip(permission, show_reject_tooltip);
        return;
    }
    int index = data.image_button_focus_id;
    if (is_warehouse_building && index >= 1 && index <= 2) {
        int worker_rejects = building_storage_get_permission(BUILDING_STORAGE_PERMISSION_WORKER, storage_building);
        *translation = worker_rejects
            ? "TR_TOOLTIP_BUTTON_REJECT_WORKERS"
            : "TR_TOOLTIP_BUTTON_ACCEPT_WORKERS";
        return;
    }
    switch (index) {
        case 3:
            *translation = "TR_TOOLTIP_BUTTON_REJECT_ALL";
            return;
        case 4:
            *translation = "TR_TOOLTIP_BUTTON_ACCEPT_ALL";
            return;
        case 5:
            *translation = "TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION";
            return;
        default:
            return;
    }
}

int window_building_handle_mouse_storage_orders(const mouse *m, building_info_context *c)
{
    Building building = Building(building_get(c->building_id));
    int y_offset = window_building_get_vertical_offset(c, 28);
    data.building_id = c->building_id;
    for (int i = 0; i < 2; i++) {
        distribution_orders_buttons[i].focused = 0;
    }
    const resource_list *list = stored_resources_for_type(building.type());

    unsigned int buttons_to_show = list->size < scrollbar.elements_in_view
        ? list->size : scrollbar.elements_in_view;

    int x_offset = scrollbar.max_scroll_position > 0 ? 142 : 172;
    int result = 0;
    result += (scrollbar_handle_mouse(&scrollbar, m, 1));
    result += GenericButtonList(orders_resource_buttons, buttons_to_show).handle_mouse(
        *m,
        c->x_offset + x_offset,
        y_offset + 46,
        &data.resource_focus_button_id
    );
    result += GenericButtonList(orders_partial_resource_buttons, buttons_to_show).handle_mouse(
        *m,
        c->x_offset + x_offset,
        y_offset + 46,
        &data.partial_resource_focus_button_id
    );
    result += image_buttons_handle_mouse(m, c->x_offset - 4, c->y_offset - 4,
            distribution_orders_buttons, 2, 0); // focus handled on image_button level
    result += GenericButtonList(storage_empty_all_button, 1).handle_mouse(
        *m,
        0,
        0,
        &data.orders_focus_button_id
    );

    return result > 0 ? 1 : 0;
}

void window_building_primary_product_producer_stockpiling_tooltip(translation_key *translation)
{
    if (data.primary_product_stockpiling_id) {
        if (building_stockpiling_enabled(building_get(data.building_id))) {
            *translation = "TR_TOOLTIP_BUTTON_STOCKPILING_OFF";
        } else {
            *translation = "TR_TOOLTIP_BUTTON_STOCKPILING_ON";
        }
    }
}

static void on_scroll(void)
{
    window_invalidate();
}

static void go_to_orders(const generic_button *button)
{
    window_building_info_show_storage_orders();
}

static void resource_state_forward(const generic_button *button)
{
    toggle_resource_state(button, 0);
}

static void resource_state_back(const generic_button *button)
{
    toggle_resource_state(button, 1);
}

static void toggle_resource_state(const generic_button *button, int reverse_order)
{
    int index = button->parameter1;
    building *b = building_get(data.building_id);
    index += scrollbar.scroll_position - 1;
    resource_type resource;
    Building building(b);
    const auto *definition = building.type_definition();
    if (building.has_supplier_inventory_type() ||
        (definition && std::strcmp(definition->attr(), "dock") == 0)) {
        resource = data.stored_resources.items[index];
        building.toggle_accepted_good(resource);
    } else {
        const resource_list *list = stored_resources_for_type(building.type());
        resource = list->items[index];
        building_storage_cycle_resource_state(b->storage_id, resource, reverse_order);
    }
    window_invalidate();
}

static void market_orders(const generic_button *button)
{
    int index = button->parameter1;
    building *b = building_get(data.building_id);
    if (index == 0) {
        Building market_building(b);
        if (affect_all_button_distribution_state() == ACCEPT_ALL) {
            set_distribution_acceptance(market_building, 1);
        } else {
            set_distribution_acceptance(market_building, 0);
        }
    }
    window_invalidate();
}

static void storage_toggle_permissions(const generic_button *button)
{
    building_storage_permission_states index = permission_from_button(button);
    building *b = building_get(data.building_id);
    Building storage_building(b);
    building_storage_toggle_permission(index, storage_building);
    window_invalidate();
}

static void toggle_mantain(int param1, int param2)
{
    building *b = building_get(data.building_id);
    Building storage_building(b);
    building_storage_toggle_permission(BUILDING_STORAGE_PERMISSION_WORKER, storage_building);
    window_invalidate();
}

static void toggle_partial_resource_state(const generic_button *button, int reverse_order)
{
    int index = button->parameter1;
    building *b = building_get(data.building_id);
    resource_type resource;
    Building building(b);
    const auto &type = building.type();
    const resource_list *list = stored_resources_for_type(type);
    resource = list->items[index + scrollbar.scroll_position - 1];
    building_storage_cycle_partial_resource_state(b->storage_id, resource, reverse_order);
    window_invalidate();
}

static void toggle_partial_resource_state_forward(const generic_button *button)
{
    toggle_partial_resource_state(button, 0);
}

static void toggle_partial_resource_state_reverse(const generic_button *button)
{
    toggle_partial_resource_state(button, 1);
}

static void button_stockpiling(const generic_button *button)
{
    building *b = building_get(data.building_id);
    if (building_is_primary_product_producer(b->type)) {
        building_stockpiling_toggle(b);
    }
    window_invalidate();
}

static void dock_toggle_route(const generic_button *button)
{
    int route_id = button->parameter1;
    int can_trade = building_dock_can_trade_with_route(route_id, data.building_id);
    building_dock_set_can_trade_with_route(route_id, data.building_id, !can_trade);
    window_invalidate();
}

static void storage_empty_all(const generic_button *button)
{
    int storage_id = building_get(data.building_id)->storage_id;
    building_storage_toggle_empty_all(storage_id);
    window_invalidate();
}

static void storage_toggle_all_states(int param1, int param2)
{

    int storage_id = building_get(data.building_id)->storage_id;
    if (param1 == 0) {
        building_storage_accept_all(storage_id);
    } else {
        building_storage_accept_none(storage_id);
    }
    window_invalidate();
}


static void roadblock_orders(int param1, int param2)
{
    window_building_info_show_roadblock_orders();
}

void window_building_draw_mess_hall(building_info_context *c)
{
    c->advisor_button = ADVISOR_MILITARY;
    building *b = building_get(c->building_id);
    int mess_hall_fulfillment_display = 100 - city_mess_hall_food_missing_month();
    int food_stress = city_mess_hall_food_stress();
    translation_key hunger_text;
    int y_offset = 0;
    int food_types = count_food_types_in_stock(b);

    window_building_play_sound(c, "wavs/warehouse2.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for_key("TR_BUILDING_MESS_HALL"),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    if (b->num_workers <= 0 && food_types <= 0) {
        window_building_draw_description_at(c, 50, "TR_BUILDING_MESS_HALL_NO_EMPLOYEES");
    } else if (b->num_workers > 0 && food_types <= 0) {
        window_building_draw_description_at(c, 50, "TR_BUILDING_MESS_HALL_NO_FOOD");
    } else {
        draw_food_stocks(c, b, 50);
    }
    if (city_military_total_soldiers_in_city() > 0) {
        int width = text_draw(translation_for_key("TR_BUILDING_MESS_HALL_FULFILLMENT"),
            c->x_offset + 32, c->y_offset + 92 + y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw_percentage(mess_hall_fulfillment_display,
            c->x_offset + 32 + width, c->y_offset + 92 + y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        width = text_draw(translation_for_key("TR_BUILDING_MESS_HALL_TROOP_HUNGER"),
            c->x_offset + 32, c->y_offset + 117 + y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        if (food_stress < 3) {
            hunger_text = "TR_BUILDING_MESS_HALL_TROOP_HUNGER_1";
        } else if (food_stress > 80) {
            hunger_text = "TR_BUILDING_MESS_HALL_TROOP_HUNGER_5";
        } else if (food_stress > 60) {
            hunger_text = "TR_BUILDING_MESS_HALL_TROOP_HUNGER_4";
        } else if (food_stress > 40) {
            hunger_text = "TR_BUILDING_MESS_HALL_TROOP_HUNGER_3";
        } else {
            hunger_text = "TR_BUILDING_MESS_HALL_TROOP_HUNGER_2";
        }

        text_draw(translation_for(hunger_text), c->x_offset + 32 + width, c->y_offset + 117 + y_offset,
            FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

        width = text_draw(translation_for_key("TR_BUILDING_MESS_HALL_MONTHS_FOOD_STORED"), c->x_offset + 32,
            c->y_offset + 142 + y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw_number(city_mess_hall_months_food_stored(), '@', " ",
            c->x_offset + 32 + width, c->y_offset + 142 + y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

        if (city_mess_hall_food_types() == 2) {
            text_draw_multiline(translation_for_key("TR_BUILDING_MESS_HALL_FOOD_TYPES_BONUS_1"), c->x_offset + 32,
                c->y_offset + 167 + y_offset, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else if (city_mess_hall_food_types() >= 3) {
            text_draw_multiline(translation_for_key("TR_BUILDING_MESS_HALL_FOOD_TYPES_BONUS_2"), c->x_offset + 32,
                c->y_offset + 167 + y_offset, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
    } else {
        text_draw_centered(translation_for_key("TR_BUILDING_MESS_HALL_NO_SOLDIERS"), c->x_offset, c->y_offset + 130 + y_offset,
            BLOCK_SIZE * (c->width_blocks), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
    text_draw_multiline(translation_for_key("TR_BUILDING_MESS_HALL_DESC"), c->x_offset + 32, c->y_offset + 316 + y_offset,
        BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

    inner_panel_draw(c->x_offset + 16, c->y_offset + 236 + y_offset, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 240 + y_offset);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 244 + y_offset);
    window_building_distributor_draw_foreground(c);
}

static void window_building_draw_monument_caravanserai_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_CARAVANSERAI_PHASE_1",
        "TR_BUILDING_CARAVANSERAI_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_lighthouse_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_LIGHTHOUSE_PHASE_1",
        "TR_BUILDING_LIGHTHOUSE_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

int window_building_handle_mouse_caravanserai(const mouse *m, building_info_context *c)
{
    return GenericButtonList(go_to_caravanserai_action_button, 1).handle_mouse(
        *m,
        c->x_offset + 32,
        c->y_offset + 160 + data.caravanserai_button_y_offset,
        &data.caravanserai_focus_button_id
    );
}

void window_building_draw_caravanserai_foreground(building_info_context *c)
{
    ImageBorder::image_medium().draw(
        c->x_offset + 32,
        c->y_offset + 160 + data.caravanserai_button_y_offset,
        data.caravanserai_focus_button_id == 1 ? COLOR_BORDER_RED : COLOR_BORDER_GREEN);
}

static void apply_policy_land(int selected_policy)
{
    if (selected_policy == NO_POLICY) {
        return;
    }
    city_trade_policy_set(LAND_TRADE_POLICY, static_cast<trade_policy>(selected_policy));
    sound_speech_play_file(land_trade_policy.wav_file);
    city_finance_process_sundry(TRADE_POLICY_COST);
}

static void button_caravanserai_policy(const generic_button *button)
{
    building *b = building_get(data.building_id);
    if (b && is_monument_working(b)) {
        window_option_popup_show(land_trade_policy.title, land_trade_policy.subtitle,
            &land_trade_policy.items[1], 3, apply_policy_land, city_trade_policy_get(LAND_TRADE_POLICY),
            TRADE_POLICY_COST, OPTION_MENU_SMALL_ROW);
    }
}

void window_building_draw_caravanserai(building_info_context *c)
{
    building *b = building_get(c->building_id);

    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_TRADE;
        window_building_play_sound(c, "wavs/market2.wav");
        int food_types = count_food_types_in_stock(b);
        int y_offset = 0;
        data.caravanserai_button_y_offset = y_offset;
        c->height_blocks = (c->height_blocks < 30 ? 26 : 38) + y_offset / BLOCK_SIZE;

        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

        if (b->num_workers <= 0 && food_types <= 0) {
            window_building_draw_description_at(c, 50, "TR_BUILDING_CARAVANSERAI_NO_EMPLOYEES");
        } else if (b->num_workers > 0 && food_types <= 0) {
            window_building_draw_description_at(c, 50, "TR_BUILDING_CARAVANSERAI_NO_FOOD");
        } else {
            draw_food_stocks(c, b, 38);
            draw_caravanserai_food_per_month(c, 70);
        }

        if (!c->has_road_access) {
            window_building_draw_description_at(c, 91, 69, 25);
        } else if (building_monument_has_labour_problems(b)) {
            text_draw_multiline(translation_for_key("TR_BUILDING_CARAVANSERAI_NEEDS_WORKERS"),
                c->x_offset + 32, c->y_offset + 91 + y_offset, BLOCK_SIZE * (c->width_blocks - 3), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            const Building caravanserai(b);
            if (!building_caravanserai_enough_foods(caravanserai) && food_types >= 0) {
                text_draw_multiline(translation_for_key("TR_BUILDING_CARAVANSERAI_FOOD_SHORTAGE"),
                    c->x_offset + 32, c->y_offset + 91 + y_offset, BLOCK_SIZE * (c->width_blocks - 3), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            } else {
                text_draw_multiline(translation_for_key("TR_BUILDING_CARAVANSERAI_DESC"),
                    c->x_offset + 32, c->y_offset + 91 + y_offset, BLOCK_SIZE * (c->width_blocks - 3), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            }
        }

        if (!land_trade_policy.items[0].image_id) {
            int base_policy_image = assets_get_image_id("UI",
                land_trade_policy.base_image_name);
            land_trade_policy.items[0].image_id = base_policy_image;
            land_trade_policy.items[1].image_id = base_policy_image + 1;
            land_trade_policy.items[2].image_id = base_policy_image + 2;
            land_trade_policy.items[3].image_id = base_policy_image + 3;
        }

        trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);

        text_draw_multiline(translation_for(land_trade_policy.items[policy].header),
            c->x_offset + 160, c->y_offset + 170 + y_offset, 260, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        if (policy != NO_POLICY) {
            text_draw_multiline(translation_for(land_trade_policy.items[policy].desc),
                c->x_offset + 160, c->y_offset + 195 + y_offset, 260, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        Image::from_id(land_trade_policy.items[policy].image_id).draw(c->x_offset + 32, c->y_offset + 160 + y_offset);

        inner_panel_draw(c->x_offset + 16, c->y_offset + 270 + y_offset, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 274 + y_offset);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 278 + y_offset);

        if (c->height_blocks >= 38) {
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 350 + y_offset);
            Image::from_id(assets_get_image_id("UI", "Caravanserai Banner")).draw(c->x_offset + 37, c->y_offset + 355 + y_offset);
        }
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        window_building_draw_monument_caravanserai_construction_process(c);
    }

    text_draw_centered(translation_for_key("TR_BUILDING_CARAVANSERAI"), c->x_offset, c->y_offset + 12,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
}

int window_building_handle_mouse_lighthouse(const mouse *m, building_info_context *c)
{
    return GenericButtonList(go_to_lighthouse_action_button, 1).handle_mouse(
        *m,
        c->x_offset + 32,
        c->y_offset + 150,
        &data.lighthouse_focus_button_id
    );
}

void window_building_draw_lighthouse_foreground(building_info_context *c)
{
    ImageBorder::image_medium().draw(
        c->x_offset + 32,
        c->y_offset + 150,
        data.lighthouse_focus_button_id == 1 ? COLOR_BORDER_RED : COLOR_BORDER_GREEN);
}

static void apply_policy_sea(int selected_policy)
{
    if (selected_policy == NO_POLICY) {
        return;
    }
    city_trade_policy_set(SEA_TRADE_POLICY, static_cast<trade_policy>(selected_policy));
    sound_speech_play_file(sea_trade_policy.wav_file);
    city_finance_process_sundry(TRADE_POLICY_COST);
}

static void button_lighthouse_policy(const generic_button *button)
{
    building *b = building_get(data.building_id);
    if (b && is_monument_working(b)) {
        window_option_popup_show(sea_trade_policy.title, sea_trade_policy.subtitle,
            &sea_trade_policy.items[1], 3, apply_policy_sea, city_trade_policy_get(SEA_TRADE_POLICY),
            TRADE_POLICY_COST, OPTION_MENU_SMALL_ROW);
    }
}

void window_building_draw_lighthouse(building_info_context *c)
{
    building *b = building_get(c->building_id);
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_TRADE;
        window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Lighthouse.ogg");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

        resource_graphics(resource_timber()).panel_icon().draw(c->x_offset + 32, c->y_offset + 46);
        int width = lang_text_draw("main_strings.125.12", c->x_offset + 60, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        if (b->resources[resource_timber()] < 1) {
            lang_text_draw_amount(current_string_amount_key(8, 10, 0), 0, c->x_offset + 60 + width, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        } else {
            lang_text_draw_amount(current_string_amount_key(8, 10, b->resources[resource_timber()]), b->resources[resource_timber()], c->x_offset + 60 + width, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        }

        if (!c->has_road_access) {
            window_building_draw_description_at(c, 80, 69, 25);
        } else if (building_monument_has_labour_problems(b)) {
            text_draw_multiline(translation_for_key("TR_BUILDING_LIGHTHOUSE_NEEDS_WORKERS"),
                c->x_offset + 32, c->y_offset + 80, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            const Building lighthouse(b);
            if (building_lighthouse_enough_timber(lighthouse) == 0) {
                text_draw_multiline(translation_for_key("TR_BUILDING_LIGHTHOUSE_NO_TIMBER"),
                    c->x_offset + 32, c->y_offset + 80, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            } else {
                text_draw_multiline(translation_for_key("TR_BUILDING_LIGHTHOUSE_BONUS_DESC"),
                    c->x_offset + 32, c->y_offset + 80, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            }
        }

        if (!sea_trade_policy.items[0].image_id) {
            int base_policy_image = assets_get_image_id("UI",
                sea_trade_policy.base_image_name);
            sea_trade_policy.items[0].image_id = base_policy_image;
            sea_trade_policy.items[1].image_id = base_policy_image + 1;
            sea_trade_policy.items[2].image_id = base_policy_image + 2;
            sea_trade_policy.items[3].image_id = base_policy_image + 3;
        }

        trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);

        text_draw_multiline(translation_for(sea_trade_policy.items[policy].header),
            c->x_offset + 160, c->y_offset + 156, 260, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        if (policy != NO_POLICY) {
            text_draw_multiline(translation_for(sea_trade_policy.items[policy].desc),
                c->x_offset + 160, c->y_offset + 181, 260, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        Image::from_id(sea_trade_policy.items[policy].image_id).draw(c->x_offset + 32, c->y_offset + 150);

        inner_panel_draw(c->x_offset + 16, c->y_offset + 270, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 278);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 278);

        if (c->height_blocks >= 38) {
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 350);
            Image::from_id(assets_get_image_id("UI", "Lighthouse Banner")).draw(c->x_offset + 37, c->y_offset + 355);
        }

    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        window_building_draw_monument_lighthouse_construction_process(c);
    }
    text_draw_centered(translation_for_key("TR_BUILDING_LIGHTHOUSE"),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
}
