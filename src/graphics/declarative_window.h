#pragma once

#include "graphics/color.h"
#include "graphics/font.h"
#include "graphics/ui_constants.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

enum class DeclarativeWidgetType {
    Unknown,
    Panel,
    Image,
    Label,
    RichText,
    Scrollbar,
    TextButton,
    ImageButton,
};

enum class DeclarativeDrawPhase {
    Background,
    Foreground,
};

enum class DeclarativeCoordinateSpace {
    Dialog,
    Screen,
};

enum class DeclarativeWidgetStyle {
    None,
    OuterPanel,
    InnerPanel,
    Solid,
    Label,
    LargeLabel,
};

enum class DeclarativeTextAlignment {
    Left,
    Center,
    Right,
};

enum class DeclarativeVisibility {
    Always,
    MainMenu,
    NotFileDialog,
};

enum class DeclarativeAnchor {
    Near,
    Far,
};

struct DeclarativeWidgetDefinition {
    std::string id;
    DeclarativeWidgetType type = DeclarativeWidgetType::Unknown;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int width_blocks = 0;
    int height_blocks = 0;
    int padding_x = 0;
    int padding_y = 0;
    int draw_offset_x = 0;
    int draw_offset_y = 0;
    int line_spacing = 5;
    int paragraph_spacing = 0;
    int font_size_delta = 0;
    font_t font = FONT_NORMAL_BLACK;
    color_t color = COLOR_MASK_NONE;
    std::string text;
    std::string translation;
    std::string binding;
    std::string action;
    std::string width_from_text;
    std::string visible_if_side_margin_lt_text;
    std::string stretch_to_widget;
    std::string assetlist_name;
    std::string image_name;
    int image_collection = 0;
    int image_offset = 0;
    int label_type = 1;
    int stretch_margin_y = 18;
    int stretch_width = 0;
    int stretch_height = 0;
    int fullscreen = 0;
    int text_offset_x = 0;
    int text_offset_y = 0;
    int width_adjust = 0;
    int width_round_up_to = 0;
    int max_screen_height = -1;
    int side_margin_text_padding = 0;
    int invert_visibility_condition = 0;
    int color_declared = 0;
    DeclarativeDrawPhase draw_phase = DeclarativeDrawPhase::Foreground;
    DeclarativeCoordinateSpace coordinate_space = DeclarativeCoordinateSpace::Dialog;
    DeclarativeWidgetStyle style = DeclarativeWidgetStyle::None;
    DeclarativeTextAlignment text_alignment = DeclarativeTextAlignment::Left;
    DeclarativeVisibility visibility = DeclarativeVisibility::Always;
    DeclarativeAnchor anchor_x = DeclarativeAnchor::Near;
    DeclarativeAnchor anchor_y = DeclarativeAnchor::Near;

    int resolved_x(int window_width, int base_width) const;
    int resolved_y(int window_height, int base_height) const;
    int resolved_width(int window_width, int base_width) const;
    int resolved_height(int window_height, int base_height) const;
    int resolved_width_blocks(int window_width, int base_width) const;
    int resolved_height_blocks(int window_height, int base_height) const;
};

class DeclarativeWindowDefinition {
public:
    explicit DeclarativeWindowDefinition(std::string id = {});

    const std::string &id() const;
    int base_width() const;
    int base_height() const;
    int min_blocks_width() const;
    int min_blocks_height() const;
    int margin_x_blocks() const;
    int margin_y_blocks() const;
    const std::string &escape_action() const;
    const std::string &load_file_action() const;

    void set_base_size(int width, int height);
    void set_block_layout(int min_width, int min_height, int margin_x, int margin_y);
    void set_input_actions(std::string escape_action, std::string load_file_action);
    void add_widget(DeclarativeWidgetDefinition widget);
    const DeclarativeWidgetDefinition *widget(std::string_view id) const;
    const std::vector<DeclarativeWidgetDefinition> &widgets() const;
    int has_widget(std::string_view id) const;

private:
    std::string id_;
    int base_width_ = 640;
    int base_height_ = 480;
    int min_blocks_width_ = 40;
    int min_blocks_height_ = 30;
    int margin_x_blocks_ = 40;
    int margin_y_blocks_ = 10;
    std::string escape_action_;
    std::string load_file_action_;
    std::vector<DeclarativeWidgetDefinition> widgets_;
    std::unordered_map<std::string, size_t> widget_indices_;
};

class DeclarativeWindow {
public:
    explicit DeclarativeWindow(const DeclarativeWindowDefinition &definition);

    const DeclarativeWindowDefinition &definition() const;
    const DeclarativeWidgetDefinition *widget(std::string_view id) const;
    int has_widget(std::string_view id) const;

private:
    const DeclarativeWindowDefinition *definition_ = nullptr;
};

int declarative_window_registry_load(void);
const char *declarative_window_registry_get_failure_reason(void);
const DeclarativeWindowDefinition *declarative_window_definition(std::string_view id);
const DeclarativeWindow *declarative_window(std::string_view id);
