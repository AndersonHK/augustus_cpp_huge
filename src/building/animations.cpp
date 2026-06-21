#include "building/count.h"
#include "building/image.h"
#include "building/industry.h"
#include "city/festival.h"
#include "map/image.h"

#include "building/animations.h"

#include "assets/assets.h"
#include "assets/image_group_entry.h"
#include "building/building.h"
#include "figure/figure.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/distribution.h"
#include "building/building_type.h"
#include "building/building_type_api.h"
#include "building/building_type_registry_internal.h"
#include "core/crash_context.h"
#include "graphics/image.h"
#include "graphics/runtime_texture.h"

#include "building/monument.h"
#include "building/properties.h"
#include "core/calc.h"
#include "core/image.h"
#include "game/animation.h"
#include "game/resource_graphics.h"
#include "map/property.h"
#include "map/sprite.h"
#include "scenario/property.h"

#include <cstdio>
#include <cstring>
#include <memory>
#include <utility>

namespace building_type_registry_impl {

namespace {

int definition_is(const BuildingType *definition, const char *attr)
{
    return definition && definition->attr() && std::strcmp(definition->attr(), attr) == 0;
}

int definition_is_any(const BuildingType *definition, const char *const *attrs, int count)
{
    for (int i = 0; i < count; i++) {
        if (definition_is(definition, attrs[i])) {
            return 1;
        }
    }
    return 0;
}

building_type runtime_type_from_attr(const char *attr)
{
    return building_type_registry_impl::type_from_attr(attr);
}

int has_first_distribution_resource(const building &building, const BuildingType *definition)
{
    const Distribution *distribution = definition ? definition->distribution() : nullptr;
    if (!distribution) {
        return 1;
    }
    for (const DistributionResourceRule &rule : distribution->resources()) {
        if (resource_is_inventory(rule.resource)) {
            return building.resources[rule.resource] > 0;
        }
    }
    return 1;
}

void advance_monument_secondary_animation(building &building)
{
    if (definition_is(definition_for_type(building.type), "grand_temple_ceres") && building.monument.upgrades == 1) {
        building.monument.secondary_frame++;
        if (building.monument.secondary_frame > 4) {
            building.monument.secondary_frame = 0;
        }
    }
}

int normalized_animation_frame(int animation_cursor, const RuntimeAnimationTrack &track)
{
    if (animation_cursor < 0 || track.num_frames <= 0) {
        return 0;
    }
    // The map sprite animation byte is 1-based; bit 7 stores reverse playback
    // state for ping-pong animations and must not be treated as part of the frame.
    int current_frame = map_sprite_animation_at(animation_cursor) & 0x7f;
    if (current_frame < 1 || current_frame > track.num_frames) {
        current_frame = 1;
        map_sprite_animation_set(animation_cursor, current_frame);
    }
    return current_frame;
}

int graphics_condition_matches(const GraphicsCondition &condition, const Building &building)
{
    int matches = 0;
    switch (condition.type) {
        case GraphicsConditionType::HasWorkers:
            matches = building.worker_count();
            break;
        case GraphicsConditionType::Working:
            matches = building.is_working();
            break;
        case GraphicsConditionType::WaterAccess:
            matches = building.has_water_access();
            break;
        case GraphicsConditionType::FigureSlotOccupied:
            switch (condition.figure_slot) {
                case FigureSlot::Primary:
                    matches = building.has_primary_figure();
                    break;
                case FigureSlot::Secondary:
                    matches = building.has_secondary_figure();
                    break;
                case FigureSlot::Quaternary:
                    matches = building.has_quaternary_figure();
                    break;
                case FigureSlot::None:
                default:
                    matches = 0;
                    break;
            }
            break;
        case GraphicsConditionType::ResourcePositive:
            matches = building.resource_amount(condition.resource) > 0;
            break;
        case GraphicsConditionType::ResourceAmount:
        {
            const int amount = building.resource_amount(condition.resource);
            switch (condition.comparison) {
                case GraphicComparison::LessThan:
                    matches = amount < condition.threshold;
                    break;
                case GraphicComparison::LessThanOrEqual:
                    matches = amount <= condition.threshold;
                    break;
                case GraphicComparison::Equal:
                    matches = amount == condition.threshold;
                    break;
                case GraphicComparison::GreaterThan:
                    matches = amount > condition.threshold;
                    break;
                case GraphicComparison::GreaterThanOrEqual:
                    matches = amount >= condition.threshold;
                    break;
                case GraphicComparison::None:
                default:
                    matches = 0;
                    break;
            }
            break;
        }
        case GraphicsConditionType::Climate:
            matches = scenario_property_climate() == condition.climate;
            break;
        case GraphicsConditionType::MonumentUpgrade:
            matches = building.monument_upgrade_level() == condition.monument_upgrade;
            break;
        case GraphicsConditionType::FestivalGames:
            matches = city_festival_games_active() == condition.festival_games;
            break;
        case GraphicsConditionType::Days1Positive:
            matches = building.entertainment_days1() > 0;
            break;
        case GraphicsConditionType::Days1NotPositive:
            matches = building.entertainment_days1() <= 0;
            break;
        case GraphicsConditionType::Days2Positive:
            matches = building.entertainment_days2() > 0;
            break;
        case GraphicsConditionType::Days1OrDays2Positive:
            matches = building.entertainment_days1() > 0 || building.entertainment_days2() > 0;
            break;
        case GraphicsConditionType::Desirability:
            switch (condition.comparison) {
                case GraphicComparison::LessThan:
                    matches = building.desirability() < condition.threshold;
                    break;
                case GraphicComparison::LessThanOrEqual:
                    matches = building.desirability() <= condition.threshold;
                    break;
                case GraphicComparison::Equal:
                    matches = building.desirability() == condition.threshold;
                    break;
                case GraphicComparison::GreaterThan:
                    matches = building.desirability() > condition.threshold;
                    break;
                case GraphicComparison::GreaterThanOrEqual:
                    matches = building.desirability() >= condition.threshold;
                    break;
                case GraphicComparison::None:
                default:
                    matches = 0;
                    break;
            }
            break;
        case GraphicsConditionType::None:
        default:
            matches = 0;
            break;
    }
    return matches;
}

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

const GraphicsTarget *resolve_target_for_direct_graphics(Building building)
{
    return building.type ? BuildingType::resolve_graphics_target_for_image(building.type, building) : nullptr;
}

int draw_resource_storage_footprint(Building building, const BuildingDrawContext &ctx)
{
    if (!ctx.force_draw_tile && !map_property_is_draw_tile(ctx.grid_offset)) {
        return 1;
    }
    const resource_type resource = building.warehouse_resource_id();
    const int loads = resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT ? building.resource_amount(resource) : 0;
    const resource_type graphic_resource = loads > 0 ? resource : RESOURCE_NONE;
    resource_graphics(graphic_resource).storage_image(loads).draw(ctx.x, ctx.y, ctx.color_mask, ctx.scale);
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
    resource_graphics(graphic_resource).storage_image(loads).draw_top(ctx.x, ctx.y, ctx.color_mask, ctx.scale);
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

GraphicsTarget &GraphicsDefinition::default_target()
{
    return default_target_;
}

const GraphicsTarget &GraphicsDefinition::default_target() const
{
    return default_target_;
}

void GraphicsDefinition::mark_default_node()
{
    has_default_node_ = 1;
}

GraphicsVariant &GraphicsDefinition::add_variant()
{
    variants_.emplace_back();
    return variants_.back();
}

GraphicsVariant *GraphicsDefinition::last_variant()
{
    return variants_.empty() ? nullptr : &variants_.back();
}

const GraphicsVariant *GraphicsDefinition::last_variant() const
{
    return variants_.empty() ? nullptr : &variants_.back();
}

int GraphicsDefinition::has_path() const
{
    return default_target_.has_path() || default_target_.has_options() || default_target_.is_resource_storage();
}

int GraphicsDefinition::has_default_node() const
{
    return has_default_node_;
}

int GraphicsDefinition::has_variants() const
{
    return !variants_.empty();
}

const std::vector<GraphicsVariant> &GraphicsDefinition::variants() const
{
    return variants_;
}

const GraphicsTarget *GraphicsDefinition::resolve_target(const Building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
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

int GraphicsDefinition::draw_footprint(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = resolve_target_for_direct_graphics(building)) {
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

int GraphicsDefinition::draw_top(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = resolve_target_for_direct_graphics(building)) {
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

int GraphicsDefinition::draw_animation(Building building, const BuildingDrawContext &ctx) const
{
    if (const GraphicsTarget *target = resolve_target_for_direct_graphics(building)) {
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

unsigned char GraphicsDefinition::upgrade_level_for(const Building &building) const
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

BuildingAnimation::BuildingAnimation(Building building)
    : record_(building.record_)
    , definition_(building.type)
{
}

building &BuildingAnimation::record()
{
    return *record_;
}

const building &BuildingAnimation::record() const
{
    return *record_;
}

int BuildingAnimation::legacy_gate_offset(int animation_cursor, int *offset) const
{
    const building &building = record();

    *offset = 0;
    // These gates deliberately mirror the old animation decisions: XML-authored
    // animations and legacy image animations must both pause for the same runtime
    // building states, otherwise placing a migrated graphic changes gameplay cues.
    if (definition_ && definition_->water_access().has_requirements() && !building.has_water_access) {
        return 1;
    }
    if (building_is_workshop(building.type)) {
        if (building.num_workers <= 0 || building.strike_duration_days > 0 ||
            !building_industry_has_raw_materials_for_production(&building)) {
            return 1;
        }
    }
    if (definition_is(definition_, "concrete_maker")) {
        if (building.data.industry.progress == 0) {
            return 1;
        }
    }
    static const char *worker_paused_service_posts[] = { "prefecture", "engineers_post" };
    if (definition_is_any(definition_, worker_paused_service_posts, 2) && building.num_workers <= 0) {
        return 1;
    }
    if (definition_ && definition_->has_market() && building.num_workers <= 0) {
        return 1;
    }
    if (definition_ && definition_->is_warehouse() && building.num_workers < model_get_building(building.type)->laborers) {
        return 1;
    }
    if (definition_ && std::strcmp(definition_->attr(), "dock") == 0 && building.data.dock.num_ships <= 0) {
        map_sprite_animation_set(animation_cursor, 1);
        *offset = 1;
        return 1;
    }
    if (building_is_raw_resource_producer(building.type) &&
        (building.num_workers <= 0 || building.strike_duration_days > 0)) {
        return 1;
    }
    if (definition_is(definition_, "gladiator_school")) {
        if (building.num_workers <= 0) {
            map_sprite_animation_set(animation_cursor, 1);
            *offset = 1;
            return 1;
        }
    } else if (((definition_ && definition_->is_theater()) ||
        definition_is(definition_, "chariot_maker")) &&
        !definition_is(definition_, "hippodrome") && building.num_workers <= 0) {
        return 1;
    }
    if (definition_ && definition_->is_granary() && building.num_workers < model_get_building(building.type)->laborers) {
        return 1;
    }
    if (building_monument_is_monument(&building) &&
        (!(definition_ && definition_->is_oracle()) && !definition_is(definition_, "nymphaeum") &&
            (building.num_workers <= 0 || building.monument.phase != MONUMENT_FINISHED))) {
        return 1;
    }
    if (definition_is(definition_, "city_mint") &&
        ((building.output_resource_id == resource_denarii() &&
            !Building(const_cast<::building *>(&building)).native_production_has_raw_materials()) ||
            building.num_workers <= 0 ||
            (building_count_active(runtime_type_from_attr("senate")) == 0))) {
        return 1;
    }
    static const char *worker_paused_buildings[] = { "architect_guild", "mess_hall", "arena" };
    if (definition_is_any(definition_, worker_paused_buildings, 3) && building.num_workers <= 0) {
        return 1;
    }
    if (definition_is(definition_, "tavern") &&
        (building.num_workers <= 0 || !has_first_distribution_resource(building, definition_))) {
        return 1;
    }
    if (definition_is(definition_, "watchtower") && (building.num_workers <= 0 || !building.figure_id4)) {
        return 1;
    }
    if (definition_is(definition_, "cart_depot") && building.num_workers <= 0) {
        return 1;
    }
    if (definition_ && definition_->is_armoury() && building.num_workers <= 0) {
        return 1;
    }
    if (definition_is(definition_, "amphitheater") && building.num_workers <= 0) {
        return 1;
    }
    return 0;
}

int BuildingAnimation::advance_wine_workshop_offset(
    int animation_cursor, int max_frame, int clamp_to_available) const
{
    const building &building = record();
    const int pct_done = calc_percentage(
        building.data.industry.progress, building_industry_get_max_progress(&building));
    const int current_sprite = map_sprite_animation_at(animation_cursor);
    int new_sprite = 0;
    if (pct_done <= 0) {
        new_sprite = 0;
    } else if (pct_done < 4) {
        new_sprite = 1;
    } else if (pct_done < 8) {
        new_sprite = 2;
    } else if (pct_done < 12) {
        new_sprite = 3;
    } else if (pct_done < 96) {
        const int first_mid_sprite = 4;
        const int last_mid_sprite = clamp_to_available && max_frame < 8 ? max_frame : 8;
        if (current_sprite < first_mid_sprite) {
            new_sprite = first_mid_sprite;
        } else {
            new_sprite = current_sprite + 1;
            if (new_sprite > last_mid_sprite) {
                new_sprite = first_mid_sprite;
            }
        }
    } else {
        const int first_late_sprite = clamp_to_available && max_frame < 9 ? max_frame : 9;
        const int last_late_sprite = clamp_to_available ? max_frame : 12;
        if (current_sprite < first_late_sprite) {
            new_sprite = first_late_sprite;
        } else {
            new_sprite = current_sprite + 1;
            if (new_sprite > last_late_sprite) {
                new_sprite = last_late_sprite;
            }
        }
    }
    map_sprite_animation_set(animation_cursor, new_sprite);
    return new_sprite;
}

int BuildingAnimation::advance_reversible_offset(int animation_cursor, int max_frame) const
{
    int is_reverse = 0;
    if (map_sprite_animation_at(animation_cursor) & 0x80) {
        is_reverse = 1;
    }
    const int current_sprite = map_sprite_animation_at(animation_cursor) & 0x7f;
    int new_sprite = 0;
    if (is_reverse) {
        new_sprite = current_sprite - 1;
        if (new_sprite < 1) {
            new_sprite = 1;
            is_reverse = 0;
        }
    } else {
        new_sprite = current_sprite + 1;
        if (new_sprite > max_frame) {
            new_sprite = max_frame;
            is_reverse = 1;
        }
    }
    map_sprite_animation_set(animation_cursor, is_reverse ? new_sprite | 0x80 : new_sprite);
    return new_sprite;
}

int BuildingAnimation::advance_looping_offset(int animation_cursor, int max_frame)
{
    int new_sprite = map_sprite_animation_at(animation_cursor) + 1;
    if (new_sprite > max_frame) {
        advance_monument_secondary_animation(record());
        new_sprite = 1;
    }
    map_sprite_animation_set(animation_cursor, new_sprite);
    return new_sprite;
}

int BuildingAnimation::runtime_track_offset(const ::RuntimeAnimationTrack &track, int should_advance, int animation_cursor)
{
    if (animation_cursor < 0 || track.num_frames <= 0) {
        return 0;
    }

    if (!should_advance || !track.speed_id) {
        return normalized_animation_frame(animation_cursor, track);
    }
    if (!game_animation_should_advance(track.speed_id)) {
        return normalized_animation_frame(animation_cursor, track);
    }

    if (definition_is(definition_, "wine_workshop")) {
        return advance_wine_workshop_offset(animation_cursor, track.num_frames, 1);
    }
    if (track.can_reverse) {
        return advance_reversible_offset(animation_cursor, track.num_frames);
    }
    return advance_looping_offset(animation_cursor, track.num_frames);
}

int BuildingAnimation::offset_for(const Image &image, int animation_cursor)
{
    const ::image &img = image.legacy();
    int offset = 0;
    if (legacy_gate_offset(animation_cursor, &offset)) {
        return offset;
    }
    if (definition_is(definition_, "colosseum")) {
        // The colosseum uses the terrain image as part of the animated facade, so
        // the legacy cursor is also the map grid offset that must be rewritten.
        map_image_set(animation_cursor, building_image_get(&record()));
    }

    if (!img.animation) {
        return 0;
    }
    if (!game_animation_should_advance(img.animation->speed_id)) {
        return map_sprite_animation_at(animation_cursor) & 0x7f;
    }

    if (definition_is(definition_, "wine_workshop")) {
        return advance_wine_workshop_offset(animation_cursor, img.animation->num_sprites, 0);
    }
    if (img.animation->can_reverse) {
        return advance_reversible_offset(animation_cursor, img.animation->num_sprites);
    }
    return advance_looping_offset(animation_cursor, img.animation->num_sprites);
}

int BuildingAnimation::advance_storage_flag(const Image &image)
{
    building &building = record();
    // Storage yards keep the flag frame on the building data instead of the map
    // sprite byte because their visible tile image is already driven by storage
    // quantity/state.
    const ::image &img = image.legacy();
    if (!img.animation) {
        return 0;
    }
    if (!img.animation->speed_id) {
        return 0;
    }
    if (game_animation_should_advance(img.animation->speed_id)) {
        building.data.warehouse.flag_frame++;
    }

    if (building.data.warehouse.flag_frame > img.animation->num_sprites) {
        building.data.warehouse.flag_frame = 0;
    }
    return building.data.warehouse.flag_frame;
}

int BuildingAnimation::advance_fumigation()
{
    building &building = record();
    // Fumigation predates authored image metadata and uses the fixed legacy
    // animation speed 8 with a 0..5 frame range.
    if (game_animation_should_advance(8)) {
        if (building.fumigation_direction) {
            building.fumigation_frame++;
        } else {
            building.fumigation_frame--;
        }
    }

    if (building.fumigation_frame > 5) {
        building.fumigation_frame = 0;
    }
    return building.fumigation_frame;
}

}
