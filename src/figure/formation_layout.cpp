#include "formation_layout.h"

#include "core/log.h"
#include "core/xml_value.h"

#include <exception>
#include <utility>

namespace {

int clamped_slot_count(int live_slot_count, int declared_capacity)
{
    if (live_slot_count <= 0) {
        return 0;
    }
    if (declared_capacity > 0 && live_slot_count > declared_capacity) {
        return declared_capacity;
    }
    return live_slot_count;
}

} // namespace

FormationLayoutDef::FormationLayoutDef(std::string key, int legacy_id,
    std::vector<FormationLayoutPosition> positions, ArmyOffsets army_offsets, std::string army_offsets_reference,
    int stationary_facing)
    : stationary_facing(stationary_facing), key_(std::move(key)), legacy_id_(legacy_id), positions_(std::move(positions)),
      army_offsets_(std::move(army_offsets)), army_offsets_reference_(std::move(army_offsets_reference))
{}

const char *FormationLayoutDef::key() const
{
    return key_.c_str();
}

bool FormationLayoutDef::matches_key(const char *key) const
{
    return xml_value::equals(key_.c_str(), key);
}

int FormationLayoutDef::legacy_id() const
{
    return legacy_id_;
}

int FormationLayoutDef::authored_position_count() const
{
    return static_cast<int>(positions_.size());
}

bool FormationLayoutDef::try_position(
    int index,
    int declared_capacity,
    int grid_width,
    int grid_height,
    FormationLayoutPosition *position) const
{
    if (!position || index < 0 || (declared_capacity > 0 && index >= declared_capacity)) {
        return false;
    }
    if ((declared_capacity <= 0 || declared_capacity <= authored_position_count()) && index < authored_position_count()) {
        *position = positions_[static_cast<size_t>(index)];
        return true;
    }
    if (grid_width <= 0 || grid_height <= 0 || index >= grid_width * grid_height) {
        return false;
    }
    *position = {index % grid_width, index / grid_width};
    return true;
}

bool FormationLayoutDef::has_authored_army_offsets() const
{
    for (const std::vector<FormationLayoutPosition> &orientation : army_offsets_) {
        if (orientation.empty()) return false;
    }
    return true;
}

const char *FormationLayoutDef::army_offsets_reference() const
{
    return army_offsets_reference_.c_str();
}

void FormationLayoutDef::bind_army_offsets_reference(const FormationLayoutDef *layout)
{
    army_offsets_definition_ = layout;
}

FormationLayoutPosition FormationLayoutDef::army_offset(int orientation, int formation_index) const
{
    const FormationLayoutDef *source = army_offsets_definition_ ? army_offsets_definition_ : this;
    if (orientation < 0 || orientation >= static_cast<int>(source->army_offsets_.size()) || formation_index < 0) {
        log_error("FormationLayout received an invalid army offset request", key_.c_str(), formation_index);
        std::terminate();
    }
    const std::vector<FormationLayoutPosition> &offsets = source->army_offsets_[static_cast<size_t>(orientation)];
    if (formation_index >= static_cast<int>(offsets.size())) {
        log_error("FormationLayout lacks a required army offset", key_.c_str(), formation_index);
        std::terminate();
    }
    return offsets[static_cast<size_t>(formation_index)];
}

bool FormationLayoutDef::positions(
    int live_slot_count,
    int declared_capacity,
    int grid_width,
    int grid_height,
    std::vector<FormationLayoutPosition> *positions) const
{
    if (!positions) {
        return false;
    }
    const int slot_count = clamped_slot_count(live_slot_count, declared_capacity);
    positions->clear();
    positions->reserve(slot_count);
    for (int index = 0; index < slot_count; index++) {
        FormationLayoutPosition position = {};
        if (!try_position(index, declared_capacity, grid_width, grid_height, &position)) {
            positions->clear();
            return false;
        }
        positions->push_back(position);
    }
    return true;
}
