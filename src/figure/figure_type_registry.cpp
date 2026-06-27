#

#include "figure/figure_type_registry_internal.h"

#include "building/building_type_registry_internal.h"
#include "core/crash_context.h"
#include "core/xml_value.h"
#include "figure/action.h"
#include "game/mod_manager.h"

#include "core/log.h"
#include "building/properties.h"
#include "core/image.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"

#include <array>
#include <cstddef>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
std::string g_failure_reason;

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

void FigureTypeDefinition::set_graphics_policy(const GraphicsPolicy &graphics_policy)
{
    graphics_policy_ = graphics_policy;
}

const GraphicsPolicy &FigureTypeDefinition::graphics_policy() const
{
    return graphics_policy_;
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

static void append_unique_candidate_path(std::vector<std::string> &paths, const char *mod_path)
{
    if (!mod_path || !*mod_path) {
        return;
    }

    std::string candidate = std::string(mod_path) + "FigureType/";
    for (const std::string &existing : paths) {
        if (existing == candidate) {
            return;
        }
    }
    paths.push_back(std::move(candidate));
}

std::vector<std::string> build_candidate_definition_paths()
{
    std::vector<std::string> paths;
    append_unique_candidate_path(paths, mod_manager::mod_path().c_str());

    const auto &mod_names = mod_manager::mod_names();
    const auto &mod_paths = mod_manager::mod_paths();
    for (size_t i = 0; i < mod_names.size() && i < mod_paths.size(); ++i) {
        const std::string &name = mod_names[i];
        if (xml_value::equals(name.c_str(), "Augustus") || xml_value::equals(name.c_str(), "Julius")) {
            append_unique_candidate_path(paths, mod_paths[i].c_str());
        }
    }
    return paths;
}

const FigureTypeDefinition *definition_for(figure_type type)
{
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        return nullptr;
    }
    return g_figure_types[type].get();
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

    static constexpr std::array<NamedGroup, 22> kImageGroups = { {
        { "labor_seeker", GROUP_FIGURE_LABOR_SEEKER },
        { "engineer", GROUP_FIGURE_ENGINEER },
        { "prefect", GROUP_FIGURE_PREFECT },
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
        { "lion_tamer", GROUP_FIGURE_LION_TAMER },
        { "charioteer", GROUP_FIGURE_CHARIOTEER },
        { "teacher_librarian", GROUP_FIGURE_TEACHER_LIBRARIAN },
        { "barber", GROUP_FIGURE_BARBER },
        { "bathhouse_worker", GROUP_FIGURE_BATHHOUSE_WORKER },
        { "school_child", GROUP_FIGURE_SCHOOL_CHILD },
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
    const GraphicsPolicy &graphics_policy)
{
    if (!definition || definition->type() != FIGURE_DEPOT_CART_PUSHER) {
        return 1;
    }

    if (graphics_policy.image_asset == ASSET_MAX_KEY ||
        graphics_policy.corpse_image_asset == ASSET_MAX_KEY ||
        graphics_policy.cart_mode != CartGraphicsMode::ResourceLoad) {
        *parse_error = true;
        log_error("Depot cart pusher graphics requires image_asset, corpse_image_asset, and cart_mode resource_load",
            definition->attr(), 0);
        return 0;
    }
    return 1;
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

struct ParseState {
    std::unique_ptr<FigureTypeDefinition> definition;
    FigureTypeProfile *current_profile = nullptr;
    bool saw_root = false;
    bool saw_profiles = false;
    bool saw_profile_native = false;
    bool saw_profile_owner = false;
    bool saw_profile_movement = false;
    bool saw_profile_pathing = false;
    bool saw_graphics = false;
    bool error = false;
};

static ParseState g_parse_state;

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

    g_parse_state.definition = std::make_unique<FigureTypeDefinition>(type, type_attr ? type_attr : "");
    g_parse_state.saw_root = true;
    return 1;
}

static int parse_profiles_node()
{
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

static int parse_graphics_node()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.saw_graphics) {
        g_parse_state.error = true;
        log_error("FigureType xml contains duplicate graphics nodes", g_parse_state.definition->attr(), 0);
        return 0;
    }
    const bool has_image_group = xml_parser_has_attribute("image_group");
    const bool has_image_asset = xml_parser_has_attribute("image_asset");
    const bool has_path_pattern = xml_parser_has_attribute("path_pattern");
    const bool has_image_pattern = xml_parser_has_attribute("image_pattern");
    const int source_count = (has_image_group ? 1 : 0) + (has_image_asset ? 1 : 0) + (has_path_pattern || has_image_pattern ? 1 : 0);
    if (source_count != 1) {
        g_parse_state.error = true;
        log_error("FigureType graphics node must choose exactly one image_group, image_asset, or path/image pattern", 0, 0);
        return 0;
    }
    if (has_path_pattern != has_image_pattern) {
        g_parse_state.error = true;
        log_error("FigureType graphics path pattern requires both path_pattern and image_pattern", 0, 0);
        return 0;
    }

    GraphicsPolicy graphics_policy;
    if (has_image_group) {
        graphics_policy.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
        if (!graphics_policy.image_group) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has an unknown image_group", xml_parser_get_attribute_string("image_group"), 0);
            return 0;
        }
    } else if (has_image_asset) {
        graphics_policy.image_asset = parse_image_asset_name(xml_parser_get_attribute_string("image_asset"));
        if (graphics_policy.image_asset == ASSET_MAX_KEY) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has an unknown image_asset", xml_parser_get_attribute_string("image_asset"), 0);
            return 0;
        }
    } else {
        graphics_policy.path_pattern = xml_value::trim_copy(xml_parser_get_attribute_string("path_pattern"));
        graphics_policy.image_pattern = xml_value::trim_copy(xml_parser_get_attribute_string("image_pattern"));
        if (graphics_policy.path_pattern.empty() || graphics_policy.image_pattern.empty()) {
            g_parse_state.error = true;
            log_error("FigureType graphics path pattern requires non-empty path_pattern and image_pattern", 0, 0);
            return 0;
        }
    }
    const bool has_sprite_offset_x = xml_parser_has_attribute("sprite_offset_x");
    const bool has_sprite_offset_y = xml_parser_has_attribute("sprite_offset_y");
    if (has_sprite_offset_x != has_sprite_offset_y) {
        g_parse_state.error = true;
        log_error("FigureType graphics sprite offset requires both sprite_offset_x and sprite_offset_y", 0, 0);
        return 0;
    }
    if (has_sprite_offset_x) {
        graphics_policy.has_sprite_offset = 1;
        graphics_policy.sprite_offset_x = xml_parser_get_attribute_int("sprite_offset_x");
        graphics_policy.sprite_offset_y = xml_parser_get_attribute_int("sprite_offset_y");
    }
    if (xml_parser_has_attribute("max_image_offset")) {
        graphics_policy.max_image_offset = xml_parser_get_attribute_int("max_image_offset");
        if (graphics_policy.max_image_offset <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive max_image_offset", 0, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("base_image_offset")) {
        graphics_policy.image_group_offset = xml_parser_get_attribute_int("base_image_offset");
        if (graphics_policy.image_group_offset < 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a non-negative base_image_offset", 0, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("direction_stride")) {
        graphics_policy.direction_frame_stride = xml_parser_get_attribute_int("direction_stride");
        if (graphics_policy.direction_frame_stride <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive direction_stride", 0, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("static_frame_count")) {
        graphics_policy.static_frame_count = xml_parser_get_attribute_int("static_frame_count");
        if (graphics_policy.static_frame_count <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive static_frame_count", 0, 0);
            return 0;
        }
    }
    const bool has_action_state = xml_parser_has_attribute("action_state");
    const bool has_action_path_pattern = xml_parser_has_attribute("action_path_pattern");
    const bool has_action_image_pattern = xml_parser_has_attribute("action_image_pattern");
    if (has_action_state != has_action_path_pattern || has_action_state != has_action_image_pattern) {
        g_parse_state.error = true;
        log_error("FigureType action graphics requires action_state, action_path_pattern, and action_image_pattern", 0, 0);
        return 0;
    }
    if (has_action_state) {
        graphics_policy.action_state = parse_action_state_name(xml_parser_get_attribute_string("action_state"));
        graphics_policy.action_path_pattern =
            xml_value::trim_copy(xml_parser_get_attribute_string("action_path_pattern"));
        graphics_policy.action_image_pattern =
            xml_value::trim_copy(xml_parser_get_attribute_string("action_image_pattern"));
        if (!graphics_policy.action_state ||
            graphics_policy.action_path_pattern.empty() ||
            graphics_policy.action_image_pattern.empty()) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has invalid action graphics attributes",
                xml_parser_get_attribute_string("action_state"), 0);
            return 0;
        }
        if (xml_parser_has_attribute("action_min_wait_ticks")) {
            graphics_policy.action_min_wait_ticks = xml_parser_get_attribute_int("action_min_wait_ticks");
            if (graphics_policy.action_min_wait_ticks < 0) {
                g_parse_state.error = true;
                log_error("FigureType graphics node requires a non-negative action_min_wait_ticks", 0, 0);
                return 0;
            }
        }
    }
    const bool has_corpse_image_group = xml_parser_has_attribute("corpse_image_group");
    const bool has_corpse_image_asset = xml_parser_has_attribute("corpse_image_asset");
    const bool has_corpse_path_pattern = xml_parser_has_attribute("corpse_path_pattern");
    const bool has_corpse_image_pattern = xml_parser_has_attribute("corpse_image_pattern");
    const int corpse_source_count =
        (has_corpse_image_group ? 1 : 0) +
        (has_corpse_image_asset ? 1 : 0) +
        (has_corpse_path_pattern || has_corpse_image_pattern ? 1 : 0);
    if (corpse_source_count > 1) {
        g_parse_state.error = true;
        log_error("FigureType graphics node must choose only one corpse image source", 0, 0);
        return 0;
    }
    if (has_corpse_path_pattern != has_corpse_image_pattern) {
        g_parse_state.error = true;
        log_error("FigureType corpse graphics path pattern requires both corpse_path_pattern and corpse_image_pattern", 0, 0);
        return 0;
    }
    if (has_corpse_image_group) {
        graphics_policy.corpse_image_group =
            parse_image_group_name(xml_parser_get_attribute_string("corpse_image_group"));
        if (!graphics_policy.corpse_image_group) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has an unknown corpse_image_group",
                xml_parser_get_attribute_string("corpse_image_group"), 0);
            return 0;
        }
    }
    if (has_corpse_path_pattern) {
        graphics_policy.corpse_path_pattern = xml_value::trim_copy(xml_parser_get_attribute_string("corpse_path_pattern"));
        graphics_policy.corpse_image_pattern = xml_value::trim_copy(xml_parser_get_attribute_string("corpse_image_pattern"));
        if (graphics_policy.corpse_path_pattern.empty() || graphics_policy.corpse_image_pattern.empty()) {
            g_parse_state.error = true;
            log_error("FigureType corpse graphics path pattern requires non-empty corpse_path_pattern and corpse_image_pattern", 0, 0);
            return 0;
        }
    }
    if (has_corpse_image_asset) {
        graphics_policy.corpse_image_asset =
            parse_image_asset_name(xml_parser_get_attribute_string("corpse_image_asset"));
        if (graphics_policy.corpse_image_asset == ASSET_MAX_KEY) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has an unknown corpse_image_asset",
                xml_parser_get_attribute_string("corpse_image_asset"), 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("corpse_base_image_offset")) {
        graphics_policy.corpse_image_group_offset = xml_parser_get_attribute_int("corpse_base_image_offset");
        if (graphics_policy.corpse_image_group_offset < 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a non-negative corpse_base_image_offset", 0, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("corpse_frame_count")) {
        graphics_policy.corpse_frame_count = xml_parser_get_attribute_int("corpse_frame_count");
        if (graphics_policy.corpse_frame_count <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive corpse_frame_count", 0, 0);
            return 0;
        }
    }
    if (xml_parser_has_attribute("cart_mode")) {
        const char *cart_mode = xml_parser_get_attribute_string("cart_mode");
        if (!is_known_cart_graphics_mode_name(cart_mode)) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has an unknown cart_mode", cart_mode, 0);
            return 0;
        }
        graphics_policy.cart_mode = parse_cart_graphics_mode_name(cart_mode);
    }
    if (graphics_policy.cart_mode == CartGraphicsMode::ResourceLoad) {
        if (!xml_parser_has_attribute("cart_offsets_x") ||
            !xml_parser_has_attribute("cart_offsets_y") ||
            !xml_parser_has_attribute("cart_high_load_threshold") ||
            !xml_parser_has_attribute("cart_high_load_y_adjust") ||
            !xml_parser_has_attribute("cart_direction_3_y_adjust")) {
            g_parse_state.error = true;
            log_error("FigureType resource_load cart graphics is missing required cart attributes", 0, 0);
            return 0;
        }
        if (!parse_int_array_8(xml_parser_get_attribute_string("cart_offsets_x"), graphics_policy.cart_offsets_x)) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has invalid cart_offsets_x", 0, 0);
            return 0;
        }
        if (!parse_int_array_8(xml_parser_get_attribute_string("cart_offsets_y"), graphics_policy.cart_offsets_y)) {
            g_parse_state.error = true;
            log_error("FigureType graphics node has invalid cart_offsets_y", 0, 0);
            return 0;
        }
        graphics_policy.cart_high_load_threshold = xml_parser_get_attribute_int("cart_high_load_threshold");
        if (graphics_policy.cart_high_load_threshold < 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a non-negative cart_high_load_threshold", 0, 0);
            return 0;
        }
        graphics_policy.cart_high_load_y_adjust = xml_parser_get_attribute_int("cart_high_load_y_adjust");
        graphics_policy.cart_direction_3_y_adjust = xml_parser_get_attribute_int("cart_direction_3_y_adjust");
    }
    if (!validate_depot_cart_graphics(g_parse_state.definition.get(), &g_parse_state.error, graphics_policy)) {
        return 0;
    }

    g_parse_state.definition->set_graphics_policy(graphics_policy);
    g_parse_state.saw_graphics = true;
    return 1;
}

static int parse_pathing_node()
{
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

static const std::array<xml_parser_element, 10> XML_ELEMENTS = { {
    { "figure", parse_definition_root, nullptr, nullptr, nullptr },
    { "profiles", parse_profiles_node, nullptr, "figure", nullptr },
    { "profile", parse_profile_node, finish_profile_node, "profiles", nullptr },
    { "native", parse_native_node, nullptr, "profile", nullptr },
    { "owner", parse_owner_node, nullptr, "profile", nullptr },
    { "movement", parse_movement_node, nullptr, "profile", nullptr },
    { "graphics", parse_graphics_node, nullptr, "figure", nullptr },
    { "pathing", parse_pathing_node, nullptr, "profile", nullptr },
    { "venue_targets", parse_venue_targets_node, nullptr, "profile", nullptr },
    { "venue", parse_venue_node, nullptr, "venue_targets", nullptr }
} };

static int parse_definition_file(const char *filename)
{
    g_parse_state = {};
    const ErrorContextScope scope("FigureType XML", filename);
    const int parsed = xml_definition::parse_file(
        filename,
        "FigureType",
        XML_ELEMENTS.data(),
        static_cast<int>(XML_ELEMENTS.size()));
    if (!parsed) {
        set_failure_reason("Failed to parse FigureType definition.", filename);
        return 0;
    }

    if (g_parse_state.error ||
        !g_parse_state.saw_root ||
        !g_parse_state.saw_profiles ||
        !g_parse_state.saw_graphics ||
        !g_parse_state.definition ||
        g_parse_state.definition->profiles().empty() ||
        !g_parse_state.definition->default_profile()) {
        error_context_report_error("Invalid FigureType XML", filename);
        set_failure_reason("Failed to parse FigureType definition.", filename);
        return 0;
    }

    figure_type type = g_parse_state.definition->type();
    if (type <= FIGURE_NONE || type >= FIGURE_TYPE_MAX) {
        set_failure_reason("FigureType definition has an invalid figure id.", filename);
        return 0;
    }

    if (!g_figure_types[type]) {
        g_figure_types[type] = std::move(g_parse_state.definition);
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
                if (figure_definition && figure_profile) {
                    continue;
                }

                std::string detail = "building=";
                detail += building_definition->attr();
                detail += " figure=";
                detail += figure_definition ? figure_definition->attr() : std::to_string(static_cast<int>(policy.spawn_figure));
                detail += " profile=";
                detail += policy.profile;
                set_failure_reason("BuildingType spawn FigureType profile is missing.", detail.c_str());
                log_error("BuildingType spawn FigureType profile is missing", detail.c_str(), 0);
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

void figure_type_registry_reset(void)
{
    figure_type_registry_impl::g_failure_reason.clear();
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_types) {
        definition.reset();
    }
}

int figure_type_registry_load(void)
{
    figure_type_registry_reset();

    // Definitions are resolved by precedence: selected mod first, then Augustus,
    // then Julius. The first loaded definition for a figure type wins.
    const std::vector<std::string> candidate_paths = figure_type_registry_impl::build_candidate_definition_paths();
    int found_any_definition_file = 0;

    for (const std::string &path : candidate_paths) {
        if (!xml_definition::directory_exists(path.c_str())) {
            continue;
        }

        bool found_in_path = false;
        if (!xml_definition::for_each_definition_file(
            path,
            "FigureType",
            false,
            [](const xml_definition::DefinitionFile &file, const std::string &) {
                if (figure_type_registry_impl::parse_definition_file(file.full_path.c_str())) {
                    return true;
                }
                log_error("Unable to parse FigureType xml", file.full_path.c_str(), 0);
                return false;
            },
            &found_in_path)) {
            return 0;
        }
        if (found_in_path) {
            found_any_definition_file = 1;
        }
    }

    if (!found_any_definition_file) {
        std::string detail;
        for (size_t i = 0; i < candidate_paths.size(); i++) {
            if (i) {
                detail += "\n";
            }
            detail += candidate_paths[i];
        }
        figure_type_registry_impl::set_failure_reason(
            "No FigureType xml files were found in the configured mod precedence.",
            detail.c_str());
        log_error("No FigureType xml files found in configured mod precedence", 0, 0);
        return 0;
    }

    return 1;
}

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
