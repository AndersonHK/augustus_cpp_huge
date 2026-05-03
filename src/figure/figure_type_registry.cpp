#include "figure/figure_type_registry_internal.h"

#include "core/crash_context.h"
#include "core/xml_value.h"

extern "C" {
#include "building/properties.h"
#include "core/dir.h"
#include "core/file.h"
#include "core/image.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "game/mod_manager.h"
#include "platform/file_manager.h"
}

#include <array>
#include <cstddef>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace figure_type_registry_impl {

std::array<std::unique_ptr<FigureTypeDefinition>, FIGURE_TYPE_MAX> g_figure_types;
std::string g_failure_reason;

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

static int stop_on_first_entry([[maybe_unused]] const char *name, [[maybe_unused]] long unused)
{
    return LIST_MATCH;
}


class ScopedFile {
public:
    explicit ScopedFile(FILE *file)
        : file_(file)
    {
    }

    ~ScopedFile()
    {
        if (file_) {
            file_close(file_);
        }
    }

    FILE *get() const
    {
        return file_;
    }

private:
    FILE *file_ = nullptr;
};

int directory_exists(const char *path)
{
    return platform_file_manager_list_directory_contents(path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_entry) != LIST_ERROR;
}

void set_failure_reason(const char *message, const char *detail)
{
    if (detail && *detail) {
        g_failure_reason = std::string(message ? message : "") + "\n\n" + detail;
    } else {
        g_failure_reason = message ? message : "";
    }
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
    append_unique_candidate_path(paths, mod_manager_get_mod_path());

    const int mod_count = mod_manager_get_mod_count();
    for (int i = 0; i < mod_count; i++) {
        const char *name = mod_manager_get_mod_name_at(i);
        if (!name) {
            continue;
        }
        if (xml_value::equals(name, "Augustus") || xml_value::equals(name, "Julius")) {
            append_unique_candidate_path(paths, mod_manager_get_mod_path_at(i));
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

static figure_type parse_figure_type_name(const char *name)
{
    struct NamedFigure {
        std::string_view name;
        figure_type type;
    };

    static constexpr std::array<NamedFigure, 13> kFigureNames = { {
        { "labor_seeker", FIGURE_LABOR_SEEKER },
        { "engineer", FIGURE_ENGINEER },
        { "prefect", FIGURE_PREFECT },
        { "priest", FIGURE_PRIEST },
        { "actor", FIGURE_ACTOR },
        { "gladiator", FIGURE_GLADIATOR },
        { "lion_tamer", FIGURE_LION_TAMER },
        { "charioteer", FIGURE_CHARIOTEER },
        { "teacher", FIGURE_TEACHER },
        { "librarian", FIGURE_LIBRARIAN },
        { "barber", FIGURE_BARBER },
        { "bathhouse_worker", FIGURE_BATHHOUSE_WORKER },
        { "school_child", FIGURE_SCHOOL_CHILD }
    } };

    for (const NamedFigure &entry : kFigureNames) {
        if (xml_value::equals(name, entry.name)) {
            return entry.type;
        }
    }
    return FIGURE_NONE;
}

static building_type parse_building_type_name(const char *attr)
{
    if (!attr || xml_value::equals(attr, "any")) {
        return BUILDING_ANY;
    }

    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX;
        type = static_cast<building_type>(static_cast<int>(type) + 1)) {
        const building_properties *properties = building_properties_for_type(type);
        if (!properties || !properties->event_data.attr) {
            continue;
        }
        if (xml_parser_compare_multiple(properties->event_data.attr, attr)) {
            return type;
        }
    }
    return BUILDING_NONE;
}

static NativeClassId parse_native_class_name(const char *name)
{
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

    static constexpr std::array<NamedGroup, 12> kImageGroups = { {
        { "labor_seeker", GROUP_FIGURE_LABOR_SEEKER },
        { "engineer", GROUP_FIGURE_ENGINEER },
        { "prefect", GROUP_FIGURE_PREFECT },
        { "priest", GROUP_FIGURE_PRIEST },
        { "actor", GROUP_FIGURE_ACTOR },
        { "gladiator", GROUP_FIGURE_GLADIATOR },
        { "lion_tamer", GROUP_FIGURE_LION_TAMER },
        { "charioteer", GROUP_FIGURE_CHARIOTEER },
        { "teacher_librarian", GROUP_FIGURE_TEACHER_LIBRARIAN },
        { "barber", GROUP_FIGURE_BARBER },
        { "bathhouse_worker", GROUP_FIGURE_BATHHOUSE_WORKER },
        { "school_child", GROUP_FIGURE_SCHOOL_CHILD }
    } };

    for (const NamedGroup &entry : kImageGroups) {
        if (xml_value::equals(name, entry.name)) {
            return entry.image_group_id;
        }
    }
    return 0;
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
        xml_value::equals(name, "entertainment_hippodrome");
}

static bool is_road_only_terrain_usage(int terrain_usage)
{
    return terrain_usage == TERRAIN_USAGE_ROADS ||
        terrain_usage == TERRAIN_USAGE_ROADS_HIGHWAY;
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
    figure_type type = parse_figure_type_name(type_attr);
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
        owner_binding.required_building_type = parse_building_type_name(building_attr);
        if (owner_binding.required_building_type == BUILDING_NONE &&
            (!building_attr || !xml_value::equals(building_attr, "any"))) {
            g_parse_state.error = true;
            log_error("FigureType owner node has an unknown building type", building_attr, 0);
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
    if (!xml_parser_has_attribute("terrain_usage") ||
        !xml_parser_has_attribute("roam_ticks") ||
        !xml_parser_has_attribute("max_roam_length")) {
        g_parse_state.error = true;
        log_error("FigureType movement node is missing required attributes", 0, 0);
        return 0;
    }

    MovementProfile movement_profile;
    movement_profile.terrain_usage = parse_terrain_usage_name(xml_parser_get_attribute_string("terrain_usage"));
    if (movement_profile.terrain_usage < 0) {
        g_parse_state.error = true;
        log_error("FigureType movement node has an invalid terrain_usage", xml_parser_get_attribute_string("terrain_usage"), 0);
        return 0;
    }
    if (movement_profile.terrain_usage > std::numeric_limits<unsigned char>::max()) {
        g_parse_state.error = true;
        log_error("FigureType movement node terrain_usage exceeds figure storage range", 0, 0);
        return 0;
    }
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
    if (!xml_parser_has_attribute("image_group")) {
        g_parse_state.error = true;
        log_error("FigureType graphics node is missing required attribute 'image_group'", 0, 0);
        return 0;
    }

    GraphicsPolicy graphics_policy;
    graphics_policy.image_group = parse_image_group_name(xml_parser_get_attribute_string("image_group"));
    if (!graphics_policy.image_group) {
        g_parse_state.error = true;
        log_error("FigureType graphics node has an unknown image_group", xml_parser_get_attribute_string("image_group"), 0);
        return 0;
    }
    if (xml_parser_has_attribute("max_image_offset")) {
        graphics_policy.max_image_offset = xml_parser_get_attribute_int("max_image_offset");
        if (graphics_policy.max_image_offset <= 0) {
            g_parse_state.error = true;
            log_error("FigureType graphics node requires a positive max_image_offset", 0, 0);
            return 0;
        }
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
    if (!xml_parser_has_attribute("mode")) {
        g_parse_state.error = true;
        log_error("FigureType pathing node is missing required attribute 'mode'", 0, 0);
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
    const char *effect_text = xml_parser_get_attribute_string("effect");
    pathing_policy.effect = parse_service_effect_name(effect_text);

    if (pathing_policy.mode->requires_service_effect) {
        // Smart service pathing compares road-tile history for one explicit service effect.
        if (pathing_policy.effect == ROAD_SERVICE_EFFECT_NONE) {
            g_parse_state.error = true;
            error_context_report_error("FigureType smart_service pathing requires a service effect",
                profile->id());
            return 0;
        }
    }
    if (pathing_policy.mode->requires_road &&
        (!g_parse_state.saw_profile_movement ||
            !is_road_only_terrain_usage(profile->movement_profile().terrain_usage))) {
        g_parse_state.error = true;
        error_context_report_error(
            "FigureType pathing requires terrain_usage roads or roads_highway",
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
    target.building = parse_building_type_name(xml_parser_get_attribute_string("building"));
    target.show_slot = parse_show_slot_name(xml_parser_get_attribute_string("show_slot"));
    if (target.building == BUILDING_NONE ||
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

static int load_file_to_buffer(const char *filename, std::vector<char> &buffer)
{
    ScopedFile fp(file_open(filename, "rb"));
    if (!fp.get()) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    if (fseek(fp.get(), 0, SEEK_END) != 0) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    long size = ftell(fp.get());
    if (size < 0) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    rewind(fp.get());
    buffer.resize(static_cast<size_t>(size));
    const size_t read = buffer.empty() ? 0 : fread(buffer.data(), 1, buffer.size(), fp.get());
    if (read != buffer.size()) {
        set_failure_reason("Failed to load FigureType definition.", filename);
        return 0;
    }

    return 1;
}

static int parse_definition_file(const char *filename)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer)) {
        return 0;
    }
    if (buffer.size() > std::numeric_limits<unsigned int>::max()) {
        set_failure_reason("FigureType definition is too large to parse.", filename);
        return 0;
    }

    g_parse_state = {};
    if (!xml_parser_init(XML_ELEMENTS.data(), static_cast<int>(XML_ELEMENTS.size()), 1)) {
        set_failure_reason("Failed to initialize FigureType XML parser.", filename);
        return 0;
    }

    const ErrorContextScope scope("FigureType XML", filename);
    const int parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1);
    xml_parser_free();

    if (!parsed ||
        g_parse_state.error ||
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

} // namespace figure_type_registry_impl

extern "C" const char *figure_type_registry_get_failure_reason(void)
{
    return figure_type_registry_impl::g_failure_reason.c_str();
}

extern "C" void figure_type_registry_reset(void)
{
    figure_type_registry_impl::g_failure_reason.clear();
    for (std::unique_ptr<figure_type_registry_impl::FigureTypeDefinition> &definition :
        figure_type_registry_impl::g_figure_types) {
        definition.reset();
    }
}

extern "C" int figure_type_registry_load(void)
{
    figure_type_registry_reset();

    // Definitions are resolved by precedence: selected mod first, then Augustus,
    // then Julius. The first loaded definition for a figure type wins.
    const std::vector<std::string> candidate_paths = figure_type_registry_impl::build_candidate_definition_paths();
    int found_any_definition_file = 0;

    for (const std::string &path : candidate_paths) {
        if (!figure_type_registry_impl::directory_exists(path.c_str())) {
            continue;
        }

        const dir_listing *files = dir_find_files_with_extension(path.c_str(), "xml");
        if (!files || files->num_files <= 0) {
            continue;
        }

        found_any_definition_file = 1;
        for (int i = 0; i < files->num_files; i++) {
            char full_path[FILE_NAME_MAX];
            snprintf(full_path, FILE_NAME_MAX, "%s%s", path.c_str(), files->files[i].name);
            if (!figure_type_registry_impl::parse_definition_file(full_path)) {
                log_error("Unable to parse FigureType xml", full_path, 0);
                return 0;
            }
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
