#include "figure/PathingMode.h"

#include <cstring>

namespace figure_type_registry_impl {

// These declarations are the authoritative contract for XML pathing modes.
// Keep each requirement explicit here so adding a mode does not require
// duplicating its behavior in scattered helper functions.
const PathingMode VanillaRoaming(
    "vanilla_roaming",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode SmartService(
    "smart_service",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::RequiresServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode NearestUnemployed(
    "nearest_unemployed",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode VenueSeeker(
    "venue_seeker",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::RequiresVenueTargets);
const PathingMode StorageFetch(
    "storage_fetch",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode FollowLeader(
    "follow_leader",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode StandStill(
    "stand_still",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);
const PathingMode TransientWander(
    "transient_wander",
    PathingMode::RoadRequirement::RequiresRoadMovement,
    PathingMode::ServiceEffectRequirement::NoServiceEffect,
    PathingMode::VenueTargetRequirement::NoVenueTargets);

const PathingMode *pathing_mode_from_xml_id(const char *xml_id)
{
    if (!xml_id) {
        return nullptr;
    }

    const PathingMode *modes[] = {
        &VanillaRoaming,
        &SmartService,
        &NearestUnemployed,
        &VenueSeeker,
        &StorageFetch,
        &FollowLeader,
        &StandStill,
        &TransientWander
    };
    for (const PathingMode *mode : modes) {
        if (std::strcmp(xml_id, mode->xml_id) == 0) {
            return mode;
        }
    }
    return nullptr;
}

} // namespace figure_type_registry_impl
