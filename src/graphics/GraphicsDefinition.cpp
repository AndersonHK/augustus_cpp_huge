#include "graphics/GraphicsDefinition.h"

#include <cstdio>

int GraphicsOrientation::normalized_quarter_turns() const
{
    int result = quarter_turns % 4;
    return result < 0 ? result + 4 : result;
}

int GraphicsPoint::is_zero() const
{
    return x == 0 && y == 0;
}

GraphicsPoint GraphicsPoint::translated(int dx, int dy) const
{
    return { x + dx, y + dy };
}

int GraphicsAnimationFrame::clamped_frame() const
{
    if (frame_count <= 0) {
        return 0;
    }
    if (frame < 1) {
        return 1;
    }
    return frame > frame_count ? frame_count : frame;
}

const char *graphics_definition_kind_name(GraphicsDefinitionKind kind)
{
    switch (kind) {
        case GraphicsDefinitionKind::Building:
            return "building";
        case GraphicsDefinitionKind::Figure:
            return "figure";
        case GraphicsDefinitionKind::Resource:
            return "resource";
        case GraphicsDefinitionKind::Base:
        default:
            return "base";
    }
}

const char *graphics_target_role_name(GraphicsTargetRole role)
{
    switch (role) {
        case GraphicsTargetRole::Action:
            return "action";
        case GraphicsTargetRole::Corpse:
            return "corpse";
        case GraphicsTargetRole::Overlay:
            return "overlay";
        case GraphicsTargetRole::Icon:
            return "icon";
        case GraphicsTargetRole::ResourceStorage:
            return "resource_storage";
        case GraphicsTargetRole::ResourceCart:
            return "resource_cart";
        case GraphicsTargetRole::Default:
        default:
            return "default";
    }
}

int graphics_normalize_direction8(int direction)
{
    direction %= GRAPHICS_DIRECTION8_COUNT;
    return direction < 0 ? direction + GRAPHICS_DIRECTION8_COUNT : direction;
}

const char *graphics_direction8_suffix(int direction)
{
    static constexpr const char *suffixes[GRAPHICS_DIRECTION8_COUNT] = {
        "ne",
        "e",
        "se",
        "s",
        "sw",
        "w",
        "nw",
        "n"
    };
    return suffixes[graphics_normalize_direction8(direction)];
}

std::string graphics_uppercase_pattern_token(std::string text)
{
    for (char &ch : text) {
        if (ch >= 'a' && ch <= 'z') {
            ch = static_cast<char>(ch - 'a' + 'A');
        }
    }
    return text;
}

std::string graphics_frame_pattern_token(int frame)
{
    char buffer[8];
    std::snprintf(buffer, sizeof(buffer), "%02d", frame);
    return buffer;
}

void graphics_replace_pattern_token(
    std::string &text,
    const char *token,
    const std::string &replacement)
{
    const std::string token_text = token ? token : "";
    if (token_text.empty()) {
        return;
    }

    size_t pos = 0;
    while ((pos = text.find(token_text, pos)) != std::string::npos) {
        text.replace(pos, token_text.size(), replacement);
        pos += replacement.size();
    }
}

std::string graphics_expand_direction_frame_pattern(
    const std::string &pattern,
    int direction_index,
    int frame)
{
    std::string result = pattern;
    const std::string direction_text = graphics_direction8_suffix(direction_index);
    graphics_replace_pattern_token(result, "{dir}", direction_text);
    graphics_replace_pattern_token(result, "{dir_upper}", graphics_uppercase_pattern_token(direction_text));
    graphics_replace_pattern_token(result, "{frame}", graphics_frame_pattern_token(frame));
    return result;
}

int graphics_compare_int(int value, GraphicComparison comparison, int threshold)
{
    switch (comparison) {
        case GraphicComparison::LessThan:
            return value < threshold;
        case GraphicComparison::LessThanOrEqual:
            return value <= threshold;
        case GraphicComparison::Equal:
            return value == threshold;
        case GraphicComparison::GreaterThan:
            return value > threshold;
        case GraphicComparison::GreaterThanOrEqual:
            return value >= threshold;
        case GraphicComparison::None:
        default:
            return 0;
    }
}

GraphicsDefinition::GraphicsDefinition(GraphicsDefinitionKind kind)
    : kind_(kind)
{
}

GraphicsDefinition::~GraphicsDefinition() = default;

GraphicsDefinitionKind GraphicsDefinition::kind() const
{
    return kind_;
}
