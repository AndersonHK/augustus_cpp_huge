#

#include "figure/figure_type_registry_internal.h"
#include "figure/figure_type_registry.h"

#include "assets/image_group_payload.h"
#include "building/building_type_registry_internal.h"
#include "core/crash_context.h"
#include "core/direction.h"
#include "core/xml_value.h"
#include "figure/action.h"
#include "game/mod_definition_loader.h"

#include "core/log.h"
#include "building/properties.h"
#include "core/image.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_graphics_only;
std::string g_failure_reason;
mod_definition::DefinitionOverlayTracker g_figure_type_overlays;

static int building_reference_is_any_or_empty(const std::string &reference)
{
    return reference.empty() || xml_value::equals(reference.c_str(), "any");
}

static int resolve_building_reference(
    const std::string &reference,
    building_type &out_type,
    const char *context,
    int allow_any)
{
    if (building_reference_is_any_or_empty(reference)) {
        out_type = BUILDING_NONE;
        if (allow_any || reference.empty()) {
            return 1;
        }
    } else {
        out_type = building_type_registry_impl::runtime_id_from_text(reference.c_str());
        if (out_type != BUILDING_NONE) {
            return 1;
        }
    }

    std::string detail = context ? context : "";
    if (!detail.empty()) {
        detail += " ";
    }
    detail += "building=";
    detail += reference;
    set_failure_reason("FigureType building reference is unknown.", detail.c_str());
    log_error("FigureType building reference is unknown", detail.c_str(), 0);
    return 0;
}

building_type OwnerBinding::resolved_required_building_type() const
{
    if (required_building_type != BUILDING_NONE || required_building_reference.empty()) {
        return required_building_type;
    }
    return building_type_registry_impl::runtime_id_from_text(required_building_reference.c_str());
}

bool OwnerBinding::requires_owner() const
{
    return slot != FigureSlot::None ||
        resolved_required_building_type() != BUILDING_NONE ||
        !required_building_reference.empty() ||
        required_owner_state != OwnerStateRequirement::Any;
}

building_type EntertainmentVenueTarget::resolved_building_type() const
{
    if (building != BUILDING_NONE || building_reference.empty()) {
        return building;
    }
    return building_type_registry_impl::runtime_id_from_text(building_reference.c_str());
}

FigureTypeProfile::FigureTypeProfile(std::string id)
    : id_(std::move(id))
{
}

const char *FigureTypeProfile::id() const
{
    return id_.c_str();
}

void FigureTypeProfile::set_native_class(NativeClassId native_class_id)
{
    native_class_id_ = native_class_id;
}

NativeClassId FigureTypeProfile::native_class() const
{
    return native_class_id_;
}

void FigureTypeProfile::set_owner_binding(const OwnerBinding &owner_binding)
{
    owner_binding_ = owner_binding;
}

const OwnerBinding &FigureTypeProfile::owner_binding() const
{
    return owner_binding_;
}

bool FigureTypeProfile::requires_owner() const
{
    return owner_binding_.requires_owner();
}

void FigureTypeProfile::set_movement_profile(const MovementProfile &movement_profile)
{
    movement_profile_ = movement_profile;
}

const MovementProfile &FigureTypeProfile::movement_profile() const
{
    return movement_profile_;
}

void FigureTypeProfile::set_pathing_policy(const PathingPolicy &pathing_policy)
{
    pathing_policy_ = pathing_policy;
}

const PathingPolicy &FigureTypeProfile::pathing_policy() const
{
    return pathing_policy_;
}

void FigureTypeProfile::set_show_duration(int show_duration)
{
    show_duration_ = show_duration;
}

int FigureTypeProfile::show_duration() const
{
    return show_duration_;
}

void FigureTypeProfile::add_venue_target(const EntertainmentVenueTarget &target)
{
    venue_targets_.push_back(target);
}

const std::vector<EntertainmentVenueTarget> &FigureTypeProfile::venue_targets() const
{
    return venue_targets_;
}

void FigureTypeProfile::set_explicit_spawn_behavior(const ProfileSpawnBehavior &spawn_behavior)
{
    explicit_spawn_behavior_ = spawn_behavior;
    has_explicit_spawn_behavior_ = true;
}

bool FigureTypeProfile::has_explicit_spawn_behavior() const
{
    return has_explicit_spawn_behavior_;
}

ProfileSpawnBehavior FigureTypeProfile::spawn_behavior() const
{
    if (has_explicit_spawn_behavior_) {
        return explicit_spawn_behavior_;
    }
    switch (native_class_id_) {
        case NativeClassId::EngineerService:
            return { FIGURE_ACTION_60_ENGINEER_CREATED, true, false };
        case NativeClassId::PrefectService:
            return { FIGURE_ACTION_70_PREFECT_CREATED, true, false };
        case NativeClassId::EntertainmentService:
            return { FIGURE_ACTION_94_ENTERTAINER_ROAMING, true, true };
        case NativeClassId::EntertainmentVenueSeeker:
            return { FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED, true, false };
        case NativeClassId::MarketSupplier:
            return { FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE, true, false };
        case NativeClassId::DepotCartPusher:
            return { FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL, true, false };
        case NativeClassId::FishingBoat:
            return { FIGURE_ACTION_190_FISHING_BOAT_CREATED, true, false };
        case NativeClassId::TransientWanderer:
            return pathing_policy_.mode == &StandStill ?
                ProfileSpawnBehavior { 0, true, false } :
                ProfileSpawnBehavior { FIGURE_ACTION_125_ROAMING, true, true };
        case NativeClassId::LegacyAction:
        case NativeClassId::DeliveryFollower:
            return {};
        case NativeClassId::RoamingService:
        case NativeClassId::None:
        default:
            return { FIGURE_ACTION_125_ROAMING, true, true };
    }
}

int FigureTypeProfile::resolve_building_references(const char *figure_attr)
{
    std::string context = "figure=";
    context += figure_attr ? figure_attr : "";
    context += " profile=";
    context += id();

    if (!resolve_building_reference(
        owner_binding_.required_building_reference,
        owner_binding_.required_building_type,
        context.c_str(),
        1)) {
        return 0;
    }
    for (EntertainmentVenueTarget &target : venue_targets_) {
        if (!resolve_building_reference(target.building_reference, target.building, context.c_str(), 0)) {
            return 0;
        }
    }
    return 1;
}

FigureTypeDefinition::FigureTypeDefinition(figure_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
{
}

figure_type FigureTypeDefinition::type() const
{
    return type_;
}

const char *FigureTypeDefinition::attr() const
{
    return attr_.c_str();
}

void FigureTypeDefinition::set_native_class(NativeClassId native_class_id)
{
    native_class_id_ = native_class_id;
}

NativeClassId FigureTypeDefinition::native_class() const
{
    return native_class_id_;
}

void FigureTypeDefinition::set_owner_binding(const OwnerBinding &owner_binding)
{
    owner_binding_ = owner_binding;
}

const OwnerBinding &FigureTypeDefinition::owner_binding() const
{
    return owner_binding_;
}

void FigureTypeDefinition::set_movement_profile(const MovementProfile &movement_profile)
{
    movement_profile_ = movement_profile;
}

const MovementProfile &FigureTypeDefinition::movement_profile() const
{
    return movement_profile_;
}

void FigureTypeDefinition::set_graphics(const FigureGraphics &graphics_definition)
{
    graphics_ = graphics_definition;
}

const FigureGraphics &FigureTypeDefinition::graphics() const
{
    return graphics_;
}

int FigureTypeDefinition::cache_graphics_bindings()
{
    return graphics_.cache_native_payload_bindings(*this);
}

const GraphicsTargetBinding *FigureTypeDefinition::graphics_binding(
    GraphicsTargetRole role,
    int direction_index,
    int frame) const
{
    return graphics_.cached_target_binding(role, direction_index, frame);
}

const GraphicsTargetBinding *FigureTypeDefinition::graphics_binding_for_state(
    int figure_action_state,
    int wait_ticks,
    int image_offset,
    int corpse_frame_offset,
    int direction_index) const
{
    return graphics_.cached_target_binding_for_state(
        figure_action_state,
        wait_ticks,
        image_offset,
        corpse_frame_offset,
        direction_index);
}

void FigureTypeDefinition::set_pathing_policy(const PathingPolicy &pathing_policy)
{
    pathing_policy_ = pathing_policy;
}

const PathingPolicy &FigureTypeDefinition::pathing_policy() const
{
    return pathing_policy_;
}

void FigureTypeDefinition::set_default_profile_id(std::string profile_id)
{
    default_profile_id_ = std::move(profile_id);
}

const char *FigureTypeDefinition::default_profile_id() const
{
    return default_profile_id_.c_str();
}

FigureTypeProfile &FigureTypeDefinition::add_profile(std::string profile_id)
{
    profiles_.emplace_back(std::move(profile_id));
    return profiles_.back();
}

FigureTypeProfile *FigureTypeDefinition::last_profile()
{
    return profiles_.empty() ? nullptr : &profiles_.back();
}

const FigureTypeProfile *FigureTypeDefinition::profile(const char *profile_id) const
{
    if (!profile_id || !*profile_id) {
        return default_profile();
    }
    for (const FigureTypeProfile &profile : profiles_) {
        if (xml_value::equals(profile_id, profile.id())) {
            return &profile;
        }
    }
    return nullptr;
}

const FigureTypeProfile *FigureTypeDefinition::default_profile() const
{
    if (!default_profile_id_.empty()) {
        return profile(default_profile_id_.c_str());
    }
    return profiles_.size() == 1 ? &profiles_.front() : nullptr;
}

const std::vector<FigureTypeProfile> &FigureTypeDefinition::profiles() const
{
    return profiles_;
}

int FigureTypeDefinition::resolve_building_references()
{
    for (FigureTypeProfile &profile : profiles_) {
        if (!profile.resolve_building_references(attr())) {
            return 0;
        }
    }
    return 1;
}

void set_failure_reason(const char *message, const char *detail)
{
    g_failure_reason = xml_definition::format_failure_reason(message, detail);
}

const FigureTypeDefinition *definition_for(figure_type type)
{
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        return nullptr;
    }
    return g_figure_types[type].get();
}

const FigureGraphics *graphics_for(figure_type type)
{
    if (const FigureTypeDefinition *definition = definition_for(type)) {
        return &definition->graphics();
    }
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        return nullptr;
    }
    const FigureTypeDefinition *graphics_only = g_figure_graphics_only[type].get();
    return graphics_only ? &graphics_only->graphics() : nullptr;
}

const FigureTypeProfile *profile_for(figure_type type, const char *profile_id)
{
    const FigureTypeDefinition *definition = definition_for(type);
    return definition ? definition->profile(profile_id) : nullptr;
}

const FigureTypeProfile *default_profile_for(figure_type type)
{
    const FigureTypeDefinition *definition = definition_for(type);
    return definition ? definition->default_profile() : nullptr;
}

static std::string parse_building_type_reference(const char *attr)
{
    if (!attr) {
        return "";
    }
    std::string reference = xml_value::trim_copy(attr);
    return xml_value::equals(reference.c_str(), "any") ? "" : reference;
}

static NativeClassId parse_native_class_name(const char *name)
{
    if (xml_value::equals(name, "legacy_action")) {
        return NativeClassId::LegacyAction;
    }
    if (xml_value::equals(name, "roaming_service")) {
        return NativeClassId::RoamingService;
    }
    if (xml_value::equals(name, "engineer_service")) {
        return NativeClassId::EngineerService;
    }
    if (xml_value::equals(name, "prefect_service")) {
        return NativeClassId::PrefectService;
    }
    if (xml_value::equals(name, "entertainment_service")) {
        return NativeClassId::EntertainmentService;
    }
    if (xml_value::equals(name, "entertainment_venue_seeker")) {
        return NativeClassId::EntertainmentVenueSeeker;
    }
    if (xml_value::equals(name, "market_supplier")) {
        return NativeClassId::MarketSupplier;
    }
    if (xml_value::equals(name, "delivery_follower")) {
        return NativeClassId::DeliveryFollower;
    }
    if (xml_value::equals(name, "transient_wanderer")) {
        return NativeClassId::TransientWanderer;
    }
    if (xml_value::equals(name, "depot_cart_pusher")) {
        return NativeClassId::DepotCartPusher;
    }
    if (xml_value::equals(name, "fishing_boat")) {
        return NativeClassId::FishingBoat;
    }
    return NativeClassId::None;
}

static FigureSlot parse_figure_slot_name(const char *name)
{
    if (!name || xml_value::equals(name, "none")) {
        return FigureSlot::None;
    }
    if (xml_value::equals(name, "primary")) {
        return FigureSlot::Primary;
    }
    if (xml_value::equals(name, "secondary")) {
        return FigureSlot::Secondary;
    }
    if (xml_value::equals(name, "quaternary")) {
        return FigureSlot::Quaternary;
    }
    return FigureSlot::None;
}

static bool is_known_figure_slot_name(const char *name)
{
    return !name ||
        xml_value::equals(name, "none") ||
        xml_value::equals(name, "primary") ||
        xml_value::equals(name, "secondary") ||
        xml_value::equals(name, "quaternary");
}

static OwnerStateRequirement parse_owner_state_name(const char *name)
{
    if (!name || xml_value::equals(name, "in_use")) {
        return OwnerStateRequirement::InUse;
    }
    if (xml_value::equals(name, "in_use_or_mothballed")) {
        return OwnerStateRequirement::InUseOrMothballed;
    }
    if (xml_value::equals(name, "any")) {
        return OwnerStateRequirement::Any;
    }
    return OwnerStateRequirement::InUse;
}

static bool is_known_owner_state_name(const char *name)
{
    return !name ||
        xml_value::equals(name, "any") ||
        xml_value::equals(name, "in_use") ||
        xml_value::equals(name, "in_use_or_mothballed");
}

static ReturnMode parse_return_mode_name(const char *name)
{
    if (xml_value::equals(name, "none")) {
        return ReturnMode::None;
    }
    if (xml_value::equals(name, "die_at_limit")) {
        return ReturnMode::DieAtLimit;
    }
    return ReturnMode::ReturnToOwnerRoad;
}

static bool is_known_return_mode_name(const char *name)
{
    return !name ||
        xml_value::equals(name, "none") ||
        xml_value::equals(name, "return_to_owner_road") ||
        xml_value::equals(name, "die_at_limit");
}

static int parse_terrain_usage_name(const char *name)
{
    if (!name || xml_value::equals(name, "any")) {
        return TERRAIN_USAGE_ANY;
    }
    if (xml_value::equals(name, "roads")) {
        return TERRAIN_USAGE_ROADS;
    }
    if (xml_value::equals(name, "roads_highway")) {
        return TERRAIN_USAGE_ROADS_HIGHWAY;
    }
    if (xml_value::equals(name, "prefer_roads")) {
        return TERRAIN_USAGE_PREFER_ROADS;
    }
    if (xml_value::equals(name, "prefer_roads_highway")) {
        return TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }
    return -1;
}

static int parse_image_group_name(const char *name)
{
    struct NamedGroup {
        std::string_view name;
        int image_group_id;
    };

    static constexpr std::array<NamedGroup, 39> kImageGroups = { {
        { "labor_seeker", GROUP_FIGURE_LABOR_SEEKER },
        { "engineer", GROUP_FIGURE_ENGINEER },
        { "prefect", GROUP_FIGURE_PREFECT },
        { "prefect_with_bucket", GROUP_FIGURE_PREFECT_WITH_BUCKET },
        { "priest", GROUP_FIGURE_PRIEST },
        { "doctor", GROUP_FIGURE_DOCTOR_SURGEON },
        { "surgeon", GROUP_FIGURE_DOCTOR_SURGEON },
        { "tax_collector", GROUP_FIGURE_TAX_COLLECTOR },
        { "missionary", GROUP_FIGURE_MISSIONARY },
        { "patrician", GROUP_FIGURE_PATRICIAN },
        { "beggar", GROUP_FIGURE_HOMELESS },
        { "market_lady", GROUP_FIGURE_MARKET_LADY },
        { "market_supplier", GROUP_FIGURE_MARKET_LADY },
        { "delivery_boy", GROUP_FIGURE_DELIVERY_BOY },
        { "actor", GROUP_FIGURE_ACTOR },
        { "gladiator", GROUP_FIGURE_GLADIATOR },
        { "lion", GROUP_FIGURE_LION },
        { "lion_tamer", GROUP_FIGURE_LION_TAMER },
        { "lion_tamer_whip", GROUP_FIGURE_LION_TAMER_WHIP },
        { "cartpusher", GROUP_FIGURE_CARTPUSHER },
        { "cartpusher_cart", GROUP_FIGURE_CARTPUSHER_CART },
        { "migrant", GROUP_FIGURE_MIGRANT },
        { "migrant_cart", GROUP_FIGURE_MIGRANT_CART },
        { "fort_standard_pole", GROUP_FIGURE_FORT_STANDARD_POLE },
        { "fort_flags", GROUP_FIGURE_FORT_FLAGS },
        { "fort_standard_icons", GROUP_FIGURE_FORT_STANDARD_ICONS },
        { "map_flag_flags", GROUP_FIGURE_MAP_FLAG_FLAGS },
        { "map_flag_icons", GROUP_FIGURE_MAP_FLAG_ICONS },
        { "hippodrome_horse_1", GROUP_FIGURE_HIPPODROME_HORSE_1 },
        { "hippodrome_horse_2", GROUP_FIGURE_HIPPODROME_HORSE_2 },
        { "hippodrome_cart_1", GROUP_FIGURE_HIPPODROME_CART_1 },
        { "hippodrome_cart_2", GROUP_FIGURE_HIPPODROME_CART_2 },
        { "charioteer", GROUP_FIGURE_CHARIOTEER },
        { "teacher_librarian", GROUP_FIGURE_TEACHER_LIBRARIAN },
        { "barber", GROUP_FIGURE_BARBER },
        { "bathhouse_worker", GROUP_FIGURE_BATHHOUSE_WORKER },
        { "school_child", GROUP_FIGURE_SCHOOL_CHILD },
        { "ballista", GROUP_FIGURE_BALLISTA },
        { "ship", GROUP_FIGURE_SHIP }
    } };

    for (const NamedGroup &entry : kImageGroups) {
        if (xml_value::equals(name, entry.name)) {
            return entry.image_group_id;
        }
    }
    return 0;
}

static asset_id parse_image_asset_name(const char *name)
{
    if (xml_value::equals(name, "ox")) {
        return ASSET_OX;
    }
    return ASSET_MAX_KEY;
}

static int parse_action_state_name(const char *name)
{
    if (xml_value::equals(name, "corpse")) {
        return FIGURE_ACTION_149_CORPSE;
    }
    if (xml_value::equals(name, "prefect_going_to_fire")) {
        return FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE;
    }
    if (xml_value::equals(name, "prefect_at_fire")) {
        return FIGURE_ACTION_75_PREFECT_AT_FIRE;
    }
    if (xml_value::equals(name, "roaming")) {
        return FIGURE_ACTION_125_ROAMING;
    }
    if (xml_value::equals(name, "supplier_returning")) {
        return FIGURE_ACTION_146_SUPPLIER_RETURNING;
    }
    if (xml_value::equals(name, "fishing_boat_fishing")) {
        return FIGURE_ACTION_192_FISHING_BOAT_FISHING;
    }
    if (xml_value::equals(name, "immigrant_arriving")) {
        return FIGURE_ACTION_2_IMMIGRANT_ARRIVING;
    }
    if (xml_value::equals(name, "emigrant_leaving")) {
        return FIGURE_ACTION_6_EMIGRANT_LEAVING;
    }
    if (xml_value::equals(name, "ballista_created")) {
        return FIGURE_ACTION_180_BALLISTA_CREATED;
    }
    if (xml_value::equals(name, "ballista_firing")) {
        return FIGURE_ACTION_181_BALLISTA_FIRING;
    }
    if (xml_value::equals(name, "tax_collector_created")) {
        return FIGURE_ACTION_40_TAX_COLLECTOR_CREATED;
    }
    if (xml_value::equals(name, "work_camp_worker_created")) {
        return FIGURE_ACTION_203_WORK_CAMP_WORKER_CREATED;
    }
    if (xml_value::equals(name, "work_camp_architect_working")) {
        return FIGURE_ACTION_208_WORK_CAMP_ARCHITECT_WORKING_ON_MONUMENT;
    }
    return 0;
}

static CartGraphicsMode parse_cart_graphics_mode_name(const char *name)
{
    if (!name || xml_value::equals(name, "none")) {
        return CartGraphicsMode::None;
    }
    if (xml_value::equals(name, "resource_load")) {
        return CartGraphicsMode::ResourceLoad;
    }
    return CartGraphicsMode::None;
}

static bool is_known_cart_graphics_mode_name(const char *name)
{
    return !name ||
        xml_value::equals(name, "none") ||
        xml_value::equals(name, "resource_load");
}

static int parse_int_array_8(const char *text, std::array<int, 8> &out_values)
{
    std::string remaining = text ? text : "";
    for (int index = 0; index < 8; index++) {
        const size_t comma = remaining.find(',');
        std::string token = comma == std::string::npos ?
            remaining :
            remaining.substr(0, comma);
        std::string trimmed = xml_value::trim_copy(token);
        int value = 0;
        if (!xml_value::parse_int_strict(std::string_view(trimmed), &value)) {
            return 0;
        }
        out_values[index] = value;

        if (index == 7) {
            return comma == std::string::npos;
        }
        if (comma == std::string::npos) {
            return 0;
        }
        remaining = remaining.substr(comma + 1);
    }
    return 0;
}

static int validate_depot_cart_graphics(
    const FigureTypeDefinition *definition,
    bool *parse_error,
    const FigureGraphics &graphics_definition)
{
    if (!definition || definition->type() != FIGURE_DEPOT_CART_PUSHER) {
        return 1;
    }

    if (!graphics_definition.has_legacy_resource_cart_graphics()) {
        *parse_error = true;
        std::string detail = "figure=";
        detail += definition->attr();
        detail += " profile=* context=cart";
        set_failure_reason(
            "Depot cart pusher graphics requires image_asset, corpse_image_asset, and cart_mode resource_load.",
            detail.c_str());
        log_error("Depot cart pusher graphics requires image_asset, corpse_image_asset, and cart_mode resource_load",
            detail.c_str(), 0);
        return 0;
    }
    return 1;
}

static int parse_int_attribute_strict(const char *name, int &out_value)
{
    const std::string text = xml_value::trim_copy(xml_parser_get_attribute_string(name));
    return xml_value::parse_int_strict(std::string_view(text), &out_value);
}

static int parse_overlay_visible_actions(const char *text, std::vector<int> &out_actions)
{
    std::string remaining = xml_value::trim_copy(text);
    if (remaining == "any") {
        return 1;
    }
    while (!remaining.empty()) {
        const std::size_t comma = remaining.find(',');
        const std::string token = xml_value::trim_copy(
            comma == std::string::npos ? remaining : remaining.substr(0, comma));
        int action = parse_action_state_name(token.c_str());
        if (!action && (!xml_value::parse_int_strict(std::string_view(token), &action) ||
                action <= 0 || action > std::numeric_limits<unsigned char>::max())) {
            out_actions.clear();
            return 0;
        }
        if (std::find(out_actions.begin(), out_actions.end(), action) != out_actions.end()) {
            out_actions.clear();
            return 0;
        }
        out_actions.push_back(action);
        if (comma == std::string::npos) {
            return 1;
        }
        remaining = remaining.substr(comma + 1);
    }
    out_actions.clear();
    return 0;
}

static int parse_bounded_int_list(
    const char *text,
    int minimum,
    int maximum,
    std::vector<int> &out_values)
{
    std::string remaining = text ? text : "";
    out_values.clear();
    while (!remaining.empty()) {
        const size_t comma = remaining.find(',');
        const std::string token = comma == std::string::npos ? remaining : remaining.substr(0, comma);
        const std::string trimmed = xml_value::trim_copy(token);
        int value = 0;
        if (!xml_value::parse_int_strict(std::string_view(trimmed), &value) ||
            value < minimum || value > maximum || out_values.size() >= 128) {
            out_values.clear();
            return 0;
        }
        out_values.push_back(value);
        if (comma == std::string::npos) {
            return 1;
        }
        remaining = remaining.substr(comma + 1);
    }
    out_values.clear();
    return 0;
}

static FigureMissileLauncherCursor parse_missile_launcher_cursor(const char *name)
{
    if (xml_value::equals(name, "attack_image_offset")) {
        return FigureMissileLauncherCursor::AttackImageOffset;
    }
    if (xml_value::equals(name, "missile_wait_ticks")) {
        return FigureMissileLauncherCursor::MissileWaitTicks;
    }
    return FigureMissileLauncherCursor::None;
}

static FigureResourceCartSource parse_resource_cart_source(const char *name)
{
    if (xml_value::equals(name, "resource_id")) {
        return FigureResourceCartSource::ResourceId;
    }
    if (xml_value::equals(name, "collecting_item_on_action")) {
        return FigureResourceCartSource::CollectingItemOnAction;
    }
    return FigureResourceCartSource::None;
}

static int parse_resource_cart_load_mode(
    const char *name,
    FigureResourceCartLoadMode &out_mode)
{
    if (xml_value::equals(name, "fixed_one")) {
        out_mode = FigureResourceCartLoadMode::FixedOne;
        return 1;
    }
    if (xml_value::equals(name, "carried_loads")) {
        out_mode = FigureResourceCartLoadMode::CarriedLoads;
        return 1;
    }
    if (xml_value::equals(name, "carried_or_one")) {
        out_mode = FigureResourceCartLoadMode::CarriedOrOne;
        return 1;
    }
    return 0;
}

static int parse_resource_cart_state_source(
    const char *name,
    FigureResourceCartStateSource &out_source)
{
    if (xml_value::equals(name, "action")) {
        out_source = FigureResourceCartStateSource::Action;
        return 1;
    }
    if (xml_value::equals(name, "runtime_state")) {
        out_source = FigureResourceCartStateSource::RuntimeState;
        return 1;
    }
    return 0;
}

static int parse_int_array_7(const char *text, std::array<int, 7> &out_values)
{
    std::string remaining = text ? text : "";
    for (int index = 0; index < 7; ++index) {
        const size_t comma = remaining.find(',');
        const std::string token = comma == std::string::npos ? remaining : remaining.substr(0, comma);
        const std::string trimmed = xml_value::trim_copy(token);
        int value = 0;
        if (!xml_value::parse_int_strict(std::string_view(trimmed), &value)) {
            return 0;
        }
        out_values[index] = value;
        if (index == 6) {
            return comma == std::string::npos;
        }
        if (comma == std::string::npos) {
            return 0;
        }
        remaining = remaining.substr(comma + 1);
    }
    return 0;
}

static int parse_cardinal_orientation_name(const char *name)
{
    if (xml_value::equals(name, "top")) {
        return DIR_0_TOP;
    }
    if (xml_value::equals(name, "right")) {
        return DIR_2_RIGHT;
    }
    if (xml_value::equals(name, "bottom")) {
        return DIR_4_BOTTOM;
    }
    if (xml_value::equals(name, "left")) {
        return DIR_6_LEFT;
    }
    return -1;
}

static int graphics_target_has_image_without_path(const std::string &path, const std::string &image)
{
    return path.empty() && !image.empty();
}

static road_service_effect parse_service_effect_name(const char *name)
{
    if (!name || xml_value::equals(name, "none")) {
        return ROAD_SERVICE_EFFECT_NONE;
    }
    if (xml_value::equals(name, "labor")) {
        return ROAD_SERVICE_EFFECT_LABOR;
    }
    if (xml_value::equals(name, "academy")) {
        return ROAD_SERVICE_EFFECT_ACADEMY;
    }
    if (xml_value::equals(name, "library")) {
        return ROAD_SERVICE_EFFECT_LIBRARY;
    }
    if (xml_value::equals(name, "barber")) {
        return ROAD_SERVICE_EFFECT_BARBER;
    }
    if (xml_value::equals(name, "bathhouse")) {
        return ROAD_SERVICE_EFFECT_BATHHOUSE;
    }
    if (xml_value::equals(name, "school")) {
        return ROAD_SERVICE_EFFECT_SCHOOL;
    }
    if (xml_value::equals(name, "damage_risk")) {
        return ROAD_SERVICE_EFFECT_DAMAGE_RISK;
    }
    if (xml_value::equals(name, "fire_risk")) {
        return ROAD_SERVICE_EFFECT_FIRE_RISK;
    }
    if (xml_value::equals(name, "religion_ceres")) {
        return ROAD_SERVICE_EFFECT_RELIGION_CERES;
    }
    if (xml_value::equals(name, "religion_neptune")) {
        return ROAD_SERVICE_EFFECT_RELIGION_NEPTUNE;
    }
    if (xml_value::equals(name, "religion_mercury")) {
        return ROAD_SERVICE_EFFECT_RELIGION_MERCURY;
    }
    if (xml_value::equals(name, "religion_mars")) {
        return ROAD_SERVICE_EFFECT_RELIGION_MARS;
    }
    if (xml_value::equals(name, "religion_venus")) {
        return ROAD_SERVICE_EFFECT_RELIGION_VENUS;
    }
    if (xml_value::equals(name, "religion_pantheon")) {
        return ROAD_SERVICE_EFFECT_RELIGION_PANTHEON;
    }
    if (xml_value::equals(name, "entertainment_theater")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_THEATER;
    }
    if (xml_value::equals(name, "entertainment_amphitheater_actor")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_AMPHITHEATER_ACTOR;
    }
    if (xml_value::equals(name, "entertainment_amphitheater_gladiator")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_AMPHITHEATER_GLADIATOR;
    }
    if (xml_value::equals(name, "entertainment_arena_gladiator")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_ARENA_GLADIATOR;
    }
    if (xml_value::equals(name, "entertainment_arena_lion")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_ARENA_LION;
    }
    if (xml_value::equals(name, "entertainment_colosseum_gladiator")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_COLOSSEUM_GLADIATOR;
    }
    if (xml_value::equals(name, "entertainment_colosseum_lion")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_COLOSSEUM_LION;
    }
    if (xml_value::equals(name, "entertainment_hippodrome")) {
        return ROAD_SERVICE_EFFECT_ENTERTAINMENT_HIPPODROME;
    }
    if (xml_value::equals(name, "market_goods")) {
        return ROAD_SERVICE_EFFECT_MARKET_GOODS;
    }
    if (xml_value::equals(name, "doctor")) {
        return ROAD_SERVICE_EFFECT_DOCTOR;
    }
    if (xml_value::equals(name, "surgeon")) {
        return ROAD_SERVICE_EFFECT_SURGEON;
    }
    if (xml_value::equals(name, "tax_collector")) {
        return ROAD_SERVICE_EFFECT_TAX_COLLECTOR;
    }
    return ROAD_SERVICE_EFFECT_NONE;
}

static bool is_known_service_effect_name(const char *name)
{
    return !name ||
        xml_value::equals(name, "none") ||
        xml_value::equals(name, "labor") ||
        xml_value::equals(name, "academy") ||
        xml_value::equals(name, "library") ||
        xml_value::equals(name, "barber") ||
        xml_value::equals(name, "bathhouse") ||
        xml_value::equals(name, "school") ||
        xml_value::equals(name, "damage_risk") ||
        xml_value::equals(name, "fire_risk") ||
        xml_value::equals(name, "religion_ceres") ||
        xml_value::equals(name, "religion_neptune") ||
        xml_value::equals(name, "religion_mercury") ||
        xml_value::equals(name, "religion_mars") ||
        xml_value::equals(name, "religion_venus") ||
        xml_value::equals(name, "religion_pantheon") ||
        xml_value::equals(name, "entertainment_theater") ||
        xml_value::equals(name, "entertainment_amphitheater_actor") ||
        xml_value::equals(name, "entertainment_amphitheater_gladiator") ||
        xml_value::equals(name, "entertainment_arena_gladiator") ||
        xml_value::equals(name, "entertainment_arena_lion") ||
        xml_value::equals(name, "entertainment_colosseum_gladiator") ||
        xml_value::equals(name, "entertainment_colosseum_lion") ||
        xml_value::equals(name, "entertainment_hippodrome") ||
        xml_value::equals(name, "market_goods") ||
        xml_value::equals(name, "doctor") ||
        xml_value::equals(name, "surgeon") ||
        xml_value::equals(name, "tax_collector");
}

static EntertainmentShowSlot parse_show_slot_name(const char *name)
{
    if (xml_value::equals(name, "days1")) {
        return EntertainmentShowSlot::Days1;
    }
    if (xml_value::equals(name, "days2")) {
        return EntertainmentShowSlot::Days2;
    }
    return EntertainmentShowSlot::None;
}

enum class GraphicsTargetKind {
    None,
    Default,
    Action,
    Corpse
};

struct ParseState {
    std::unique_ptr<FigureTypeDefinition> definition;
    FigureTypeProfile *current_profile = nullptr;
    FigureGraphics graphics_definition;
    GraphicsTargetKind current_graphics_target = GraphicsTargetKind::None;
    bool disabled = false;
    bool graphics_only = false;
    bool saw_root = false;
    bool saw_profiles = false;
    bool saw_profile_native = false;
    bool saw_profile_spawn = false;
    bool saw_profile_owner = false;
    bool saw_profile_movement = false;
    bool saw_profile_pathing = false;
    bool saw_graphics = false;
    bool saw_graphics_default = false;
    bool saw_graphics_action = false;
    bool saw_graphics_corpse = false;
    bool saw_graphics_cart = false;
    bool error = false;
};

struct StagedFigureType {
    std::unique_ptr<FigureTypeDefinition> definition;
    mod_definition::DefinitionSource source;
    bool disabled = false;
    bool graphics_only = false;
    bool saw_profiles = false;
    bool saw_graphics = false;
};

struct FigureIdentityReservation {
    figure_type type = FIGURE_NONE;
    std::string attr;
    mod_definition::DefinitionSource source;
};

struct StagedFigureTypes {
    std::array<std::optional<StagedFigureType>, FIGURE_TYPE_MAX> winners;
    std::array<std::optional<FigureIdentityReservation>, FIGURE_TYPE_MAX> slots;
    std::unordered_map<std::string, FigureIdentityReservation> ids;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

static ParseState g_parse_state;

static int parse_enabled_content(const char *element)
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        g_parse_state.error = true;
        log_error("Disabled FigureType definition must contain only its root identity", element, 0);
        return 0;
    }
    return 1;
}

static FigureTypeProfile *current_profile_or_error(const char *node_name)
{
    if (!g_parse_state.definition || !g_parse_state.current_profile) {
        g_parse_state.error = true;
        log_error("FigureType profile child appears outside a profile", node_name, 0);
        return nullptr;
    }
    return g_parse_state.current_profile;
}

static int parse_definition_root()
{
    if (g_parse_state.saw_root) {
        g_parse_state.error = true;
        log_error("Duplicate FigureType root node", 0, 0);
        return 0;
    }
    if (!xml_parser_has_attribute("type")) {
        g_parse_state.error = true;
        log_error("FigureType root is missing required attribute 'type'", 0, 0);
        return 0;
    }

    const char *type_attr = xml_parser_get_attribute_string("type");
    figure_type type = figure_type_from_xml_name(type_attr);
    if (type == FIGURE_NONE) {
        g_parse_state.error = true;
        log_error("FigureType root has an unknown figure type", type_attr, 0);
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        g_parse_state.error = true;
        log_error("FigureType root has invalid Boolean attribute 'disabled'", type_attr, 0);
        return 0;
    }
    int graphics_only = 0;
    if (xml_parser_has_attribute("graphics_only") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("graphics_only"), &graphics_only)) {
        g_parse_state.error = true;
        log_error("FigureType root has invalid Boolean attribute 'graphics_only'", type_attr, 0);
        return 0;
    }
    if (disabled && graphics_only) {
        g_parse_state.error = true;
        log_error("Disabled FigureType root cannot declare graphics_only", type_attr, 0);
        return 0;
    }

    g_parse_state.definition = std::make_unique<FigureTypeDefinition>(type, type_attr ? type_attr : "");
    g_parse_state.disabled = disabled != 0;
    g_parse_state.graphics_only = graphics_only != 0;
    g_parse_state.saw_root = true;
    return 1;
}

static int parse_profiles_node()
{
    if (!parse_enabled_content("profiles")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_profiles) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate profiles nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    if (xml_parser_has_attribute("default")) {
        g_parse_state.definition->set_default_profile_id(xml_parser_get_attribute_string("default"));
    }
    g_parse_state.saw_profiles = true;
    return 1;
}

static int parse_profile_node()
{
    if (!parse_enabled_content("profile")) {
        return 0;
    }
    if (!g_parse_state.definition || !g_parse_state.saw_profiles) {
        g_parse_state.error = true;
        log_error("FigureType profile appears before profiles node", 0, 0);
        return 0;
    }
    if (!xml_parser_has_attribute("id")) {
        g_parse_state.error = true;
        log_error("FigureType profile is missing required attribute 'id'", 0, 0);
        return 0;
    }

    const char *profile_id = xml_parser_get_attribute_string("id");
    if (g_parse_state.definition->profile(profile_id)) {
        g_parse_state.error = true;
        log_error("FigureType profile id is duplicated", profile_id, 0);
        return 0;
    }

    g_parse_state.current_profile = &g_parse_state.definition->add_profile(profile_id ? profile_id : "");
    g_parse_state.saw_profile_native = false;
    g_parse_state.saw_profile_spawn = false;
    g_parse_state.saw_profile_owner = false;
    g_parse_state.saw_profile_movement = false;
    g_parse_state.saw_profile_pathing = false;
    return 1;
}

static void finish_profile_node()
{
    if (!g_parse_state.current_profile) {
        return;
    }

    if (!g_parse_state.saw_profile_native ||
        !g_parse_state.saw_profile_owner ||
        !g_parse_state.saw_profile_movement ||
        !g_parse_state.saw_profile_pathing) {
        g_parse_state.error = true;
        log_error("FigureType profile is missing a required child node", g_parse_state.current_profile->id(), 0);
    }
    if (g_parse_state.current_profile->pathing_policy().mode->requires_venue_targets &&
        g_parse_state.current_profile->venue_targets().empty()) {
        g_parse_state.error = true;
        log_error("FigureType venue seeker profile is missing venue targets", g_parse_state.current_profile->id(), 0);
    }
    if ((g_parse_state.current_profile->native_class() == NativeClassId::EntertainmentVenueSeeker) !=
        (g_parse_state.current_profile->pathing_policy().mode == &VenueSeeker)) {
        g_parse_state.error = true;
        log_error("FigureType venue seeker native class must use venue_seeker pathing",
            g_parse_state.current_profile->id(), 0);
    }
    if (g_parse_state.current_profile->has_explicit_spawn_behavior() &&
        g_parse_state.current_profile->native_class() != NativeClassId::LegacyAction) {
        g_parse_state.error = true;
        log_error("FigureType explicit spawn behavior is valid only for legacy_action profiles",
            g_parse_state.current_profile->id(), 0);
    }
    if (g_parse_state.current_profile->native_class() == NativeClassId::DepotCartPusher) {
        const OwnerBinding &owner = g_parse_state.current_profile->owner_binding();
        if (owner.slot != FigureSlot::None ||
            !xml_value::equals(owner.required_building_reference.c_str(), "cart_depot") ||
            owner.required_owner_state != OwnerStateRequirement::InUse ||
            g_parse_state.current_profile->pathing_policy().mode != &DepotOrderRoute) {
            g_parse_state.error = true;
            log_error("Depot cart pusher profile must be owned by an in-use cart_depot with depot_order_route pathing",
                g_parse_state.current_profile->id(), 0);
        }
    }
    g_parse_state.current_profile = nullptr;
}

static int parse_native_node()
{
    if (!parse_enabled_content("native")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("native");
    if (!profile) {
        return 0;
    }
    if (g_parse_state.saw_profile_native) {
        g_parse_state.error = true;
        log_error("FigureType profile contains duplicate native nodes", profile->id(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("class")) {
        g_parse_state.error = true;
        log_error("FigureType native node is missing required attribute 'class'", 0, 0);
        return 0;
    }

    NativeClassId native_class_id = parse_native_class_name(xml_parser_get_attribute_string("class"));
    if (native_class_id == NativeClassId::None) {
        g_parse_state.error = true;
        log_error("FigureType native node has an unknown class", xml_parser_get_attribute_string("class"), 0);
        return 0;
    }

    profile->set_native_class(native_class_id);
    g_parse_state.saw_profile_native = true;
    return 1;
}

static int parse_owner_node()
{
    if (!parse_enabled_content("owner")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("owner");
    if (!profile) {
        return 0;
    }
    if (g_parse_state.saw_profile_owner) {
        g_parse_state.error = true;
        log_error("FigureType profile contains duplicate owner nodes", profile->id(), 0);
        return 0;
    }

    OwnerBinding owner_binding;
    if (xml_parser_has_attribute("slot")) {
        if (!is_known_figure_slot_name(xml_parser_get_attribute_string("slot"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown slot", xml_parser_get_attribute_string("slot"), 0);
            return 0;
        }
        owner_binding.slot = parse_figure_slot_name(xml_parser_get_attribute_string("slot"));
    }
    if (xml_parser_has_attribute("building")) {
        const char *building_attr = xml_parser_get_attribute_string("building");
        owner_binding.required_building_reference = parse_building_type_reference(building_attr);
        if (building_attr && !xml_value::equals(building_attr, "any") &&
            owner_binding.required_building_reference.empty()) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an empty building type", building_attr, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("state")) {
        if (!is_known_owner_state_name(xml_parser_get_attribute_string("state"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown state", xml_parser_get_attribute_string("state"), 0);
            return 0;
        }
        owner_binding.required_owner_state = parse_owner_state_name(xml_parser_get_attribute_string("state"));
    }

    profile->set_owner_binding(owner_binding);
    g_parse_state.saw_profile_owner = true;
    return 1;
}

static int parse_movement_node()
{
    if (!parse_enabled_content("movement")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("movement");
    if (!profile) {
        return 0;
    }
    if (g_parse_state.saw_profile_movement) {
        g_parse_state.error = true;
        log_error("FigureType profile contains duplicate movement nodes", profile->id(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("roam_ticks") ||
        !xml_parser_has_attribute("max_roam_length")) {
        g_parse_state.error = true;
        log_error("FigureType movement node is missing required attributes", 0, 0);
        return 0;
    }

    MovementProfile movement_profile;
    movement_profile.roam_ticks = xml_parser_get_attribute_int("roam_ticks");
    movement_profile.max_roam_length = xml_parser_get_attribute_int("max_roam_length");
    if (movement_profile.roam_ticks <= 0 || movement_profile.max_roam_length <= 0) {
        g_parse_state.error = true;
        log_error("FigureType movement node requires positive roam_ticks and max_roam_length", 0, 0);
        return 0;
    }
    if (movement_profile.max_roam_length > std::numeric_limits<short>::max()) {
        g_parse_state.error = true;
        log_error("FigureType movement node max_roam_length exceeds figure storage range", 0, 0);
        return 0;
    }
    if (xml_parser_has_attribute("return_mode")) {
        if (!is_known_return_mode_name(xml_parser_get_attribute_string("return_mode"))) {
            g_parse_state.error = true;
            log_error("FigureType movement node has an unknown return_mode", xml_parser_get_attribute_string("return_mode"), 0);
            return 0;
        }
        movement_profile.return_mode = parse_return_mode_name(xml_parser_get_attribute_string("return_mode"));
    }

    profile->set_movement_profile(movement_profile);
    g_parse_state.saw_profile_movement = true;
    return 1;
}

static int parse_spawn_node()
{
    if (!parse_enabled_content("spawn")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("spawn");
    if (!profile) {
        return 0;
    }
    if (g_parse_state.saw_profile_spawn) {
        g_parse_state.error = true;
        log_error("FigureType profile contains duplicate spawn nodes", profile->id(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("action_state")) {
        g_parse_state.error = true;
        log_error("FigureType spawn node is missing required action_state", profile->id(), 0);
        return 0;
    }

    ProfileSpawnBehavior spawn_behavior;
    spawn_behavior.action_state = parse_action_state_name(xml_parser_get_attribute_string("action_state"));
    spawn_behavior.has_action_state = spawn_behavior.action_state != 0;
    if (!spawn_behavior.has_action_state) {
        g_parse_state.error = true;
        log_error("FigureType spawn node has an unknown action_state",
            xml_parser_get_attribute_string("action_state"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("init_roaming")) {
        int init_roaming = 0;
        if (!xml_value::parse_bool(xml_parser_get_attribute_string("init_roaming"), &init_roaming)) {
            g_parse_state.error = true;
            log_error("FigureType spawn node has an invalid init_roaming",
                xml_parser_get_attribute_string("init_roaming"), 0);
            return 0;
        }
        spawn_behavior.init_roaming = init_roaming != 0;
    }

    profile->set_explicit_spawn_behavior(spawn_behavior);
    g_parse_state.saw_profile_spawn = true;
    return 1;
}

static int parse_graphics_logical_dimension(const char *attribute_name, render_logical_unit &out_value)
{
    const int pixels = xml_parser_get_attribute_int(attribute_name);
    const int max_pixels =
        std::numeric_limits<render_logical_unit>::max() / RENDER_LOGICAL_UNITS_PER_PIXEL;
    if (pixels <= 0 || pixels > max_pixels) {
        g_parse_state.error = true;
        log_error("FigureType graphics logical size requires positive pixel dimensions", attribute_name, 0);
        return 0;
    }
    out_value = static_cast<render_logical_unit>(pixels * RENDER_LOGICAL_UNITS_PER_PIXEL);
    return 1;
}

static int graphics_parse_error(const char *message, const char *detail = nullptr)
{
    g_parse_state.error = true;
    log_error(message, detail, 0);
    return 0;
}

static int parse_graphics_sprite_offset(FigureGraphics &graphics_definition)
{
    const bool has_sprite_offset_x = xml_parser_has_attribute("sprite_offset_x");
    const bool has_sprite_offset_y = xml_parser_has_attribute("sprite_offset_y");
    if (has_sprite_offset_x != has_sprite_offset_y) {
        return graphics_parse_error(
            "FigureType graphics sprite offset requires both sprite_offset_x and sprite_offset_y");
    }
    if (!has_sprite_offset_x) {
        return 1;
    }
    if (graphics_definition.has_sprite_offset) {
        return graphics_parse_error("FigureType graphics sprite offset is duplicated");
    }
    graphics_definition.has_sprite_offset = 1;
    graphics_definition.sprite_offset_x = xml_parser_get_attribute_int("sprite_offset_x");
    graphics_definition.sprite_offset_y = xml_parser_get_attribute_int("sprite_offset_y");
    return 1;
}

static int parse_graphics_logical_size(FigureGraphics &graphics_definition)
{
    const bool has_logical_width = xml_parser_has_attribute("logical_width");
    const bool has_logical_height = xml_parser_has_attribute("logical_height");
    if (has_logical_width != has_logical_height) {
        return graphics_parse_error(
            "FigureType graphics logical size requires both logical_width and logical_height");
    }
    if (!has_logical_width) {
        return 1;
    }
    if (graphics_definition.has_fixed_logical_size()) {
        return graphics_parse_error("FigureType graphics logical size is duplicated");
    }
    return parse_graphics_logical_dimension("logical_width", graphics_definition.fixed_logical_size.width) &&
        parse_graphics_logical_dimension("logical_height", graphics_definition.fixed_logical_size.height);
}

static int parse_graphics_default_source_attributes(
    FigureGraphics &graphics_definition,
    const char *image_group_attr,
    const char *image_asset_attr,
    const char *path_attr,
    const char *image_attr)
{
    const bool has_image_group = image_group_attr && xml_parser_has_attribute(image_group_attr);
    const bool has_image_asset = image_asset_attr && xml_parser_has_attribute(image_asset_attr);
    const bool has_path_pattern = path_attr && xml_parser_has_attribute(path_attr);
    const bool has_image_pattern = image_attr && xml_parser_has_attribute(image_attr);
    const int source_count =
        (has_image_group ? 1 : 0) +
        (has_image_asset ? 1 : 0) +
        (has_path_pattern || has_image_pattern ? 1 : 0);
    if (!source_count) {
        return 1;
    }
    if (graphics_definition.default_source_count()) {
        return graphics_parse_error("FigureType graphics default target is duplicated");
    }
    if (source_count != 1) {
        return graphics_parse_error(
            "FigureType graphics node must choose exactly one image_group, image_asset, or path/image pattern");
    }
    if (has_path_pattern != has_image_pattern) {
        return graphics_parse_error(
            "FigureType graphics path pattern requires both path_pattern and image_pattern");
    }
    if (has_image_group) {
        graphics_definition.image_group = parse_image_group_name(xml_parser_get_attribute_string(image_group_attr));
        if (!graphics_definition.image_group) {
            return graphics_parse_error(
                "FigureType graphics node has an unknown image_group",
                xml_parser_get_attribute_string(image_group_attr));
        }
    } else if (has_image_asset) {
        graphics_definition.image_asset = parse_image_asset_name(xml_parser_get_attribute_string(image_asset_attr));
        if (graphics_definition.image_asset == ASSET_MAX_KEY) {
            return graphics_parse_error(
                "FigureType graphics node has an unknown image_asset",
                xml_parser_get_attribute_string(image_asset_attr));
        }
    } else {
        graphics_definition.path_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(path_attr));
        graphics_definition.image_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(image_attr));
        if (graphics_definition.path_pattern.empty() || graphics_definition.image_pattern.empty()) {
            return graphics_parse_error(
                "FigureType graphics path pattern requires non-empty path_pattern and image_pattern");
        }
    }
    return 1;
}

static int parse_graphics_default_options(FigureGraphics &graphics_definition)
{
    if (xml_parser_has_attribute("max_image_offset")) {
        graphics_definition.max_image_offset = xml_parser_get_attribute_int("max_image_offset");
        if (graphics_definition.max_image_offset <= 0) {
            return graphics_parse_error("FigureType graphics node requires a positive max_image_offset");
        }
    }
    if (xml_parser_has_attribute("base_image_offset")) {
        graphics_definition.image_group_offset = xml_parser_get_attribute_int("base_image_offset");
        if (graphics_definition.image_group_offset < 0) {
            return graphics_parse_error("FigureType graphics node requires a non-negative base_image_offset");
        }
    }
    if (xml_parser_has_attribute("direction_stride")) {
        graphics_definition.direction_frame_stride = xml_parser_get_attribute_int("direction_stride");
        if (graphics_definition.direction_frame_stride <= 0) {
            return graphics_parse_error("FigureType graphics node requires a positive direction_stride");
        }
    }
    if (xml_parser_has_attribute("static_frame_count")) {
        graphics_definition.static_frame_count = xml_parser_get_attribute_int("static_frame_count");
        if (graphics_definition.static_frame_count <= 0) {
            return graphics_parse_error("FigureType graphics node requires a positive static_frame_count");
        }
    }
    return 1;
}

static int parse_graphics_action_source_attributes(
    FigureGraphics &graphics_definition,
    const char *image_group_attr,
    const char *path_attr,
    const char *image_attr)
{
    const bool has_image_group = image_group_attr && xml_parser_has_attribute(image_group_attr);
    const bool has_path_pattern = path_attr && xml_parser_has_attribute(path_attr);
    const bool has_image_pattern = image_attr && xml_parser_has_attribute(image_attr);
    const int source_count =
        (has_image_group ? 1 : 0) +
        (has_path_pattern || has_image_pattern ? 1 : 0);
    if (!source_count) {
        return 1;
    }
    if (graphics_definition.action_source_count()) {
        return graphics_parse_error("FigureType action graphics target is duplicated");
    }
    if (source_count != 1) {
        return graphics_parse_error(
            "FigureType action graphics target must choose exactly one image_group or path/image pattern");
    }
    if (has_path_pattern != has_image_pattern) {
        return graphics_parse_error(
            "FigureType action graphics requires both path_pattern and image_pattern");
    }

    if (has_image_group) {
        graphics_definition.action_image_group = parse_image_group_name(xml_parser_get_attribute_string(image_group_attr));
        if (!graphics_definition.action_image_group) {
            return graphics_parse_error(
                "FigureType graphics node has an unknown action image_group",
                xml_parser_get_attribute_string(image_group_attr));
        }
    } else {
        graphics_definition.action_path_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(path_attr));
        graphics_definition.action_image_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(image_attr));
        if (graphics_definition.action_path_pattern.empty() || graphics_definition.action_image_pattern.empty()) {
            return graphics_parse_error("FigureType graphics node has invalid action graphics attributes");
        }
    }
    return 1;
}

static int parse_graphics_action_state(FigureGraphics &graphics_definition, const char *state_attr)
{
    if (!xml_parser_has_attribute(state_attr)) {
        return graphics_parse_error("FigureType action graphics requires a state");
    }
    graphics_definition.action_state = parse_action_state_name(xml_parser_get_attribute_string(state_attr));
    if (!graphics_definition.action_state) {
        return graphics_parse_error(
            "FigureType graphics node has invalid action graphics attributes",
            xml_parser_get_attribute_string(state_attr));
    }
    return 1;
}

static int parse_graphics_action_wait_ticks(FigureGraphics &graphics_definition, const char *wait_attr)
{
    if (!xml_parser_has_attribute(wait_attr)) {
        return 1;
    }
    graphics_definition.action_min_wait_ticks = xml_parser_get_attribute_int(wait_attr);
    if (graphics_definition.action_min_wait_ticks < 0) {
        return graphics_parse_error("FigureType graphics node requires a non-negative action_min_wait_ticks");
    }
    return 1;
}

static int parse_graphics_corpse_source_attributes(
    FigureGraphics &graphics_definition,
    const char *image_group_attr,
    const char *image_asset_attr,
    const char *path_attr,
    const char *image_attr)
{
    const bool has_corpse_image_group = image_group_attr && xml_parser_has_attribute(image_group_attr);
    const bool has_corpse_image_asset = image_asset_attr && xml_parser_has_attribute(image_asset_attr);
    const bool has_corpse_path_pattern = path_attr && xml_parser_has_attribute(path_attr);
    const bool has_corpse_image_pattern = image_attr && xml_parser_has_attribute(image_attr);
    const int corpse_source_count =
        (has_corpse_image_group ? 1 : 0) +
        (has_corpse_image_asset ? 1 : 0) +
        (has_corpse_path_pattern || has_corpse_image_pattern ? 1 : 0);
    if (!corpse_source_count) {
        return 1;
    }
    if (graphics_definition.corpse_source_count()) {
        return graphics_parse_error("FigureType corpse graphics target is duplicated");
    }
    if (corpse_source_count > 1) {
        return graphics_parse_error("FigureType graphics node must choose only one corpse image source");
    }
    if (has_corpse_path_pattern != has_corpse_image_pattern) {
        return graphics_parse_error(
            "FigureType corpse graphics path pattern requires both path_pattern and image_pattern");
    }
    if (has_corpse_image_group) {
        graphics_definition.corpse_image_group =
            parse_image_group_name(xml_parser_get_attribute_string(image_group_attr));
        if (!graphics_definition.corpse_image_group) {
            return graphics_parse_error(
                "FigureType graphics node has an unknown corpse_image_group",
                xml_parser_get_attribute_string(image_group_attr));
        }
    }
    if (has_corpse_path_pattern) {
        graphics_definition.corpse_path_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(path_attr));
        graphics_definition.corpse_image_pattern = xml_value::trim_copy(xml_parser_get_attribute_string(image_attr));
        if (graphics_definition.corpse_path_pattern.empty() || graphics_definition.corpse_image_pattern.empty()) {
            return graphics_parse_error(
                "FigureType corpse graphics path pattern requires non-empty path_pattern and image_pattern");
        }
    }
    if (has_corpse_image_asset) {
        graphics_definition.corpse_image_asset =
            parse_image_asset_name(xml_parser_get_attribute_string(image_asset_attr));
        if (graphics_definition.corpse_image_asset == ASSET_MAX_KEY) {
            return graphics_parse_error(
                "FigureType graphics node has an unknown corpse_image_asset",
                xml_parser_get_attribute_string(image_asset_attr));
        }
    }
    return 1;
}

static int parse_graphics_corpse_options(
    FigureGraphics &graphics_definition,
    const char *base_offset_attr,
    const char *frame_count_attr)
{
    if (xml_parser_has_attribute(base_offset_attr)) {
        graphics_definition.corpse_image_group_offset = xml_parser_get_attribute_int(base_offset_attr);
        if (graphics_definition.corpse_image_group_offset < 0) {
            return graphics_parse_error("FigureType graphics node requires a non-negative corpse base_image_offset");
        }
    }
    if (xml_parser_has_attribute(frame_count_attr)) {
        graphics_definition.corpse_frame_count = xml_parser_get_attribute_int(frame_count_attr);
        if (graphics_definition.corpse_frame_count <= 0) {
            return graphics_parse_error("FigureType graphics node requires a positive corpse frame_count");
        }
    }
    return 1;
}

static int parse_graphics_cart_attributes(
    FigureGraphics &graphics_definition,
    const char *mode_attr,
    const char *offsets_x_attr,
    const char *offsets_y_attr,
    const char *high_load_threshold_attr,
    const char *high_load_y_adjust_attr,
    const char *direction_3_y_adjust_attr,
    int mode_required)
{
    if (!xml_parser_has_attribute(mode_attr)) {
        return mode_required ?
            graphics_parse_error("FigureType cart graphics is missing mode") :
            1;
    }

    const char *cart_mode = xml_parser_get_attribute_string(mode_attr);
    if (!is_known_cart_graphics_mode_name(cart_mode)) {
        return graphics_parse_error("FigureType graphics node has an unknown cart_mode", cart_mode);
    }
    graphics_definition.cart_mode = parse_cart_graphics_mode_name(cart_mode);
    if (graphics_definition.cart_mode == CartGraphicsMode::ResourceLoad) {
        if (!xml_parser_has_attribute(offsets_x_attr) ||
            !xml_parser_has_attribute(offsets_y_attr) ||
            !xml_parser_has_attribute(high_load_threshold_attr) ||
            !xml_parser_has_attribute(high_load_y_adjust_attr) ||
            !xml_parser_has_attribute(direction_3_y_adjust_attr)) {
            return graphics_parse_error("FigureType resource_load cart graphics is missing required cart attributes");
        }
        if (!parse_int_array_8(xml_parser_get_attribute_string(offsets_x_attr), graphics_definition.cart_offsets_x)) {
            return graphics_parse_error("FigureType graphics node has invalid cart_offsets_x");
        }
        if (!parse_int_array_8(xml_parser_get_attribute_string(offsets_y_attr), graphics_definition.cart_offsets_y)) {
            return graphics_parse_error("FigureType graphics node has invalid cart_offsets_y");
        }
        graphics_definition.cart_high_load_threshold = xml_parser_get_attribute_int(high_load_threshold_attr);
        if (graphics_definition.cart_high_load_threshold < 0) {
            return graphics_parse_error("FigureType graphics node requires a non-negative cart_high_load_threshold");
        }
        graphics_definition.cart_high_load_y_adjust = xml_parser_get_attribute_int(high_load_y_adjust_attr);
        graphics_definition.cart_direction_3_y_adjust = xml_parser_get_attribute_int(direction_3_y_adjust_attr);
    }
    return 1;
}

static int parse_graphics_default_attributes(FigureGraphics &graphics_definition)
{
    return parse_graphics_default_source_attributes(
            graphics_definition, "image_group", "image_asset", "path_pattern", "image_pattern") &&
        parse_graphics_sprite_offset(graphics_definition) &&
        parse_graphics_logical_size(graphics_definition) &&
        parse_graphics_default_options(graphics_definition);
}

static int parse_graphics_legacy_action_attributes(FigureGraphics &graphics_definition)
{
    const bool has_action_state = xml_parser_has_attribute("action_state");
    const bool has_action_path_pattern = xml_parser_has_attribute("action_path_pattern");
    const bool has_action_image_pattern = xml_parser_has_attribute("action_image_pattern");
    if (!has_action_state && !has_action_path_pattern && !has_action_image_pattern) {
        return 1;
    }
    if (g_parse_state.saw_graphics_action) {
        return graphics_parse_error("FigureType action graphics target is duplicated");
    }
    if (has_action_state != has_action_path_pattern || has_action_state != has_action_image_pattern) {
        return graphics_parse_error(
            "FigureType action graphics requires action_state, action_path_pattern, and action_image_pattern");
    }
    if (!parse_graphics_action_state(graphics_definition, "action_state") ||
        !parse_graphics_action_source_attributes(
            graphics_definition, nullptr, "action_path_pattern", "action_image_pattern") ||
        !parse_graphics_action_wait_ticks(graphics_definition, "action_min_wait_ticks")) {
        return 0;
    }
    g_parse_state.saw_graphics_action = true;
    return 1;
}

static int parse_graphics_legacy_corpse_attributes(FigureGraphics &graphics_definition)
{
    const int source_count_before = graphics_definition.corpse_source_count();
    if (!parse_graphics_corpse_source_attributes(
            graphics_definition,
            "corpse_image_group",
            "corpse_image_asset",
            "corpse_path_pattern",
            "corpse_image_pattern") ||
        !parse_graphics_corpse_options(graphics_definition, "corpse_base_image_offset", "corpse_frame_count")) {
        return 0;
    }
    if (!source_count_before && graphics_definition.corpse_source_count()) {
        g_parse_state.saw_graphics_corpse = true;
    }
    return 1;
}

static int parse_graphics_legacy_cart_attributes(FigureGraphics &graphics_definition)
{
    if (!xml_parser_has_attribute("cart_mode")) {
        return 1;
    }
    if (g_parse_state.saw_graphics_cart) {
        return graphics_parse_error("FigureType cart graphics target is duplicated");
    }
    if (!parse_graphics_cart_attributes(
            graphics_definition,
            "cart_mode",
            "cart_offsets_x",
            "cart_offsets_y",
            "cart_high_load_threshold",
            "cart_high_load_y_adjust",
            "cart_direction_3_y_adjust",
            0)) {
        return 0;
    }
    g_parse_state.saw_graphics_cart = true;
    return 1;
}

static int parse_graphics_node()
{
    if (!parse_enabled_content("graphics")) {
        return 0;
    }
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_graphics) {
        return graphics_parse_error(
            "FigureType xml contains duplicate graphics nodes",
            g_parse_state.definition->attr());
    }

    g_parse_state.graphics_definition = {};
    g_parse_state.saw_graphics = true;
    if (!parse_graphics_default_attributes(g_parse_state.graphics_definition) ||
        !parse_graphics_legacy_action_attributes(g_parse_state.graphics_definition) ||
        !parse_graphics_legacy_corpse_attributes(g_parse_state.graphics_definition) ||
        !parse_graphics_legacy_cart_attributes(g_parse_state.graphics_definition)) {
        return 0;
    }
    if (g_parse_state.graphics_definition.default_source_count()) {
        g_parse_state.saw_graphics_default = true;
    }
    return 1;
}

static int parse_graphics_default_node()
{
    if (!parse_enabled_content("default")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || g_parse_state.current_graphics_target != GraphicsTargetKind::None) {
        return graphics_parse_error("FigureType graphics default target appears outside graphics");
    }
    if (g_parse_state.saw_graphics_default) {
        return graphics_parse_error("FigureType graphics default target is duplicated");
    }
    if (!parse_graphics_default_attributes(g_parse_state.graphics_definition)) {
        return 0;
    }
    g_parse_state.saw_graphics_default = true;
    g_parse_state.current_graphics_target = GraphicsTargetKind::Default;
    return 1;
}

static int parse_graphics_action_node()
{
    if (!parse_enabled_content("action")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || g_parse_state.current_graphics_target != GraphicsTargetKind::None) {
        return graphics_parse_error("FigureType graphics action target appears outside graphics");
    }
    if (g_parse_state.saw_graphics_action) {
        return graphics_parse_error("FigureType action graphics target is duplicated");
    }
    if (xml_parser_has_attribute("state") &&
        !parse_graphics_action_state(g_parse_state.graphics_definition, "state")) {
        return 0;
    }
    if (!parse_graphics_action_source_attributes(
            g_parse_state.graphics_definition, "image_group", "path_pattern", "image_pattern") ||
        !parse_graphics_action_wait_ticks(g_parse_state.graphics_definition, "min_wait_ticks")) {
        return 0;
    }
    g_parse_state.saw_graphics_action = true;
    g_parse_state.current_graphics_target = GraphicsTargetKind::Action;
    return 1;
}

static int parse_graphics_corpse_node()
{
    if (!parse_enabled_content("corpse")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || g_parse_state.current_graphics_target != GraphicsTargetKind::None) {
        return graphics_parse_error("FigureType graphics corpse target appears outside graphics");
    }
    if (g_parse_state.saw_graphics_corpse) {
        return graphics_parse_error("FigureType corpse graphics target is duplicated");
    }
    if (!parse_graphics_corpse_source_attributes(
            g_parse_state.graphics_definition,
            "image_group",
            "image_asset",
            "path_pattern",
            "image_pattern") ||
        !parse_graphics_corpse_options(g_parse_state.graphics_definition, "base_image_offset", "frame_count")) {
        return 0;
    }
    g_parse_state.saw_graphics_corpse = true;
    g_parse_state.current_graphics_target = GraphicsTargetKind::Corpse;
    return 1;
}

static int parse_graphics_cart_node()
{
    if (!parse_enabled_content("cart")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics) {
        return graphics_parse_error("FigureType graphics cart target appears outside graphics");
    }
    if (g_parse_state.saw_graphics_cart) {
        return graphics_parse_error("FigureType cart graphics target is duplicated");
    }
    if (!parse_graphics_cart_attributes(
            g_parse_state.graphics_definition,
            "mode",
            "offsets_x",
            "offsets_y",
            "high_load_threshold",
            "high_load_y_adjust",
            "direction_3_y_adjust",
            1)) {
        return 0;
    }
    g_parse_state.saw_graphics_cart = true;
    return 1;
}

static int parse_graphics_overlay_node()
{
    if (!parse_enabled_content("overlay")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics) {
        return graphics_parse_error("FigureType graphics overlay appears outside graphics");
    }
    if (!xml_parser_has_attribute("role") ||
        !xml_parser_has_attribute("image_group") ||
        !xml_parser_has_attribute("direction") ||
        !xml_parser_has_attribute("frame") ||
        !xml_parser_has_attribute("direction_stride") ||
        !xml_parser_has_attribute("resource_base_offset") ||
        !xml_parser_has_attribute("resource_stride") ||
        !xml_parser_has_attribute("offsets_x") ||
        !xml_parser_has_attribute("offsets_y") ||
        !xml_parser_has_attribute("actions") ||
        !xml_parser_has_attribute("hide_on_corpse")) {
        return graphics_parse_error("FigureType graphics overlay is missing required attributes");
    }

    FigureGraphicsOverlay overlay;
    overlay.role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
    overlay.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    if (overlay.role.empty() || !overlay.image_group) {
        return graphics_parse_error("FigureType graphics overlay has an invalid role or image_group");
    }

    const char *direction = xml_parser_get_attribute_string("direction");
    if (xml_value::equals(direction, "static")) {
        overlay.direction = FigureOverlayDirection::Static;
    } else if (xml_value::equals(direction, "figure")) {
        overlay.direction = FigureOverlayDirection::Figure;
    } else {
        return graphics_parse_error("FigureType graphics overlay has an invalid direction", direction);
    }

    const char *frame = xml_parser_get_attribute_string("frame");
    if (xml_value::equals(frame, "static")) {
        overlay.frame = FigureOverlayFrame::Static;
    } else if (xml_value::equals(frame, "figure")) {
        overlay.frame = FigureOverlayFrame::Figure;
    } else {
        return graphics_parse_error("FigureType graphics overlay has an invalid frame", frame);
    }

    if (!parse_int_attribute_strict("direction_stride", overlay.direction_frame_stride) ||
        !parse_int_attribute_strict("resource_base_offset", overlay.resource_base_image_offset) ||
        !parse_int_attribute_strict("resource_stride", overlay.resource_stride) ||
        !parse_int_array_8(xml_parser_get_attribute_string("offsets_x"), overlay.offsets_x) ||
        !parse_int_array_8(xml_parser_get_attribute_string("offsets_y"), overlay.offsets_y) ||
        !parse_overlay_visible_actions(
            xml_parser_get_attribute_string("actions"), overlay.visible_actions) ||
        !xml_value::parse_bool(
            xml_parser_get_attribute_string("hide_on_corpse"),
            &overlay.hide_on_corpse)) {
        return graphics_parse_error("FigureType graphics overlay has invalid arithmetic, visibility, or corpse data");
    }
    if (!g_parse_state.graphics_definition.add_overlay(std::move(overlay))) {
        return graphics_parse_error("FigureType graphics overlay is invalid or duplicates a role/action");
    }
    return 1;
}

static int parse_graphics_standard_node()
{
    if (!parse_enabled_content("standard")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || !g_parse_state.definition ||
        g_parse_state.definition->type() != FIGURE_FORT_STANDARD) {
        return graphics_parse_error("FigureType standard graphics is valid only for fort_standard");
    }
    if (!xml_parser_has_attribute("moving_frame_divisor")) {
        return graphics_parse_error("FigureType standard graphics is missing moving_frame_divisor");
    }
    if (!g_parse_state.graphics_definition.configure_standard(
            xml_parser_get_attribute_int("moving_frame_divisor"))) {
        return graphics_parse_error("FigureType standard graphics is duplicated or has an invalid frame divisor");
    }
    return 1;
}

static int parse_graphics_standard_flag_node()
{
    if (!parse_enabled_content("flag")) {
        return 0;
    }
    if (!g_parse_state.graphics_definition.standard().enabled ||
        !xml_parser_has_attribute("unit") ||
        !xml_parser_has_attribute("image_group") ||
        !xml_parser_has_attribute("moving_base") ||
        !xml_parser_has_attribute("halted_frame")) {
        return graphics_parse_error("FigureType standard flag is outside standard or missing required attributes");
    }

    FigureStandardFlagGraphics flag;
    flag.unit_type = figure_type_from_xml_name(xml_parser_get_attribute_string("unit"));
    flag.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    flag.moving_base_offset = xml_parser_get_attribute_int("moving_base");
    flag.halted_frame_offset = xml_parser_get_attribute_int("halted_frame");
    if (!g_parse_state.graphics_definition.add_standard_flag(std::move(flag))) {
        return graphics_parse_error("FigureType standard flag is invalid or duplicates a unit");
    }
    return 1;
}

static int parse_graphics_missile_launcher_node()
{
    if (!parse_enabled_content("missile_launcher")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics ||
        !xml_parser_has_attribute("cursor") ||
        !xml_parser_has_attribute("frame_divisor") ||
        !xml_parser_has_attribute("frames") ||
        !xml_parser_has_attribute("after_frame")) {
        return graphics_parse_error(
            "FigureType missile-launcher graphics is outside graphics or missing required attributes");
    }

    const FigureMissileLauncherCursor cursor =
        parse_missile_launcher_cursor(xml_parser_get_attribute_string("cursor"));
    int frame_divisor = 0;
    int after_frame = 0;
    const std::string frame_divisor_text =
        xml_value::trim_copy(xml_parser_get_attribute_string("frame_divisor"));
    const std::string after_frame_text =
        xml_value::trim_copy(xml_parser_get_attribute_string("after_frame"));
    std::vector<int> frames;
    if (cursor == FigureMissileLauncherCursor::None ||
        !xml_value::parse_int_strict(frame_divisor_text, &frame_divisor) ||
        !xml_value::parse_int_strict(after_frame_text, &after_frame) ||
        frame_divisor <= 0 || frame_divisor > 255 ||
        after_frame < -1 || after_frame > 255 ||
        !parse_bounded_int_list(
            xml_parser_get_attribute_string("frames"), -1, 255, frames) ||
        !g_parse_state.graphics_definition.configure_missile_launcher(
            cursor, frame_divisor, std::move(frames), after_frame)) {
        return graphics_parse_error(
            "FigureType missile-launcher graphics is duplicated or invalid");
    }
    return 1;
}

static int parse_graphics_resource_cart_node()
{
    if (!parse_enabled_content("resource_cart")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics ||
        !xml_parser_has_attribute("empty_image_group") ||
        !xml_parser_has_attribute("resource_source") ||
        !xml_parser_has_attribute("resource_action") ||
        !xml_parser_has_attribute("load_mode") ||
        !xml_parser_has_attribute("state_source") ||
        !xml_parser_has_attribute("suppress_body_when_hidden") ||
        !xml_parser_has_attribute("lift_food_at_loads") ||
        !xml_parser_has_attribute("lift_food_y_adjust") ||
        !xml_parser_has_attribute("offsets_x") ||
        !xml_parser_has_attribute("offsets_y") ||
        !xml_parser_has_attribute("hide_on_corpse")) {
        return graphics_parse_error(
            "FigureType resource-cart graphics is outside graphics or missing required attributes");
    }

    FigureResourceCartGraphics resource_cart;
    resource_cart.empty_image_group =
        parse_image_group_name(xml_parser_get_attribute_string("empty_image_group"));
    resource_cart.resource_source =
        parse_resource_cart_source(xml_parser_get_attribute_string("resource_source"));
    const char *resource_action = xml_parser_get_attribute_string("resource_action");
    resource_cart.resource_action = parse_action_state_name(resource_action);
    if (!resource_cart.resource_action) {
        const std::string resource_action_text = xml_value::trim_copy(resource_action);
        if (!xml_value::parse_int_strict(resource_action_text, &resource_cart.resource_action) ||
            resource_cart.resource_action < 0 ||
            resource_cart.resource_action > std::numeric_limits<unsigned char>::max()) {
            return graphics_parse_error("FigureType resource-cart graphics has an invalid resource_action");
        }
    }
    const std::string lift_food_text =
        xml_value::trim_copy(xml_parser_get_attribute_string("lift_food_at_loads"));
    const std::string lift_food_y_text =
        xml_value::trim_copy(xml_parser_get_attribute_string("lift_food_y_adjust"));
    if (!resource_cart.empty_image_group ||
        resource_cart.resource_source == FigureResourceCartSource::None ||
        (resource_cart.resource_source == FigureResourceCartSource::CollectingItemOnAction &&
            resource_cart.resource_action <= 0) ||
        !parse_resource_cart_load_mode(
            xml_parser_get_attribute_string("load_mode"), resource_cart.load_mode) ||
        !parse_resource_cart_state_source(
            xml_parser_get_attribute_string("state_source"), resource_cart.state_source) ||
        !xml_value::parse_bool(
            xml_parser_get_attribute_string("suppress_body_when_hidden"),
            &resource_cart.suppress_body_when_hidden) ||
        !xml_value::parse_int_strict(lift_food_text, &resource_cart.lift_food_at_loads) ||
        resource_cart.lift_food_at_loads < 0 ||
        resource_cart.lift_food_at_loads > std::numeric_limits<unsigned char>::max() ||
        !xml_value::parse_int_strict(lift_food_y_text, &resource_cart.lift_food_y_adjust) ||
        resource_cart.lift_food_y_adjust < std::numeric_limits<signed char>::min() ||
        resource_cart.lift_food_y_adjust > std::numeric_limits<signed char>::max() ||
        !parse_int_array_8(xml_parser_get_attribute_string("offsets_x"), resource_cart.offsets_x) ||
        !parse_int_array_8(xml_parser_get_attribute_string("offsets_y"), resource_cart.offsets_y) ||
        !xml_value::parse_bool(
            xml_parser_get_attribute_string("hide_on_corpse"), &resource_cart.hide_on_corpse) ||
        !g_parse_state.graphics_definition.configure_resource_cart(std::move(resource_cart))) {
        return graphics_parse_error("FigureType resource-cart graphics is duplicated or invalid");
    }
    return 1;
}

static int parse_graphics_directional_node()
{
    if (!parse_enabled_content("directional")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics ||
        !xml_parser_has_attribute("image_group") ||
        !xml_parser_has_attribute("default_base_offset") ||
        !xml_parser_has_attribute("view_adjustments") ||
        !xml_parser_has_attribute("frame_divisor") ||
        !xml_parser_has_attribute("frame_stride")) {
        return graphics_parse_error(
            "FigureType directional graphics is outside graphics or missing required attributes");
    }

    FigureDirectionalGraphics directional;
    directional.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    if (!directional.image_group ||
        !parse_int_attribute_strict("default_base_offset", directional.default_base_image_offset) ||
        !parse_int_attribute_strict("view_adjustments", directional.view_adjustments) ||
        !parse_int_attribute_strict("frame_divisor", directional.frame_divisor) ||
        !parse_int_attribute_strict("frame_stride", directional.frame_stride) ||
        !g_parse_state.graphics_definition.configure_directional(std::move(directional))) {
        return graphics_parse_error("FigureType directional graphics is duplicated or invalid");
    }
    return 1;
}

static int parse_graphics_directional_pose_node()
{
    if (!parse_enabled_content("pose")) {
        return 0;
    }
    if (!xml_parser_has_attribute("role") ||
        !xml_parser_has_attribute("action") ||
        !xml_parser_has_attribute("base_offset")) {
        return graphics_parse_error("FigureType directional pose is missing required attributes");
    }

    FigureDirectionalPoseGraphics pose;
    pose.role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
    pose.action_state = parse_action_state_name(xml_parser_get_attribute_string("action"));
    if (!parse_int_attribute_strict("base_offset", pose.base_image_offset) ||
        !g_parse_state.graphics_definition.add_directional_pose(std::move(pose))) {
        return graphics_parse_error("FigureType directional pose is invalid or duplicates a role/action");
    }
    return 1;
}

static int parse_graphics_state_layer_node()
{
    if (!parse_enabled_content("state")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics ||
        !xml_parser_has_attribute("role") ||
        !xml_parser_has_attribute("action") ||
        !xml_parser_has_attribute("image_group") ||
        !xml_parser_has_attribute("base_offset") ||
        !xml_parser_has_attribute("direction") ||
        !xml_parser_has_attribute("view_adjustments") ||
        !xml_parser_has_attribute("frame") ||
        !xml_parser_has_attribute("frame_divisor") ||
        !xml_parser_has_attribute("direction_stride")) {
        return graphics_parse_error("FigureType graphics state layer is outside graphics or missing required attributes");
    }

    FigureGraphicsStateLayer state_layer;
    state_layer.role = xml_value::trim_copy(xml_parser_get_attribute_string("role"));
    state_layer.action_state = parse_action_state_name(xml_parser_get_attribute_string("action"));
    state_layer.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    state_layer.base_image_offset = xml_parser_get_attribute_int("base_offset");
    state_layer.view_adjustments = xml_parser_get_attribute_int("view_adjustments");
    state_layer.frame_divisor = xml_parser_get_attribute_int("frame_divisor");
    state_layer.direction_frame_stride = xml_parser_get_attribute_int("direction_stride");

    const char *direction = xml_parser_get_attribute_string("direction");
    if (xml_value::equals(direction, "movement")) {
        state_layer.direction = FigureStateDirection::Movement;
    } else if (xml_value::equals(direction, "attack")) {
        state_layer.direction = FigureStateDirection::Attack;
    } else {
        return graphics_parse_error("FigureType graphics state layer has an invalid direction", direction);
    }

    const char *frame = xml_parser_get_attribute_string("frame");
    if (xml_value::equals(frame, "static")) {
        state_layer.frame = FigureStateFrame::Static;
    } else if (xml_value::equals(frame, "image_offset")) {
        state_layer.frame = FigureStateFrame::ImageOffset;
    } else if (xml_value::equals(frame, "missile_launcher")) {
        state_layer.frame = FigureStateFrame::MissileLauncher;
    } else {
        return graphics_parse_error("FigureType graphics state layer has an invalid frame", frame);
    }

    if (!g_parse_state.graphics_definition.add_state_layer(std::move(state_layer))) {
        return graphics_parse_error("FigureType graphics state layer is invalid or duplicates a role/action");
    }
    return 1;
}

static int parse_graphics_map_flag_node()
{
    if (!parse_enabled_content("map_flag")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || !g_parse_state.definition ||
        g_parse_state.definition->type() != FIGURE_MAP_FLAG ||
        !xml_parser_has_attribute("resource_min") ||
        !xml_parser_has_attribute("resource_max_exclusive") ||
        !xml_parser_has_attribute("base_direction") ||
        !xml_parser_has_attribute("view_adjustments") ||
        !xml_parser_has_attribute("frame_divisor") ||
        !xml_parser_has_attribute("frame_stride") ||
        !xml_parser_has_attribute("number_x") ||
        !xml_parser_has_attribute("number_y")) {
        return graphics_parse_error("FigureType map-flag graphics is outside map_flag or missing required attributes");
    }
    GraphicsPoint number_offset;
    number_offset.x = xml_parser_get_attribute_int("number_x");
    number_offset.y = xml_parser_get_attribute_int("number_y");
    if (!g_parse_state.graphics_definition.configure_map_flag(
            xml_parser_get_attribute_int("resource_min"),
            xml_parser_get_attribute_int("resource_max_exclusive"),
            xml_parser_get_attribute_int("base_direction"),
            xml_parser_get_attribute_int("view_adjustments"),
            xml_parser_get_attribute_int("frame_divisor"),
            xml_parser_get_attribute_int("frame_stride"),
            number_offset)) {
        return graphics_parse_error("FigureType map-flag graphics is duplicated or invalid");
    }
    return 1;
}

static int parse_graphics_map_flag_marker_node()
{
    if (!parse_enabled_content("marker")) {
        return 0;
    }
    if (!g_parse_state.graphics_definition.map_flag().enabled ||
        !xml_parser_has_attribute("resource_min") ||
        !xml_parser_has_attribute("resource_max_exclusive") ||
        !xml_parser_has_attribute("image_group") ||
        !xml_parser_has_attribute("image_offset") ||
        !xml_parser_has_attribute("number_base")) {
        return graphics_parse_error("FigureType map-flag marker is outside map_flag or missing required attributes");
    }
    FigureMapFlagMarkerGraphics marker;
    marker.resource_min = xml_parser_get_attribute_int("resource_min");
    marker.resource_max_exclusive = xml_parser_get_attribute_int("resource_max_exclusive");
    marker.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    marker.image_offset = xml_parser_get_attribute_int("image_offset");
    marker.number_base = xml_parser_get_attribute_int("number_base");
    if (!g_parse_state.graphics_definition.add_map_flag_marker(std::move(marker))) {
        return graphics_parse_error("FigureType map-flag marker is invalid or overlaps another marker");
    }
    return 1;
}

static int parse_graphics_hippodrome_node()
{
    if (!parse_enabled_content("hippodrome_race")) {
        return 0;
    }
    if (!g_parse_state.saw_graphics || !g_parse_state.definition ||
        g_parse_state.definition->type() != FIGURE_HIPPODROME_HORSES ||
        !xml_parser_has_attribute("resource_min") ||
        !xml_parser_has_attribute("resource_max_exclusive") ||
        !xml_parser_has_attribute("horse_view_adjustments") ||
        !xml_parser_has_attribute("cart_view_adjustments") ||
        !xml_parser_has_attribute("frame_stride") ||
        !xml_parser_has_attribute("cart_direction_offset") ||
        !xml_parser_has_attribute("cart_offsets_x") ||
        !xml_parser_has_attribute("cart_offsets_y")) {
        return graphics_parse_error("FigureType hippodrome-race graphics is outside hippodrome_horses or missing required attributes");
    }
    std::array<int, 8> cart_offsets_x = {};
    std::array<int, 8> cart_offsets_y = {};
    if (!parse_int_array_8(xml_parser_get_attribute_string("cart_offsets_x"), cart_offsets_x) ||
        !parse_int_array_8(xml_parser_get_attribute_string("cart_offsets_y"), cart_offsets_y) ||
        !g_parse_state.graphics_definition.configure_hippodrome(
            xml_parser_get_attribute_int("resource_min"),
            xml_parser_get_attribute_int("resource_max_exclusive"),
            xml_parser_get_attribute_int("horse_view_adjustments"),
            xml_parser_get_attribute_int("cart_view_adjustments"),
            xml_parser_get_attribute_int("frame_stride"),
            xml_parser_get_attribute_int("cart_direction_offset"),
            std::move(cart_offsets_x),
            std::move(cart_offsets_y))) {
        return graphics_parse_error("FigureType hippodrome-race graphics is duplicated or invalid");
    }
    return 1;
}

static int parse_graphics_hippodrome_team_node()
{
    if (!parse_enabled_content("team")) {
        return 0;
    }
    if (!g_parse_state.graphics_definition.hippodrome().enabled ||
        !xml_parser_has_attribute("resource_min") ||
        !xml_parser_has_attribute("resource_max_exclusive") ||
        !xml_parser_has_attribute("horse_image_group") ||
        !xml_parser_has_attribute("cart_image_group")) {
        return graphics_parse_error("FigureType hippodrome team is outside hippodrome_race or missing required attributes");
    }
    FigureHippodromeTeamGraphics team;
    team.resource_min = xml_parser_get_attribute_int("resource_min");
    team.resource_max_exclusive = xml_parser_get_attribute_int("resource_max_exclusive");
    team.horse_image_group = parse_image_group_name(xml_parser_get_attribute_string("horse_image_group"));
    team.cart_image_group = parse_image_group_name(xml_parser_get_attribute_string("cart_image_group"));
    if (!g_parse_state.graphics_definition.add_hippodrome_team(std::move(team))) {
        return graphics_parse_error("FigureType hippodrome team is invalid or overlaps another team");
    }
    return 1;
}

static int parse_graphics_hippodrome_offsets_node()
{
    if (!parse_enabled_content("offsets")) {
        return 0;
    }
    if (!g_parse_state.graphics_definition.hippodrome().enabled ||
        !xml_parser_has_attribute("orientation") ||
        !xml_parser_has_attribute("max_wait_ticks") ||
        !xml_parser_has_attribute("x") ||
        !xml_parser_has_attribute("y")) {
        return graphics_parse_error("FigureType hippodrome offsets is outside hippodrome_race or missing required attributes");
    }
    FigureHippodromeOffsetSchedule schedule;
    schedule.orientation = parse_cardinal_orientation_name(xml_parser_get_attribute_string("orientation"));
    if (schedule.orientation < 0 ||
        !parse_int_array_7(xml_parser_get_attribute_string("max_wait_ticks"), schedule.max_wait_ticks) ||
        !parse_int_array_7(xml_parser_get_attribute_string("x"), schedule.x) ||
        !parse_int_array_7(xml_parser_get_attribute_string("y"), schedule.y) ||
        !g_parse_state.graphics_definition.add_hippodrome_offset_schedule(std::move(schedule))) {
        return graphics_parse_error("FigureType hippodrome offsets is invalid or duplicates an orientation");
    }
    return 1;
}

static int parse_graphics_target_value_node(int path_node)
{
    if (g_parse_state.current_graphics_target == GraphicsTargetKind::None) {
        return graphics_parse_error("FigureType graphics path/image appears outside a graphics target");
    }
    const char *value_text = xml_parser_get_attribute_string("value");
    if (!value_text) {
        return graphics_parse_error("FigureType graphics path/image is missing value");
    }
    std::string value = xml_value::trim_copy(value_text);
    if (value.empty()) {
        return graphics_parse_error("FigureType graphics path/image value is empty");
    }

    FigureGraphics &graphics_definition = g_parse_state.graphics_definition;
    switch (g_parse_state.current_graphics_target) {
        case GraphicsTargetKind::Default:
            if (graphics_definition.image_group || graphics_definition.image_asset != ASSET_MAX_KEY) {
                return graphics_parse_error("FigureType graphics default target source is duplicated");
            }
            if (path_node) {
                if (!graphics_definition.path_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics default path is duplicated");
                }
                graphics_definition.path_pattern = std::move(value);
            } else {
                if (!graphics_definition.image_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics default image is duplicated");
                }
                graphics_definition.image_pattern = std::move(value);
            }
            return 1;
        case GraphicsTargetKind::Action:
            if (graphics_definition.action_image_group) {
                return graphics_parse_error("FigureType graphics action target source is duplicated");
            }
            if (path_node) {
                if (!graphics_definition.action_path_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics action path is duplicated");
                }
                graphics_definition.action_path_pattern = std::move(value);
            } else {
                if (!graphics_definition.action_image_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics action image is duplicated");
                }
                graphics_definition.action_image_pattern = std::move(value);
            }
            return 1;
        case GraphicsTargetKind::Corpse:
            if (graphics_definition.corpse_image_group || graphics_definition.corpse_image_asset != ASSET_MAX_KEY) {
                return graphics_parse_error("FigureType graphics corpse target source is duplicated");
            }
            if (path_node) {
                if (!graphics_definition.corpse_path_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics corpse path is duplicated");
                }
                graphics_definition.corpse_path_pattern = std::move(value);
            } else {
                if (!graphics_definition.corpse_image_pattern.empty()) {
                    return graphics_parse_error("FigureType graphics corpse image is duplicated");
                }
                graphics_definition.corpse_image_pattern = std::move(value);
            }
            return 1;
        case GraphicsTargetKind::None:
        default:
            return graphics_parse_error("FigureType graphics path/image target is invalid");
    }
}

static int parse_graphics_path_node()
{
    if (!parse_enabled_content("path")) {
        return 0;
    }
    return parse_graphics_target_value_node(1);
}

static int parse_graphics_image_node()
{
    if (!parse_enabled_content("image")) {
        return 0;
    }
    return parse_graphics_target_value_node(0);
}

static int validate_graphics_target_policy()
{
    const FigureGraphics &graphics_definition = g_parse_state.graphics_definition;
    const int specialized_policy_count =
        (graphics_definition.missile_launcher().enabled() ? 1 : 0) +
        (graphics_definition.resource_cart().enabled ? 1 : 0) +
        (graphics_definition.directional().enabled ? 1 : 0);
    const int specialized_policy_only =
        graphics_definition.default_source_count() == 0 &&
        specialized_policy_count == 1 &&
        graphics_definition.action_source_count() == 0 &&
        graphics_definition.corpse_source_count() == 0 &&
        graphics_definition.overlays().empty() &&
        graphics_definition.state_layers().empty() &&
        !graphics_definition.standard().enabled &&
        !graphics_definition.map_flag().enabled &&
        !graphics_definition.hippodrome().enabled &&
        graphics_definition.cart_mode == CartGraphicsMode::None;
    const int state_sources_only =
        graphics_definition.default_source_count() == 0 &&
        specialized_policy_count <= 1 &&
        graphics_definition.action_source_count() == 0 &&
        graphics_definition.corpse_source_count() == 0 &&
        graphics_definition.overlays().empty() &&
        !graphics_definition.state_layers().empty() &&
        !graphics_definition.standard().enabled &&
        !graphics_definition.map_flag().enabled &&
        !graphics_definition.hippodrome().enabled &&
        graphics_definition.cart_mode == CartGraphicsMode::None;
    if ((!specialized_policy_only && !state_sources_only && graphics_definition.default_source_count() != 1) ||
        graphics_target_has_image_without_path(graphics_definition.path_pattern, graphics_definition.image_pattern)) {
        return graphics_parse_error(
            "FigureType graphics node must choose one default source, state-owned sources, or one specialized policy-only target");
    }
    if (g_parse_state.saw_graphics_action) {
        if (graphics_definition.action_source_count() != 1 ||
            graphics_target_has_image_without_path(
                graphics_definition.action_path_pattern,
                graphics_definition.action_image_pattern)) {
            return graphics_parse_error("FigureType action graphics requires one complete source");
        }
        if (graphics_definition.has_action_native_payload() && !graphics_definition.action_state) {
            return graphics_parse_error("FigureType native action graphics requires a state");
        }
    }
    if (graphics_definition.corpse_source_count() > 1 ||
        graphics_target_has_image_without_path(
            graphics_definition.corpse_path_pattern,
            graphics_definition.corpse_image_pattern)) {
        return graphics_parse_error("FigureType corpse graphics target has an image without a path");
    }
    if (g_parse_state.saw_graphics_corpse && graphics_definition.corpse_source_count() != 1) {
        return graphics_parse_error("FigureType corpse graphics target requires a source");
    }
    if (graphics_definition.standard().enabled && graphics_definition.standard().flags.empty()) {
        return graphics_parse_error("FigureType standard graphics requires at least one flag");
    }
    if (graphics_definition.map_flag().enabled && !graphics_definition.map_flag().covers_authored_range()) {
        return graphics_parse_error("FigureType map-flag markers must cover the complete authored resource range");
    }
    if (graphics_definition.hippodrome().enabled && !graphics_definition.hippodrome().is_complete()) {
        return graphics_parse_error("FigureType hippodrome-race graphics requires complete teams and four offset schedules");
    }
    for (const FigureGraphicsStateLayer &state_layer : graphics_definition.state_layers()) {
        if (state_layer.frame == FigureStateFrame::MissileLauncher &&
            !graphics_definition.missile_launcher().enabled()) {
            return graphics_parse_error(
                "FigureType state graphics uses missile_launcher frame policy without a missile_launcher definition");
        }
    }
    return validate_depot_cart_graphics(
        g_parse_state.definition.get(),
        &g_parse_state.error,
        graphics_definition);
}

static int validate_current_graphics_target_policy()
{
    const FigureGraphics &graphics_definition = g_parse_state.graphics_definition;
    switch (g_parse_state.current_graphics_target) {
        case GraphicsTargetKind::Default:
            if (graphics_definition.default_source_count() != 1 ||
                graphics_target_has_image_without_path(
                    graphics_definition.path_pattern,
                    graphics_definition.image_pattern)) {
                return graphics_parse_error("FigureType graphics default target requires one complete source");
            }
            return 1;
        case GraphicsTargetKind::Action:
            if (graphics_definition.action_source_count() != 1 ||
                graphics_target_has_image_without_path(
                    graphics_definition.action_path_pattern,
                    graphics_definition.action_image_pattern)) {
                return graphics_parse_error("FigureType action graphics target requires one complete source");
            }
            if (graphics_definition.has_action_native_payload() && !graphics_definition.action_state) {
                return graphics_parse_error("FigureType native action graphics requires a state");
            }
            return 1;
        case GraphicsTargetKind::Corpse:
            if (graphics_definition.corpse_source_count() != 1 ||
                graphics_target_has_image_without_path(
                    graphics_definition.corpse_path_pattern,
                    graphics_definition.corpse_image_pattern)) {
                return graphics_parse_error("FigureType corpse graphics target requires one complete source");
            }
            return 1;
        case GraphicsTargetKind::None:
        default:
            return 1;
    }
}

static void finish_graphics_target_node()
{
    if (g_parse_state.error || g_parse_state.current_graphics_target == GraphicsTargetKind::None) {
        g_parse_state.current_graphics_target = GraphicsTargetKind::None;
        return;
    }

    validate_current_graphics_target_policy();
    g_parse_state.current_graphics_target = GraphicsTargetKind::None;
}

static void finish_graphics_node()
{
    if (g_parse_state.error || !g_parse_state.definition) {
        return;
    }
    if (!validate_graphics_target_policy()) {
        return;
    }
    g_parse_state.definition->set_graphics(g_parse_state.graphics_definition);
}

static int parse_pathing_node()
{
    if (!parse_enabled_content("pathing")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("pathing");
    if (!profile) {
        return 0;
    }
    if (g_parse_state.saw_profile_pathing) {
        g_parse_state.error = true;
        log_error("FigureType profile contains duplicate pathing nodes", profile->id(), 0);
        return 0;
    }
    if (!xml_parser_has_attribute("mode") || !xml_parser_has_attribute("terrain")) {
        g_parse_state.error = true;
        log_error("FigureType pathing node is missing required attributes", 0, 0);
        return 0;
    }
    const PathingMode *mode = pathing_mode_from_xml_id(xml_parser_get_attribute_string("mode"));
    if (!mode) {
        g_parse_state.error = true;
        log_error("FigureType pathing node has an unknown mode", xml_parser_get_attribute_string("mode"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("effect") &&
        !is_known_service_effect_name(xml_parser_get_attribute_string("effect"))) {
        g_parse_state.error = true;
        log_error("FigureType pathing node has an unknown effect", xml_parser_get_attribute_string("effect"), 0);
        return 0;
    }

    PathingPolicy pathing_policy;
    pathing_policy.mode = mode;
    const int terrain_usage = parse_terrain_usage_name(xml_parser_get_attribute_string("terrain"));
    if (terrain_usage < 0) {
        g_parse_state.error = true;
        log_error("FigureType pathing node has an invalid terrain", xml_parser_get_attribute_string("terrain"), 0);
        return 0;
    }
    pathing_policy.terrain = PathingMode::terrainFromLegacyUsage(terrain_usage);
    const char *effect_text = xml_parser_get_attribute_string("effect");
    pathing_policy.effect = parse_service_effect_name(effect_text);

    // Smart service pathing compares road-tile history for one explicit service effect.
    if (!pathing_policy.hasRequiredServiceEffect()) {
        g_parse_state.error = true;
        error_context_report_error("FigureType smart_service pathing requires a service effect",
            profile->id());
        return 0;
    }
    if (!pathing_policy.hasRequiredTerrainAccess()) {
        g_parse_state.error = true;
        error_context_report_error(
            "FigureType pathing requires road-capable terrain",
            profile->id());
        return 0;
    }

    profile->set_pathing_policy(pathing_policy);
    g_parse_state.saw_profile_pathing = true;
    return 1;
}

static int parse_venue_targets_node()
{
    if (!parse_enabled_content("venue_targets")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("venue_targets");
    if (!profile) {
        return 0;
    }
    if (xml_parser_has_attribute("ranking")) {
        g_parse_state.error = true;
        log_error("FigureType venue_targets ranking is retired; route-weighted venue scoring is used for all profiles",
            profile->id(), 0);
        return 0;
    }
    if (xml_parser_has_attribute("show_duration")) {
        const int show_duration = xml_parser_get_attribute_int("show_duration");
        if (show_duration <= 0 || show_duration > std::numeric_limits<unsigned char>::max()) {
            g_parse_state.error = true;
            log_error("FigureType venue_targets has an invalid show_duration", 0, 0);
            return 0;
        }
        profile->set_show_duration(show_duration);
    }
    return 1;
}

static int parse_venue_node()
{
    if (!parse_enabled_content("venue")) {
        return 0;
    }
    FigureTypeProfile *profile = current_profile_or_error("venue");
    if (!profile) {
        return 0;
    }
    if (!xml_parser_has_attribute("building") ||
        !xml_parser_has_attribute("show_slot")) {
        g_parse_state.error = true;
        log_error("FigureType venue is missing required attributes", 0, 0);
        return 0;
    }

    EntertainmentVenueTarget target;
    target.building_reference = parse_building_type_reference(xml_parser_get_attribute_string("building"));
    target.show_slot = parse_show_slot_name(xml_parser_get_attribute_string("show_slot"));
    if (target.building_reference.empty() ||
        target.show_slot == EntertainmentShowSlot::None) {
        g_parse_state.error = true;
        log_error("FigureType venue has an invalid building or show_slot", profile->id(), 0);
        return 0;
    }

    profile->add_venue_target(target);
    return 1;
}

static const std::array<xml_parser_element, 30> XML_ELEMENTS = { {
    { "figure", parse_definition_root, nullptr, nullptr, nullptr },
    { "profiles", parse_profiles_node, nullptr, "figure", nullptr },
    { "profile", parse_profile_node, finish_profile_node, "profiles", nullptr },
    { "native", parse_native_node, nullptr, "profile", nullptr },
    { "spawn", parse_spawn_node, nullptr, "profile", nullptr },
    { "owner", parse_owner_node, nullptr, "profile", nullptr },
    { "movement", parse_movement_node, nullptr, "profile", nullptr },
    { "graphics", parse_graphics_node, finish_graphics_node, "figure", nullptr },
    { "default", parse_graphics_default_node, finish_graphics_target_node, "graphics", nullptr },
    { "action", parse_graphics_action_node, finish_graphics_target_node, "graphics", nullptr },
    { "corpse", parse_graphics_corpse_node, finish_graphics_target_node, "graphics", nullptr },
    { "cart", parse_graphics_cart_node, nullptr, "graphics", nullptr },
    { "overlay", parse_graphics_overlay_node, nullptr, "graphics", nullptr },
    { "standard", parse_graphics_standard_node, nullptr, "graphics", nullptr },
    { "flag", parse_graphics_standard_flag_node, nullptr, "standard", nullptr },
    { "missile_launcher", parse_graphics_missile_launcher_node, nullptr, "graphics", nullptr },
    { "resource_cart", parse_graphics_resource_cart_node, nullptr, "graphics", nullptr },
    { "directional", parse_graphics_directional_node, nullptr, "graphics", nullptr },
    { "pose", parse_graphics_directional_pose_node, nullptr, "directional", nullptr },
    { "state", parse_graphics_state_layer_node, nullptr, "graphics", nullptr },
    { "map_flag", parse_graphics_map_flag_node, nullptr, "graphics", nullptr },
    { "marker", parse_graphics_map_flag_marker_node, nullptr, "map_flag", nullptr },
    { "hippodrome_race", parse_graphics_hippodrome_node, nullptr, "graphics", nullptr },
    { "team", parse_graphics_hippodrome_team_node, nullptr, "hippodrome_race", nullptr },
    { "offsets", parse_graphics_hippodrome_offsets_node, nullptr, "hippodrome_race", nullptr },
    { "path", parse_graphics_path_node, nullptr, "default|action|corpse", nullptr },
    { "image", parse_graphics_image_node, nullptr, "default|action|corpse", nullptr },
    { "pathing", parse_pathing_node, nullptr, "profile", nullptr },
    { "venue_targets", parse_venue_targets_node, nullptr, "profile", nullptr },
    { "venue", parse_venue_node, nullptr, "venue_targets", nullptr }
} };

static int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    StagedFigureType *out_definition)
{
    g_parse_state = {};
    const ErrorContextScope scope("FigureType XML", filename);
    const int parsed = xml_definition::parse_buffer(
        filename,
        "FigureType",
        XML_ELEMENTS.data(),
        static_cast<int>(XML_ELEMENTS.size()),
        buffer);
    if (!parsed) {
        return 0;
    }

    if (g_parse_state.error ||
        !g_parse_state.saw_root ||
        !g_parse_state.definition) {
        error_context_report_error("Invalid FigureType XML", filename);
        return 0;
    }

    figure_type type = g_parse_state.definition->type();
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        return 0;
    }

    if (out_definition) {
        out_definition->definition = std::move(g_parse_state.definition);
        out_definition->disabled = g_parse_state.disabled;
        out_definition->graphics_only = g_parse_state.graphics_only;
        out_definition->saw_profiles = g_parse_state.saw_profiles;
        out_definition->saw_graphics = g_parse_state.saw_graphics;
    }
    return 1;
}

static int parse_definition_file(
    const mod_definition::DefinitionSource &source,
    StagedFigureType *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "FigureType")) {
        return 0;
    }
    return parse_definition_buffer(source.full_path.c_str(), buffer, out_definition);
}

static int stage_definition(StagedFigureTypes &staged, StagedFigureType definition)
{
    if (!definition.definition) {
        staged.failure_reason = "FigureType definition is missing its root identity.";
        return 0;
    }
    const figure_type type = definition.definition->type();
    const std::string attr = definition.definition->attr();
    const std::optional<FigureIdentityReservation> &slot_identity = staged.slots[type];
    if (slot_identity && slot_identity->attr != attr) {
        staged.failure_reason = "FigureType runtime slot is already owned by '" + slot_identity->attr +
            "' from " + slot_identity->source.full_path + "; " + definition.source.full_path +
            " cannot replace it with alias '" + attr + "'.";
        return 0;
    }
    const auto id_identity = staged.ids.find(attr);
    if (id_identity != staged.ids.end() && id_identity->second.type != type) {
        staged.failure_reason = "FigureType id '" + attr + "' changes runtime slot between " +
            id_identity->second.source.full_path + " and " + definition.source.full_path + ".";
        return 0;
    }
    if (!staged.overlays.apply(attr, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    if (!slot_identity) {
        FigureIdentityReservation identity{type, attr, definition.source};
        staged.slots[type] = identity;
        staged.ids.emplace(attr, std::move(identity));
    }
    staged.winners[type] = std::move(definition);
    return 1;
}

static int validate_final_winners(StagedFigureTypes &staged)
{
    for (std::optional<StagedFigureType> &winner : staged.winners) {
        if (!winner || winner->disabled) {
            continue;
        }
        FigureTypeDefinition &definition = *winner->definition;
        const int incomplete_graphics_only = winner->graphics_only &&
            (!winner->saw_graphics || winner->saw_profiles || !definition.profiles().empty());
        const int incomplete_runtime_definition = !winner->graphics_only &&
            (!winner->saw_profiles || !winner->saw_graphics || definition.profiles().empty() ||
                !definition.default_profile());
        if (incomplete_graphics_only || incomplete_runtime_definition) {
            staged.failure_reason = "Final FigureType winner is incomplete: " + winner->source.full_path;
            return 0;
        }
        if (!definition.cache_graphics_bindings()) {
            staged.failure_reason = "Final FigureType winner has invalid graphics: " + winner->source.full_path;
            return 0;
        }
    }
    return 1;
}

static int validate_building_spawn_profile_references()
{
    // BuildingType XML may now be the only place that names a FigureType
    // profile. Validate those edges after FigureType precedence is resolved so
    // a missing residential or service profile fails at load time, not when a
    // city later tries to spawn the walker.
    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &building_definition :
        building_type_registry_impl::g_building_types) {
        if (!building_definition) {
            continue;
        }

        for (const building_type_registry_impl::SpawnDelayGroup &group : building_definition->spawn_groups()) {
            for (const building_type_registry_impl::SpawnPolicy &policy : group.policies) {
                if (policy.profile.empty()) {
                    continue;
                }

                const FigureTypeDefinition *figure_definition = definition_for(policy.spawn_figure);
                const FigureTypeProfile *figure_profile = profile_for(policy.spawn_figure, policy.profile.c_str());
                if (figure_definition && figure_profile && figure_profile->spawn_behavior().has_action_state) {
                    continue;
                }

                std::string detail = "building=";
                detail += building_definition->attr();
                detail += " figure=";
                detail += figure_definition ? figure_definition->attr() : std::to_string(static_cast<int>(policy.spawn_figure));
                detail += " profile=";
                detail += policy.profile;
                set_failure_reason("BuildingType spawn FigureType profile is missing or cannot initialize a spawned figure.", detail.c_str());
                log_error("BuildingType spawn FigureType profile is missing or cannot initialize a spawned figure", detail.c_str(), 0);
                return 0;
            }
        }
    }
    return 1;
}

} // namespace figure_type_registry_impl

const char *figure_type_registry_get_failure_reason(void)
{
    return figure_type_registry_impl::g_failure_reason.c_str();
}

const char *figure_type_definition_source_path(const char *type_attr)
{
    if (!type_attr || !*type_attr) {
        return nullptr;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        figure_type_registry_impl::g_figure_type_overlays.find(type_attr);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int figure_type_definition_is_suppressed(const char *type_attr)
{
    if (!type_attr || !*type_attr) {
        return 0;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        figure_type_registry_impl::g_figure_type_overlays.find(type_attr);
    return entry && entry->disabled;
}

void figure_type_registry_reset(void)
{
    figure_type_registry_impl::g_failure_reason.clear();
    figure_type_registry_impl::g_figure_type_overlays.clear();
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_types) {
        definition.reset();
    }
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_graphics_only) {
        definition.reset();
    }
}

int figure_type_registry_load(void)
{
    using namespace figure_type_registry_impl;

    StagedFigureTypes staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"FigureType"},
            "FigureType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedFigureType definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse FigureType xml: " + source.full_path;
                    return false;
                }
                return stage_definition(staged, std::move(definition)) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        g_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        return 0;
    }
    if (!validate_final_winners(staged)) {
        g_failure_reason = staged.failure_reason;
        return 0;
    }

    std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> published;
    std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> published_graphics_only;
    for (std::size_t type = 0; type < staged.winners.size(); ++type) {
        if (staged.winners[type] && !staged.winners[type]->disabled) {
            if (staged.winners[type]->graphics_only) {
                published_graphics_only[type] = std::move(staged.winners[type]->definition);
            } else {
                published[type] = std::move(staged.winners[type]->definition);
            }
        }
    }
    g_figure_types = std::move(published);
    g_figure_graphics_only = std::move(published_graphics_only);
    g_figure_type_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int figure_type_layered_definition_buffers_are_valid_for_test(
    const figure_type_layer_test_input *inputs,
    int input_count,
    const char *query_attr,
    figure_type_layer_test_result *result)
{
    using namespace figure_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedFigureTypes staged;
    for (int index = 0; index < input_count; ++index) {
        const figure_type_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedFigureType definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "FigureTypeLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "FigureType";
        if (!parse_definition_buffer(definition.source.full_path.c_str(), buffer, &definition) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!validate_final_winners(staged)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_source_layer = -1;
        const std::string attr = query_attr ? query_attr : "";
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(attr);
        const auto identity = staged.ids.find(attr);
        if (overlay && identity != staged.ids.end()) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            const std::optional<StagedFigureType> &winner = staged.winners[identity->second.type];
            if (winner && !winner->disabled && winner->definition &&
                (winner->graphics_only || winner->definition->default_profile())) {
                if (const FigureTypeProfile *profile = winner->definition->default_profile()) {
                    result->queried_max_roam_length = profile->movement_profile().max_roam_length;
                }
                const std::vector<FigureGraphicsOverlay> &overlays = winner->definition->graphics().overlays();
                result->queried_overlay_count = static_cast<int>(overlays.size());
                if (!overlays.empty()) {
                    const FigureGraphicsOverlay &overlay_definition = overlays.front();
                    result->queried_overlay_image_group = overlay_definition.image_group;
                    result->queried_overlay_follows_direction =
                        overlay_definition.direction == FigureOverlayDirection::Figure;
                    result->queried_overlay_follows_frame =
                        overlay_definition.frame == FigureOverlayFrame::Figure;
                    result->queried_overlay_action_count =
                        static_cast<int>(overlay_definition.visible_actions.size());
                    result->queried_overlay_resource_stride = overlay_definition.resource_stride;
                    result->queried_overlay_direction_stride = overlay_definition.direction_frame_stride;
                    result->queried_overlay_hide_on_corpse = overlay_definition.hide_on_corpse;
                    result->queried_overlay_sample_image_offset =
                        overlay_definition.legacy_image_offset(3, 4, 2);
                    const GraphicsPoint sample_offset = overlay_definition.legacy_draw_offset(3);
                    result->queried_overlay_sample_x_offset = sample_offset.x;
                    result->queried_overlay_sample_y_offset = sample_offset.y;
                }
                const FigureStandardGraphics &standard = winner->definition->graphics().standard();
                result->queried_standard_enabled = standard.enabled;
                result->queried_standard_flag_count = static_cast<int>(standard.flags.size());
                result->queried_standard_moving_frame_divisor = standard.moving_frame_divisor;
                result->queried_standard_sample_moving_offset =
                    standard.flag_image_offset(FIGURE_FORT_LEGIONARY, 0, 10);
                result->queried_standard_sample_halted_offset =
                    standard.flag_image_offset(FIGURE_FORT_LEGIONARY, 1, 10);
                const FigureGraphics &graphics = winner->definition->graphics();
                result->queried_state_layer_count = static_cast<int>(graphics.state_layers().size());
                if (const FigureGraphicsStateLayer *going =
                        graphics.state_layer_for_action(FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE)) {
                    result->queried_bucket_going_image_group = going->image_group;
                    result->queried_bucket_going_sample_offset = going->legacy_image_offset(5, 2, 3);
                }
                if (const FigureGraphicsStateLayer *at_fire =
                        graphics.state_layer_for_action(FIGURE_ACTION_75_PREFECT_AT_FIRE)) {
                    result->queried_bucket_at_fire_image_group = at_fire->image_group;
                    result->queried_bucket_at_fire_sample_offset = at_fire->legacy_image_offset(7, 2, 6);
                }
                if (const FigureGraphicsStateLayer *idle =
                        graphics.state_layer_for_action(FIGURE_ACTION_180_BALLISTA_CREATED)) {
                    result->queried_ballista_idle_image_group = idle->image_group;
                    result->queried_ballista_idle_sample_offset = idle->legacy_image_offset(5, 2, 6);
                }
                if (const FigureGraphicsStateLayer *firing =
                        graphics.state_layer_for_action(FIGURE_ACTION_181_BALLISTA_FIRING)) {
                    result->queried_ballista_firing_image_group = firing->image_group;
                    result->queried_ballista_firing_uses_missile_launcher =
                        firing->frame == FigureStateFrame::MissileLauncher;
                    result->queried_ballista_firing_sample_offset = firing->legacy_image_offset(5, 2, 6);
                }
                const FigureMapFlagGraphics &map_flag = graphics.map_flag();
                result->queried_map_flag_enabled = map_flag.enabled;
                result->queried_map_flag_marker_count = static_cast<int>(map_flag.markers.size());
                result->queried_map_flag_sample_base_offset = map_flag.legacy_base_image_offset(2, 6);
                if (const FigureMapFlagMarkerGraphics *invasion = map_flag.marker_for(4)) {
                    result->queried_map_flag_invasion_image_group = invasion->image_group;
                    result->queried_map_flag_invasion_image_offset = invasion->image_offset;
                }
                result->queried_map_flag_invasion_number = map_flag.number_for(11);
                if (const FigureMapFlagMarkerGraphics *fishing = map_flag.marker_for(21)) {
                    result->queried_map_flag_fishing_image_group = fishing->image_group;
                }
                result->queried_map_flag_fishing_number = map_flag.number_for(21);
                if (const FigureMapFlagMarkerGraphics *herd = map_flag.marker_for(25)) {
                    result->queried_map_flag_herd_image_offset = herd->image_offset;
                }
                result->queried_map_flag_number_x = map_flag.number_offset.x;
                result->queried_map_flag_number_y = map_flag.number_offset.y;
                const FigureHippodromeGraphics &hippodrome = graphics.hippodrome();
                result->queried_hippodrome_enabled = hippodrome.enabled;
                result->queried_hippodrome_team_count = static_cast<int>(hippodrome.teams.size());
                result->queried_hippodrome_schedule_count = static_cast<int>(hippodrome.offset_schedules.size());
                result->queried_hippodrome_sample_horse_offset =
                    hippodrome.horse_image_offset(6, 2, 3);
                result->queried_hippodrome_sample_cart_offset =
                    hippodrome.cart_image_offset(6, 2);
                const GraphicsPoint cart_offset = hippodrome.cart_draw_offset(6, 2);
                result->queried_hippodrome_sample_cart_x = cart_offset.x;
                result->queried_hippodrome_sample_cart_y = cart_offset.y;
                result->queried_hippodrome_destination_horse_offset =
                    hippodrome.horse_image_offset(DIR_FIGURE_AT_DESTINATION, 0, 0);
                result->queried_hippodrome_destination_cart_offset =
                    hippodrome.cart_image_offset(DIR_FIGURE_AT_DESTINATION, 0);
                if (const FigureHippodromeTeamGraphics *team0 = hippodrome.team_for(0)) {
                    result->queried_hippodrome_team0_horse_group = team0->horse_image_group;
                }
                if (const FigureHippodromeTeamGraphics *team1 = hippodrome.team_for(1)) {
                    result->queried_hippodrome_team1_cart_group = team1->cart_image_group;
                }
                if (const FigureHippodromeOffsetSchedule *top = hippodrome.schedule_for(DIR_0_TOP)) {
                    const GraphicsPoint offset = top->offset_for(11);
                    result->queried_hippodrome_top_x = offset.x;
                    result->queried_hippodrome_top_y = offset.y;
                }
                if (const FigureHippodromeOffsetSchedule *right = hippodrome.schedule_for(DIR_2_RIGHT)) {
                    result->queried_hippodrome_right_x = right->offset_for(11).x;
                }
                if (const FigureHippodromeOffsetSchedule *bottom = hippodrome.schedule_for(DIR_4_BOTTOM)) {
                    result->queried_hippodrome_bottom_x = bottom->offset_for(10).x;
                }
                if (const FigureHippodromeOffsetSchedule *left = hippodrome.schedule_for(DIR_6_LEFT)) {
                    result->queried_hippodrome_left_y = left->offset_for(14).y;
                }
                const FigureMissileLauncherGraphics &launcher = graphics.missile_launcher();
                result->queried_missile_launcher_enabled = launcher.enabled();
                result->queried_missile_launcher_uses_attack_cursor =
                    launcher.cursor == FigureMissileLauncherCursor::AttackImageOffset;
                result->queried_missile_launcher_uses_wait_cursor =
                    launcher.cursor == FigureMissileLauncherCursor::MissileWaitTicks;
                result->queried_missile_launcher_frame_divisor = launcher.frame_divisor;
                result->queried_missile_launcher_frame_count = static_cast<int>(launcher.frames.size());
                result->queried_missile_launcher_after_frame = launcher.after_frame;
                result->queried_missile_launcher_sample_start = launcher.frame_for(0, 0);
                result->queried_missile_launcher_sample_middle = launcher.frame_for(20, 20);
                result->queried_missile_launcher_sample_last = launcher.frame_for(30, 60);
                result->queried_missile_launcher_sample_after = launcher.frame_for(32, 128);
                const FigureResourceCartGraphics &resource_cart = graphics.resource_cart();
                result->queried_resource_cart_enabled = resource_cart.enabled;
                result->queried_resource_cart_empty_image_group = resource_cart.empty_image_group;
                result->queried_resource_cart_collecting_item_source =
                    resource_cart.resource_source == FigureResourceCartSource::CollectingItemOnAction;
                result->queried_resource_cart_resource_action = resource_cart.resource_action;
                result->queried_resource_cart_fixed_one_load =
                    resource_cart.load_mode == FigureResourceCartLoadMode::FixedOne;
                result->queried_resource_cart_carried_or_one_load =
                    resource_cart.load_mode == FigureResourceCartLoadMode::CarriedOrOne;
                result->queried_resource_cart_runtime_state =
                    resource_cart.state_source == FigureResourceCartStateSource::RuntimeState;
                result->queried_resource_cart_suppress_body_when_hidden =
                    resource_cart.suppress_body_when_hidden;
                result->queried_resource_cart_lift_food_at_loads = resource_cart.lift_food_at_loads;
                result->queried_resource_cart_lift_food_y_adjust = resource_cart.lift_food_y_adjust;
                result->queried_resource_cart_hide_on_corpse = resource_cart.hide_on_corpse;
                result->queried_resource_cart_sample_x = resource_cart.offsets_x[3];
                result->queried_resource_cart_sample_y = resource_cart.offsets_y[3];
                const FigureDirectionalGraphics &directional = graphics.directional();
                result->queried_directional_enabled = directional.enabled;
                result->queried_directional_image_group = directional.image_group;
                result->queried_directional_pose_count = static_cast<int>(directional.poses.size());
                result->queried_directional_default_sample_offset =
                    directional.image_offset_for(0, 6, 2, 7);
                result->queried_directional_pose_sample_offset =
                    directional.image_offset_for(FIGURE_ACTION_192_FISHING_BOAT_FISHING, 6, 2, 7);
            }
        }
    }
    return 1;
}
#endif

int figure_type_registry_resolve_building_references(void)
{
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_types) {
        if (definition && !definition->resolve_building_references()) {
            return 0;
        }
    }
    return figure_type_registry_impl::validate_building_spawn_profile_references();
}
