#include "building/animations.h"

#include "assets/assets.h"
#include "assets/image_group_entry.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"

extern "C" {
#include "building/building.h"
#include "building/count.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/festival.h"
#include "core/calc.h"
#include "core/image.h"
#include "game/animation.h"
#include "map/image.h"
#include "map/sprite.h"
#include "scenario/property.h"
}

#include <utility>

namespace building_type_registry_impl {

namespace {

void advance_monument_secondary_animation(building &building)
{
    if (building.type == BUILDING_GRAND_TEMPLE_CERES && building.monument.upgrades == 1) {
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

}

void GraphicsTarget::set_path(std::string path)
{
    path_ = std::move(path);
}

void GraphicsTarget::set_image(std::string image)
{
    image_ = std::move(image);
}

GraphicsTarget &GraphicsTarget::add_option()
{
    options_.emplace_back();
    return options_.back();
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

int GraphicsTarget::has_options() const
{
    return !options_.empty();
}

int GraphicsTarget::option_count() const
{
    return static_cast<int>(options_.size());
}

GraphicsTarget GraphicsTarget::resolved_option(unsigned char variant) const
{
    if (options_.empty()) {
        return *this;
    }

    // Options are authored as partial targets. Materialize one effective target so
    // the renderer and validator can keep using the normal path/image lookup path.
    GraphicsTarget resolved = options_[variant % options_.size()];
    if (!resolved.has_path()) {
        resolved.set_path(path_);
    }
    return resolved;
}

int GraphicsCondition::matches(const ::building &building) const
{
    switch (type) {
        case GraphicsConditionType::HasWorkers:
            return building.num_workers > 0;
        case GraphicsConditionType::Working:
            return building.num_workers > 0 && building.has_water_access;
        case GraphicsConditionType::WaterAccess:
            return building.has_water_access;
        case GraphicsConditionType::FigureSlotOccupied:
            switch (figure_slot) {
                case FigureSlot::Primary:
                    return building.figure_id > 0;
                case FigureSlot::Secondary:
                    return building.figure_id2 > 0;
                case FigureSlot::Quaternary:
                    return building.figure_id4 > 0;
                case FigureSlot::None:
                default:
                    return 0;
            }
        case GraphicsConditionType::ResourcePositive:
            return resource > RESOURCE_NONE && resource < RESOURCE_MAX && building.resources[resource] > 0;
        case GraphicsConditionType::Climate:
            return scenario_property_climate() == climate;
        case GraphicsConditionType::MonumentUpgrade:
            return building.monument.upgrades == monument_upgrade;
        case GraphicsConditionType::FestivalGames:
            return city_festival_games_active() == festival_games;
        case GraphicsConditionType::Days1Positive:
            return building.data.entertainment.days1 > 0;
        case GraphicsConditionType::Days1NotPositive:
            return building.data.entertainment.days1 <= 0;
        case GraphicsConditionType::Days2Positive:
            return building.data.entertainment.days2 > 0;
        case GraphicsConditionType::Days1OrDays2Positive:
            return building.data.entertainment.days1 > 0 || building.data.entertainment.days2 > 0;
        case GraphicsConditionType::Desirability:
            switch (comparison) {
                case GraphicComparison::LessThan:
                    return building.desirability < threshold;
                case GraphicComparison::LessThanOrEqual:
                    return building.desirability <= threshold;
                case GraphicComparison::Equal:
                    return building.desirability == threshold;
                case GraphicComparison::GreaterThan:
                    return building.desirability > threshold;
                case GraphicComparison::GreaterThanOrEqual:
                    return building.desirability >= threshold;
                case GraphicComparison::None:
                default:
                    return 0;
            }
        case GraphicsConditionType::None:
        default:
            return 0;
    }
}

int GraphicsVariant::matches(const ::building &building) const
{
    for (const GraphicsCondition &condition : conditions) {
        if (!condition.matches(building)) {
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
    return default_target_.has_path();
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

const GraphicsTarget *GraphicsDefinition::resolve_target(const ::building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
        if (variant.target.has_path() && variant.matches(building)) {
            return &variant.target;
        }
    }
    if (default_target_.has_path()) {
        return &default_target_;
    }
    return nullptr;
}

unsigned char GraphicsDefinition::upgrade_level_for(const ::building &building) const
{
    for (const GraphicsVariant &variant : variants_) {
        if (!variant.target.has_path() || !variant.matches(building)) {
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

BuildingAnimation::BuildingAnimation(::building &building)
    : BuildingAnimation(building, definition_for_type(building.type))
{
}

BuildingAnimation::BuildingAnimation(::building &building, const BuildingType *definition)
    : building_(building)
    , definition_(definition)
{
}

int BuildingAnimation::gated_offset(int animation_cursor, int *offset) const
{
    *offset = 0;
    // These gates deliberately mirror the old animation.c decisions: XML-authored
    // animations and legacy image animations must both pause for the same runtime
    // building states, otherwise placing a migrated graphic changes gameplay cues.
    if (definition_ && definition_->water_access().has_requirements() && !building_.has_water_access) {
        return 1;
    }
    if (building_is_workshop(building_.type)) {
        if (building_.num_workers <= 0 || building_.strike_duration_days > 0 ||
            !building_industry_has_raw_materials_for_production(&building_)) {
            return 1;
        }
    }
    if (building_.type == BUILDING_CONCRETE_MAKER) {
        if (building_.data.industry.progress == 0) {
            return 1;
        }
    }
    if ((building_.type == BUILDING_PREFECTURE || building_.type == BUILDING_ENGINEERS_POST) &&
        building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_MARKET && building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_WAREHOUSE && building_.num_workers < model_get_building(building_.type)->laborers) {
        return 1;
    }
    if (building_.type == BUILDING_DOCK && building_.data.dock.num_ships <= 0) {
        map_sprite_animation_set(animation_cursor, 1);
        *offset = 1;
        return 1;
    }
    if (building_is_raw_resource_producer(building_.type) &&
        (building_.num_workers <= 0 || building_.strike_duration_days > 0)) {
        return 1;
    }
    if (building_.type == BUILDING_GLADIATOR_SCHOOL) {
        if (building_.num_workers <= 0) {
            map_sprite_animation_set(animation_cursor, 1);
            *offset = 1;
            return 1;
        }
    } else if ((building_.type == BUILDING_THEATER ||
        (building_.type >= BUILDING_HIPPODROME && building_.type <= BUILDING_CHARIOT_MAKER)) &&
        building_.type != BUILDING_HIPPODROME && building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_GRANARY && building_.num_workers < model_get_building(building_.type)->laborers) {
        return 1;
    }
    if (building_monument_is_monument(&building_) &&
        (building_.type != BUILDING_ORACLE && building_.type != BUILDING_NYMPHAEUM &&
            (building_.num_workers <= 0 || building_.monument.phase != MONUMENT_FINISHED))) {
        return 1;
    }
    if (building_.type == BUILDING_CITY_MINT &&
        ((building_.output_resource_id == RESOURCE_DENARII &&
            building_.resources[RESOURCE_GOLD] < BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN) ||
            building_.num_workers <= 0 ||
            (building_count_active(BUILDING_SENATE) == 0))) {
        return 1;
    }
    if ((building_.type == BUILDING_ARCHITECT_GUILD || building_.type == BUILDING_MESS_HALL ||
        building_.type == BUILDING_ARENA) && building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_TAVERN && (building_.num_workers <= 0 || !building_.resources[RESOURCE_WINE])) {
        return 1;
    }
    if (building_.type == BUILDING_WATCHTOWER && (building_.num_workers <= 0 || !building_.figure_id4)) {
        return 1;
    }
    if (building_.type == BUILDING_DEPOT && building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_ARMOURY && building_.num_workers <= 0) {
        return 1;
    }
    if (building_.type == BUILDING_AMPHITHEATER && building_.num_workers <= 0) {
        return 1;
    }
    return 0;
}

int BuildingAnimation::advance_wine_workshop_offset(
    int animation_cursor, int max_frame, int clamp_to_available) const
{
    const int pct_done = calc_percentage(
        building_.data.industry.progress, building_industry_get_max_progress(&building_));
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

int BuildingAnimation::advance_looping_offset(int animation_cursor, int max_frame) const
{
    int new_sprite = map_sprite_animation_at(animation_cursor) + 1;
    if (new_sprite > max_frame) {
        advance_monument_secondary_animation(building_);
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

    int offset = 0;
    if (gated_offset(animation_cursor, &offset)) {
        return offset;
    }

    if (!should_advance || !track.speed_id) {
        return normalized_animation_frame(animation_cursor, track);
    }
    if (!game_animation_should_advance(track.speed_id)) {
        return normalized_animation_frame(animation_cursor, track);
    }

    if (building_.type == BUILDING_WINE_WORKSHOP) {
        return advance_wine_workshop_offset(animation_cursor, track.num_frames, 1);
    }
    if (track.can_reverse) {
        return advance_reversible_offset(animation_cursor, track.num_frames);
    }
    return advance_looping_offset(animation_cursor, track.num_frames);
}

int BuildingAnimation::legacy_offset(int image_id, int animation_cursor)
{
    return legacy_offset_for_image(image_get(image_id), animation_cursor);
}

int BuildingAnimation::legacy_offset_for_image(const ::image *img, int animation_cursor)
{
    if (!img) {
        return 0;
    }
    int offset = 0;
    if (gated_offset(animation_cursor, &offset)) {
        return offset;
    }
    if (building_.type == BUILDING_COLOSSEUM) {
        // The colosseum uses the terrain image as part of the animated facade, so
        // the legacy cursor is also the map grid offset that must be rewritten.
        map_image_set(animation_cursor, building_image_get(&building_));
    }

    if (!img->animation) {
        return 0;
    }
    if (!game_animation_should_advance(img->animation->speed_id)) {
        return map_sprite_animation_at(animation_cursor) & 0x7f;
    }

    if (building_.type == BUILDING_WINE_WORKSHOP) {
        return advance_wine_workshop_offset(animation_cursor, img->animation->num_sprites, 0);
    }
    if (img->animation->can_reverse) {
        return advance_reversible_offset(animation_cursor, img->animation->num_sprites);
    }
    return advance_looping_offset(animation_cursor, img->animation->num_sprites);
}

int BuildingAnimation::advance_storage_flag(int image_id)
{
    // Storage yards keep the flag frame on the building data instead of the map
    // sprite byte because their visible tile image is already driven by storage
    // quantity/state.
    const image *img = assets_get_image(image_id);
    if (!img || !img->animation) {
        return 0;
    }
    if (!img->animation->speed_id) {
        return 0;
    }
    if (game_animation_should_advance(img->animation->speed_id)) {
        building_.data.warehouse.flag_frame++;
    }

    if (building_.data.warehouse.flag_frame > img->animation->num_sprites) {
        building_.data.warehouse.flag_frame = 0;
    }
    return building_.data.warehouse.flag_frame;
}

int BuildingAnimation::advance_fumigation()
{
    // Fumigation predates authored image metadata and uses the fixed legacy
    // animation speed 8 with a 0..5 frame range.
    if (game_animation_should_advance(8)) {
        if (building_.fumigation_direction) {
            building_.fumigation_frame++;
        } else {
            building_.fumigation_frame--;
        }
    }

    if (building_.fumigation_frame > 5) {
        building_.fumigation_frame = 0;
    }
    return building_.fumigation_frame;
}

}
