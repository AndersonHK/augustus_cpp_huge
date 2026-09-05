#pragma once

#include <array>

namespace building_construction {

struct PlacementRotationCandidates {
    std::array<int, 4> rotations{};
    int count = 0;
};

PlacementRotationCandidates placement_rotation_candidates(
    int preferred_rotation,
    int retained_rotation,
    bool has_retained_rotation,
    bool can_rotate);

} // namespace building_construction
