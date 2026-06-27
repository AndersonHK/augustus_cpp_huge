#pragma once

#include "assets/assets.h"
#include "building/building_type.h"
#include "figure/type.h"
#include "graphics/renderer.h"

#include "figure/PathingMode.h"

#include <array>
#include <memory>
#include <string>
#include <vector>

namespace figure_type_registry_impl {

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

enum class CartGraphicsMode {
    None,
    ResourceLoad
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
};

struct MovementProfile {
    int roam_ticks = 1;
    int max_roam_length = 0;
    ReturnMode return_mode = ReturnMode::ReturnToOwnerRoad;
};

struct GraphicsPolicy {
    int image_group = 0;
    asset_id image_asset = ASSET_MAX_KEY;
    std::string path_pattern;
    std::string image_pattern;
    int has_sprite_offset = 0;
    int sprite_offset_x = 0;
    int sprite_offset_y = 0;
    render_logical_size fixed_logical_size = {};
    render_scaling_policy scaling_policy = RENDER_SCALING_POLICY_AUTO;
    int action_state = 0;
    int action_min_wait_ticks = 0;
    std::string action_path_pattern;
    std::string action_image_pattern;
    int image_group_offset = 0;
    int max_image_offset = 12;
    int direction_frame_stride = 8;
    int static_frame_count = 0;
    int corpse_image_group = 0;
    asset_id corpse_image_asset = ASSET_MAX_KEY;
    std::string corpse_path_pattern;
    std::string corpse_image_pattern;
    int corpse_image_group_offset = 96;
    int corpse_frame_count = 0;
    CartGraphicsMode cart_mode = CartGraphicsMode::None;
    std::array<int, 8> cart_offsets_x = {};
    std::array<int, 8> cart_offsets_y = {};
    int cart_high_load_threshold = 0;
    int cart_high_load_y_adjust = 0;
    int cart_direction_3_y_adjust = 0;

    int default_source_count() const;
    int corpse_source_count() const;
    int has_native_payload() const;
    int has_action_native_payload() const;
    int has_corpse_native_payload() const;
    int action_graphics_matches(int figure_action_state, int wait_ticks) const;
    int has_fixed_logical_size() const;
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

    void set_movement_profile(const MovementProfile &movement_profile);
    const MovementProfile &movement_profile() const;

    void set_pathing_policy(const PathingPolicy &pathing_policy);
    const PathingPolicy &pathing_policy() const;

    void set_show_duration(int show_duration);
    int show_duration() const;
    void add_venue_target(const EntertainmentVenueTarget &target);
    const std::vector<EntertainmentVenueTarget> &venue_targets() const;
    int resolve_building_references(const char *figure_attr);
    ProfileSpawnBehavior spawn_behavior() const;

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
    int resolve_building_references();

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

void set_failure_reason(const char *message, const char *detail = nullptr);
std::vector<std::string> build_candidate_definition_paths();
const FigureTypeDefinition *definition_for(figure_type type);
const FigureTypeProfile *profile_for(figure_type type, const char *profile_id);
const FigureTypeProfile *default_profile_for(figure_type type);

} // namespace figure_type_registry_impl
