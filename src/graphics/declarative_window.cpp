#

#include "graphics/declarative_window.h"

#include "core/crash_context.h"
#include "core/xml_value.h"

#include <array>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/file.h"
extern "C" {
#include "core/xml_parser.h"
#include "game/mod_manager.h"
}

namespace {

constexpr const char *kMissionBriefingWindowId = "mission_briefing";
constexpr const char *kMissionBriefingPath = "UI/windows/mission_briefing.xml";

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
    widget.binding = parse_optional_string("binding");
    widget.action = parse_optional_string("action");
    widget.stretch_to_widget = parse_optional_string("stretch_to_widget");
    widget.assetlist_name = parse_optional_string("assetlist");
    widget.image_name = parse_optional_string("image");
    widget.image_collection = parse_optional_int("image_collection", 0);
    widget.image_offset = parse_optional_int("image_offset", 0);
    widget.label_type = parse_optional_int("label_type", 1);
    widget.stretch_margin_y = parse_optional_int("stretch_margin_y", widget.stretch_margin_y);
    widget.stretch_width = xml_parser_get_attribute_bool("stretch_width");
    widget.stretch_height = xml_parser_get_attribute_bool("stretch_height");
    widget.anchor_x = parse_anchor(xml_parser_get_attribute_string("anchor_x"));
    widget.anchor_y = parse_anchor(xml_parser_get_attribute_string("anchor_y"));

    if ((widget.type == DeclarativeWidgetType::Panel || widget.type == DeclarativeWidgetType::Label ||
        widget.type == DeclarativeWidgetType::RichText || widget.type == DeclarativeWidgetType::TextButton) &&
        widget.width <= 0 && widget.width_blocks <= 0) {
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

} // namespace

int DeclarativeWidgetDefinition::resolved_x(int window_width, int base_width) const
{
    const int resolved = resolved_width(window_width, base_width);
    if (anchor_x == DeclarativeAnchor::Far) {
        return window_width - (base_width - x - width) - resolved;
    }
    return x;
}

int DeclarativeWidgetDefinition::resolved_y(int window_height, int base_height) const
{
    const int resolved = resolved_height(window_height, base_height);
    return anchor_y == DeclarativeAnchor::Far ? window_height - (base_height - y - height) - resolved : y;
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

int declarative_window_registry_load(void)
{
    g_windows.clear();
    g_constructed_windows.clear();
    g_failure_reason.clear();

    char filename[FILE_NAME_MAX];
    snprintf(filename, sizeof(filename), "%s%s", mod_manager_get_mod_path(), kMissionBriefingPath);
    if (!parse_definition_file(filename)) {
        return 0;
    }
    if (!validate_mission_briefing()) {
        return 0;
    }

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
