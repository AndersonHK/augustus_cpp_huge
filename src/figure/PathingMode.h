#pragma once

namespace figure_type_registry_impl {

class PathingMode {
public:
    enum class RoadRequirement {
        AllowsNonRoadMovement,
        RequiresRoadMovement
    };

    enum class ServiceEffectRequirement {
        NoServiceEffect,
        RequiresServiceEffect
    };

    enum class VenueTargetRequirement {
        NoVenueTargets,
        RequiresVenueTargets
    };

    constexpr PathingMode(
        const char *xml_id,
        RoadRequirement road_requirement,
        ServiceEffectRequirement service_effect_requirement,
        VenueTargetRequirement venue_target_requirement)
        : xml_id(xml_id),
          requires_road(road_requirement == RoadRequirement::RequiresRoadMovement),
          requires_service_effect(service_effect_requirement == ServiceEffectRequirement::RequiresServiceEffect),
          requires_venue_targets(venue_target_requirement == VenueTargetRequirement::RequiresVenueTargets)
    {
    }

    // These public attributes are immutable metadata for XML/runtime validation.
    // Profiles store a pointer to one of the declared mode objects below.
    const char *xml_id;
    bool requires_road;
    bool requires_service_effect;
    bool requires_venue_targets;
};

extern const PathingMode VanillaRoaming;
extern const PathingMode SmartService;
extern const PathingMode NearestUnemployed;
extern const PathingMode VenueSeeker;
extern const PathingMode StorageFetch;
extern const PathingMode FollowLeader;
extern const PathingMode StandStill;
extern const PathingMode TransientWander;

const PathingMode *pathing_mode_from_xml_id(const char *xml_id);

} // namespace figure_type_registry_impl
