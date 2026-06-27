#pragma once

#include "graphics/runtime_texture.h"

#include <cstddef>
#include <vector>

class Building;
class Image;
struct building;

class Animation {
public:
    static constexpr int TIMER_COUNT = 51;

    static void init();
    static void update_timers();
    static int should_advance(int speed);

    void set_sprite_offset(int x, int y);
    void set_reversible(int value);
    void set_speed_id(int value);
    void add_frame(RuntimeDrawSlice frame);
    void replace_frames(std::vector<RuntimeDrawSlice> frames);
    void clear_frames();

    int frame_count() const;
    int has_frames() const;
    int sprite_offset_x() const;
    int sprite_offset_y() const;
    int can_reverse() const;
    int speed_id() const;
    RuntimeDrawSlice frame_slice_at_offset(int one_based_frame, int include_sprite_offset = 1) const;

private:
    int sprite_offset_x_ = 0;
    int sprite_offset_y_ = 0;
    int can_reverse_ = 0;
    int speed_id_ = 0;
    std::vector<RuntimeDrawSlice> frames_;
};

namespace building_type_registry_impl {

class BuildingType;

// Frame-selection policy for one building instance. Rendering code asks this
// object what frame should draw; the object owns legacy cursor quirks.
class BuildingAnimation {
public:
    explicit BuildingAnimation(Building building);

    int frame_offset(const ::Animation &animation, int should_advance, int animation_cursor);
    int offset_for(const Image &image, int animation_cursor);
    int advance_storage_flag(const Image &image);
    int advance_fumigation();

private:
    building &record();
    const building &record() const;
    const building &state_record() const;

    int legacy_gate_offset(int animation_cursor, int *offset) const;
    int advance_wine_workshop_offset(int animation_cursor, int max_frame, int clamp_to_available) const;
    int advance_reversible_offset(int animation_cursor, int max_frame) const;
    int advance_looping_offset(int animation_cursor, int max_frame);

    ::building *record_;
    const BuildingType *definition_;
    ::building *state_record_;
    const BuildingType *state_definition_;
};

}
