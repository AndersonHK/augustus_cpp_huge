#pragma once

#include "building/building_type.h"
#include "figure/FigureGraphics.h"
#include "figure/PathingMode.h"
#include "figure/type.h"
#include "graphics/image.h"

#include <memory>
#include <string>
#include <vector>

namespace figure_type_registry_impl {

class FigureTypeDefinition;
int action_state_from_xml_name(const char *name);

enum class NativeClassId {
    None,
    LegacyAction,
    RoamingService,
    EngineerService,
    PrefectService,
    EntertainmentService,
    EntertainmentVenueSeeker,
    MarketSupplier,
    DeliveryFollower,
    TransientWanderer,
    DepotCartPusher,
    FishingBoat
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
    building_type required_building_type = BUILDING_NONE;
    std::string required_building_reference;
    OwnerStateRequirement required_owner_state = OwnerStateRequirement::InUse;

    building_type resolved_required_building_type() const;
    bool requires_owner() const;
};

struct MovementProfile {
    int roam_ticks = 1;
    int max_roam_length = 0;
    ReturnMode return_mode = ReturnMode::ReturnToOwnerRoad;
};

struct EntertainmentVenueTarget {
    building_type building = BUILDING_NONE;
    std::string building_reference;
    EntertainmentShowSlot show_slot = EntertainmentShowSlot::None;

    building_type resolved_building_type() const;
};

struct ProfileSpawnBehavior {
    int action_state = 0;
    bool has_action_state = false;
    bool init_roaming = false;
};

class FigureTypeProfile {
public:
    explicit FigureTypeProfile(std::string id);

    const char *id() const;

    void set_native_class(NativeClassId native_class_id);
    NativeClassId native_class() const;

    void set_owner_binding(const OwnerBinding &owner_binding);
    const OwnerBinding &owner_binding() const;
    bool requires_owner() const;

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_pathing_policy(const PathingPolicy &pathing_policy);
    const PathingPolicy &pathing_policy() const;

    void set_show_duration(int show_duration);
    int show_duration() const;
    void add_venue_target(const EntertainmentVenueTarget &target);
    const std::vector<EntertainmentVenueTarget> &venue_targets() const;
    void set_explicit_spawn_behavior(const ProfileSpawnBehavior &spawn_behavior);
    bool has_explicit_spawn_behavior() const;
    int resolve_building_references(const char *figure_attr);
    ProfileSpawnBehavior spawn_behavior() const;

private:
    std::string id_;
    NativeClassId native_class_id_ = NativeClassId::None;
    OwnerBinding owner_binding_;
    MovementProfile movement_profile_;
    PathingPolicy pathing_policy_;
    ProfileSpawnBehavior explicit_spawn_behavior_;
    bool has_explicit_spawn_behavior_ = false;
    int show_duration_ = 32;
    std::vector<EntertainmentVenueTarget> venue_targets_;
};

class FigureTypeDefinition {
public:
    FigureTypeDefinition(figure_type type, std::string attr);

    figure_type type() const;
    const char *attr() const;

    void set_presentation(std::string name_key, ImageGroupEntryRef portrait) { name_key_ = std::move(name_key); portrait_ = std::move(portrait); }
    const std::string &name_key() const { return name_key_; }
    const ImageGroupEntryRef &portrait() const { return portrait_; }

    void set_native_class(NativeClassId native_class_id);
    NativeClassId native_class() const;

    void set_owner_binding(const OwnerBinding &owner_binding);
    const OwnerBinding &owner_binding() const;

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_graphics(const FigureGraphics &graphics);
    const FigureGraphics &graphics() const;
    int cache_graphics_bindings();
    const GraphicsTargetBinding *graphics_binding(GraphicsTargetRole role, int direction_index, int frame) const;
    const GraphicsTargetBinding *graphics_binding_for_state(int figure_action_state, int wait_ticks, int image_offset, int corpse_frame_offset, int direction_index) const;

    void set_pathing_policy(const PathingPolicy &pathing_policy);
    const PathingPolicy &pathing_policy() const;

    void set_default_profile_id(std::string profile_id);
    const char *default_profile_id() const;
    FigureTypeProfile &add_profile(std::string profile_id);
    FigureTypeProfile *last_profile();
    const FigureTypeProfile *profile(const char *profile_id) const;
    const FigureTypeProfile *default_profile() const;
    const std::vector<FigureTypeProfile> &profiles() const;
    int resolve_building_references();

private:
    figure_type type_ = FIGURE_NONE;
    std::string attr_;
    std::string name_key_;
    ImageGroupEntryRef portrait_;
    NativeClassId native_class_id_ = NativeClassId::None;
    OwnerBinding owner_binding_;
    MovementProfile movement_profile_;
    std::shared_ptr<const FigureGraphics> graphics_;
    PathingPolicy pathing_policy_;
    std::string default_profile_id_;
    std::vector<FigureTypeProfile> profiles_;
};

extern std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
extern std::string g_failure_reason;

void set_failure_reason(const char *message, const char *detail = nullptr);
std::vector<std::string> build_candidate_definition_paths();
const FigureTypeDefinition *definition_for(figure_type type);
const FigureGraphics *graphics_for(figure_type type);
const FigureTypeProfile *profile_for(figure_type type, const char *profile_id);
const FigureTypeProfile *default_profile_for(figure_type type);

} // namespace figure_type_registry_impl
