#include "formation_layout.h"

#include "core/log.h"
#include "core/xml_value.h"

#include <exception>
#include <algorithm>
#include <utility>

FormationLayoutDef::FormationLayoutDef(std::string key, int legacy_id, FormationLayoutGeometry geometry, ArmyOffsets army_offsets, std::string army_offsets_reference, int stationary_facing, bool restore_when_idle)
    : stationary_facing(stationary_facing), restore_when_idle(restore_when_idle), key_(std::move(key)), legacy_id_(legacy_id), geometry_(geometry),
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

bool FormationLayoutDef::try_position(int index, int declared_capacity, int grid_width, int grid_height, FormationLayoutPosition *position) const
{
    if (!position || index < 0 || (declared_capacity > 0 && index >= declared_capacity)) {
        return false;
    }
    if (grid_width <= 0 || grid_height <= 0 || index >= grid_width * grid_height) {
        return false;
    }

    // Every population is a prefix of a fixed growth order. Recruitment never
    // moves incumbents, and square prefixes grow along both axes from the anchor.
    const auto centered_coordinate = [](int rank) { return rank % 2 ? (rank + 1) / 2 : -rank / 2; };
    int x = 0;
    int y = 0;
    const bool staggered = geometry_.shape == FormationLayoutShape::Staggered;
    const bool line = staggered || geometry_.shape == FormationLayoutShape::Ranks;
    // Grow rectangles with the layout's aspect ratio, independently of roster
    // capacity. A partially recruited line must not become a small square.
    const int line_scale = staggered ? 4 : 2;
    const int width_growth = line ? line_scale * line_scale : 1;
    const int width_limit = line ? grid_width * grid_height : grid_width;
    const int height_limit = line ? grid_width * grid_height : grid_height;
    {
        int remaining = index;
        for (int shell = 0; shell < std::max(width_limit, height_limit); shell++) {
            const int previous_width = std::min(shell * width_growth, width_limit);
            const int previous_height = std::min(shell, height_limit);
            const int width = std::min((shell + 1) * width_growth, width_limit);
            const int height = std::min(shell + 1, height_limit);
            const int shell_size = width * height - previous_width * previous_height;
            if (remaining >= shell_size) {
                remaining -= shell_size;
                continue;
            }
            const int column_size = (width - previous_width) * previous_height;
            if (remaining < column_size) {
                x = previous_width + remaining / previous_height;
                y = remaining % previous_height;
            } else {
                x = remaining - column_size;
                y = height - 1;
            }
            break;
        }
        if (line || geometry_.centered || geometry_.shape == FormationLayoutShape::Scatter) {
            x = centered_coordinate(x);
            if (!line) y = centered_coordinate(y);
        }
        if (staggered) y = 2 * y + (std::abs(x) % 2);
    }
    if (geometry_.shape == FormationLayoutShape::Scatter && index > 0) {
        // Separate lattice cells keep deterministic jitter unique and reproducible.
        const unsigned int jitter = static_cast<unsigned int>(index) * 2654435761u;
        x = 2 * x + static_cast<int>((jitter >> 16) & 1);
        y = 2 * y + static_cast<int>((jitter >> 24) & 1);
    }
    *position = geometry_.transpose ? FormationLayoutPosition{y, x} : FormationLayoutPosition{x, y};
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
