#pragma once

#include "game/resource.h"
#include "graphics/color.h"
#include "graphics/GraphicsDefinition.h"
#include "graphics/image.h"

#include <array>
#include <string>
#include <vector>

class Image;
class Building;
struct BuildingDrawContext;

namespace building_type_registry_impl {

class BuildingType;

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
    ResourceAmount,
    Climate,
    MonumentUpgrade,
    FestivalGames,
    Desirability,
    Days1Positive,
    Days1NotPositive,
    Days2Positive,
    Days1OrDays2Positive
};

enum class GraphicsOptionSelection {
    StableVariant,
    BuildRotation,
    Connectable,
    StorageLoad,
    Orientation,
    ProductionProgress,
    StoragePermission,
    GatehouseOrientation
};

enum class GraphicsLayerStage {
    Auto,
    Footprint,
    Top,
    Animation
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
};

struct GraphicsLayerOption {
    std::string path;
    std::string image;
    int has_x_offset = 0;
    int x_offset = 0;
    int has_y_offset = 0;
    int y_offset = 0;
};

struct GraphicsLayer {
    void set_path(std::string path);
    void set_image(std::string image);
    void set_role(std::string role);
    void set_option_selection(GraphicsOptionSelection selection);
    void set_animation_enabled(int enabled);
    void set_stage(GraphicsLayerStage stage);
    void set_offset(int x, int y);
    GraphicsLayerOption &add_option();
    void add_condition(GraphicsCondition condition);

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;
    int has_role() const;
    const char *role() const;
    int role_is(const char *role) const;
    GraphicsOptionSelection option_selection() const;
    int animation_enabled() const;
    GraphicsLayerStage stage() const;
    int x_offset() const;
    int y_offset() const;
    int has_options() const;
    int option_count() const;
    const GraphicsLayerOption *option(int index) const;
    const std::vector<GraphicsCondition> &conditions() const;
    int matches(const Building &building) const;
    GraphicsLayer resolved_option(unsigned char variant) const;

private:
    std::string path_;
    std::string image_;
    std::string role_;
    GraphicsOptionSelection option_selection_ = GraphicsOptionSelection::StableVariant;
    int animation_enabled_ = 1;
    GraphicsLayerStage stage_ = GraphicsLayerStage::Auto;
    int x_offset_ = 0;
    int y_offset_ = 0;
    std::vector<GraphicsLayerOption> options_;
    std::vector<GraphicsCondition> conditions_;
};

// A graphics target is either one direct path/image pair or a set of equivalent
// options that materialize into one direct target after stable variant selection.
struct GraphicsTarget {
    void set_path(std::string path);
    void set_image(std::string image);
    void set_option_selection(GraphicsOptionSelection selection);
    void set_resource_storage(int value);
    void set_animation_enabled(int enabled);
    GraphicsTarget &add_option();
    GraphicsLayer &add_layer();

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;
    GraphicsOptionSelection option_selection() const;
    int is_resource_storage() const;
    int animation_enabled() const;
    int has_options() const;
    int option_count() const;
    const GraphicsTarget *option(int index) const;
    const std::vector<GraphicsLayer> &layers() const;
    GraphicsTarget resolved_option(unsigned char variant) const;

private:
    std::string path_;
    std::string image_;
    GraphicsOptionSelection option_selection_ = GraphicsOptionSelection::StableVariant;
    int resource_storage_ = 0;
    int animation_enabled_ = 1;
    std::vector<GraphicsTarget> options_;
    std::vector<GraphicsLayer> layers_;
};

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;
    std::string role;

    int matches(const Building &building) const;
};

class BuildingGraphicsDef : public ::GraphicsDefinition {
public:
    BuildingGraphicsDef()
        : ::GraphicsDefinition(::GraphicsDefinitionKind::Building)
    {
    }

    static void set_resource_storage_images(resource_type resource, std::array<ImageGroupEntryRef, 4> images);
    static const ImageGroupEntryRef &resource_storage_image(resource_type resource, int loads);
    static void reset_resource_storage_images();

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
    const GraphicsTarget *resolve_target(const Building &building) const;
    int draw_footprint(Building building, const BuildingDrawContext &ctx) const;
    int draw_top(Building building, const BuildingDrawContext &ctx) const;
    int draw_animation(Building building, const BuildingDrawContext &ctx) const;
    int draw_gatehouse_overlay(Building building, const BuildingDrawContext &ctx, int view_orientation) const;
    int mothball_status_icon_offset(
        Building building,
        int grid_offset,
        int icon_width,
        int icon_height,
        int *x,
        int *y) const;
    int production_progress_option_count() const;
    int draws_overlay_summary_at(Building building, int grid_offset, int view_orientation) const;
    unsigned char upgrade_level_for(const Building &building) const;

private:
    const ImageGroupEntryRef &resource_storage_image_for(Building building) const;
    int draw_resource_storage(Building building, const BuildingDrawContext &ctx, GraphicsLayerStage stage) const;
    int gatehouse_overlay_draw_tile_matches(Building building, int grid_offset, int view_orientation) const;
    int draws_mothball_status_at(Building building, int grid_offset) const;

    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    int has_default_node_ = 0;
};

}
