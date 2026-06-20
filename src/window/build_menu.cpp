#include "building/construction.h"
#include "translation/translation.h"
#include "building/rotation.h"
#include "city/warning.h"
#include "game/resource_graphics.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "input/input.h"
#include "widget/city.h"
#include "widget/sidebar/city.h"
#include "window/city.h"

#include "build_menu.h"


#include "building/building_type_api.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/view.h"
#include "core/config.h"
#include "core/image.h"
#include "core/locale.h"
#include "core/string.h"
#include "game/resource.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "building/building_type_registry_internal.h"
#include <building/industry.h>

#include <cstddef>
#include <cstring>
#include <vector>

#define MENU_X_OFFSET 298
#define MENU_Y_OFFSET 110
#define MENU_ITEM_HEIGHT 24
#define MENU_LABEL_BLOCKS 18
#define MENU_ITEM_WIDTH 290
#define MENU_CLICK_MARGIN 20
#define MENU_TEXT_X_OFFSET 8
// Center build labels on the visible button rectangle; the retained hit box is wider than the label art.
#define MENU_TEXT_CENTER_X_OFFSET -5

#define MENU_RESOURCE_ICON_SIZE 16

#define MENU_ICON_WIDTH 14
#define MENU_ICON_X_OFFSET 3
#define MENU_ICON_Y_OFFSET 3
#define MENU_ITEM_MONEY_OFFSET 88

#define TOOLTIP_TEXT_LENGTH 1000

static uint8_t tooltip_text[TOOLTIP_TEXT_LENGTH];

static const building_type_registry_impl::BuildingType *building_type_definition_from_attr(const char *text_id)
{
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (definition && std::strcmp(definition->attr(), text_id) == 0) {
            return definition.get();
        }
    }
    return nullptr;
}

static building_type building_type_from_attr(const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition = building_type_definition_from_attr(text_id);
    return definition ? definition->type() : BUILDING_NONE;
}

static int building_type_attr_is(building_type type, const char *text_id)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

class BuildMenuButton {
public:
    void bind(build_menu_group submenu, int item_index, unsigned int display_index, generic_button &button_widget);
    void clear();
    int is_bound() const;
    building_type type() const;
    int shortcut_index() const;
    const uint8_t *display_name() const;
    const ImageGroupEntryRef &menu_icon() const;
    int cost() const;
    int has_rotation_icon() const;
    int has_monument_icon() const;
    int is_auto_cycle() const;
    void draw(int item_x_align, int x_offset, int focused) const;
    void activate() const;

private:
    building_type cost_type() const;
    const building_type_registry_impl::BuildingType *definition() const;

    build_menu_group submenu = SUBMENU_NONE;
    int menu_item_index = -1;
    unsigned int display_index = 0;
    building_type building = BUILDING_NONE;
    const building_type_registry_impl::BuildingType *type_definition = nullptr;
    ImageGroupEntryRef menu_icon_ref;
};

static void button_menu_button_clicked(const generic_button *button);
static void rebuild_visible_menu_buttons(void);
static void request_visible_menu_button_rebuild(void);
static const BuildMenuButton *focused_menu_button(void);
static ImageGroupEntryRef produced_resource_icon(building_type type);

static std::vector<generic_button> build_menu_button_widgets;
static std::vector<BuildMenuButton> build_menu_buttons;

static const int Y_MENU_OFFSETS[] = {
    0, 322, 306, 274, 258, 226, 210, 178, 162, 130, 114,
    82, 66, 34, 18, -30, -46, -62, -78, -78, -94,
    -94, -110, -110,
    0, 0, 0, 0, 0, 0
};

static int menu_y_offset_for_count(unsigned int count)
{
    unsigned int offset_count = sizeof(Y_MENU_OFFSETS) / sizeof(Y_MENU_OFFSETS[0]);
    if (count < offset_count) {
        return Y_MENU_OFFSETS[count];
    }
    return Y_MENU_OFFSETS[offset_count - 1];
}

static struct {
    build_menu_group selected_submenu;
    unsigned int num_items;
    int y_offset;
    unsigned int focus_button_id;
    int handling_button_mouse;
    int rebuild_buttons_after_input;
} data = { SUBMENU_NONE };

void BuildMenuButton::bind(build_menu_group button_submenu, int item_index, unsigned int button_display_index,
    generic_button &button_widget)
{
    submenu = button_submenu;
    menu_item_index = item_index;
    display_index = button_display_index;
    building = building_menu_type(submenu, menu_item_index);
    type_definition = building_type_registry_impl::definition_for_type(cost_type());
    menu_icon_ref = definition() ? definition()->button_icon_ref() : ImageGroupEntryRef();
    if (!menu_icon_ref.is_bound()) {
        menu_icon_ref = produced_resource_icon(building);
    }

    button_widget.reset();
    button_widget.set_bounds(0, static_cast<short>(MENU_ITEM_HEIGHT * display_index), MENU_ITEM_WIDTH, 20);
    button_widget.set_handlers(button_menu_button_clicked, 0);
    button_widget.set_context(this);
    button_widget.debug_name = "city-build-menu-button";
}

void BuildMenuButton::clear()
{
    submenu = SUBMENU_NONE;
    menu_item_index = -1;
    display_index = 0;
    building = BUILDING_NONE;
    type_definition = nullptr;
    menu_icon_ref = ImageGroupEntryRef();
}

int BuildMenuButton::is_bound() const
{
    return menu_item_index >= 0;
}

building_type BuildMenuButton::type() const
{
    return building;
}

int BuildMenuButton::shortcut_index() const
{
    if (config_get(CONFIG_UI_ENABLE_BUILD_MENU_SHORTCUTS) && display_index < 10) {
        return static_cast<int>(display_index + 1);
    }
    return 0;
}

static void rebuild_visible_menu_buttons(void)
{
    build_menu_buttons.clear();
    build_menu_button_widgets.clear();
    build_menu_buttons.resize(data.num_items);
    build_menu_button_widgets.resize(data.num_items);

    int item_index = -1;
    for (std::size_t i = 0; i < build_menu_buttons.size(); i++) {
        item_index = building_menu_next_index(data.selected_submenu, item_index);
        build_menu_buttons[i].bind(data.selected_submenu, item_index, static_cast<unsigned int>(i),
            build_menu_button_widgets[i]);
    }
}

static void request_visible_menu_button_rebuild(void)
{
    if (data.handling_button_mouse) {
        data.rebuild_buttons_after_input = 1;
        return;
    }
    rebuild_visible_menu_buttons();
}

static const BuildMenuButton *focused_menu_button(void)
{
    if (!data.focus_button_id || static_cast<std::size_t>(data.focus_button_id) > build_menu_buttons.size()) {
        return 0;
    }
    const BuildMenuButton *button = &build_menu_buttons[data.focus_button_id - 1];
    return button->is_bound() ? button : 0;
}

static int init(build_menu_group submenu)
{
    data.selected_submenu = submenu;
    data.num_items = building_menu_count_items(submenu);
    data.y_offset = menu_y_offset_for_count(data.num_items);
    data.focus_button_id = 0;
    rebuild_visible_menu_buttons();
    if (submenu == BUILD_MENU_VACANT_HOUSE && !build_menu_buttons.empty()) {
        build_menu_buttons[0].activate();
        return 0;
    } else {
        return 1;
    }
}

int window_build_menu_image(void)
{
    building_type type = building_construction_selection_type();
    int image_base = Image::group(GROUP_PANEL_WINDOWS);
    if (type == BUILDING_NONE) {
        return image_base + 12;
    }
    switch (building_menu_for_type(type)) {
        default:
        case BUILD_MENU_VACANT_HOUSE:
            return image_base;
        case BUILD_MENU_CLEAR_LAND:
            if (scenario_property_climate() == CLIMATE_DESERT) {
                return Image::group(GROUP_PANEL_WINDOWS_DESERT);
            } else {
                return image_base + 11;
            }
        case BUILD_MENU_ROAD:
            if (scenario_property_climate() == CLIMATE_DESERT) {
                return Image::group(GROUP_PANEL_WINDOWS_DESERT) + 1;
            } else {
                return image_base + 10;
            }
        case BUILD_MENU_WATER:
            if (scenario_property_climate() == CLIMATE_DESERT) {
                return Image::group(GROUP_PANEL_WINDOWS_DESERT) + 2;
            } else {
                return image_base + 3;
            }
        case BUILD_MENU_HEALTH:
            return image_base + 5;
        case BUILD_MENU_TEMPLES:
        case BUILD_MENU_SMALL_TEMPLES:
        case BUILD_MENU_LARGE_TEMPLES:
            return image_base + 1;
        case BUILD_MENU_EDUCATION:
            return image_base + 6;
        case BUILD_MENU_ENTERTAINMENT:
            return image_base + 4;
        case BUILD_MENU_ADMINISTRATION:
            return image_base + 2;
        case BUILD_MENU_ENGINEERING:
            return image_base + 7;
        case BUILD_MENU_SECURITY:
        case BUILD_MENU_FORTS:
            if (scenario_property_climate() == CLIMATE_DESERT) {
                return Image::group(GROUP_PANEL_WINDOWS_DESERT) + 3;
            } else {
                return image_base + 8;
            }
        case BUILD_MENU_INDUSTRY:
        case BUILD_MENU_FARMS:
        case BUILD_MENU_RAW_MATERIALS:
        case BUILD_MENU_WORKSHOPS:
            return image_base + 9;
    }
}

static void draw_background(void)
{
    window_city_draw_panels();
}

static int get_sidebar_x_offset(void)
{
    int view_x, view_y, view_width, view_height;
    city_view_get_viewport(&view_x, &view_y, &view_width, &view_height);
    return screen_pixel_to_ui(view_x + view_width);
}

static int is_auto_cycle_button(build_menu_group submenu, building_type type)
{
    return (building_type_attr_is(type, "small_temples") && submenu == BUILD_MENU_SMALL_TEMPLES) ||
        (building_type_attr_is(type, "large_temples") && submenu == BUILD_MENU_LARGE_TEMPLES) ||
        (building_type_attr_is(type, "shrines") && submenu == BUILD_MENU_SHRINES) ||
        (building_type_attr_is(type, "trees") && submenu == BUILD_MENU_TREES) ||
        (building_type_attr_is(type, "paths") && submenu == BUILD_MENU_PATHS) ||
        (building_type_attr_is(type, "all_gardens") && submenu == BUILD_MENU_GARDENS);
}

// Costs share the right-side status column with auto-cycle text, so center the composed money string there.
static void draw_menu_money_centered(int value, int x, int y, int box_width, font_t font, int pixel_size)
{
    const uint8_t *postfix = locale_translate_money_dn() ? lang_get_string("main_strings.6.0") : string_from_ascii("Dn");
    int width = text_get_number_width(value, '@', "", font, pixel_size) + text_get_width(postfix, font, pixel_size);
    int offset = (box_width - width) / 2;
    if (offset < 0) {
        offset = 0;
    }
    text_draw_money(value, x + offset, y, font, pixel_size);
}

static ImageGroupEntryRef produced_resource_icon(building_type type)
{
    resource_type r = building_output_resource(type);
    if (r != RESOURCE_NONE) {
        return resource_graphics(r).panel_icon();
    }
    return ImageGroupEntryRef();
}

static void draw_resource_icon_scaled(const ImageGroupEntryRef &image, int x, int y, int max_size)
{
    if (!image.is_bound()) {
        return;
    }
    const int image_width = image.width();
    const int image_height = image.height();
    if (image_width <= 0 || image_height <= 0) {
        return;
    }
    const int larger_dimension = image_width > image_height ? image_width : image_height;
    const float draw_scale = larger_dimension > max_size ?
        static_cast<float>(larger_dimension) / static_cast<float>(max_size) :
        SCALE_NONE;
    const int scaled_width = static_cast<int>(image_width / draw_scale);
    const int scaled_height = static_cast<int>(image_height / draw_scale);
    image.draw(x + (max_size - scaled_width) / 2, y + (max_size - scaled_height) / 2, COLOR_MASK_NONE, draw_scale);
}

building_type BuildMenuButton::cost_type() const
{
    if (building_type_attr_is(building, "draggable_reservoir")) {
        return building_type_from_attr("reservoir");
    }
    return building;
}

const building_type_registry_impl::BuildingType *BuildMenuButton::definition() const
{
    return type_definition;
}

const uint8_t *BuildMenuButton::display_name() const
{
    if (is_auto_cycle()) {
        return translation_for_key("TR_AUTO_CYCLE_TEMPLES");
    }
    const char *text_key = definition() ? definition()->button_text_key() : nullptr;
    if (text_key && *text_key) {
        const uint8_t *display_name = lang_get_string_by_key(text_key);
        if (display_name) {
            return display_name;
        }
    }
    text_key = definition() && definition()->identity().has_name_key() ? definition()->identity().name_key() : nullptr;
    if (text_key && *text_key) {
        const uint8_t *display_name = lang_get_string_by_key(text_key);
        if (display_name) {
            return display_name;
        }
    }
    return lang_get_building_type_string(building);
}

const ImageGroupEntryRef &BuildMenuButton::menu_icon() const
{
    return menu_icon_ref;
}

int BuildMenuButton::cost() const
{
    return model_get_building(cost_type())->cost;
}

int BuildMenuButton::has_rotation_icon() const
{
    return building_rotation_type_has_rotations(cost_type());
}

int BuildMenuButton::has_monument_icon() const
{
    return building_monument_type_is_monument(cost_type());
}

int BuildMenuButton::is_auto_cycle() const
{
    return is_auto_cycle_button(submenu, building);
}

void BuildMenuButton::draw(int item_x_align, int x_offset, int focused) const
{
    if (!is_bound()) {
        return;
    }
    int item_y = data.y_offset + MENU_Y_OFFSET + static_cast<int>(MENU_ITEM_HEIGHT * display_index);
    int menu_index = shortcut_index();

    label_draw(item_x_align, item_y, MENU_LABEL_BLOCKS, focused ? 1 : 2);

    if (is_auto_cycle()) {
        if (menu_index > 0) {
            text_draw_build_menu_with_index(display_name(), menu_index % 10,
                item_x_align + MENU_TEXT_CENTER_X_OFFSET, item_y + 4,
                MENU_ITEM_WIDTH, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
        } else {
            text_draw_centered(display_name(),
                item_x_align + MENU_TEXT_CENTER_X_OFFSET, item_y + 4,
                MENU_ITEM_WIDTH, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
        }
        lang_text_draw_centered(current_string_key(18, 5 - building_construction_is_auto_cycling()), x_offset - MENU_ITEM_MONEY_OFFSET, item_y + 4, MENU_ITEM_MONEY_OFFSET, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        return;
    }

    const ImageGroupEntryRef &icon = menu_icon();
    if (icon.is_bound() && config_get(CONFIG_UI_CV_BUILD_MENU_ICONS)) {
        draw_resource_icon_scaled(icon, item_x_align + MENU_TEXT_X_OFFSET + 2 +
            (has_monument_icon() + has_rotation_icon()) * MENU_ICON_WIDTH,
            item_y + 2, MENU_RESOURCE_ICON_SIZE);
    }

    int build_cost = cost();
    if (menu_index > 0) {
        text_draw_build_menu_with_index(display_name(), menu_index % 10,
            item_x_align + MENU_TEXT_CENTER_X_OFFSET, item_y + 4,
            MENU_ITEM_WIDTH, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    } else {
        text_draw_centered(display_name(), item_x_align + MENU_TEXT_CENTER_X_OFFSET, item_y + 4,
            MENU_ITEM_WIDTH, FONT_NORMAL_GREEN,
            screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    }

    if (build_cost) {
        draw_menu_money_centered(build_cost, x_offset - MENU_ITEM_MONEY_OFFSET, item_y + 4, MENU_ITEM_MONEY_OFFSET,
            FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        ImageGroupEntryRef::from_group("UI\\Expand_Menu_Icon", "Expand Menu Icon").draw(item_x_align + MENU_ICON_X_OFFSET + 268, item_y + MENU_ICON_Y_OFFSET);
    }

    int icons_drawn = 0;
    if (has_rotation_icon()) {
        ImageGroupEntryRef::from_group("UI\\Rotate_Build_Icon", "Rotate Build Icon").draw(item_x_align + icons_drawn * MENU_ICON_WIDTH + MENU_ICON_X_OFFSET, item_y + MENU_ICON_Y_OFFSET);
        icons_drawn++;
    }

    if (has_monument_icon()) {
        ImageGroupEntryRef::from_group("UI\\Monument_Build_Icon", "Monument Build Icon").draw(item_x_align + icons_drawn * (MENU_ICON_WIDTH + 3) + MENU_ICON_X_OFFSET, item_y + MENU_ICON_Y_OFFSET);
    }
}

static void draw_menu_buttons(void)
{
    int x_offset = get_sidebar_x_offset();
    int item_x_align = x_offset - MENU_X_OFFSET;
    for (std::size_t i = 0; i < build_menu_buttons.size(); i++) {
        build_menu_buttons[i].draw(item_x_align, x_offset, data.focus_button_id == static_cast<unsigned int>(i + 1));
    }
}

static void draw_foreground(void)
{
    window_city_draw();
    window_city_draw_custom_variables_text_display();
    draw_menu_buttons();
}

static int click_outside_menu(const mouse *m, int x_offset)
{
    if (!m->left.went_up) {
        return 0;
    }
    return m->x < x_offset - MENU_X_OFFSET - MENU_CLICK_MARGIN ||
        m->x > x_offset + MENU_CLICK_MARGIN ||
        m->y < data.y_offset + MENU_Y_OFFSET - MENU_CLICK_MARGIN ||
        m->y > data.y_offset + MENU_Y_OFFSET + MENU_CLICK_MARGIN + MENU_ITEM_HEIGHT * static_cast<int>(data.num_items);
}

static int handle_build_submenu(const mouse *m)
{
    data.handling_button_mouse = 1;
    GenericButtonList buttons(
        build_menu_button_widgets.data(),
        static_cast<unsigned int>(build_menu_button_widgets.size()));
    int handled = buttons.handle_mouse(
        *m,
        get_sidebar_x_offset() - MENU_X_OFFSET,
        data.y_offset + MENU_Y_OFFSET,
        &data.focus_button_id
    );
    data.handling_button_mouse = 0;
    if (data.rebuild_buttons_after_input) {
        data.rebuild_buttons_after_input = 0;
        rebuild_visible_menu_buttons();
    }
    return handled;
}

static int handle_input_build_menu_index(const hotkeys *h)
{
    if (config_get(CONFIG_UI_ENABLE_BUILD_MENU_SHORTCUTS) && h->build_menu_index_num &&
        static_cast<std::size_t>(h->build_menu_index_num) <= build_menu_buttons.size()) {
        build_menu_buttons[h->build_menu_index_num - 1].activate();
        return h->build_menu_index_num;
    }
    return 0;
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    if (handle_build_submenu(m) ||
        widget_sidebar_city_handle_mouse_build_menu(m) ||
        handle_input_build_menu_index(h)) {
        return;
    }
    if (input_go_back_requested(m, h) || click_outside_menu(m, get_sidebar_x_offset())) {
        data.selected_submenu = SUBMENU_NONE;
        window_city_show();
        return;
    }
}

static void button_menu_button_clicked(const generic_button *button)
{
    const BuildMenuButton *menu_button = static_cast<const BuildMenuButton *>(button->context());
    if (menu_button) {
        menu_button->activate();
    }
}

static int set_submenu_for_type(building_type type)
{
    build_menu_group current_menu = data.selected_submenu;
    build_menu_group new_menu = static_cast<build_menu_group>(building_menu_get_submenu_for_type(type));
    if (!new_menu) {
        return 0;
    }
    data.selected_submenu = new_menu;
    return current_menu != data.selected_submenu;
}

void BuildMenuButton::activate() const
{
    if (!is_bound()) {
        return;
    }

    widget_city_clear_current_tile();

    if (is_auto_cycle()) {
        building_construction_toggle_auto_cycle();
        window_invalidate();
        return;
    }
    building_construction_set_type(building, 0);

    if (set_submenu_for_type(building)) {
        data.num_items = building_menu_count_items(data.selected_submenu);
        data.y_offset = menu_y_offset_for_count(data.num_items);
        request_visible_menu_button_rebuild();
        building_construction_clear_type();
        window_invalidate();
    } else {
        data.selected_submenu = SUBMENU_NONE;
        window_city_show();
    }
}

static inline int remanining_length(const uint8_t *index)
{
    return TOOLTIP_TEXT_LENGTH - (int) (index - tooltip_text);
}

static void generate_tooltip_text_for_monument(building_type monument)
{
    int phases = building_monument_phases(monument) - 1;
    uint8_t *index = tooltip_text;
    index += string_from_int(index, phases, 0);
    index = string_copy(lang_get_string(phases == 1 ? "TR_TOOLTIP_MONUMENT_PHASE" : "TR_TOOLTIP_MONUMENT_PHASES"),
        index, remanining_length(index));
    index = string_copy(lang_get_string("TR_TOOLTIP_MONUMENT_RESOURCE_REQUIREMENTS"),
        index, remanining_length(index));

    int has_listed_resource = 0;

    for (int i = (RESOURCE_NONE + 1); i < RESOURCE_SLOT_COUNT; ++i) {
        resource_type r = static_cast<resource_type>(i);
        int amount = 0;
        for (int phase = 1; phase <= phases; phase++) {
            amount += building_monument_resources_needed_for_monument_type(monument, r, phase);
        }
        if (amount) {
            if (has_listed_resource) {
                index = string_copy(string_from_ascii(", "), index, remanining_length(index));
            }
            index += string_from_int(index, amount, 0);
            index = string_copy(string_from_ascii(" "), index, remanining_length(index));
            index = string_copy(resource_get_data(r)->text, index, remanining_length(index));
            has_listed_resource = 1;
        }
    }
}

static void get_tooltip(tooltip_context *c)
{
    if (!data.focus_button_id || data.selected_submenu == SUBMENU_NONE ||
        mouse_get()->x > get_sidebar_x_offset() - MENU_X_OFFSET + MENU_ICON_WIDTH * 3) {
        return;
    }

    const BuildMenuButton *button = focused_menu_button();
    if (!button) {
        return;
    }

    if (button->has_monument_icon()) {
        generate_tooltip_text_for_monument(button->type());
        c->precomposed_text = tooltip_text;
        c->type = TOOLTIP_BUTTON;
    }
}

void window_build_menu_show(int submenu)
{
    if (submenu == SUBMENU_NONE || submenu == data.selected_submenu) {
        window_build_menu_hide();
        return;
    }
    if (init(static_cast<build_menu_group>(submenu))) {
        window_type window = {
            WINDOW_BUILD_MENU,
            draw_background,
            draw_foreground,
            handle_input,
            get_tooltip
        };
        window_show(&window);
    }
}

void window_build_menu_hide(void)
{
    data.selected_submenu = SUBMENU_NONE;
    window_city_show();
}
