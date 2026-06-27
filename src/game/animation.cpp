#include "game/Animation.h"

#ifndef STARTUP_PARSER_TEST
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/time.h"
#include "game/resource.h"
#include "graphics/image.h"
#include "graphics/window.h"
#include "map/image.h"
#include "map/sprite.h"
#include "window/city.h"
#endif

#include <utility>

#ifndef STARTUP_PARSER_TEST
namespace {

struct AnimationTimer {
    time_millis last_update = 0;
    int should_update = 0;
};

AnimationTimer timers[Animation::TIMER_COUNT];

int animation_view_is_active()
{
    return window_city_is_window_cityview() ||
        window_is(WINDOW_EMPIRE) ||
        window_is(WINDOW_EDITOR_MAP) ||
        window_is(WINDOW_TRADE_OPENED) ||
        window_is(WINDOW_EDITOR_EMPIRE);
}

}
#endif

void Animation::init()
{
#ifndef STARTUP_PARSER_TEST
    for (AnimationTimer &timer : timers) {
        timer.last_update = 0;
        timer.should_update = 0;
    }
#endif
}

void Animation::update_timers()
{
#ifndef STARTUP_PARSER_TEST
    const time_millis now_millis = time_get_millis();
    for (AnimationTimer &timer : timers) {
        timer.should_update = 0;
    }

    unsigned int delay_millis = 0;
    for (AnimationTimer &timer : timers) {
        if (now_millis - timer.last_update >= delay_millis && animation_view_is_active()) {
            timer.should_update = 1;
            timer.last_update = now_millis;
        }
        delay_millis += 20;
    }
#endif
}

int Animation::should_advance(int speed)
{
#ifndef STARTUP_PARSER_TEST
    if (speed < 0 || speed >= TIMER_COUNT) {
        return 0;
    }
    return timers[speed].should_update;
#else
    (void) speed;
    return 0;
#endif
}

void Animation::set_sprite_offset(int x, int y)
{
    sprite_offset_x_ = x;
    sprite_offset_y_ = y;
}

void Animation::set_reversible(int value)
{
    can_reverse_ = value ? 1 : 0;
}

void Animation::set_speed_id(int value)
{
    speed_id_ = value;
}

void Animation::add_frame(RuntimeDrawSlice frame)
{
    frames_.push_back(std::move(frame));
}

void Animation::replace_frames(std::vector<RuntimeDrawSlice> frames)
{
    frames_ = std::move(frames);
}

void Animation::clear_frames()
{
    frames_.clear();
}

int Animation::frame_count() const
{
    return static_cast<int>(frames_.size());
}

int Animation::has_frames() const
{
    return !frames_.empty();
}

int Animation::sprite_offset_x() const
{
    return sprite_offset_x_;
}

int Animation::sprite_offset_y() const
{
    return sprite_offset_y_;
}

int Animation::can_reverse() const
{
    return can_reverse_;
}

int Animation::speed_id() const
{
    return speed_id_;
}

RuntimeDrawSlice Animation::frame_slice_at_offset(int one_based_frame, int include_sprite_offset) const
{
    if (one_based_frame <= 0 || static_cast<size_t>(one_based_frame) > frames_.size()) {
        return RuntimeDrawSlice();
    }

    RuntimeDrawSlice frame_slice = frames_[static_cast<size_t>(one_based_frame - 1)];
    if (include_sprite_offset) {
        frame_slice.draw_offset_x += sprite_offset_x_;
        frame_slice.draw_offset_y += sprite_offset_y_;
    }
    return frame_slice;
}

#ifndef STARTUP_PARSER_TEST
namespace building_type_registry_impl {

namespace {

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
    if (type_attr_is(building.type, "grand_temple_ceres") && building.monument.upgrades == 1) {
        building.monument.secondary_frame++;
        if (building.monument.secondary_frame > 4) {
            building.monument.secondary_frame = 0;
        }
    }
}

int normalized_animation_frame(int animation_cursor, const Animation &animation)
{
    if (animation_cursor < 0 || animation.frame_count() <= 0) {
        return 0;
    }
    // The map sprite animation byte is 1-based; bit 7 stores reverse playback
    // state for ping-pong animations and must not be treated as part of the frame.
    int current_frame = map_sprite_animation_at(animation_cursor) & 0x7f;
    if (current_frame < 1 || current_frame > animation.frame_count()) {
        current_frame = 1;
        map_sprite_animation_set(animation_cursor, current_frame);
    }
    return current_frame;
}

} // namespace

BuildingAnimation::BuildingAnimation(Building building)
    : record_(building.record_)
    , definition_(building.type)
    , state_record_(building.record_)
    , state_definition_(building.type)
{
    Building owner = building.composition_owner();
    if (owner.id()) {
        state_record_ = owner.record_;
        state_definition_ = owner.type ? owner.type : definition_;
    }
}

building &BuildingAnimation::record()
{
    return *record_;
}

const building &BuildingAnimation::record() const
{
    return *record_;
}

const building &BuildingAnimation::state_record() const
{
    return *state_record_;
}

int BuildingAnimation::legacy_gate_offset(int animation_cursor, int *offset) const
{
    const building &building = state_record();
    const BuildingType *definition = state_definition_;

    *offset = 0;
    // These gates deliberately mirror the old animation decisions: XML-authored
    // animations and legacy image animations must both pause for the same runtime
    // building states, otherwise placing a migrated graphic changes gameplay cues.
    if (definition && definition->water_access().has_requirements() && !building.has_water_access) {
        return 1;
    }
    if (building_is_workshop(building.type)) {
        if (building.num_workers <= 0 || building.strike_duration_days > 0 ||
            !building_industry_has_raw_materials_for_production(&building)) {
            return 1;
        }
    }
    if (definition && definition->attr_is("concrete_maker")) {
        if (building.data.industry.progress == 0) {
            return 1;
        }
    }
    static const char *worker_paused_service_posts[] = { "prefecture", "engineers_post" };
    if (definition && type_attr_is_any(definition->type(), worker_paused_service_posts, 2) &&
        building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->has_market() && building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->is_warehouse() && building.num_workers < model_get_building(building.type)->laborers) {
        return 1;
    }
    if (definition && definition->attr_is("dock") && building.data.dock.num_ships <= 0) {
        map_sprite_animation_set(animation_cursor, 1);
        *offset = 1;
        return 1;
    }
    if (building_is_raw_resource_producer(building.type) &&
        (building.num_workers <= 0 || building.strike_duration_days > 0)) {
        return 1;
    }
    if (definition && definition->attr_is("gladiator_school")) {
        if (building.num_workers <= 0) {
            map_sprite_animation_set(animation_cursor, 1);
            *offset = 1;
            return 1;
        }
    } else if (((definition && definition->is_theater()) ||
        (definition && definition->attr_is("chariot_maker"))) &&
        !(definition && definition->attr_is("hippodrome")) && building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->is_granary() && building.num_workers < model_get_building(building.type)->laborers) {
        return 1;
    }
    if (building_monument_is_monument(&building) &&
        (!(definition && definition->is_oracle()) && !(definition && definition->attr_is("nymphaeum")) &&
            (building.num_workers <= 0 || building.monument.phase != MONUMENT_FINISHED))) {
        return 1;
    }
    if (definition && definition->attr_is("city_mint") &&
        ((building.output_resource_id == resource_denarii() &&
            !Building(const_cast<::building *>(&building)).native_production_has_raw_materials()) ||
            building.num_workers <= 0 ||
            (building_count_active(type_from_attr("senate")) == 0))) {
        return 1;
    }
    static const char *worker_paused_buildings[] = { "architect_guild", "mess_hall", "arena" };
    if (definition && type_attr_is_any(definition->type(), worker_paused_buildings, 3) &&
        building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->attr_is("tavern") &&
        (building.num_workers <= 0 || !has_first_distribution_resource(building, definition))) {
        return 1;
    }
    if (definition && definition->attr_is("watchtower") && (building.num_workers <= 0 || !building.figure_id4)) {
        return 1;
    }
    if (definition && definition->attr_is("cart_depot") && building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->is_armoury() && building.num_workers <= 0) {
        return 1;
    }
    if (definition && definition->attr_is("amphitheater") && building.num_workers <= 0) {
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

int BuildingAnimation::frame_offset(const ::Animation &animation, int should_advance, int animation_cursor)
{
    if (animation_cursor < 0 || animation.frame_count() <= 0) {
        return 0;
    }

    if (!should_advance || !animation.speed_id()) {
        return normalized_animation_frame(animation_cursor, animation);
    }
    if (!Animation::should_advance(animation.speed_id())) {
        return normalized_animation_frame(animation_cursor, animation);
    }

    if (definition_ && definition_->attr_is("wine_workshop")) {
        return advance_wine_workshop_offset(animation_cursor, animation.frame_count(), 1);
    }
    if (animation.can_reverse()) {
        return advance_reversible_offset(animation_cursor, animation.frame_count());
    }
    return advance_looping_offset(animation_cursor, animation.frame_count());
}

int BuildingAnimation::offset_for(const Image &image, int animation_cursor)
{
    const ::image &img = image.legacy();
    int offset = 0;
    if (legacy_gate_offset(animation_cursor, &offset)) {
        return offset;
    }
    if (definition_ && definition_->attr_is("colosseum")) {
        // The colosseum uses the terrain image as part of the animated facade, so
        // the legacy cursor is also the map grid offset that must be rewritten.
        map_image_set(animation_cursor, building_image_get(&record()));
    }

    if (!img.animation) {
        return 0;
    }
    if (!Animation::should_advance(img.animation->speed_id)) {
        return map_sprite_animation_at(animation_cursor) & 0x7f;
    }

    if (definition_ && definition_->attr_is("wine_workshop")) {
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
    if (Animation::should_advance(img.animation->speed_id)) {
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
    if (Animation::should_advance(8)) {
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

} // namespace building_type_registry_impl
#endif
