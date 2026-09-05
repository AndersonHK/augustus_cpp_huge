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
    Terrain,
    Climate,
    MonumentUpgrade,
    RaceActive,
    Population,
    Orientation,
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
    Orientation,
    ProductionProgress,
    StoragePermission,
    GatehouseOrientation,
    RoadCrossing,
    Rubble
};

enum class GraphicsLayerStage {
    Auto,
    Footprint,
    Top,
    Animation
};

enum class GraphicsOverlaySummaryPolicy {
    EveryDrawTile,
    CompositionOwner
};

int graphics_orientation_option_index(
    int building_orientation,
    int view_orientation,
    int option_count);
int graphics_farm_field_option_index(int percentage, int field_index, int option_count);

struct GraphicsCondition {
    GraphicsConditionType type = GraphicsConditionType::None;
    GraphicComparison comparison = GraphicComparison::None;
    FigureSlot figure_slot = FigureSlot::None;
    int threshold = 0;
    resource_type resource = RESOURCE_NONE;
    int terrain_mask = 0;
    int climate = 0;
    int monument_upgrade = 0;
    int orientation = 0;
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

struct GraphicsLayer : public ::GraphicsAssetReference {
    void set_role(std::string role);
    void set_option_selection(GraphicsOptionSelection selection);
    void set_animation_enabled(int enabled);
    void set_stage(GraphicsLayerStage stage);
    void set_offset(int x, int y);
    GraphicsLayerOption &add_option();
    void add_condition(GraphicsCondition condition);

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
struct GraphicsTarget : public ::GraphicsAssetReference {
    void set_option_selection(GraphicsOptionSelection selection);
    void set_resource_storage(int value);
    void set_animation_enabled(int enabled);
    void set_no_draw(int value);
    void set_terrain_foundation(int value);
    GraphicsTarget &add_option();
    GraphicsLayer &add_layer();

    GraphicsOptionSelection option_selection() const;
    int is_resource_storage() const;
    int animation_enabled() const;
    int no_draw() const;
    int uses_terrain_foundation() const;
    int has_options() const;
    int option_count() const;
    const GraphicsTarget *option(int index) const;
    const std::vector<GraphicsLayer> &layers() const;
    int has_layer_role(const char *role) const;
    GraphicsTarget resolved_option(unsigned char variant) const;

private:
    GraphicsOptionSelection option_selection_ = GraphicsOptionSelection::StableVariant;
    int resource_storage_ = 0;
    int animation_enabled_ = 1;
    int no_draw_ = 0;
    int terrain_foundation_ = 0;
    std::vector<GraphicsTarget> options_;
    std::vector<GraphicsLayer> layers_;
};

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;
    std::string role;

    int matches(const Building &building) const;
};

struct ConstructionPhaseGraphics {
    int phase = 0;
    GraphicsTarget target;
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

    void set_status_icon_anchor(int x, int y);
    int status_icon_anchor(int *x, int *y) const;
    void set_overlay_summary_policy(GraphicsOverlaySummaryPolicy policy);
    GraphicsOverlaySummaryPolicy overlay_summary_policy() const;
    int has_overlay_summary_policy() const;
    void mark_default_node();
    GraphicsTarget &default_target();
    const GraphicsTarget &default_target() const;
    GraphicsVariant &add_variant();
    GraphicsVariant *last_variant();
    const GraphicsVariant *last_variant() const;
    GraphicsTarget &add_construction_phase(int phase);
    GraphicsTarget *last_construction_phase();
    const GraphicsTarget *construction_phase(int phase) const;
    const std::vector<ConstructionPhaseGraphics> &construction_phases() const;

    int has_path() const;
    int has_default_node() const;
    int has_variants() const;
    const std::vector<GraphicsVariant> &variants() const;
    const GraphicsTarget *resolve_target(const Building &building) const;
    int draw_footprint(Building building, const BuildingDrawContext &ctx) const;
    int draw_top(Building building, const BuildingDrawContext &ctx) const;
    int draw_animation(Building building, const BuildingDrawContext &ctx) const;
    int mothball_status_icon_offset(
        Building building,
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
    int draws_mothball_status(Building building) const;

    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    std::vector<ConstructionPhaseGraphics> construction_phases_;
    int has_status_icon_anchor_ = 0;
    int status_icon_anchor_x_ = 0;
    int status_icon_anchor_y_ = 0;
    GraphicsOverlaySummaryPolicy overlay_summary_policy_ = GraphicsOverlaySummaryPolicy::EveryDrawTile;
    int has_overlay_summary_policy_ = 0;
    int has_default_node_ = 0;
};

int graphics_overlay_summary_draws(
    GraphicsOverlaySummaryPolicy policy,
    int is_composed,
    int is_owner,
    int is_draw_tile);

}
