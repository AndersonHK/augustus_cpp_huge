#pragma once

#include <string>

enum class GraphicsDefinitionKind {
    Base,
    Building,
    Figure,
    Resource
};

enum class GraphicsDirection8 {
    Northeast = 0,
    East = 1,
    Southeast = 2,
    South = 3,
    Southwest = 4,
    West = 5,
    Northwest = 6,
    North = 7
};

enum class GraphicsCardinalDirection {
    North = 0,
    East = 1,
    South = 2,
    West = 3
};

enum class GraphicsConditionSubject {
    None,
    OwnerState,
    Resource,
    Climate,
    AnimationState,
    Orientation,
    Direction,
    ProductionProgress,
    ServiceAccess
};

enum class GraphicsTargetRole {
    Default,
    Action,
    Corpse,
    Overlay,
    Icon,
    ResourceStorage,
    ResourceCart
};

enum class GraphicComparison {
    None,
    LessThan,
    LessThanOrEqual,
    Equal,
    GreaterThan,
    GreaterThanOrEqual
};

inline constexpr int GRAPHICS_DIRECTION8_COUNT = 8;

struct GraphicsOrientation {
    int quarter_turns = 0;

    int normalized_quarter_turns() const;
};

struct GraphicsPoint {
    int x = 0;
    int y = 0;

    int is_zero() const;
    GraphicsPoint translated(int dx, int dy) const;
};

struct GraphicsAnimationFrame {
    int frame = 0;
    int frame_count = 0;

    int clamped_frame() const;
};

const char *graphics_definition_kind_name(GraphicsDefinitionKind kind);
const char *graphics_target_role_name(GraphicsTargetRole role);
int graphics_normalize_direction8(int direction);
const char *graphics_direction8_suffix(int direction);
std::string graphics_uppercase_pattern_token(std::string text);
std::string graphics_frame_pattern_token(int frame);
void graphics_replace_pattern_token(
    std::string &text,
    const char *token,
    const std::string &replacement);
std::string graphics_expand_direction_frame_pattern(
    const std::string &pattern,
    int direction_index,
    int frame);
int graphics_compare_int(int value, GraphicComparison comparison, int threshold);

class GraphicsDefinition {
public:
    virtual ~GraphicsDefinition();

    GraphicsDefinitionKind kind() const;

protected:
    explicit GraphicsDefinition(GraphicsDefinitionKind kind = GraphicsDefinitionKind::Base);

private:
    GraphicsDefinitionKind kind_ = GraphicsDefinitionKind::Base;
};
