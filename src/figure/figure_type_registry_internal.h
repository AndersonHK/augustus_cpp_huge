#pragma once

extern "C" {
#include "building/type.h"
#include "figure/type.h"
}

#include "figure/PathingMode.h"
#include "map/road_service_history.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace figure_type_registry_impl {

enum class NativeClassId {
    None,
    RoamingService,
    EngineerService,
    PrefectService,
    EntertainmentService,
    EntertainmentVenueSeeker
};

enum class FigureSlot {
    None,
    Primary,
    Secondary,
    Quaternary
};

enum class OwnerStateRequirement {
    Any,
    InUse,
    InUseOrMothballed
};

enum class ReturnMode {
    None,
    ReturnToOwnerRoad,
    DieAtLimit
};

enum class EntertainmentShowSlot {
    None,
    Days1,
    Days2
};

struct OwnerBinding {
    FigureSlot slot = FigureSlot::None;
    building_type required_building_type = BUILDING_ANY;
    OwnerStateRequirement required_owner_state = OwnerStateRequirement::InUse;
};

struct MovementProfile {
    int terrain_usage = TERRAIN_USAGE_ANY;
    int roam_ticks = 1;
    int max_roam_length = 0;
    ReturnMode return_mode = ReturnMode::ReturnToOwnerRoad;
};

struct GraphicsPolicy {
    int image_group = 0;
    int max_image_offset = 12;
};

struct PathingPolicy {
    const PathingMode *mode = &VanillaRoaming;
    road_service_effect effect = ROAD_SERVICE_EFFECT_NONE;
};

struct EntertainmentVenueTarget {
    building_type building = BUILDING_NONE;
    EntertainmentShowSlot show_slot = EntertainmentShowSlot::None;
};

class FigureTypeProfile {
public:
    explicit FigureTypeProfile(std::string id);

    const char *id() const;

    void set_native_class(NativeClassId native_class_id);
    NativeClassId native_class() const;

    void set_owner_binding(const OwnerBinding &owner_binding);
    const OwnerBinding &owner_binding() const;

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_pathing_policy(const PathingPolicy &pathing_policy);
    const PathingPolicy &pathing_policy() const;

    void set_show_duration(int show_duration);
    int show_duration() const;
    void add_venue_target(const EntertainmentVenueTarget &target);
    const std::vector<EntertainmentVenueTarget> &venue_targets() const;

private:
    std::string id_;
    NativeClassId native_class_id_ = NativeClassId::None;
    OwnerBinding owner_binding_;
    MovementProfile movement_profile_;
    PathingPolicy pathing_policy_;
    int show_duration_ = 32;
    std::vector<EntertainmentVenueTarget> venue_targets_;
};

class FigureTypeDefinition {
public:
    FigureTypeDefinition(figure_type type, std::string attr);

    figure_type type() const;
    const char *attr() const;

    void set_native_class(NativeClassId native_class_id);
    NativeClassId native_class() const;

    void set_owner_binding(const OwnerBinding &owner_binding);
    const OwnerBinding &owner_binding() const;

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_graphics_policy(const GraphicsPolicy &graphics_policy);
    const GraphicsPolicy &graphics_policy() const;

    void set_pathing_policy(const PathingPolicy &pathing_policy);
    const PathingPolicy &pathing_policy() const;

    void set_default_profile_id(std::string profile_id);
    const char *default_profile_id() const;
    FigureTypeProfile &add_profile(std::string profile_id);
    FigureTypeProfile *last_profile();
    const FigureTypeProfile *profile(const char *profile_id) const;
    const FigureTypeProfile *default_profile() const;
    const std::vector<FigureTypeProfile> &profiles() const;

private:
    figure_type type_ = FIGURE_NONE;
    std::string attr_;
    NativeClassId native_class_id_ = NativeClassId::None;
    OwnerBinding owner_binding_;
    MovementProfile movement_profile_;
    GraphicsPolicy graphics_policy_;
    PathingPolicy pathing_policy_;
    std::string default_profile_id_;
    std::vector<FigureTypeProfile> profiles_;
};

extern std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
extern std::string g_failure_reason;

int directory_exists(const char *path);
void set_failure_reason(const char *message, const char *detail = nullptr);
std::vector<std::string> build_candidate_definition_paths();
const FigureTypeDefinition *definition_for(figure_type type);
const FigureTypeProfile *profile_for(figure_type type, const char *profile_id);
const FigureTypeProfile *default_profile_for(figure_type type);

} // namespace figure_type_registry_impl
