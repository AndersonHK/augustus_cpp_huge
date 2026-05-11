#pragma once

extern "C" {
#include "game/resource.h"
}

#include <string>
#include <vector>

struct building;
struct image;
struct RuntimeAnimationTrack;

namespace building_type_registry_impl {

class BuildingType;

enum class GraphicComparison {
    None,
    LessThan,
    LessThanOrEqual,
    Equal,
    GreaterThan,
    GreaterThanOrEqual
};

enum class FigureSlot {
    None,
    Primary,
    Secondary,
    Quaternary
};

enum class GraphicsConditionType {
    None,
    HasWorkers,
    Working,
    WaterAccess,
    FigureSlotOccupied,
    ResourcePositive,
    Climate,
    MonumentUpgrade,
    FestivalGames,
    Desirability,
    Days1Positive,
    Days1NotPositive,
    Days2Positive,
    Days1OrDays2Positive
};

// A graphics target is either one direct path/image pair or a set of equivalent
// options that materialize into one direct target after stable variant selection.
struct GraphicsTarget {
    void set_path(std::string path);
    void set_image(std::string image);
    GraphicsTarget &add_option();

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;
    int has_options() const;
    int option_count() const;
    GraphicsTarget resolved_option(unsigned char variant) const;

private:
    std::string path_;
    std::string image_;
    std::vector<GraphicsTarget> options_;
};

struct GraphicsCondition {
    GraphicsConditionType type = GraphicsConditionType::None;
    GraphicComparison comparison = GraphicComparison::None;
    FigureSlot figure_slot = FigureSlot::None;
    int threshold = 0;
    resource_type resource = RESOURCE_NONE;
    int climate = 0;
    int monument_upgrade = 0;
    int festival_games = 0;

    int matches(const ::building &building) const;
};

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;

    int matches(const ::building &building) const;
};

class GraphicsDefinition {
public:
    void mark_default_node();
    GraphicsTarget &default_target();
    const GraphicsTarget &default_target() const;
    GraphicsVariant &add_variant();
    GraphicsVariant *last_variant();
    const GraphicsVariant *last_variant() const;

    int has_path() const;
    int has_default_node() const;
    int has_variants() const;
    const std::vector<GraphicsVariant> &variants() const;
    const GraphicsTarget *resolve_target(const ::building &building) const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    int has_default_node_ = 0;
};

// Frame-selection policy for one building instance. Rendering code asks this
// object what frame should draw; the object owns legacy cursor quirks.
class BuildingAnimation {
public:
    explicit BuildingAnimation(::building &building);
    BuildingAnimation(::building &building, const BuildingType *definition);

    int runtime_track_offset(const ::RuntimeAnimationTrack &track, int should_advance, int animation_cursor);
    int legacy_offset(int image_id, int animation_cursor);
    int legacy_offset_for_image(const ::image *img, int animation_cursor);
    int advance_storage_flag(int image_id);
    int advance_fumigation();

private:
    int gated_offset(int animation_cursor, int *offset) const;
    int advance_wine_workshop_offset(int animation_cursor, int max_frame, int clamp_to_available) const;
    int advance_reversible_offset(int animation_cursor, int max_frame) const;
    int advance_looping_offset(int animation_cursor, int max_frame) const;

    ::building &building_;
    const BuildingType *definition_;
};

}
