#

#include "graphics/declarative_window.h"

#include "core/crash_context.h"
#include "core/xml_value.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/file.h"
#include "core/xml_parser.h"
#include "game/mod_definition_loader.h"
#include "graphics/image.h"
#include "graphics/graphics.h"
#include "graphics/image_border.h"
#include "graphics/text.h"
#include "graphics/ui_runtime_api.h"
#include "translation/translation.h"
#include "window/main_menu.h"

namespace {

constexpr const char *kMissionBriefingWindowId = "mission_briefing";
constexpr const char *kMainMenuWindowId = "main_menu";

std::unordered_map<std::string, std::unique_ptr<DeclarativeWindowDefinition>> g_windows;
std::unordered_map<std::string, std::unique_ptr<DeclarativeWindow>> g_constructed_windows;
std::string g_failure_reason;

struct ParseState {
    std::unique_ptr<DeclarativeWindowDefinition> definition;
    int saw_root = 0;
    int error = 0;
};

ParseState g_parse_state;

class ScopedFile {
public:
    explicit ScopedFile(FILE *file)
        : file_(file)
    {
    }

    ~ScopedFile()
    {
        if (file_) {
            file_close(file_);
        }
    }

    FILE *get() const
    {
        return file_;
    }

private:
    FILE *file_ = nullptr;
};

int parse_required_int(const char *attribute, int *out_value)
{
    if (!xml_parser_has_attribute(attribute) || !out_value) {
        return 0;
    }
    *out_value = xml_parser_get_attribute_int(attribute);
    return 1;
}

int parse_optional_int(const char *attribute, int default_value)
{
    return xml_parser_has_attribute(attribute) ? xml_parser_get_attribute_int(attribute) : default_value;
}

std::string parse_optional_string(const char *attribute)
{
    const char *value = xml_parser_get_attribute_string(attribute);
    return value ? value : "";
}

DeclarativeWidgetType parse_widget_type(const char *value)
{
    if (xml_value::equals(value, "panel")) {
        return DeclarativeWidgetType::Panel;
    }
    if (xml_value::equals(value, "image")) {
        return DeclarativeWidgetType::Image;
    }
    if (xml_value::equals(value, "label")) {
        return DeclarativeWidgetType::Label;
    }
    if (xml_value::equals(value, "rich_text")) {
        return DeclarativeWidgetType::RichText;
    }
    if (xml_value::equals(value, "scrollbar")) {
        return DeclarativeWidgetType::Scrollbar;
    }
    if (xml_value::equals(value, "text_button")) {
        return DeclarativeWidgetType::TextButton;
    }
    if (xml_value::equals(value, "image_button")) {
        return DeclarativeWidgetType::ImageButton;
    }
    return DeclarativeWidgetType::Unknown;
}

int attribute_is_one_of(const char *attribute, const char *first, const char *second = nullptr,
    const char *third = nullptr, const char *fourth = nullptr, const char *fifth = nullptr,
    const char *sixth = nullptr)
{
    if (!xml_parser_has_attribute(attribute)) {
        return 1;
    }
    const char *value = xml_parser_get_attribute_string(attribute);
    return xml_value::equals(value, first) ||
        (second && xml_value::equals(value, second)) ||
        (third && xml_value::equals(value, third)) ||
        (fourth && xml_value::equals(value, fourth)) ||
        (fifth && xml_value::equals(value, fifth)) ||
        (sixth && xml_value::equals(value, sixth));
}

DeclarativeDrawPhase parse_draw_phase(const char *value)
{
    return xml_value::equals(value, "background") ?
        DeclarativeDrawPhase::Background : DeclarativeDrawPhase::Foreground;
}

DeclarativeCoordinateSpace parse_coordinate_space(const char *value)
{
    return xml_value::equals(value, "screen") ?
        DeclarativeCoordinateSpace::Screen : DeclarativeCoordinateSpace::Dialog;
}

DeclarativeWidgetStyle parse_widget_style(const char *value)
{
    if (xml_value::equals(value, "outer_panel")) {
        return DeclarativeWidgetStyle::OuterPanel;
    }
    if (xml_value::equals(value, "inner_panel")) {
        return DeclarativeWidgetStyle::InnerPanel;
    }
    if (xml_value::equals(value, "solid")) {
        return DeclarativeWidgetStyle::Solid;
    }
    if (xml_value::equals(value, "label")) {
        return DeclarativeWidgetStyle::Label;
    }
    if (xml_value::equals(value, "large_label")) {
        return DeclarativeWidgetStyle::LargeLabel;
    }
    if (xml_value::equals(value, "image_small_border")) {
        return DeclarativeWidgetStyle::ImageSmallBorder;
    }
    return DeclarativeWidgetStyle::None;
}

DeclarativeTextAlignment parse_text_alignment(const char *value)
{
    if (xml_value::equals(value, "center")) {
        return DeclarativeTextAlignment::Center;
    }
    if (xml_value::equals(value, "right")) {
        return DeclarativeTextAlignment::Right;
    }
    return DeclarativeTextAlignment::Left;
}

DeclarativeVisibility parse_visibility(const char *value)
{
    if (xml_value::equals(value, "main_menu")) {
        return DeclarativeVisibility::MainMenu;
    }
    if (xml_value::equals(value, "not_file_dialog")) {
        return DeclarativeVisibility::NotFileDialog;
    }
    return DeclarativeVisibility::Always;
}

DeclarativeAnchor parse_anchor(const char *value)
{
    return xml_value::equals(value, "far") || xml_value::equals(value, "right") || xml_value::equals(value, "bottom") ?
        DeclarativeAnchor::Far :
        DeclarativeAnchor::Near;
}

font_t parse_font(const char *value, font_t default_font)
{
    if (xml_value::equals(value, "normal_plain")) {
        return FONT_NORMAL_PLAIN;
    }
    if (xml_value::equals(value, "normal_black")) {
        return FONT_NORMAL_BLACK;
    }
    if (xml_value::equals(value, "normal_white")) {
        return FONT_NORMAL_WHITE;
    }
    if (xml_value::equals(value, "normal_red")) {
        return FONT_NORMAL_RED;
    }
    if (xml_value::equals(value, "large_plain")) {
        return FONT_LARGE_PLAIN;
    }
    if (xml_value::equals(value, "large_black")) {
        return FONT_LARGE_BLACK;
    }
    if (xml_value::equals(value, "large_brown")) {
        return FONT_LARGE_BROWN;
    }
    if (xml_value::equals(value, "small_plain")) {
        return FONT_SMALL_PLAIN;
    }
    if (xml_value::equals(value, "normal_green")) {
        return FONT_NORMAL_GREEN;
    }
    if (xml_value::equals(value, "normal_brown")) {
        return FONT_NORMAL_BROWN;
    }
    return default_font;
}

color_t parse_color(const char *value, color_t default_color)
{
    if (xml_value::equals(value, "light_gray")) {
        return COLOR_FONT_LIGHT_GRAY;
    }
    if (xml_value::equals(value, "red")) {
        return COLOR_BORDER_RED;
    }
    if (xml_value::equals(value, "green")) {
        return COLOR_BORDER_GREEN;
    }
    if (xml_value::equals(value, "black")) {
        return COLOR_BLACK;
    }
    if (xml_value::equals(value, "white")) {
        return COLOR_WHITE;
    }
    return default_color;
}

void set_failure_reason(const char *message, const char *detail)
{
    if (detail && *detail) {
        g_failure_reason = std::string(message ? message : "") + "\n\n" + detail;
    } else {
        g_failure_reason = message ? message : "";
    }
}

int parse_window_node(void)
{
    const char *id = xml_parser_get_attribute_string("id");
    if (!id || !*id) {
        set_failure_reason("Declarative window XML is missing a window id.", nullptr);
        return 0;
    }

    int base_width = 0;
    int base_height = 0;
    if (!parse_required_int("base_width", &base_width) || !parse_required_int("base_height", &base_height)) {
        set_failure_reason("Declarative window XML is missing base dimensions.", id);
        return 0;
    }
    if (base_width <= 0 || base_height <= 0) {
        set_failure_reason("Declarative window XML has invalid base dimensions.", id);
        return 0;
    }

    g_parse_state.definition = std::make_unique<DeclarativeWindowDefinition>(id);
    g_parse_state.definition->set_base_size(base_width, base_height);
    g_parse_state.definition->set_block_layout(
        parse_optional_int("min_blocks_width", 40),
        parse_optional_int("min_blocks_height", 30),
        parse_optional_int("margin_x_blocks", 40),
        parse_optional_int("margin_y_blocks", 10));
    g_parse_state.definition->set_input_actions(
        parse_optional_string("escape_action"),
        parse_optional_string("load_file_action"));
    g_parse_state.saw_root = 1;
    return 1;
}

int parse_widget_node(void)
{
    if (!g_parse_state.definition) {
        return 0;
    }

    DeclarativeWidgetDefinition widget;
    widget.id = parse_optional_string("id");
    widget.type = parse_widget_type(xml_parser_get_attribute_string("type"));
    if (widget.id.empty() || widget.type == DeclarativeWidgetType::Unknown) {
        set_failure_reason("Declarative widget is missing a valid id or type.", g_parse_state.definition->id().c_str());
        return 0;
    }
    if (g_parse_state.definition->has_widget(widget.id)) {
        set_failure_reason("Declarative widget id is duplicated.", widget.id.c_str());
        return 0;
    }

    if (!parse_required_int("x", &widget.x) || !parse_required_int("y", &widget.y)) {
        set_failure_reason("Declarative widget is missing position.", widget.id.c_str());
        return 0;
    }

    widget.width = parse_optional_int("width", 0);
    widget.height = parse_optional_int("height", 0);
    widget.width_blocks = parse_optional_int("width_blocks", 0);
    widget.height_blocks = parse_optional_int("height_blocks", 0);
    widget.padding_x = parse_optional_int("padding_x", 0);
    widget.padding_y = parse_optional_int("padding_y", 0);
    widget.draw_offset_x = parse_optional_int("draw_offset_x", 0);
    widget.draw_offset_y = parse_optional_int("draw_offset_y", 0);
    widget.line_spacing = parse_optional_int("line_spacing", 5);
    widget.paragraph_spacing = parse_optional_int("paragraph_spacing", 0);
    widget.font_size_delta = parse_optional_int("font_size_delta", 0);
    widget.font = parse_font(xml_parser_get_attribute_string("font"), FONT_NORMAL_BLACK);
    widget.color = parse_color(xml_parser_get_attribute_string("color"), COLOR_MASK_NONE);
    widget.color_declared = xml_parser_has_attribute("color");
    widget.text = parse_optional_string("text");
    widget.translation = parse_optional_string("translation");
    widget.binding = parse_optional_string("binding");
    widget.action = parse_optional_string("action");
    widget.repeat_source = parse_optional_string("repeat_source");
    widget.visible_binding = parse_optional_string("visible_binding");
    widget.enabled_binding = parse_optional_string("enabled_binding");
    widget.selected_binding = parse_optional_string("selected_binding");
    widget.tooltip_binding = parse_optional_string("tooltip_binding");
    widget.width_from_text = parse_optional_string("width_from_text");
    widget.visible_if_side_margin_lt_text = parse_optional_string("visible_if_side_margin_lt_text");
    widget.stretch_to_widget = parse_optional_string("stretch_to_widget");
    widget.assetlist_name = parse_optional_string("assetlist");
    widget.image_name = parse_optional_string("image");
    widget.pressed_image_name = parse_optional_string("pressed_image");
    widget.image_collection = parse_optional_int("image_collection", 0);
    widget.image_offset = parse_optional_int("image_offset", 0);
    widget.label_type = parse_optional_int("label_type", 1);
    widget.stretch_margin_y = parse_optional_int("stretch_margin_y", widget.stretch_margin_y);
    widget.stretch_width = xml_parser_get_attribute_bool("stretch_width");
    widget.stretch_height = xml_parser_get_attribute_bool("stretch_height");
    widget.fullscreen = xml_parser_get_attribute_bool("fullscreen");
    widget.text_offset_x = parse_optional_int("text_offset_x", 0);
    widget.text_offset_y = parse_optional_int("text_offset_y", 0);
    widget.width_adjust = parse_optional_int("width_adjust", 0);
    widget.width_round_up_to = parse_optional_int("width_round_up_to", 0);
    widget.max_screen_height = parse_optional_int("max_screen_height", -1);
    widget.side_margin_text_padding = parse_optional_int("side_margin_text_padding", 0);
    widget.repeat_columns = parse_optional_int("repeat_columns", 1);
    widget.repeat_spacing_x = parse_optional_int("repeat_spacing_x", 0);
    widget.repeat_spacing_y = parse_optional_int("repeat_spacing_y", 0);
    widget.invert_visibility_condition = xml_parser_get_attribute_bool("invert_visibility_condition");
    widget.activate_on_press = xml_parser_get_attribute_bool("activate_on_press");
    widget.repeat_on_hold = xml_parser_get_attribute_bool("repeat_on_hold");
    widget.draw_phase = parse_draw_phase(xml_parser_get_attribute_string("phase"));
    widget.coordinate_space = parse_coordinate_space(xml_parser_get_attribute_string("coordinate_space"));
    widget.style = parse_widget_style(xml_parser_get_attribute_string("style"));
    widget.text_alignment = parse_text_alignment(xml_parser_get_attribute_string("text_alignment"));
    widget.visibility = parse_visibility(xml_parser_get_attribute_string("visible_when"));
    widget.anchor_x = parse_anchor(xml_parser_get_attribute_string("anchor_x"));
    widget.anchor_y = parse_anchor(xml_parser_get_attribute_string("anchor_y"));

    if (widget.repeat_columns < 1 || !attribute_is_one_of("phase", "background", "foreground") ||
        !attribute_is_one_of("coordinate_space", "dialog", "screen") ||
        !attribute_is_one_of("style", "outer_panel", "inner_panel", "solid", "label", "large_label", "image_small_border") ||
        !attribute_is_one_of("text_alignment", "left", "center", "right") ||
        !attribute_is_one_of("visible_when", "main_menu", "not_file_dialog")) {
        set_failure_reason("Declarative widget contains an unsupported main-menu attribute value.", widget.id.c_str());
        return 0;
    }

    if ((widget.type == DeclarativeWidgetType::Panel || widget.type == DeclarativeWidgetType::Label ||
        widget.type == DeclarativeWidgetType::RichText || widget.type == DeclarativeWidgetType::TextButton) &&
        widget.width <= 0 && widget.width_blocks <= 0 && widget.width_from_text.empty()) {
        set_failure_reason("Declarative widget is missing width.", widget.id.c_str());
        return 0;
    }
    if ((widget.type == DeclarativeWidgetType::Panel || widget.type == DeclarativeWidgetType::RichText ||
        widget.type == DeclarativeWidgetType::TextButton) &&
        widget.height <= 0 && widget.height_blocks <= 0) {
        set_failure_reason("Declarative widget is missing height.", widget.id.c_str());
        return 0;
    }

    g_parse_state.definition->add_widget(std::move(widget));
    return 1;
}

const xml_parser_element kXmlElements[] = {
    { "window", parse_window_node, nullptr, nullptr, nullptr },
    { "widget", parse_widget_node, nullptr, "window", nullptr }
};

int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    ScopedFile fp(file_open(filename, "rb"));
    if (!fp.get()) {
        set_failure_reason("Failed to load declarative window XML.", filename);
        return 0;
    }

    if (fseek(fp.get(), 0, SEEK_END) != 0) {
        set_failure_reason("Failed to load declarative window XML.", filename);
        return 0;
    }

    const long size = ftell(fp.get());
    if (size < 0) {
        set_failure_reason("Failed to load declarative window XML.", filename);
        return 0;
    }

    rewind(fp.get());
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp.get());
    if (read != buffer.size()) {
        set_failure_reason("Failed to load declarative window XML.", filename);
        return 0;
    }
    return 1;
}

int parse_definition_file(const char *filename)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }
    if (buffer.size() > std::numeric_limits<unsigned int>::max()) {
        set_failure_reason("Declarative window XML is too large to parse.", filename);
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(kXmlElements, static_cast<int>(sizeof(kXmlElements) / sizeof(kXmlElements[0])), 1)) {
        set_failure_reason("Failed to initialize declarative window XML parser.", filename);
        return 0;
    }

    const ErrorContextScope scope("Declarative UI XML", filename);
    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || !g_parse_state.definition) {
        error_context_report_error("Invalid declarative window XML.", filename);
        set_failure_reason("Failed to parse declarative window XML.", filename);
        return 0;
    }

    g_windows[g_parse_state.definition->id()] = std::move(g_parse_state.definition);
    return 1;
}

int validate_required_widget(const DeclarativeWindowDefinition &definition, const char *id, DeclarativeWidgetType type)
{
    const DeclarativeWidgetDefinition *widget = definition.widget(id);
    if (widget && widget->type == type) {
        return 1;
    }

    char detail[512];
    snprintf(detail, sizeof(detail), "window=%s widget=%s", definition.id().c_str(), id);
    set_failure_reason("Declarative window XML is missing a required widget or widget type.", detail);
    error_context_report_error("Declarative window XML is missing a required widget or widget type.", detail);
    return 0;
}

int validate_mission_briefing()
{
    const DeclarativeWindowDefinition *definition = declarative_window_definition(kMissionBriefingWindowId);
    if (!definition) {
        set_failure_reason("Required declarative window was not loaded.", kMissionBriefingWindowId);
        return 0;
    }

    struct RequiredWidget {
        const char *id;
        DeclarativeWidgetType type;
    };

    static constexpr std::array<RequiredWidget, 18> kRequiredWidgets = { {
        { "outer_panel", DeclarativeWidgetType::Panel },
        { "title", DeclarativeWidgetType::Label },
        { "subtitle", DeclarativeWidgetType::Label },
        { "objectives_panel", DeclarativeWidgetType::Panel },
        { "objectives_header", DeclarativeWidgetType::Label },
        { "objective_0", DeclarativeWidgetType::Label },
        { "objective_1", DeclarativeWidgetType::Label },
        { "objective_2", DeclarativeWidgetType::Label },
        { "objective_3", DeclarativeWidgetType::Label },
        { "objective_4", DeclarativeWidgetType::Label },
        { "immediate_goal", DeclarativeWidgetType::Label },
        { "body_panel", DeclarativeWidgetType::Panel },
        { "body", DeclarativeWidgetType::RichText },
        { "scrollbar", DeclarativeWidgetType::Scrollbar },
        { "button_cancel", DeclarativeWidgetType::ImageButton },
        { "button_cancel_text", DeclarativeWidgetType::Label },
        { "button_to_city_text", DeclarativeWidgetType::Label },
        { "button_to_city", DeclarativeWidgetType::ImageButton }
    } };

    for (const RequiredWidget &widget : kRequiredWidgets) {
        if (!validate_required_widget(*definition, widget.id, widget.type)) {
            return 0;
        }
    }
    return 1;
}

int validate_main_menu()
{
    const DeclarativeWindowDefinition *definition = declarative_window_definition(kMainMenuWindowId);
    if (!definition) {
        set_failure_reason("Required declarative window was not loaded.", kMainMenuWindowId);
        return 0;
    }
    if ((!definition->escape_action().empty() &&
            !window_main_menu_action_is_supported(definition->escape_action())) ||
        (!definition->load_file_action().empty() &&
            !window_main_menu_action_is_supported(definition->load_file_action()))) {
        set_failure_reason("Main menu declarative window uses an unsupported input action.", kMainMenuWindowId);
        return 0;
    }

    for (const DeclarativeWidgetDefinition &widget : definition->widgets()) {
        if (widget.type != DeclarativeWidgetType::Image &&
            widget.type != DeclarativeWidgetType::Panel &&
            widget.type != DeclarativeWidgetType::Label &&
            widget.type != DeclarativeWidgetType::TextButton &&
            widget.type != DeclarativeWidgetType::ImageButton) {
            set_failure_reason("Main menu contains a widget type that its interpreter does not support.", widget.id.c_str());
            return 0;
        }
        if (widget.type == DeclarativeWidgetType::Image || widget.type == DeclarativeWidgetType::ImageButton) {
            const int has_named_image = !widget.assetlist_name.empty() && !widget.image_name.empty();
            const int has_legacy_image = widget.image_collection > 0;
            if (has_named_image == has_legacy_image) {
                set_failure_reason("Main menu image must declare exactly one image source.", widget.id.c_str());
                return 0;
            }
            if (has_named_image &&
                !ImageGroupEntryRef::from_group(widget.assetlist_name, widget.image_name).runtime_slice().is_valid()) {
                set_failure_reason("Main menu image source does not resolve to a loaded asset.", widget.id.c_str());
                return 0;
            }
            if (has_legacy_image) {
                if (widget.image_collection >= IMAGE_MAX_GROUPS) {
                    set_failure_reason("Main menu legacy image group is outside the supported range.", widget.id.c_str());
                    return 0;
                }
                const image *legacy_image = image_get(image_group(widget.image_collection) + widget.image_offset);
                if (!legacy_image || legacy_image->width <= 0 || legacy_image->height <= 0) {
                    set_failure_reason("Main menu legacy image source does not resolve to a loaded image.", widget.id.c_str());
                    return 0;
                }
            }
        }
        if (widget.type == DeclarativeWidgetType::Panel && widget.style == DeclarativeWidgetStyle::None) {
            set_failure_reason("Main menu panel is missing a style.", widget.id.c_str());
            return 0;
        }
        if (widget.style == DeclarativeWidgetStyle::Solid && !widget.color_declared) {
            set_failure_reason("Main menu solid widget is missing a color.", widget.id.c_str());
            return 0;
        }
        if (widget.type == DeclarativeWidgetType::TextButton || widget.type == DeclarativeWidgetType::ImageButton) {
            if (widget.action.empty()) {
                set_failure_reason("Main menu button is missing an action.", widget.id.c_str());
                return 0;
            }
            if (widget.type == DeclarativeWidgetType::TextButton &&
                (widget.style == DeclarativeWidgetStyle::None ||
                    (widget.translation.empty() && widget.text.empty() && widget.binding.empty()))) {
                set_failure_reason("Main menu text button is missing a style or text.", widget.id.c_str());
                return 0;
            }
            if (widget.type == DeclarativeWidgetType::ImageButton &&
                (widget.width <= 0 || widget.height <= 0)) {
                set_failure_reason("Main menu image button is missing hit-test dimensions.", widget.id.c_str());
                return 0;
            }
            if (!window_main_menu_action_is_supported(widget.action)) {
                set_failure_reason("Main menu text button uses an unsupported action.", widget.id.c_str());
                return 0;
            }
        }
        if (widget.type == DeclarativeWidgetType::Label && widget.translation.empty() &&
            widget.text.empty() && widget.binding.empty()) {
            set_failure_reason("Main menu label is missing text or a binding.", widget.id.c_str());
            return 0;
        }
        if (!widget.binding.empty() && widget.binding != "system.version") {
            set_failure_reason("Main menu widget uses an unsupported binding.", widget.id.c_str());
            return 0;
        }
        if (!widget.width_from_text.empty() && !definition->has_widget(widget.width_from_text)) {
            set_failure_reason("Main menu widget sizes itself from an unknown text widget.", widget.id.c_str());
            return 0;
        }
        if (!widget.visible_if_side_margin_lt_text.empty() &&
            !definition->has_widget(widget.visible_if_side_margin_lt_text)) {
            set_failure_reason("Main menu widget visibility references an unknown text widget.", widget.id.c_str());
            return 0;
        }
        if (widget.invert_visibility_condition && widget.max_screen_height < 0 &&
            widget.visible_if_side_margin_lt_text.empty()) {
            set_failure_reason("Main menu widget inverts an empty visibility condition.", widget.id.c_str());
            return 0;
        }
    }
    return 1;
}

} // namespace

int DeclarativeWidgetDefinition::resolved_x(int window_width, int base_width) const
{
    const int resolved = resolved_width(window_width, base_width);
    const int base = width > 0 ? width : width_blocks * BLOCK_SIZE;
    if (anchor_x == DeclarativeAnchor::Far) {
        return window_width - (base_width - x - base) - resolved;
    }
    return x;
}

int DeclarativeWidgetDefinition::resolved_y(int window_height, int base_height) const
{
    const int resolved = resolved_height(window_height, base_height);
    const int base = height > 0 ? height : height_blocks * BLOCK_SIZE;
    return anchor_y == DeclarativeAnchor::Far ? window_height - (base_height - y - base) - resolved : y;
}

int DeclarativeWidgetDefinition::resolved_width(int window_width, int base_width) const
{
    const int base = width > 0 ? width : width_blocks * BLOCK_SIZE;
    if (!stretch_width) {
        return base;
    }
    const int right_offset = base_width - x - base;
    const int resolved = window_width - x - right_offset;
    return resolved > 0 ? resolved : base;
}

int DeclarativeWidgetDefinition::resolved_height(int window_height, int base_height) const
{
    const int base = height > 0 ? height : height_blocks * BLOCK_SIZE;
    if (!stretch_height) {
        return base;
    }
    const int bottom_offset = base_height - y - base;
    const int resolved = window_height - y - bottom_offset;
    return resolved > 0 ? resolved : base;
}

int DeclarativeWidgetDefinition::resolved_width_blocks(int window_width, int base_width) const
{
    if (width_blocks > 0 && !stretch_width) {
        return width_blocks;
    }
    return (resolved_width(window_width, base_width) + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

int DeclarativeWidgetDefinition::resolved_height_blocks(int window_height, int base_height) const
{
    if (height_blocks > 0 && !stretch_height) {
        return height_blocks;
    }
    return (resolved_height(window_height, base_height) + BLOCK_SIZE - 1) / BLOCK_SIZE;
}

DeclarativeWindowDefinition::DeclarativeWindowDefinition(std::string id)
    : id_(std::move(id))
{
}

const std::string &DeclarativeWindowDefinition::id() const
{
    return id_;
}

int DeclarativeWindowDefinition::base_width() const
{
    return base_width_;
}

int DeclarativeWindowDefinition::base_height() const
{
    return base_height_;
}

int DeclarativeWindowDefinition::min_blocks_width() const
{
    return min_blocks_width_;
}

int DeclarativeWindowDefinition::min_blocks_height() const
{
    return min_blocks_height_;
}

int DeclarativeWindowDefinition::margin_x_blocks() const
{
    return margin_x_blocks_;
}

int DeclarativeWindowDefinition::margin_y_blocks() const
{
    return margin_y_blocks_;
}

const std::string &DeclarativeWindowDefinition::escape_action() const
{
    return escape_action_;
}

const std::string &DeclarativeWindowDefinition::load_file_action() const
{
    return load_file_action_;
}

void DeclarativeWindowDefinition::set_base_size(int width, int height)
{
    base_width_ = width;
    base_height_ = height;
}

void DeclarativeWindowDefinition::set_block_layout(int min_width, int min_height, int margin_x, int margin_y)
{
    min_blocks_width_ = min_width;
    min_blocks_height_ = min_height;
    margin_x_blocks_ = margin_x;
    margin_y_blocks_ = margin_y;
}

void DeclarativeWindowDefinition::set_input_actions(std::string escape_action, std::string load_file_action)
{
    escape_action_ = std::move(escape_action);
    load_file_action_ = std::move(load_file_action);
}

void DeclarativeWindowDefinition::add_widget(DeclarativeWidgetDefinition widget)
{
    widget_indices_[widget.id] = widgets_.size();
    widgets_.push_back(std::move(widget));
}

const DeclarativeWidgetDefinition *DeclarativeWindowDefinition::widget(std::string_view id) const
{
    const auto found = widget_indices_.find(std::string(id));
    return found != widget_indices_.end() ? &widgets_[found->second] : nullptr;
}

const std::vector<DeclarativeWidgetDefinition> &DeclarativeWindowDefinition::widgets() const
{
    return widgets_;
}

int DeclarativeWindowDefinition::has_widget(std::string_view id) const
{
    return widget(id) != nullptr;
}

DeclarativeWindow::DeclarativeWindow(const DeclarativeWindowDefinition &definition)
    : definition_(&definition)
{
}

const DeclarativeWindowDefinition &DeclarativeWindow::definition() const
{
    return *definition_;
}

const DeclarativeWidgetDefinition *DeclarativeWindow::widget(std::string_view id) const
{
    return definition_ ? definition_->widget(id) : nullptr;
}

int DeclarativeWindow::has_widget(std::string_view id) const
{
    return widget(id) != nullptr;
}

int validate_race_bet()
{
    const DeclarativeWindowDefinition *definition = declarative_window_definition("race_bet");
    if (!definition) return 1;
    static constexpr const char *kBindings[] = {
        "race.team.portrait", "race.selected_description", "race.wager", "race.savings", "race.confirm_label"
    };
    static constexpr const char *kConditions[] = { "race.can_edit", "race.can_confirm", "race.team.selected" };
    static constexpr const char *kActions[] = {
        "race.team.select", "race.wager.decrease", "race.wager.increase", "race.confirm", "window.close"
    };
    const auto contains = [](std::string_view value, const auto &values) {
        for (const char *candidate : values) if (value == candidate) return true;
        return false;
    };
    for (const DeclarativeWidgetDefinition &widget : definition->widgets()) {
        if ((!widget.binding.empty() && !contains(widget.binding, kBindings)) ||
            (!widget.visible_binding.empty() && !contains(widget.visible_binding, kConditions)) ||
            (!widget.enabled_binding.empty() && !contains(widget.enabled_binding, kConditions)) ||
            (!widget.selected_binding.empty() && !contains(widget.selected_binding, kConditions)) ||
            (!widget.action.empty() && !contains(widget.action, kActions)) ||
            (!widget.repeat_source.empty() && widget.repeat_source != "race.teams") ||
            (!widget.tooltip_binding.empty() && widget.tooltip_binding != "race.team.tooltip")) {
            set_failure_reason("Race betting window contains an unsupported binding, condition, action, repeater, or tooltip.",
                widget.id.c_str());
            return 0;
        }
    }
    static constexpr std::array<const char *, 6> kRequired = {
        "outer_panel", "team", "team_description", "amount", "confirm", "close"
    };
    for (const char *id : kRequired) {
        if (!definition->has_widget(id)) {
            set_failure_reason("Race betting window is missing a required widget.", id);
            return 0;
        }
    }
    const auto widget_matches = [&](const char *id, DeclarativeWidgetType type, int x, int y, int width, int height) {
        const DeclarativeWidgetDefinition *widget = definition->widget(id);
        return widget && widget->type == type && widget->x == x && widget->y == y &&
            widget->width == width && widget->height == height;
    };
    if (definition->base_width() != 480 || definition->base_height() != 400 ||
        !widget_matches("outer_panel", DeclarativeWidgetType::Panel, 0, 0, 480, 400) ||
        !widget_matches("title", DeclarativeWidgetType::Label, 0, 20, 480, 24) ||
        !widget_matches("description", DeclarativeWidgetType::RichText, 25, 65, 438, 52) ||
        !widget_matches("team", DeclarativeWidgetType::ImageButton, 34, 145, 81, 91) ||
        !widget_matches("team_description", DeclarativeWidgetType::RichText, 25, 250, 438, 42) ||
        !widget_matches("amount_panel", DeclarativeWidgetType::Panel, 18, 300, 448, 32) ||
        !widget_matches("amount_label", DeclarativeWidgetType::Label, 18, 310, 80, 16) ||
        !widget_matches("amount_down", DeclarativeWidgetType::ImageButton, 106, 306, 24, 24) ||
        !widget_matches("amount_up", DeclarativeWidgetType::ImageButton, 130, 306, 24, 24) ||
        !widget_matches("amount", DeclarativeWidgetType::Label, 165, 310, 92, 16) ||
        !widget_matches("savings", DeclarativeWidgetType::Label, 284, 310, 175, 16) ||
        !widget_matches("confirm", DeclarativeWidgetType::TextButton, 90, 354, 300, 20) ||
        !widget_matches("close", DeclarativeWidgetType::ImageButton, 424, 354, 24, 24)) {
        set_failure_reason("Race betting window no longer matches the established layout contract.", definition->id().c_str());
        return 0;
    }
    const DeclarativeWidgetDefinition *team = definition->widget("team");
    const DeclarativeWidgetDefinition *down = definition->widget("amount_down");
    const DeclarativeWidgetDefinition *up = definition->widget("amount_up");
    const DeclarativeWidgetDefinition *confirm = definition->widget("confirm");
    const DeclarativeWidgetDefinition *close = definition->widget("close");
    if (team->draw_offset_x != 5 || team->draw_offset_y != 5 || team->repeat_columns != 4 ||
        team->repeat_spacing_x != 110 || team->style != DeclarativeWidgetStyle::ImageSmallBorder ||
        down->assetlist_name != "UI\\Arrow_Button" || down->image_name != "Decrease" ||
        down->pressed_image_name != "Decrease_Pressed" || !down->activate_on_press || !down->repeat_on_hold ||
        up->assetlist_name != "UI\\Arrow_Button" || up->image_name != "Increase" ||
        up->pressed_image_name != "Increase_Pressed" || !up->activate_on_press || !up->repeat_on_hold ||
        confirm->text_offset_y != 4 || close->assetlist_name != "UI\\Context_Icons" ||
        close->image_name != "Image_0004" || close->pressed_image_name != "Image_0005") {
        set_failure_reason("Race betting window controls no longer match the established presentation contract.", definition->id().c_str());
        return 0;
    }
    return 1;
}

int DeclarativeWindowController::repeat_count(std::string_view source) const
{
    return source.empty() ? 1 : 0;
}

std::string DeclarativeWindowController::text(std::string_view binding, int item_index) const
{
    (void)binding;
    (void)item_index;
    return {};
}

ImageGroupEntryRef DeclarativeWindowController::image(std::string_view binding, int item_index) const
{
    (void)binding;
    (void)item_index;
    return {};
}

int DeclarativeWindowController::condition(std::string_view binding, int item_index) const
{
    (void)binding;
    (void)item_index;
    return 0;
}

const char *DeclarativeWindowController::tooltip(std::string_view binding, int item_index) const
{
    (void)binding;
    (void)item_index;
    return nullptr;
}

DeclarativeWindowRuntime::DeclarativeWindowRuntime(const DeclarativeWindowDefinition &definition, DeclarativeWindowController &controller)
    : definition_(&definition), controller_(&controller)
{}

static void declarative_widget_bounds(const DeclarativeWindowDefinition &window,
    const DeclarativeWidgetDefinition &widget, int item, int width, int height,
    int *x, int *y, int *widget_width, int *widget_height)
{
    const int columns = std::max(1, widget.repeat_columns);
    *x = widget.resolved_x(width, window.base_width()) + (item >= 0 ? (item % columns) * widget.repeat_spacing_x : 0);
    *y = widget.resolved_y(height, window.base_height()) + (item >= 0 ? (item / columns) * widget.repeat_spacing_y : 0);
    *widget_width = widget.resolved_width(width, window.base_width());
    *widget_height = widget.resolved_height(height, window.base_height());
}

void DeclarativeWindowRuntime::draw(DeclarativeDrawPhase phase, int width, int height) const
{
    if (!definition_ || !controller_) return;
    for (const DeclarativeWidgetDefinition &widget : definition_->widgets()) {
        if (widget.draw_phase != phase) continue;
        const int count = widget.repeat_source.empty() ? 1 : controller_->repeat_count(widget.repeat_source);
        for (int index = 0; index < count; ++index) {
            const int item = widget.repeat_source.empty() ? -1 : index;
            if (!widget.visible_binding.empty() && !controller_->condition(widget.visible_binding, item)) continue;
            int x = 0;
            int y = 0;
            int widget_width = 0;
            int widget_height = 0;
            declarative_widget_bounds(*definition_, widget, item, width, height, &x, &y, &widget_width, &widget_height);
            const int enabled = widget.enabled_binding.empty() || controller_->condition(widget.enabled_binding, item);
            const int selected = !widget.selected_binding.empty() && controller_->condition(widget.selected_binding, item);
            const int focused = focused_widget_ == widget.id && focused_item_ == item;
            const int pressed = pressed_widget_ == widget.id && pressed_item_ == item;
            if (widget.type == DeclarativeWidgetType::Panel) {
                if (widget.style == DeclarativeWidgetStyle::OuterPanel) {
                    outer_panel_draw(x, y, (widget_width + BLOCK_SIZE - 1) / BLOCK_SIZE,
                        (widget_height + BLOCK_SIZE - 1) / BLOCK_SIZE);
                } else if (widget.style == DeclarativeWidgetStyle::InnerPanel) {
                    inner_panel_draw(x, y, (widget_width + BLOCK_SIZE - 1) / BLOCK_SIZE,
                        (widget_height + BLOCK_SIZE - 1) / BLOCK_SIZE);
                }
                if (selected) button_border_draw(x, y, widget_width, widget_height, 1);
                continue;
            }
            if (widget.type == DeclarativeWidgetType::Image || widget.type == DeclarativeWidgetType::ImageButton) {
                const std::string &image_name = pressed && !widget.pressed_image_name.empty() ?
                    widget.pressed_image_name : widget.image_name;
                ImageGroupEntryRef image = !widget.binding.empty() ? controller_->image(widget.binding, item) :
                    ImageGroupEntryRef::from_group(widget.assetlist_name, image_name);
                if (image.is_bound()) image.draw(x + widget.draw_offset_x, y + widget.draw_offset_y);
                if (widget.type == DeclarativeWidgetType::ImageButton && widget.style == DeclarativeWidgetStyle::ImageSmallBorder) {
                    ImageBorder::image_small().draw(x, y, selected || focused ? COLOR_BORDER_RED : COLOR_BORDER_GREEN);
                } else if (widget.type == DeclarativeWidgetType::ImageButton && selected) {
                    button_border_draw(x, y, widget_width, widget_height, 1);
                }
                continue;
            }
            std::string dynamic_text = widget.binding.empty() ? std::string() : controller_->text(widget.binding, item);
            const uint8_t *display = nullptr;
            if (!dynamic_text.empty()) display = reinterpret_cast<const uint8_t *>(dynamic_text.c_str());
            else if (!widget.translation.empty()) display = translation_for_key(widget.translation.c_str());
            else display = reinterpret_cast<const uint8_t *>(widget.text.c_str());
            const font_t font = enabled ? widget.font : FONT_NORMAL_PLAIN;
            if (widget.type == DeclarativeWidgetType::RichText) {
                text_draw_multiline(display, x + widget.padding_x, y + widget.padding_y, widget_width, 0, font,
                    screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
            } else if (widget.type == DeclarativeWidgetType::Label || widget.type == DeclarativeWidgetType::TextButton) {
                if (widget.type == DeclarativeWidgetType::TextButton) {
                    button_border_draw(x, y, widget_width, widget_height, enabled && (selected || focused));
                }
                if (widget.text_alignment == DeclarativeTextAlignment::Center || widget.type == DeclarativeWidgetType::TextButton) {
                    text_draw_centered(display, x + widget.text_offset_x, y + widget.text_offset_y, widget_width, font,
                        screen_ui_to_pixel(font_definition_for(font)->line_height), enabled ? 0 : COLOR_FONT_LIGHT_GRAY);
                } else if (widget.text_alignment == DeclarativeTextAlignment::Right) {
                    text_draw_right_aligned(display, x + widget.padding_x, y + widget.padding_y, widget_width, font,
                        screen_ui_to_pixel(font_definition_for(font)->line_height), enabled ? 0 : COLOR_FONT_LIGHT_GRAY);
                } else {
                    text_draw(display, x + widget.padding_x, y + widget.padding_y, font,
                        screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
                }
            }
        }
    }
}

int DeclarativeWindowRuntime::handle_mouse(const mouse &mouse, int width, int height)
{
    focused_widget_.clear();
    focused_item_ = -1;
    if (!definition_ || !controller_) return 0;
    for (const DeclarativeWidgetDefinition &widget : definition_->widgets()) {
        if (widget.action.empty() || (widget.type != DeclarativeWidgetType::TextButton &&
            widget.type != DeclarativeWidgetType::ImageButton)) continue;
        const int count = widget.repeat_source.empty() ? 1 : controller_->repeat_count(widget.repeat_source);
        for (int index = 0; index < count; ++index) {
            const int item = widget.repeat_source.empty() ? -1 : index;
            if ((!widget.visible_binding.empty() && !controller_->condition(widget.visible_binding, item)) ||
                (!widget.enabled_binding.empty() && !controller_->condition(widget.enabled_binding, item))) continue;
            int x = 0;
            int y = 0;
            int widget_width = 0;
            int widget_height = 0;
            declarative_widget_bounds(*definition_, widget, item, width, height, &x, &y, &widget_width, &widget_height);
            if (mouse.x < x || mouse.y < y || mouse.x >= x + widget_width || mouse.y >= y + widget_height) continue;
            focused_widget_ = widget.id;
            focused_item_ = item;
            if (mouse.left.went_down) {
                pressed_widget_ = widget.id;
                pressed_item_ = item;
                pressed_repeat_count_ = 0;
                last_repeat_time_ = time_get_millis();
                if (widget.activate_on_press) controller_->action(widget.action, item);
                return 1;
            }
            if (mouse.left.is_down && pressed_widget_ == widget.id && pressed_item_ == item) {
                if (widget.repeat_on_hold && time_get_millis() - last_repeat_time_ >= 30) {
                    static constexpr int kRepeatPattern[] = {
                        0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0,
                        0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
                        1, 0, 1, 0, 1, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0
                    };
                    last_repeat_time_ = time_get_millis();
                    pressed_repeat_count_ = std::min(pressed_repeat_count_ + 1,
                        static_cast<int>(std::size(kRepeatPattern)) - 1);
                    if (kRepeatPattern[pressed_repeat_count_]) controller_->action(widget.action, item);
                }
                return 1;
            }
            if (mouse.left.went_up) {
                if (!widget.activate_on_press && pressed_widget_ == widget.id && pressed_item_ == item) {
                    controller_->action(widget.action, item);
                }
                pressed_widget_.clear();
                pressed_item_ = -1;
                return 1;
            }
            return 0;
        }
    }
    if (mouse.left.went_up) {
        pressed_widget_.clear();
        pressed_item_ = -1;
    }
    return 0;
}

void DeclarativeWindowRuntime::tooltip(tooltip_context &context) const
{
    if (!definition_ || !controller_ || focused_widget_.empty()) return;
    const DeclarativeWidgetDefinition *widget = definition_->widget(focused_widget_);
    if (!widget || widget->tooltip_binding.empty()) return;
    const char *value = controller_->tooltip(widget->tooltip_binding, focused_item_);
    if (value && *value) {
        context.type = TOOLTIP_BUTTON;
        context.translation_key = value;
    }
}

int declarative_window_registry_load(void)
{
    g_windows.clear();
    g_constructed_windows.clear();
    g_failure_reason.clear();

    // UI documents inherit as complete files. Enumerate the whole category so
    // a mod can add a new window without adding its path to native code; an
    // upper layer replaces only the same registry-relative document.
    std::map<std::string, mod_definition::DefinitionSource> winners;
    if (!mod_definition::for_each_configured_definition_file(
            { "UI\\windows" }, "declarative window", true,
            [&](const mod_definition::DefinitionSource &source) {
                winners[source.normalized_definition_path] = source;
                return true;
            }, nullptr, &g_failure_reason)) return 0;
    for (const auto &winner : winners) {
        if (!parse_definition_file(winner.second.full_path.c_str())) return 0;
    }
    if (!validate_mission_briefing()) {
        return 0;
    }
    if (!validate_main_menu()) {
        return 0;
    }
    if (!validate_race_bet()) return 0;

    for (const auto &entry : g_windows) {
        g_constructed_windows[entry.first] = std::make_unique<DeclarativeWindow>(*entry.second);
    }
    return 1;
}

const char *declarative_window_registry_get_failure_reason(void)
{
    return g_failure_reason.c_str();
}

const DeclarativeWindowDefinition *declarative_window_definition(std::string_view id)
{
    const auto found = g_windows.find(std::string(id));
    return found != g_windows.end() ? found->second.get() : nullptr;
}

const DeclarativeWindow *declarative_window(std::string_view id)
{
    const auto found = g_constructed_windows.find(std::string(id));
    return found != g_constructed_windows.end() ? found->second.get() : nullptr;
}
