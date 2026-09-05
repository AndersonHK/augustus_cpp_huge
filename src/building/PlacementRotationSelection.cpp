#include "building/PlacementRotationSelection.h"

namespace building_construction {

namespace {

int normalize_rotation(int rotation)
{
    return (rotation % 4 + 4) % 4;
}

void append_unique(PlacementRotationCandidates &result, int rotation)
{
    const int normalized = normalize_rotation(rotation);
    for (int index = 0; index < result.count; ++index) {
        if (result.rotations[index] == normalized) {
            return;
        }
    }
    result.rotations[result.count++] = normalized;
}

} // namespace

PlacementRotationCandidates placement_rotation_candidates(
    int preferred_rotation,
    int retained_rotation,
    bool has_retained_rotation,
    bool can_rotate)
{
    PlacementRotationCandidates result;
    if (!can_rotate) {
        append_unique(result, preferred_rotation);
        return result;
    }

    if (has_retained_rotation) {
        append_unique(result, retained_rotation);
    }
    for (int step = 0; step < 4; ++step) {
        append_unique(result, preferred_rotation + step);
    }
    return result;
}

} // namespace building_construction
