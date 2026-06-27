#include "building/BuildingGraphics.h"

#ifndef STARTUP_PARSER_TEST
#include "assets/assets.h"
#include "assets/image_group_entry.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/festival.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/figure.h"
#include "graphics/image.h"
#include "graphics/runtime_texture.h"
#include "map/image.h"
#include "map/property.h"
#include "map/sprite.h"
#include "scenario/property.h"
#endif
#include "core/crash_context.h"

#include <array>
#include <cstddef>
#include <cstdio>
#include <memory>
#include <utility>

namespace building_type_registry_impl {

namespace {

constexpr int RESOURCE_STORAGE_GRAPHICS_COUNT = RESOURCE_SLOT_COUNT;

std::array<std::array<ImageGroupEntryRef, 4>, RESOURCE_STORAGE_GRAPHICS_COUNT> g_resource_storage_images;

const ImageGroupEntryRef &empty_resource_storage_image()
{
    static const ImageGroupEntryRef image = ImageGroupEntryRef::from_group(
        "Industry\\Warehouse_Storage_Empty",
        "Image_0000");
    return image;
}

const char *resource_name(resource_type resource)
{
    const resource_data *data = resource_get_data(resource);
    return data && data->xml_attr_name ? data->xml_attr_name : "unknown";
}

void report_invalid_resource_storage_graphic(const char *message, resource_type resource, int value)
{
    char detail[128];
    snprintf(detail, sizeof(detail), "%s resource=%d value=%d", resource_name(resource), resource, value);
    error_context_report_error(message, detail);
}

int valid_resource_storage_graphics_index(resource_type resource)
{
    return resource >= RESOURCE_NONE && resource < RESOURCE_STORAGE_GRAPHICS_COUNT;
}

int graphics_condition_matches(const GraphicsCondition &condition, const Building &building)
{
#ifdef STARTUP_PARSER_TEST
    (void) condition;
    (void) building;
    return 0;
#else
    const Building state = building.composition_owner();
    int matches = 0;
    switch (condition.type) {
        case GraphicsConditionType::HasWorkers:
            matches = state.worker_count();
            break;
        case GraphicsConditionType::Working:
            matches = state.is_working();
            break;
        case GraphicsConditionType::WaterAccess:
            matches = state.has_water_access();
            break;
        case GraphicsConditionType::FigureSlotOccupied:
            switch (condition.figure_slot) {
                case FigureSlot::Primary:
                    matches = state.has_primary_figure();
                    break;
                case FigureSlot::Secondary:
                    matches = state.has_secondary_figure();
                    break;
                case FigureSlot::Quaternary:
                    matches = state.has_quaternary_figure();
                    break;
                case FigureSlot::None:
                default:
                    matches = 0;
                    break;
            }
            break;
        case GraphicsConditionType::ResourcePositive:
            matches = state.resource_amount(condition.resource) > 0;
            break;
        case GraphicsConditionType::ResourceAmount:
        {
            const int amount = state.resource_amount(condition.resource);
            matches = graphics_compare_int(amount, condition.comparison, condition.threshold);
            break;
        }
        case GraphicsConditionType::Climate:
            matches = scenario_property_climate() == condition.climate;
            break;
        case GraphicsConditionType::MonumentUpgrade:
            matches = state.monument_upgrade_level() == condition.monument_upgrade;
            break;
        case GraphicsConditionType::FestivalGames:
            matches = city_festival_games_active() == condition.festival_games;
            break;
        case GraphicsConditionType::Days1Positive:
            matches = state.entertainment_days1() > 0;
            break;
        case GraphicsConditionType::Days1NotPositive:
            matches = state.entertainment_days1() <= 0;
            break;
        case GraphicsConditionType::Days2Positive:
            matches = state.entertainment_days2() > 0;
            break;
        case GraphicsConditionType::Days1OrDays2Positive:
            matches = state.entertainment_days1() > 0 || state.entertainment_days2() > 0;
            break;
        case GraphicsConditionType::Desirability:
            matches = graphics_compare_int(state.desirability(), condition.comparison, condition.threshold);
            break;
        case GraphicsConditionType::None:
        default:
            matches = 0;
            break;
    }
    return matches;
#endif
}

} // namespace

void BuildingGraphics::set_resource_storage_images(resource_type resource, std::array<ImageGroupEntryRef, 4> images)
{
    if (!valid_resource_storage_graphics_index(resource)) {
        report_invalid_resource_storage_graphic("Invalid resource storage graphics assignment", resource, 0);
        return;
    }
    g_resource_storage_images[static_cast<size_t>(resource)] = std::move(images);
}

const ImageGroupEntryRef &BuildingGraphics::resource_storage_image(resource_type resource, int loads)
{
    if (loads <= 0) {
        return empty_resource_storage_image();
    }
    if (!valid_resource_storage_graphics_index(resource)) {
        report_invalid_resource_storage_graphic("Invalid resource storage graphics lookup", resource, loads);
        return empty_resource_storage_image();
    }
    if (loads > static_cast<int>(g_resource_storage_images[static_cast<size_t>(resource)].size())) {
        report_invalid_resource_storage_graphic("Invalid resource storage graphic load", resource, loads);
        return empty_resource_storage_image();
    }
    return g_resource_storage_images[static_cast<size_t>(resource)][static_cast<size_t>(loads - 1)];
}

void BuildingGraphics::reset_resource_storage_images()
{
    g_resource_storage_images = {};
}

void GraphicsLayer::set_path(std::string path)
{
    path_ = std::move(path);
}

void GraphicsLayer::set_image(std::string image)
{
    image_ = std::move(image);
}

void GraphicsLayer::set_option_selection(GraphicsOptionSelection selection)
{
    option_selection_ = selection;
}

void GraphicsLayer::set_animation_enabled(int enabled)
{
    animation_enabled_ = enabled ? 1 : 0;
}

void GraphicsLayer::set_stage(GraphicsLayerStage stage)
{
    stage_ = stage;
}

void GraphicsLayer::set_offset(int x, int y)
{
    x_offset_ = x;
    y_offset_ = y;
}

GraphicsLayerOption &GraphicsLayer::add_option()
{
    options_.emplace_back();
    return options_.back();
}

void GraphicsLayer::add_condition(GraphicsCondition condition)
{
    conditions_.push_back(std::move(condition));
}

int GraphicsLayer::has_path() const
{
    return !path_.empty();
}

const char *GraphicsLayer::path() const
{
    return path_.c_str();
}

int GraphicsLayer::has_image() const
{
    return !image_.empty();
}

const char *GraphicsLayer::image() const
{
    return image_.c_str();
}

GraphicsOptionSelection GraphicsLayer::option_selection() const
{
    return option_selection_;
}

int GraphicsLayer::animation_enabled() const
{
    return animation_enabled_;
}

GraphicsLayerStage GraphicsLayer::stage() const
{
    return stage_;
}

int GraphicsLayer::x_offset() const
{
    return x_offset_;
}

int GraphicsLayer::y_offset() const
{
    return y_offset_;
}

int GraphicsLayer::has_options() const
{
    return !options_.empty();
}

int GraphicsLayer::option_count() const
{
    return static_cast<int>(options_.size());
}

const GraphicsLayerOption *GraphicsLayer::option(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= options_.size()) {
        return nullptr;
    }
    return &options_[static_cast<size_t>(index)];
}

const std::vector<GraphicsCondition> &GraphicsLayer::conditions() const
{
    return conditions_;
}

int GraphicsLayer::matches(const Building &building) const
{
    for (const GraphicsCondition &condition : conditions_) {
        if (!graphics_condition_matches(condition, building)) {
            return 0;
        }
    }
    return 1;
}

GraphicsLayer GraphicsLayer::resolved_option(unsigned char variant) const
{
    if (options_.empty()) {
        return *this;
    }

    GraphicsLayer resolved = *this;
    const GraphicsLayerOption &option = options_[variant % options_.size()];
    resolved.options_.clear();
    if (!option.path.empty()) {
        resolved.set_path(option.path);
    }
    if (!option.image.empty()) {
        resolved.set_image(option.image);
    }
    return resolved;
}

void GraphicsTarget::set_path(std::string path)
{
    path_ = std::move(path);
}

void GraphicsTarget::set_image(std::string image)
{
    image_ = std::move(image);
}

void GraphicsTarget::set_option_selection(GraphicsOptionSelection selection)
{
    option_selection_ = selection;
}

void GraphicsTarget::set_resource_storage(int value)
{
    resource_storage_ = value ? 1 : 0;
}

void GraphicsTarget::set_animation_enabled(int enabled)
{
    animation_enabled_ = enabled ? 1 : 0;
}

GraphicsTarget &GraphicsTarget::add_option()
{
    options_.emplace_back();
    return options_.back();
}

GraphicsLayer &GraphicsTarget::add_layer()
{
    layers_.emplace_back();
    return layers_.back();
}

int GraphicsTarget::has_path() const
{
    return !path_.empty();
}

const char *GraphicsTarget::path() const
{
    return path_.c_str();
}

int GraphicsTarget::has_image() const
{
    return !image_.empty();
}

const char *GraphicsTarget::image() const
{
    return image_.c_str();
}

GraphicsOptionSelection GraphicsTarget::option_selection() const
{
    return option_selection_;
}

int GraphicsTarget::is_resource_storage() const
{
    return resource_storage_;
}

int GraphicsTarget::animation_enabled() const
{
    return animation_enabled_;
}

int GraphicsTarget::has_options() const
{
    return !options_.empty();
}

int GraphicsTarget::option_count() const
{
    return static_cast<int>(options_.size());
}

const GraphicsTarget *GraphicsTarget::option(int index) const
{
    if (index < 0 || static_cast<size_t>(index) >= options_.size()) {
        return nullptr;
    }
    return &options_[static_cast<size_t>(index)];
}

const std::vector<GraphicsLayer> &GraphicsTarget::layers() const
{
    return layers_;
}

GraphicsTarget GraphicsTarget::resolved_option(unsigned char variant) const
{
    auto inherit_layer_paths = [](GraphicsTarget &target) {
        for (GraphicsLayer &layer : target.layers_) {
            if (!layer.has_path()) {
                layer.set_path(target.path_);
            }
        }
    };

    if (options_.empty()) {
        GraphicsTarget resolved = *this;
        inherit_layer_paths(resolved);
        return resolved;
    }

    // Options are authored as partial targets. Materialize one effective target so
    // the renderer and validator can keep using the normal path/image lookup path.
    GraphicsTarget resolved = options_[variant % options_.size()];
    if (!resolved.has_path()) {
        resolved.set_path(path_);
    }
    if (resolved.layers_.empty()) {
        resolved.layers_ = layers_;
    }
    inherit_layer_paths(resolved);
    if (!animation_enabled_) {
        resolved.set_animation_enabled(0);
    }
    return resolved;
}

int GraphicsVariant::matches(const Building &building) const
{
    if (!role.empty()) {
        return 0;
    }

    for (const GraphicsCondition &condition : conditions) {
        if (!graphics_condition_matches(condition, building)) {
            return 0;
        }
    }
    return 1;
}

GraphicsTarget &BuildingGraphics::default_target()
{
    return default_target_;
}

const GraphicsTarget &BuildingGraphics::default_target() const
{
    return default_target_;
}

void BuildingGraphics::mark_default_node()
{
    has_default_node_ = 1;
}

GraphicsVariant &BuildingGraphics::add_variant()
{
    variants_.emplace_back();
    return variants_.back();
}

GraphicsVariant *BuildingGraphics::last_variant()
{
    return variants_.empty() ? nullptr : &variants_.back();
}

const GraphicsVariant *BuildingGraphics::last_variant() const
{
    return variants_.empty() ? nullptr : &variants_.back();
}

int BuildingGraphics::has_path() const
{
    return default_target_.has_path() || default_target_.has_options() || default_target_.is_resource_storage();
}

int BuildingGraphics::has_default_node() const
{
    return has_default_node_;
}

int BuildingGraphics::has_variants() const
{
    return !variants_.empty();
}

const std::vector<GraphicsVariant> &BuildingGraphics::variants() const
{
    return variants_;
}

const GraphicsTarget *BuildingGraphics::resolve_target(const Building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
        if (!variant.role.empty()) {
            continue;
        }
        if ((variant.target.has_path() || variant.target.has_options() || variant.target.is_resource_storage()) &&
            variant.matches(building)) {
            return &variant.target;
        }
    }
    if (default_target_.has_path() || default_target_.has_options() || default_target_.is_resource_storage()) {
        return &default_target_;
    }
    return nullptr;
}

#ifndef STARTUP_PARSER_TEST
namespace {

building_runtime *runtime_for_building(Building building, std::unique_ptr<building_runtime> &temporary_runtime)
{
    Building owner = building.type && building.type->has_graphic() ? building : building.main();
    if (!owner.type) {
        return nullptr;
    }

    if (building_runtime *runtime = owner.runtime_instance()) {
        return runtime;
    }

    temporary_runtime = std::make_unique<building_runtime>(owner);
    return temporary_runtime.get();
}

int draw_resource_storage_footprint(Building building, const BuildingDrawContext &ctx)
{
    if (!ctx.force_draw_tile && !map_property_is_draw_tile(ctx.grid_offset)) {
        return 1;
    }
    const resource_type resource = building.warehouse_resource_id();
    const int loads = resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? building.resource_amount(resource) : 0;
    const resource_type graphic_resource = loads > 0 ? resource : RESOURCE_NONE;
    BuildingGraphics::resource_storage_image(graphic_resource, loads).draw(ctx.x, ctx.y, ctx.color_mask, ctx.scale);
    return 1;
}

int draw_resource_storage_top(Building building, const BuildingDrawContext &ctx)
{
    if (!ctx.force_draw_tile && !map_property_is_draw_tile(ctx.grid_offset)) {
        return 1;
    }
    const resource_type resource = building.warehouse_resource_id();
    const int loads = resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? building.resource_amount(resource) : 0;
    const resource_type graphic_resource = loads > 0 ? resource : RESOURCE_NONE;
    BuildingGraphics::resource_storage_image(graphic_resource, loads).draw_top(ctx.x, ctx.y, ctx.color_mask, ctx.scale);
    return 1;
}

void log_missing_runtime_stage_slice(const char *stage, const Building &building)
{
    char detail[256];
    if (building.type) {
        snprintf(
            detail,
            sizeof(detail),
            "type=%s grid_offset=%d stage=%s",
            building.type->attr(),
            building.grid_offset(),
            stage ? stage : "");
    } else {
        snprintf(detail, sizeof(detail), "building=null stage=%s", stage ? stage : "");
    }

    error_context_report_error("Native building draw stage had no drawable slice. Falling back to legacy rendering.", detail);
}

}

int BuildingGraphics::draw_footprint(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = BuildingType::resolve_graphics_target_for_image(building.type, building)) {
        if (target->is_resource_storage()) {
            return draw_resource_storage_footprint(building, ctx);
        }
    }

    std::unique_ptr<building_runtime> temporary_runtime;
    building_runtime *runtime = runtime_for_building(building, temporary_runtime);
    if (!runtime || !runtime->resolve_graphics_cache()) {
        return 0;
    }

    if (ctx.force_draw_tile || map_property_is_draw_tile(ctx.grid_offset)) {
        const RuntimeDrawSlice *footprint = runtime->cached_graphic_footprint();
        if (!footprint) {
            log_missing_runtime_stage_slice("footprint", building);
            return 0;
        }
        runtime_texture_draw(*footprint, ctx.x, ctx.y, ctx.color_mask, ctx.scale);
    }
    runtime->draw_cached_graphic_layers(GraphicsLayerStage::Footprint, ctx.x, ctx.y, ctx.color_mask, ctx.scale);

    return 1;
}

int BuildingGraphics::draw_top(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = BuildingType::resolve_graphics_target_for_image(building.type, building)) {
        if (target->is_resource_storage()) {
            return draw_resource_storage_top(building, ctx);
        }
    }

    std::unique_ptr<building_runtime> temporary_runtime;
    building_runtime *runtime = runtime_for_building(building, temporary_runtime);
    if (!runtime || !runtime->resolve_graphics_cache()) {
        return 0;
    }

    if (!ctx.force_draw_tile && !map_property_is_draw_tile(ctx.grid_offset)) {
        return 1;
    }

    if (const RuntimeDrawSlice *top = runtime->cached_graphic_top()) {
        runtime_texture_draw(*top, ctx.x, ctx.y, ctx.color_mask, ctx.scale);
    }
    runtime->draw_cached_graphic_layers(GraphicsLayerStage::Top, ctx.x, ctx.y, ctx.color_mask, ctx.scale);

    return 1;
}

int BuildingGraphics::draw_animation(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = BuildingType::resolve_graphics_target_for_image(building.type, building)) {
        if (target->is_resource_storage()) {
            return 1;
        }
    }

    std::unique_ptr<building_runtime> temporary_runtime;
    building_runtime *runtime = runtime_for_building(building, temporary_runtime);
    if (!runtime || !runtime->resolve_graphics_cache()) {
        return 0;
    }

    if (!ctx.force_draw_tile && !map_property_is_draw_tile(ctx.grid_offset)) {
        return 1;
    }

    if (runtime->cached_owns_graphic_animation()) {
        runtime->advance_cached_graphic_animation(ctx.grid_offset);
        if (const RuntimeDrawSlice *frame = runtime->cached_graphic_animation(ctx.grid_offset)) {
            runtime_texture_draw(*frame, ctx.x, ctx.y, ctx.color_mask, ctx.scale);
        }
    }
    runtime->draw_cached_graphic_layer_animations(ctx.grid_offset, ctx.x, ctx.y, ctx.color_mask, ctx.scale);

    return 1;
}

#endif

unsigned char BuildingGraphics::upgrade_level_for(const Building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
        if ((!variant.target.has_path() && !variant.target.has_options() && !variant.target.is_resource_storage()) ||
            !variant.matches(building)) {
            continue;
        }

        for (const GraphicsCondition &condition : variant.conditions) {
            if (condition.type == GraphicsConditionType::Desirability) {
                return 1;
            }
        }
        break;
    }
    return 0;
}

} // namespace building_type_registry_impl
