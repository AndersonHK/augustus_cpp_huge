#include "startup/startup_parser_abi.h"
#include "startup/startup_parser_graphics_test.h"

#include "assets/image_group_payload.h"
#include "building/BuildingGraphicsState.h"
#include "building/RubbleState.h"
#include "city/race_bet.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/type.h"
#include "graphics/GraphicsDefinition.h"
#include "game/defines.h"
#include "map/access_ramp_rules.h"
#include "map/road_aqueduct_rules.h"
#include "map/terrain.h"
#include "core/relationship.h"
#include "building_graphics_contract_test.h"
#include "building_type_registry_layering_test.h"
#include "composition_contracts.h"
#include "culture_module_layering_test.h"
#include "distribution_layering_test.h"
#include "FoundationDefTest.h"
#include "foundation_registry_layering_test.h"
#include "god_layering_test.h"
#include "housing_transition_planner_test.h"
#include "housing_profile_registry_layering_test.h"
#include "mod_definition_loader_test.h"
#include "mod_metadata_test.h"
#include "plague_runtime_contract_test.h"
#include "production_method_layering_test.h"
#include "resource_layering_test.h"
#include "religion_layering_test.h"
#include "storage_type_registry_layering_test.h"
#include "water_access_type_layering_test.h"
#include "unit_type_registry_layering_test.h"
#include "formation_type_registry_layering_test.h"
#include "font_vector_runtime_layering_test.h"
#include "graphics_extraction_boundary_test.h"
#include "vespasian_graphics_source_contract_test.h"
#include "figure_type_registry_layering_test.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <set>
#include <vector>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace {

struct Options {
    std::filesystem::path game_root;
    bool dump_building_graphics_metadata = false;
    int save_soak_count = 3;
    int save_soak_ticks = 3000;
};

struct StartupEnvironmentSnapshot {
    std::string game_root;
    std::vector<std::string> mod_stack;
    std::string mod_path;
};

void collect_mod_name(void *context, uint32_t index, const char *mod_name)
{
    auto *environment = static_cast<StartupEnvironmentSnapshot *>(context);
    if (!environment || index != environment->mod_stack.size()) {
        return;
    }
    environment->mod_stack.emplace_back(mod_name ? mod_name : "");
}

bool inspect_startup_environment(StartupEnvironmentSnapshot &environment)
{
    char game_root[4096] = {};
    char mod_path[4096] = {};
    startup_parser_environment_request_v1 request = {};
    request.struct_size = sizeof(request);
    request.abi_version = STARTUP_PARSER_ABI_VERSION;
    request.game_root = game_root;
    request.game_root_capacity = sizeof(game_root);
    request.mod_path = mod_path;
    request.mod_path_capacity = sizeof(mod_path);
    request.on_mod = collect_mod_name;
    request.callback_context = &environment;

    startup_parser_environment_result_v1 result = {};
    result.struct_size = sizeof(result);
    if (startup_parser_inspect_environment_v1(&request, &result) !=
            STARTUP_PARSER_STATUS_SUCCEEDED ||
        result.game_root_length >= sizeof(game_root) ||
        result.mod_path_length >= sizeof(mod_path) ||
        result.mod_count != environment.mod_stack.size()) {
        std::cerr << "Startup parser environment ABI returned incomplete or inconsistent data.\n";
        return false;
    }
    environment.game_root = game_root;
    environment.mod_path = mod_path;
    return true;
}

bool validate_building_graphics_generation_contract()
{
    BuildingGraphicsState state;
    const std::uint64_t initial = state.generation();
    if (initial == 0 || state.set_variant(0) || state.generation() != initial) {
        std::cerr << "Building graphics generation changed for a no-op variant assignment.\n";
        return false;
    }
    if (!state.set_variant(3) || state.variant() != 3 || state.generation() == initial) {
        std::cerr << "Building graphics generation did not advance with the variant.\n";
        return false;
    }
    const std::uint64_t variant_generation = state.generation();
    state.invalidate();
    if (state.generation() == variant_generation) {
        std::cerr << "Building graphics generation did not advance on explicit invalidation.\n";
        return false;
    }
    return true;
}

class RelationshipContractEndpoint : public RelationshipEndpoint {
public:
    void remove() { disconnect_relationships(RelationshipDisconnectReason::EndpointRemoved); }

    ObjectRelationship<RelationshipContractEndpoint, RelationshipContractEndpoint> *outbound = nullptr;
    RelationshipContractEndpoint *fallback = nullptr;
    int connected_events = 0;
    int disconnected_events = 0;
    RelationshipDisconnectReason last_disconnect_reason = RelationshipDisconnectReason::Explicit;

private:
    void on_relationship_event(const RelationshipEvent &event) override
    {
        if (event.type == RelationshipEventType::Connected) {
            connected_events++;
            return;
        }
        disconnected_events++;
        last_disconnect_reason = event.reason;
        if (event.reason == RelationshipDisconnectReason::EndpointRemoved && outbound && fallback) {
            outbound->retarget(*this, fallback);
        }
    }
};

bool validate_relationship_contract()
{
    RelationshipContractEndpoint source;
    RelationshipContractEndpoint first_target;
    RelationshipContractEndpoint fallback_target;
    ObjectRelationship<RelationshipContractEndpoint, RelationshipContractEndpoint> relationship("test.destination");
    source.outbound = &relationship;
    source.fallback = &fallback_target;
    relationship.retarget(source, &first_target);
    if (relationship.get_ptr() != &first_target || source.relationships().size() != 1 || first_target.relationships().size() != 1) {
        std::cerr << "Relationship endpoints did not register the initial connection.\n";
        return false;
    }

    first_target.remove();
    if (relationship.get_ptr() != &fallback_target || !first_target.relationships().empty() || source.relationships().size() != 1 || fallback_target.relationships().size() != 1 || source.last_disconnect_reason != RelationshipDisconnectReason::EndpointRemoved) {
        std::cerr << "Relationship removal notification did not preserve reentrant retargeting invariants.\n";
        return false;
    }

    source.fallback = nullptr;
    source.remove();
    if (relationship.get_ptr() || !source.relationships().empty() || !fallback_target.relationships().empty() || fallback_target.last_disconnect_reason != RelationshipDisconnectReason::EndpointRemoved) {
        std::cerr << "Relationship source removal did not clear the reciprocal endpoint.\n";
        return false;
    }
    return true;
}

void print_usage()
{
    std::cout
        << "StartupParserTest [--game-root <path>] [--save-soak-count <count>] [--save-soak-ticks <count>] [--dump-building-graphics-metadata]\n\n"
        << "Runs the headless startup XML/registry parse sequence and representative save soaks from the installed game folder.\n";
}

bool parse_positive_count(const char *text, int &value)
{
    char *end = nullptr;
    const long parsed = text ? std::strtol(text, &end, 10) : 0;
    if (!text || !*text || !end || *end || parsed <= 0 || parsed > 1000000) return false;
    value = static_cast<int>(parsed);
    return true;
}

int parse_options(int argc, char **argv, Options &options)
{
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i] ? argv[i] : "";
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--game-root") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --game-root.\n";
                return -1;
            }
            options.game_root = argv[++i];
            continue;
        }
        if (arg == "--dump-building-graphics-metadata") {
            options.dump_building_graphics_metadata = true;
            continue;
        }
        if (arg == "--save-soak-count" || arg == "--save-soak-ticks" || arg == "--save-soak-frames") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for " << arg << ".\n";
                return -1;
            }
            int &target = arg == "--save-soak-count" ? options.save_soak_count : options.save_soak_ticks;
            if (!parse_positive_count(argv[++i], target)) {
                std::cerr << "Invalid positive count for " << arg << ".\n";
                return -1;
            }
            continue;
        }
        std::cerr << "Unsupported argument: " << arg << "\n";
        print_usage();
        return -1;
    }
    return 1;
}

void print_step(void *, const startup_parser_step_v1 *step)
{
    if (!step) {
        return;
    }
    const char *label = step->label ? step->label : "";
    const char *detail = step->detail ? step->detail : "";
    std::cout << "Loading " << label << "... ";
    if (step->succeeded) {
        std::cout << "ok";
        if (*detail) {
            std::cout << " (" << detail << ")";
        }
        std::cout << "\n" << std::flush;
        return;
    }
    std::cout << "failed\n" << std::flush;
    if (*detail) {
        std::cerr << detail << "\n";
    }
}

bool run_startup_parse()
{
    startup_parser_request_v1 request = {};
    request.struct_size = sizeof(request);
    request.abi_version = STARTUP_PARSER_ABI_VERSION;
    request.flags = STARTUP_PARSER_LOAD_CONFIG |
        STARTUP_PARSER_LOAD_LOCALIZATION |
        STARTUP_PARSER_VALIDATE_MOD_LAYOUT |
        STARTUP_PARSER_PREPARE_GRAPHICS_VALIDATION;
    request.on_step = print_step;

    startup_parser_result_v1 result = {};
    result.struct_size = sizeof(result);
    const startup_parser_status_v1 status = startup_parser_run_v1(&request, &result);
    if (status != STARTUP_PARSER_STATUS_SUCCEEDED && result.failure_message[0]) {
        std::cerr << result.failure_message << "\n";
    }
    return status == STARTUP_PARSER_STATUS_SUCCEEDED && result.succeeded;
}

bool validate_startup_parser_abi_contract()
{
    startup_parser_request_v1 request = {};
    request.struct_size = sizeof(request);
    request.abi_version = STARTUP_PARSER_ABI_VERSION + 1;
    startup_parser_result_v1 result = {};
    result.struct_size = sizeof(result);
    if (startup_parser_run_v1(&request, &result) != STARTUP_PARSER_STATUS_INVALID_ABI ||
        result.abi_version != STARTUP_PARSER_ABI_VERSION ||
        !result.failure_step[0] || !result.failure_message[0] ||
        result.failure_step_length != std::strlen(result.failure_step) ||
        result.failure_message_length != std::strlen(result.failure_message)) {
        std::cerr << "Startup parser ABI did not reject an unsupported caller contract deterministically.\n";
        return false;
    }

    request.abi_version = STARTUP_PARSER_ABI_VERSION;
    request.flags = 1u << 31;
    result = {};
    result.struct_size = sizeof(result);
    if (startup_parser_run_v1(&request, &result) != STARTUP_PARSER_STATUS_INVALID_ABI) {
        std::cerr << "Startup parser ABI accepted unknown request flags.\n";
        return false;
    }

    startup_parser_result_v1 short_result = {};
    short_result.struct_size = sizeof(short_result) - 1;
    if (startup_parser_run_v1(&request, &short_result) != STARTUP_PARSER_STATUS_INVALID_ABI) {
        std::cerr << "Startup parser ABI accepted a truncated result structure.\n";
        return false;
    }

    startup_parser_environment_request_v1 environment_request = {};
    environment_request.struct_size = sizeof(environment_request);
    environment_request.abi_version = STARTUP_PARSER_ABI_VERSION + 1;
    startup_parser_environment_result_v1 environment_result = {};
    environment_result.struct_size = sizeof(environment_result);
    if (startup_parser_inspect_environment_v1(&environment_request, &environment_result) !=
        STARTUP_PARSER_STATUS_INVALID_ABI) {
        std::cerr << "Startup parser environment ABI accepted an unsupported caller contract.\n";
        return false;
    }
    return true;
}

bool validate_hippodrome_race_contract()
{
    RaceSession race;
    race.start(41, 4);
    if (race.participant_count() != 0 || race.register_participant(-1) != 0 ||
        race.register_participant(4) != 0 || race.register_participant(3) != 1 ||
        race.register_participant(0) != 2 || race.register_participant(2) != 3 ||
        race.register_participant(1) != 4 || race.register_participant(1) != 4 ||
        race.participant_count() != 4) {
        std::cerr << "Race session did not retain exactly one participant per configured team.\n";
        return false;
    }
    if (race.winner() != -1 || race.register_finish(-1) != 0 || race.register_finish(4) != 0 ||
        race.register_finish(3) != 1 || race.register_finish(0) != 2 || race.register_finish(3) != 1 ||
        race.register_finish(2) != 3 || race.register_finish(1) != 4 || race.winner() != 3 ||
        race.finish_position(3) != 1 || race.finish_position(0) != 2 ||
        race.finish_position(2) != 3 || race.finish_position(1) != 4) {
        std::cerr << "Race session did not preserve actual, unique finish order.\n";
        return false;
    }
    race.start(42, 7);
    if (race.register_finish(6) != 0 || race.register_participant(6) != 1 || race.register_finish(6) != 1 ||
        race.building_id() != 42 || race.team_capacity() != 7 || race.expected_participants() != 7 ||
        race.winner() != 6 || race.finish_position(3) != 0) {
        std::cerr << "Race session did not resize and reset cleanly for a new venue race.\n";
        return false;
    }
    race.start(43, 4, 2);
    if (race.team_capacity() != 4 || race.expected_participants() != 2 || race.is_full_configured_field() ||
        race.register_participant(2) != 1 || race.register_participant(3) != 2 || !race.has_complete_field() ||
        race.register_finish(2) != 1 || race.winner() != 2) {
        std::cerr << "Race session did not preserve configured team identity for a restored partial field.\n";
        return false;
    }
    return true;
}

bool validate_parsed_race_visual_contract()
{
    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::definition_for_type;
    using building_type_registry_impl::type_from_attr;

    const BuildingType *hippodrome = definition_for_type(type_from_attr("hippodrome"));
    if (!hippodrome || !hippodrome->has_race()) {
        std::cerr << "Parsed race visual contract is missing the hippodrome race module.\n";
        return false;
    }
    const building_type_registry_impl::RaceDefinition &race = hippodrome->race();
    if (race.track_margin <= 0 || race.lane_spacing <= 0) {
        std::cerr << "Parsed race visual contract permits racer centers on the outside track boundary.\n";
        return false;
    }
    std::set<int> distances;
    for (const building_type_registry_impl::RaceTeamDefinition &team : race.teams) {
        const int distance = race.lane_render_distance(team.lane);
        if (distance < race.track_margin || distance > 64 || !distances.insert(distance).second) {
            std::cerr << "Parsed race visual contract does not keep distinct lanes inside the supported track width.\n";
            return false;
        }
    }
    return true;
}

bool validate_building_identity_contract()
{
    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::definition_for_type;
    using building_type_registry_impl::type_from_attr;

    const BuildingType *vacant_lot = definition_for_type(type_from_attr("vacant_lot"));
    const BuildingType *goddess_statue = definition_for_type(type_from_attr("goddess_statue"));
    const BuildingType *senator_statue = definition_for_type(type_from_attr("senator_statue"));
    const BuildingType *barber = definition_for_type(type_from_attr("barber"));
    if (!vacant_lot || !vacant_lot->matches_identity("vacant_lot") ||
        !vacant_lot->matches_identity("house_vacant_lot") ||
        vacant_lot->identity().aliases().size() != 1 ||
        !goddess_statue || !goddess_statue->matches_identity("small_statue_alt") ||
        !senator_statue || !senator_statue->matches_identity("small_statue_alt_b") ||
        !barber || !barber->identity().aliases().empty() || barber->matches_identity("doctor")) {
        std::cerr << "Building identity contract failed: canonical ids and compatibility aliases overlap or are incomplete.\n";
        return false;
    }

    std::cout << "Validated canonical BuildingType identity and legacy scenario aliases without event_data duplication.\n";
    return true;
}

bool validate_rubble_repair_contract()
{
    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::definition_for_type;
    using building_type_registry_impl::type_from_attr;

    const BuildingType *farm = definition_for_type(type_from_attr("fruit_farm"));
    const BuildingType *hippodrome = definition_for_type(type_from_attr("hippodrome"));
    const BuildingType *aqueduct = definition_for_type(type_from_attr("aqueduct"));
    const BuildingType *rubble = definition_for_type(type_from_attr("rubble"));
    const BuildingType *burning_ruin = definition_for_type(type_from_attr("burning_ruin"));
    if (!aqueduct || !aqueduct->tool().is_aqueduct() || aqueduct->has_rubble() ||
        aqueduct->placement_width(0) != 1 || aqueduct->placement_height(0) != 1 ||
        !rubble || !rubble->has_rubble() || !rubble->rubble().is_rubble() || rubble->tool().is_aqueduct() ||
        rubble->placement_width(0) != 1 || rubble->placement_height(0) != 1 ||
        !burning_ruin || !burning_ruin->has_rubble() || !burning_ruin->rubble().is_burning() ||
        burning_ruin->placement_width(0) != 1 || burning_ruin->placement_height(0) != 1 ||
        burning_ruin->rubble().decay_type != rubble ||
        aqueduct->type() == rubble->type() || aqueduct->type() == burning_ruin->type()) {
        std::cerr << "Rubble repair contract failed: aqueduct/rubble BuildingType XML identities overlap or are incomplete.\n";
        return false;
    }
    const building_type_registry_impl::CompositionLayoutResult farm_layout = farm && farm->has_composition()
        ? building_type_registry_impl::build_composition_layout(farm, farm->composition(), 0, 0, 0)
        : building_type_registry_impl::CompositionLayoutResult{};
    if (!farm || !farm_layout.valid() ||
        farm_layout.bounds.width() != 3 ||
        farm_layout.bounds.height() != 3 ||
        farm->composition().children().size() != 5) {
        std::cerr << "Rubble repair contract failed: fruit farm composition is not the expected 3x3/5-part shape.\n";
        return false;
    }
    const building_type_registry_impl::CompositionLayoutResult hippodrome_layout =
        hippodrome && hippodrome->has_composition()
        ? building_type_registry_impl::build_composition_layout(
            hippodrome, hippodrome->composition(), 0, 0, 0)
        : building_type_registry_impl::CompositionLayoutResult{};
    if (!hippodrome || !hippodrome_layout.valid() ||
        hippodrome_layout.bounds.width() != 15 ||
        hippodrome_layout.bounds.height() != 5 ||
        hippodrome->composition().children().size() != 2) {
        std::cerr << "Rubble repair contract failed: hippodrome composition is not the expected 15x5/2-part shape.\n";
        return false;
    }

    RubbleState first;
    first.original_grid_offset = 1234;
    first.original_orientation = 3;
    first.original_type = hippodrome;
    RubbleState sibling = first;
    if (!first.original_type || !first.same_origin(sibling)) {
        std::cerr << "Rubble repair contract failed: identical rubble origins do not match.\n";
        return false;
    }
    sibling.original_orientation++;
    if (first.same_origin(sibling)) {
        std::cerr << "Rubble repair contract failed: distinct rubble origins match.\n";
        return false;
    }
    if (rubble_record_disposition(BUILDING_STATE_RUBBLE, true, true, false) !=
            RubbleRecordDisposition::Discard ||
        rubble_record_disposition(BUILDING_STATE_RUBBLE, true, true, true) !=
            RubbleRecordDisposition::NormalizeToRubble ||
        rubble_record_disposition(BUILDING_STATE_RUBBLE, true, false, true) !=
            RubbleRecordDisposition::Keep ||
        rubble_record_disposition(BUILDING_STATE_IN_USE, true, true, true) !=
            RubbleRecordDisposition::Keep) {
        std::cerr << "Rubble repair contract failed: rubble record lifecycle policy is inconsistent.\n";
        return false;
    }
    if (farm->placement_width(0) != 3 || farm->placement_height(0) != 3 ||
        hippodrome->placement_width(0) != 15 || hippodrome->placement_height(0) != 5 ||
        hippodrome->placement_width(1) != 5 || hippodrome->placement_height(1) != 15) {
        std::cerr << "Rubble repair contract failed: BuildingType placement dimensions are incorrect.\n";
        return false;
    }

    std::cout << "Validated rubble repair contracts (distinct aqueduct/rubble XML types, type-derived dimensions, and pointer-backed origin identity).\n";
    return true;
}

struct AnimatedIsometricContract {
    const char *group;
    const char *image;
    int top_height;
    int footprint_y;
    int top_y;
    int animated_y;
};

bool validate_animated_isometric_contracts()
{
    const AnimatedIsometricContract contracts[] = {
        { "Industry\\Iron_Mine", "Image_0000", 30, -15, -15, -3 },
        { "Industry\\Marble_Quarry", "Image_0000", 30, -15, -15, -29 },
        { "Industry\\Warehouse", "Image_0000", 15, 0, 0, -24 },
        { "Terrain_Maps\\Rubble_General", "Image_0000", 15, 0, 0, -5 },
        { "Terrain_Maps\\Rubble_General", "Image_0027", 15, 0, 0, -1 },
    };

    for (const AnimatedIsometricContract &contract : contracts) {
        if (!image_group_payload_load(contract.group)) {
            std::cerr << "Animated isometric contract failed: could not load " << contract.group << ".\n";
            return false;
        }

        const ImageGroupPayload *payload = image_group_payload_get(contract.group);
        const ImageGroupEntry *entry = payload ? payload->entry_for(contract.image) : nullptr;
        const RuntimeDrawSlice *footprint = entry ? entry->footprint() : nullptr;
        const RuntimeDrawSlice *top = entry ? entry->top() : nullptr;
        if (!entry || !footprint || !top || !entry->has_animation()) {
            std::cerr << "Animated isometric contract failed: " << contract.group << "\\"
                << contract.image << " did not materialize a footprint, top, and animation.\n";
            return false;
        }

        const RuntimeDrawSlice frame = entry->animation().frame_slice_at_offset(1);
        const int animated_y = frame.draw_offset_y + top->draw_offset_y;
        if (entry->top_height() != contract.top_height ||
            footprint->draw_offset_y != contract.footprint_y ||
            top->draw_offset_y != contract.top_y ||
            animated_y != contract.animated_y) {
            std::cerr << "Animated isometric contract failed: " << contract.group << "\\"
                << contract.image << " materialized as top_height=" << entry->top_height()
                << ", footprint_y=" << footprint->draw_offset_y
                << ", top_y=" << top->draw_offset_y
                << ", animated_y=" << animated_y << ".\n";
            return false;
        }
    }

    std::cout << "Validated explicit isometric top layers and animation offsets for mines, warehouses, and rubble.\n";
    return true;
}

bool validate_road_aqueduct_crossing_rules()
{
    using enum road_aqueduct_axis;

    if (road_aqueduct_axis_from_opposite_neighbors(2, 0) != x ||
        road_aqueduct_axis_from_opposite_neighbors(0, 2) != y ||
        road_aqueduct_axis_from_opposite_neighbors(2, 1) != none ||
        road_aqueduct_axis_from_opposite_neighbors(1, 1) != none ||
        road_aqueduct_axis_from_connectable_option(2, 0) != y ||
        road_aqueduct_axis_from_connectable_option(3, 0) != x ||
        road_aqueduct_axis_from_connectable_option(2, 1) != x ||
        road_aqueduct_axis_from_connectable_option(3, 1) != y ||
        road_aqueduct_axis_from_connectable_option(4, 0) != none ||
        !road_aqueduct_tile_allows_crossing(x, 2) ||
        road_aqueduct_tile_allows_crossing(x, 3) ||
        road_aqueduct_tile_allows_crossing(none, 2) ||
        !road_aqueduct_axes_allow_crossing(x, y, 2) ||
        road_aqueduct_axes_allow_crossing(x, x, 2) ||
        road_aqueduct_axes_allow_crossing(x, y, 3) ||
        road_aqueduct_crossing_option(0, 1) != 0 ||
        road_aqueduct_crossing_option(1, 1) != 1 ||
        road_aqueduct_crossing_option(0, 0) != 2 ||
        road_aqueduct_crossing_option(1, 0) != 3) {
        std::cerr << "Road/aqueduct crossing contract failed: straight-axis rules are inconsistent.\n";
        return false;
    }

    const building_type_registry_impl::BuildingType *aqueduct =
        building_type_registry_impl::definition_for_type(
            building_type_registry_impl::type_from_attr("aqueduct"));
    int road_variants = 0;
    if (aqueduct) {
        for (const building_type_registry_impl::GraphicsVariant &variant : aqueduct->graphics().variants()) {
            bool requires_road = false;
            for (const building_type_registry_impl::GraphicsCondition &condition : variant.conditions) {
                requires_road = requires_road ||
                    (condition.type == building_type_registry_impl::GraphicsConditionType::Terrain &&
                        condition.terrain_mask == TERRAIN_ROAD);
            }
            if (!requires_road) {
                continue;
            }
            road_variants++;
            if (variant.target.option_selection() !=
                    building_type_registry_impl::GraphicsOptionSelection::RoadCrossing ||
                variant.target.option_count() != 4) {
                std::cerr << "Road/aqueduct crossing contract failed: road variant is not a four-option native crossing target.\n";
                return false;
            }
        }
    }
    if (road_variants != 6) {
        std::cerr << "Road/aqueduct crossing contract failed: expected six climate/water road variants, found "
            << road_variants << ".\n";
        return false;
    }

    std::cout << "Validated road/aqueduct topology and XML-owned native crossing graphics.\n";
    return true;
}

bool validate_access_ramp_road_connection_rules()
{
    for (int view = 0; view < 8; view += 2) {
        for (int image_orientation = 0; image_orientation < 4; ++image_orientation) {
            const int world_orientation = (image_orientation + view / 2) % 4;
            const int along_axis = (world_orientation & 1) ? 2 : 0;
            const int across_axis = (world_orientation & 1) ? 0 : 2;
            if (!access_ramp_connects_to_road_direction(image_orientation, view, along_axis) ||
                !access_ramp_connects_to_road_direction(image_orientation, view, along_axis + 4) ||
                access_ramp_connects_to_road_direction(image_orientation, view, across_axis) ||
                access_ramp_connects_to_road_direction(image_orientation, view, across_axis + 4) ||
                !access_ramp_edge_allows_road_direction(image_orientation, -1, view, along_axis) ||
                access_ramp_edge_allows_road_direction(image_orientation, -1, view, across_axis) ||
                access_ramp_edge_allows_road_direction(-1, image_orientation, view, across_axis) ||
                access_ramp_edge_allows_road_direction(
                    image_orientation, image_orientation, view, across_axis)) {
                std::cerr << "Access-ramp road connection contract failed: a stair connected across its rails.\n";
                return false;
            }
        }
    }
    std::cout << "Validated access-ramp graphics and citizen movement edges across every map rotation.\n";
    return true;
}

bool validate_gate_terrain_foundation_contract()
{
    using namespace building_type_registry_impl;
    const BuildingType *gatehouse = definition_for_type(type_from_attr("gatehouse"));
    if (!gatehouse || !gatehouse->graphics().default_target().has_layer_role("gatehouse_overlay")) {
        std::cerr << "Gate graphics contract failed: gatehouse overlay is not declared by layer role.\n";
        return false;
    }

    static const char *const gate_types[] = {
        "garden_wall_gate",
        "hedge_gate_dark",
        "hedge_gate_light",
        "looped_garden_gate",
        "palisade_gate",
        "panelled_garden_gate",
    };

    for (const char *type_name : gate_types) {
        const BuildingType *gate = definition_for_type(type_from_attr(type_name));
        if (!gate) {
            std::cerr << "Gate terrain-foundation contract failed: missing " << type_name << ".\n";
            return false;
        }
        const GraphicsTarget &target = gate->graphics().default_target();
        if (!target.uses_terrain_foundation() || target.option_count() != 2 ||
            !target.resolved_option(0).uses_terrain_foundation() ||
            !target.resolved_option(1).uses_terrain_foundation() ||
            target.has_layer_role("gatehouse_overlay")) {
            std::cerr << "Gate terrain-foundation contract failed: " << type_name
                << " does not preserve its road underlay or incorrectly owns the gatehouse cap.\n";
            return false;
        }
    }

    std::cout << "Validated XML-owned road underlays for all decorative and palisade gates.\n";
    return true;
}

bool validate_dock_native_orientation_contract()
{
    using namespace building_type_registry_impl;
    const BuildingType *dock = definition_for_type(type_from_attr("dock"));
    if (!dock) {
        std::cerr << "Dock graphics contract failed: missing dock BuildingType.\n";
        return false;
    }
    const GraphicsTarget &target = dock->graphics().default_target();
    if (target.option_selection() != GraphicsOptionSelection::Orientation || target.option_count() != 4) {
        std::cerr << "Dock graphics contract failed: expected four native orientation options.\n";
        return false;
    }
    for (int orientation = 0; orientation < 4; ++orientation) {
        const GraphicsTarget resolved = target.resolved_option(static_cast<unsigned char>(orientation));
        if (!resolved.has_path() || !resolved.has_image()) {
            std::cerr << "Dock graphics contract failed: orientation " << orientation
                << " does not resolve to a complete native image target.\n";
            return false;
        }
    }
    std::cout << "Validated XML-owned dock orientation graphics.\n";
    return true;
}

bool validate_simple_native_building_graphics_contract()
{
    using namespace building_type_registry_impl;
    struct ExpectedTarget {
        const char *type;
        const char *path;
    };
    static const ExpectedTarget expected_targets[] = {
        { "mission_post", "Admin_Logistics\\Mission_Post" },
        { "military_academy", "Military\\Military_Academy" },
        { "barracks", "Military\\Barracks" },
    };

    for (const ExpectedTarget &expected : expected_targets) {
        const BuildingType *definition = definition_for_type(type_from_attr(expected.type));
        if (!definition || !definition->has_graphic()) {
            std::cerr << "Simple native graphics contract failed: missing graphics for "
                << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget &target = definition->graphics().default_target();
        if (!target.has_path() || std::strcmp(target.path(), expected.path) != 0 ||
            target.has_image() || target.has_options() || target.is_resource_storage()) {
            std::cerr << "Simple native graphics contract failed: " << expected.type
                << " is not an unconditional direct default target.\n";
            return false;
        }
        if (!image_group_payload_load(expected.path)) {
            std::cerr << "Simple native graphics contract failed: unable to load "
                << expected.path << ".\n";
            return false;
        }
        const ImageGroupPayload *payload = image_group_payload_get(expected.path);
        const ImageGroupEntry *entry = payload ? payload->default_entry() : nullptr;
        if (!entry || !entry->footprint()) {
            std::cerr << "Simple native graphics contract failed: " << expected.type
                << " has no default footprint entry.\n";
            return false;
        }
    }

    std::cout << "Validated direct native graphics for mission post, military academy, and barracks.\n";
    return true;
}

bool validate_native_storage_and_fort_base_graphics_contract()
{
    using namespace building_type_registry_impl;
    struct ExpectedTarget {
        const char *type;
        const char *path;
        const char *image;
        bool resource_storage;
    };
    static const ExpectedTarget expected_targets[] = {
        { "warehouse", "Industry\\Warehouse", "Image_0000", false },
        { "warehouse_space", nullptr, nullptr, true },
        { "fort_ground", "Military\\Fort", "Image_0001", false },
    };

    for (const ExpectedTarget &expected : expected_targets) {
        const BuildingType *definition = definition_for_type(type_from_attr(expected.type));
        if (!definition || !definition->has_graphic()) {
            std::cerr << "Native base graphics contract failed: missing graphics for "
                << expected.type << ".\n";
            return false;
        }
        const GraphicsTarget &target = definition->graphics().default_target();
        if (target.has_options() || static_cast<bool>(target.is_resource_storage()) != expected.resource_storage) {
            std::cerr << "Native base graphics contract failed: unexpected strategy for "
                << expected.type << ".\n";
            return false;
        }
        if (expected.resource_storage) {
            if (target.has_path() || target.has_image()) {
                std::cerr << "Native base graphics contract failed: warehouse space mixes resource storage with an image.\n";
                return false;
            }
            continue;
        }
        if (!target.has_path() || !target.has_image() ||
            std::strcmp(target.path(), expected.path) != 0 ||
            std::strcmp(target.image(), expected.image) != 0 ||
            !image_group_payload_load(expected.path)) {
            std::cerr << "Native base graphics contract failed: incomplete target for "
                << expected.type << ".\n";
            return false;
        }
        const ImageGroupPayload *payload = image_group_payload_get(expected.path);
        const ImageGroupEntry *entry = payload ? payload->entry_for(expected.image) : nullptr;
        if (!entry || !entry->footprint()) {
            std::cerr << "Native base graphics contract failed: " << expected.type
                << " has no referenced footprint entry.\n";
            return false;
        }
    }

    std::cout << "Validated native warehouse, warehouse-space, and fort-ground base graphics.\n";
    return true;
}

bool validate_colosseum_graphics_contract()
{
    using namespace building_type_registry_impl;
    const BuildingType *colosseum = definition_for_type(type_from_attr("colosseum"));
    if (!colosseum || colosseum->graphics().default_target().animation_enabled()) {
        std::cerr << "Colosseum graphics contract failed: idle graphics must not animate.\n";
        return false;
    }

    bool active_show = false;
    int festival_variants = 0;
    for (const GraphicsVariant &variant : colosseum->graphics().variants()) {
        bool has_workers = false;
        bool has_show_days = false;
        bool festival = false;
        for (const GraphicsCondition &condition : variant.conditions) {
            has_workers = has_workers || condition.type == GraphicsConditionType::HasWorkers;
            has_show_days = has_show_days || condition.type == GraphicsConditionType::Days1OrDays2Positive;
            festival = festival || condition.type == GraphicsConditionType::FestivalGames;
        }
        const char *image = variant.target.image();
        active_show = active_show ||
            (has_workers && has_show_days && image && std::strcmp(image, "Col Glad Fight") == 0);
        festival_variants += festival && has_workers;
    }
    if (!active_show || festival_variants != 3 ||
        !image_group_payload_load("Health_Culture\\Colosseum")) {
        std::cerr << "Colosseum graphics contract failed: active/festival variants are incomplete.\n";
        return false;
    }
    const ImageGroupPayload *payload = image_group_payload_get("Health_Culture\\Colosseum");
    const ImageGroupEntry *fight = payload ? payload->entry_for("Col Glad Fight") : nullptr;
    if (!fight || !fight->has_animation()) {
        std::cerr << "Colosseum graphics contract failed: active show target has no animation.\n";
        return false;
    }
    std::cout << "Validated idle, active-show, and festival Colosseum graphics.\n";
    return true;
}

bool validate_figure_owner_contracts()
{
    const figure_type_registry_impl::FigureTypeProfile *beggar = figure_type_registry_impl::profile_for(FIGURE_BEGGAR, "unemployment_wanderer");
    const figure_type_registry_impl::FigureTypeProfile *caravanserai_supplier = figure_type_registry_impl::profile_for(FIGURE_CARAVANSERAI_SUPPLIER, "legacy");
    const figure_type_registry_impl::FigureTypeProfile *homeless = figure_type_registry_impl::profile_for(FIGURE_HOMELESS, "legacy");
    const figure_type_registry_impl::FigureTypeProfile *fishing_boat = figure_type_registry_impl::profile_for(FIGURE_FISHING_BOAT, "fish_fetch");
    const figure_type_registry_impl::FigureTypeProfile *prefect = figure_type_registry_impl::profile_for(FIGURE_PREFECT, "service");
    if (!beggar || !beggar->requires_owner() ||
        beggar->owner_binding().slot != figure_type_registry_impl::FigureSlot::Quaternary ||
        beggar->owner_binding().required_owner_state != figure_type_registry_impl::OwnerStateRequirement::InUse) {
        std::cerr << "Figure owner contract failed: beggars must remain owned by their spawning house through its quaternary slot.\n";
        return false;
    }
    if (!caravanserai_supplier || !caravanserai_supplier->requires_owner() ||
        caravanserai_supplier->owner_binding().slot != figure_type_registry_impl::FigureSlot::Primary ||
        caravanserai_supplier->owner_binding().required_owner_state != figure_type_registry_impl::OwnerStateRequirement::InUse) {
        std::cerr << "Figure owner contract failed: caravanserai suppliers must remain owned through the caravanserai primary slot.\n";
        return false;
    }
    if (!fishing_boat || !fishing_boat->requires_owner() ||
        fishing_boat->owner_binding().slot != figure_type_registry_impl::FigureSlot::None ||
        fishing_boat->owner_binding().required_owner_state !=
            figure_type_registry_impl::OwnerStateRequirement::InUse) {
        std::cerr << "Figure owner contract failed: fishing boats require a live shipyard or wharf without using a generic figure slot.\n";
        return false;
    }
    if (!homeless || homeless->requires_owner() ||
        homeless->native_class() != figure_type_registry_impl::NativeClassId::LegacyAction ||
        homeless->pathing_policy().mode != &figure_type_registry_impl::VanillaRoaming) {
        std::cerr << "Figure owner contract failed: homeless figures must use ownerless legacy action with vanilla pathing.\n";
        return false;
    }
    if (!prefect || !prefect->requires_owner()) {
        std::cerr << "Figure owner contract failed: prefect service requires its owning building.\n";
        return false;
    }
    std::cout << "Validated ownerless and required-owner FigureType profile contracts.\n" << std::flush;
    return true;
}

bool validate_entertainer_graphics_contract()
{
    const figure_type entertainers[] = {
        FIGURE_ACTOR,
        FIGURE_GLADIATOR,
        FIGURE_TOURIST
    };
    for (figure_type type : entertainers) {
        const figure_type_registry_impl::FigureGraphics *graphics =
            figure_type_registry_impl::graphics_for(type);
        if (!graphics || !graphics->overlays().empty()) {
            std::cerr << "Entertainer graphics contract failed: figure "
                      << static_cast<int>(type) << " acquired "
                      << (graphics ? graphics->overlays().size() : 0)
                      << " overlay(s), or has no graphics definition.\n";
            return false;
        }
    }
    std::cout << "Validated entertainer graphics contain no resource-cart overlays.\n";
    return true;
}

bool validate_native_soldier_animation(
    const char *group,
    const std::string &entry_id,
    const std::string &source_prefix,
    const char *source_direction,
    int frame_count)
{
    if (!image_group_payload_load(group)) {
        std::cerr << "Native soldier graphics contract failed: could not load " << group << ".\n";
        return false;
    }
    const ImageGroupPayload *payload = image_group_payload_get(group);
    const ImageGroupEntry *entry = payload ? payload->entry_for(entry_id.c_str()) : nullptr;
    if (!entry || !entry->has_animation() || entry->animation().frame_count() != frame_count) {
        std::cerr << "Native soldier graphics contract failed: " << group << " entry " << entry_id
                  << " did not materialize the declared animation.\n";
        return false;
    }
    for (int frame = 1; frame <= frame_count; frame++) {
        char source_name[64];
        if (source_direction) {
            std::snprintf(source_name, sizeof(source_name), "%s_%s_%02d", source_prefix.c_str(), source_direction, frame);
        } else {
            std::snprintf(source_name, sizeof(source_name), "%s_%02d", source_prefix.c_str(), frame);
        }
        char source_group[96];
        std::snprintf(source_group, sizeof(source_group), "Warriors\\%s", source_name);
        if (!image_group_payload_load(source_group)) return false;
        const ImageGroupPayload *source_payload = image_group_payload_get(source_group);
        const ImageGroupEntry *source_entry = source_payload ? source_payload->entry_for(source_name) : nullptr;
        const RuntimeDrawSlice *source_slice = source_entry ? source_entry->footprint() : nullptr;
        const RuntimeDrawSlice scaled_slice = entry->animation().frame_slice_at_offset(frame, 0);
        if (!source_slice || !scaled_slice.is_valid() || scaled_slice.width != source_slice->width ||
            scaled_slice.height != source_slice->height ||
            startup_parser::image_resource_fingerprint(scaled_slice.handle) !=
                startup_parser::image_resource_fingerprint(source_slice->handle) ||
            scaled_slice.fixed_logical_size.width != scaled_slice.width * 96 ||
            scaled_slice.fixed_logical_size.height != scaled_slice.height * 96) {
            std::cerr << "Native soldier graphics contract failed: " << group << " entry " << entry_id
                      << " frame " << frame << " is not an 0.8-scale reference to " << source_group << ".\n";
            return false;
        }
    }
    if (entry->animation().sprite_offset_x() != 26 || entry->animation().sprite_offset_y() != 29) {
        std::cerr << "Native soldier graphics contract failed: " << group << " entry " << entry_id
                  << " does not preserve the 26,29 tile attachment anchor.\n";
        return false;
    }
    return true;
}

bool validate_native_auxiliary_soldier_graphics()
{
    if (!image_group_payload_load("Warriors\\Auxiliary_Infantry") ||
        !image_group_payload_load("Warriors\\Auxiliary_Archer")) {
        std::cerr << "Native soldier graphics contract failed: consolidated auxiliary groups did not load.\n";
        return false;
    }
    const ImageGroupPayload *infantry = image_group_payload_get("Warriors\\Auxiliary_Infantry");
    const ImageGroupPayload *archer = image_group_payload_get("Warriors\\Auxiliary_Archer");
    if (!infantry || infantry->entry_count() != 17 || !archer || archer->entry_count() != 25) {
        std::cerr << "Native soldier graphics contract failed: Auxiliary_Infantry must contain exactly 17 entries and Auxiliary_Archer exactly 25.\n";
        return false;
    }
    static const char *const lower_directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    static const char *const semantic_directions[] = { "NE", "E", "SE", "S", "SW", "W", "NW", "N" };
    for (int direction = 0; direction < 8; direction++) {
        if (!validate_native_soldier_animation("Warriors\\Auxiliary_Infantry", "Move_" + std::string(semantic_directions[direction]), "auxinf", lower_directions[direction], 12) ||
            !validate_native_soldier_animation("Warriors\\Auxiliary_Infantry", "Attack_" + std::string(semantic_directions[direction]), "auxinf_f", lower_directions[direction], 5) ||
            !validate_native_soldier_animation("Warriors\\Auxiliary_Archer", "Move_" + std::string(semantic_directions[direction]), "auxarch", lower_directions[direction], 12) ||
            !validate_native_soldier_animation("Warriors\\Auxiliary_Archer", "Melee_" + std::string(semantic_directions[direction]), "auxarch_fm", lower_directions[direction], 5) ||
            !validate_native_soldier_animation("Warriors\\Auxiliary_Archer", "Ranged_" + std::string(semantic_directions[direction]), "auxarch_fr", lower_directions[direction], 5)) {
            return false;
        }
    }
    if (!validate_native_soldier_animation("Warriors\\Auxiliary_Infantry", "Death", "auxinf_death", nullptr, 8) ||
        !validate_native_soldier_animation("Warriors\\Auxiliary_Archer", "Death", "auxarch_death", nullptr, 8)) {
        return false;
    }
    std::cout << "Validated two consolidated, tile-anchored 0.8-scale auxiliary soldier image groups.\n" << std::flush;
    return true;
}

bool image_group_entry_is_drawable(const ImageGroupEntry *entry)
{
    if (!entry) return false;
    const RuntimeDrawSlice *footprint = entry->footprint();
    if (!entry->has_animation()) return footprint && footprint->is_valid();
    if (entry->animation().frame_count() <= 0) return false;
    for (int frame = 1; frame <= entry->animation().frame_count(); frame++) {
        if (!entry->animation().frame_slice_at_offset(frame).is_valid()) return false;
    }
    return true;
}

bool validate_named_asset_entries(const char *path, const std::vector<std::string> &entry_ids)
{
    if (!image_group_payload_load(path)) {
        std::cerr << "Asset contract failed: could not load " << path << ".\n";
        return false;
    }
    const ImageGroupPayload *payload = image_group_payload_get(path);
    for (const std::string &entry_id : entry_ids) {
        if (!image_group_entry_is_drawable(payload ? payload->entry_for(entry_id.c_str()) : nullptr)) {
            std::cerr << "Asset contract failed: " << path << " entry " << entry_id << " is missing or not drawable.\n";
            return false;
        }
    }
    return true;
}

bool validate_vespasian_legacy_figure_and_projectile_scale_contract()
{
    constexpr int expected_legacy_figure_units = 96;
    constexpr int expected_projectile_units = 96;
    if (game_defines_legacy_figure_logical_units_per_source_pixel() != expected_legacy_figure_units) {
        std::cerr << "Vespasian legacy figure presentation scale is not the declared 0.8 value.\n";
        return false;
    }
    if (!image_group_payload_load("Warriors\\Projectiles")) {
        std::cerr << "Projectile asset contract failed: could not load Warriors\\Projectiles.\n";
        return false;
    }
    const ImageGroupPayload *payload = image_group_payload_get("Warriors\\Projectiles");
    for (int offset = 0; offset < 48; offset++) {
        char image_id[32];
        std::snprintf(image_id, sizeof(image_id), "Image_%04d", offset);
        const ImageGroupEntry *entry = payload ? payload->entry_for(image_id) : nullptr;
        const RuntimeDrawSlice *slice = entry ? entry->footprint() : nullptr;
        const render_logical_size logical = entry ? entry->fixed_logical_size() : render_logical_size{};
        if (!slice || !slice->is_valid() || logical.width != entry->source_pixel_width() * expected_projectile_units ||
            logical.height != entry->source_pixel_height() * expected_projectile_units) {
            std::cerr << "Projectile asset contract failed for Warriors\\Projectiles\\" << image_id << ".\n";
            return false;
        }
    }
    std::cout << "Validated Vespasian 0.8-scale legacy figures and all 48 projectile frames.\n" << std::flush;
    return true;
}

bool validate_race_betting_ui_asset_contract()
{
    return validate_named_asset_entries("UI\\Arrow_Button", { "Increase", "Increase_Pressed", "Decrease", "Decrease_Pressed" }) &&
        validate_named_asset_entries("UI\\Context_Icons", { "Image_0004", "Image_0005" }) &&
        validate_named_asset_entries("UI\\Resource_Icons", { "Image_0016" });
}

bool validate_vespasian_race_team_asset_contract()
{
    using building_type_registry_impl::BuildingType;
    using building_type_registry_impl::RaceDefinition;
    using building_type_registry_impl::definition_for_type;
    using building_type_registry_impl::type_from_attr;

    const BuildingType *hippodrome = definition_for_type(type_from_attr("hippodrome"));
    if (!hippodrome || !hippodrome->has_race()) return true;
    const RaceDefinition &race = hippodrome->race();
    if (race.teams.size() != 4) return true;

    static constexpr const char *directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    static constexpr const char *body_entries[] = { "horse_blue", "horse_red", "horse_blue", "horse_red" };
    static constexpr const char *vehicle_entries[] = { "cart_blue", "cart_red", "cart_blue", "cart_red" };
    static constexpr int vehicle_offsets[][2] = { { 13, -7 }, { 18, -1 }, { 12, 7 }, { 0, 11 }, { -13, 6 }, { -18, -1 }, { -13, -7 }, { 0, -12 } };

    for (size_t team_index = 0; team_index < race.teams.size(); ++team_index) {
        const building_type_registry_impl::RaceTeamDefinition &team = race.teams[team_index];
        if (team.graphics_path != "Walkers\\Hippodrome_Chariot_Teams" || team.body_entry != body_entries[team_index] ||
            team.vehicle_entry != vehicle_entries[team_index] ||
            team.vehicle_behind[0] != 1 || team.vehicle_behind[1] != 1 || team.vehicle_behind[5] != 1 ||
            team.vehicle_behind[6] != 1 || team.vehicle_behind[7] != 1) {
            std::cerr << "Vespasian temporary legacy race-team mapping is incorrect for team " << team.id << ".\n";
            return false;
        }
        for (int direction = 0; direction < 8; ++direction) {
            if (team.vehicle_offsets[direction].x != vehicle_offsets[direction][0] ||
                team.vehicle_offsets[direction].y != vehicle_offsets[direction][1]) {
                std::cerr << "Vespasian temporary legacy race-team composition offset is incorrect for team " << team.id
                          << " direction " << directions[direction] << ".\n";
                return false;
            }
        }
        if (!image_group_payload_load(team.graphics_path.c_str())) {
            std::cerr << "Vespasian temporary legacy race-team asset could not load " << team.graphics_path << ".\n";
            return false;
        }
        const ImageGroupPayload *payload = image_group_payload_get(team.graphics_path.c_str());
        for (int direction = 0; direction < 8; ++direction) {
            const std::string body_id = team.body_entry + "_" + directions[direction];
            const std::string vehicle_id = team.vehicle_entry + "_" + directions[direction];
            const ImageGroupEntry *body = payload ? payload->entry_for(body_id.c_str()) : nullptr;
            const ImageGroupEntry *vehicle = payload ? payload->entry_for(vehicle_id.c_str()) : nullptr;
            if (!image_group_entry_is_drawable(body) || !image_group_entry_is_drawable(vehicle)) {
                std::cerr << "Vespasian temporary legacy race-team entries are incomplete for team " << team.id
                          << " direction " << directions[direction] << ".\n";
                return false;
            }
        }
    }
    std::cout << "Validated four Vespasian teams against the temporary two-set legacy race graphics mapping.\n";
    return true;
}

std::string extracted_image_name(int offset)
{
    char image[32];
    std::snprintf(image, sizeof(image), "Image_%04d", offset);
    return image;
}

bool runtime_slices_match(const RuntimeDrawSlice &actual, const RuntimeDrawSlice &expected)
{
    const std::uint64_t actual_fingerprint = startup_parser::image_resource_fingerprint(actual.handle);
    const std::uint64_t expected_fingerprint = startup_parser::image_resource_fingerprint(expected.handle);
    return actual_fingerprint && actual_fingerprint == expected_fingerprint &&
        actual.width == expected.width && actual.height == expected.height &&
        actual.draw_offset_x == expected.draw_offset_x && actual.draw_offset_y == expected.draw_offset_y &&
        actual.is_isometric == expected.is_isometric &&
        actual.fixed_logical_size.width == expected.fixed_logical_size.width &&
        actual.fixed_logical_size.height == expected.fixed_logical_size.height;
}

bool validate_semantic_sequence_matches_legacy_offsets(
    const char *semantic_path,
    const std::string &semantic_entry_id,
    const char *extracted_group,
    const std::vector<int> &legacy_offsets)
{
    if (!image_group_payload_load(semantic_path) || !image_group_payload_load(extracted_group)) return false;
    const ImageGroupPayload *semantic_payload = image_group_payload_get(semantic_path);
    const ImageGroupPayload *extracted_payload = image_group_payload_get(extracted_group);
    const ImageGroupEntry *semantic_entry = semantic_payload ? semantic_payload->entry_for(semantic_entry_id.c_str()) : nullptr;
    if (!semantic_entry || !semantic_entry->has_animation() ||
        semantic_entry->animation().frame_count() != static_cast<int>(legacy_offsets.size())) {
        std::cerr << "Legacy image-selection parity failed: " << semantic_path << " entry " << semantic_entry_id
            << " has the wrong animation length.\n";
        return false;
    }
    for (std::size_t frame = 0; frame < legacy_offsets.size(); ++frame) {
        const std::string expected_id = extracted_image_name(legacy_offsets[frame]);
        const ImageGroupEntry *expected_entry = extracted_payload ? extracted_payload->entry_for(expected_id.c_str()) : nullptr;
        const RuntimeDrawSlice *expected_slice = expected_entry ? expected_entry->footprint() : nullptr;
        const RuntimeDrawSlice actual_slice = semantic_entry->animation().frame_slice_at_offset(static_cast<int>(frame) + 1, 0);
        if (!expected_slice || !runtime_slices_match(actual_slice, *expected_slice)) {
            std::cerr << "Legacy image-selection parity failed: " << semantic_path << " entry " << semantic_entry_id
                << " frame " << (frame + 1) << " does not select " << extracted_group << "\\" << expected_id << ".\n";
            return false;
        }
    }
    return true;
}

bool validate_semantic_static_matches_legacy_offset(
    const char *semantic_path,
    const std::string &semantic_entry_id,
    const char *extracted_group,
    int legacy_offset)
{
    if (!image_group_payload_load(semantic_path) || !image_group_payload_load(extracted_group)) return false;
    const ImageGroupPayload *semantic_payload = image_group_payload_get(semantic_path);
    const ImageGroupPayload *extracted_payload = image_group_payload_get(extracted_group);
    const ImageGroupEntry *semantic_entry = semantic_payload ? semantic_payload->entry_for(semantic_entry_id.c_str()) : nullptr;
    const std::string expected_id = extracted_image_name(legacy_offset);
    const ImageGroupEntry *expected_entry = extracted_payload ? extracted_payload->entry_for(expected_id.c_str()) : nullptr;
    const RuntimeDrawSlice *actual_slice = semantic_entry ? semantic_entry->footprint() : nullptr;
    const RuntimeDrawSlice *expected_slice = expected_entry ? expected_entry->footprint() : nullptr;
    if (!actual_slice || !expected_slice || !runtime_slices_match(*actual_slice, *expected_slice)) {
        std::cerr << "Legacy image-selection parity failed: " << semantic_path << " entry " << semantic_entry_id
            << " does not select " << extracted_group << "\\" << expected_id << ".\n";
        return false;
    }
    return true;
}

std::vector<int> direction_major_offsets(int direction, int base, int frame_count)
{
    std::vector<int> offsets;
    for (int frame = 0; frame < frame_count; ++frame) offsets.push_back(base + direction + 8 * frame);
    return offsets;
}

std::vector<int> linear_offsets(int base, int frame_count)
{
    std::vector<int> offsets;
    for (int frame = 0; frame < frame_count; ++frame) offsets.push_back(base + frame);
    return offsets;
}

bool validate_legacy_figure_image_selection_parity()
{
    static const char *const directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    static const int criminal_gesture_offsets[] = { 0, 0, 1, 2, 3, 4, 5, 6, 7, 7, 6, 5, 4, 3, 2, 1 };
    static const int flotsam_wood_offsets[] = { 0, 1, 2, 3, 4, 4, 4, 3, 2, 1, 0, 0 };
    static const int flotsam_cargo_offsets[] = { 0, 1, 1, 2, 2, 3, 3, 4, 4, 4, 3, 2, 1, 0, 0, 1, 1, 2, 2, 1, 1, 0, 0, 0 };
    static const int flotsam_debris_offsets[] = { 0, 0, 1, 1, 2, 2, 3, 3, 4, 4 };

    for (int direction = 0; direction < 8; ++direction) {
        const std::string suffix = directions[direction];
        if (!validate_semantic_sequence_matches_legacy_offsets("Environment\\wolf", "move_" + suffix,
                "Environment\\Group_234", direction_major_offsets(direction, 0, 12)) ||
            !validate_semantic_sequence_matches_legacy_offsets("Environment\\wolf", "attack_" + suffix,
                "Environment\\Group_234", direction_major_offsets(direction, 104, 6)) ||
            !validate_semantic_static_matches_legacy_offset("Environment\\wolf", "rest_" + suffix,
                "Environment\\Group_234", 152 + direction) ||
            !validate_semantic_sequence_matches_legacy_offsets("Environment\\zebra", "move_" + suffix,
                "Environment\\Group_235", direction_major_offsets(direction, 0, 12)) ||
            !validate_semantic_static_matches_legacy_offset("Environment\\zebra", "alternate_rest_" + suffix,
                "Environment\\Group_235", direction) ||
            !validate_semantic_sequence_matches_legacy_offsets("Environment\\sheep", "move_" + suffix,
                "Environment\\Group_233", direction_major_offsets(direction, 0, 6)) ||
            !validate_semantic_static_matches_legacy_offset("Environment\\sheep", "alternate_rest_" + suffix,
                "Environment\\Group_233", 96 + direction) ||
            !validate_semantic_sequence_matches_legacy_offsets("Walkers\\criminal", "move_" + suffix,
                "Walkers\\Group_115", direction_major_offsets(direction, 0, 12))) return false;

        std::vector<int> zebra_rest;
        std::vector<int> sheep_rest;
        for (int tick = 0; tick < 64; ++tick) {
            const int common = tick < 2 ? 0 : tick < 4 ? 1 : tick < 6 ? 2 : tick < 52 ? 3 : tick < 56 ? 4 : -13;
            zebra_rest.push_back(104 + direction + 8 * common);
            const int sheep = tick < 2 ? 0 : tick < 4 ? 1 : tick < 6 ? 2 : tick < 52 ? 3 : tick < 54 ? 4 : tick < 56 ? 5 : -1;
            sheep_rest.push_back(48 + direction + 8 * sheep);
        }
        if (!validate_semantic_sequence_matches_legacy_offsets("Environment\\zebra", "rest_" + suffix,
                "Environment\\Group_235", zebra_rest) ||
            !validate_semantic_sequence_matches_legacy_offsets("Environment\\sheep", "rest_" + suffix,
                "Environment\\Group_233", sheep_rest)) return false;

        for (const char *team : { "blue", "red" }) {
            const int horse_group = std::strcmp(team, "blue") == 0 ? 217 : 218;
            const int cart_group = std::strcmp(team, "blue") == 0 ? 219 : 220;
            const std::string horse_path = "Walkers\\Group_" + std::to_string(horse_group);
            const std::string cart_path = "Walkers\\Group_" + std::to_string(cart_group);
            if (!validate_semantic_sequence_matches_legacy_offsets("Walkers\\hippodrome_horses",
                    std::string("horse_") + team + "_" + suffix, horse_path.c_str(),
                    direction_major_offsets(direction, 0, 8)) ||
                !validate_semantic_static_matches_legacy_offset("Walkers\\hippodrome_horses",
                    std::string("cart_") + team + "_" + suffix, cart_path.c_str(), direction)) return false;
        }
    }

    std::vector<int> criminal_gesture_legacy_offsets;
    for (int offset : criminal_gesture_offsets) criminal_gesture_legacy_offsets.push_back(104 + offset);
    if (!validate_semantic_sequence_matches_legacy_offsets("Environment\\wolf", "corpse", "Environment\\Group_234", { 96, 97, 98, 99, 100, 101, 102, 103 }) ||
        !validate_semantic_sequence_matches_legacy_offsets("Environment\\zebra", "corpse", "Environment\\Group_235", { 96, 97, 98, 99, 100, 101, 102, 103 }) ||
        !validate_semantic_sequence_matches_legacy_offsets("Environment\\sheep", "corpse", "Environment\\Group_233", { 104, 105, 106, 107, 108, 109, 110, 111 }) ||
        !validate_semantic_sequence_matches_legacy_offsets("Walkers\\criminal", "corpse", "Walkers\\Group_115", { 96, 97, 98, 99, 100, 101, 102, 103 }) ||
        !validate_semantic_sequence_matches_legacy_offsets("Walkers\\criminal", "gesture", "Walkers\\Group_115",
            criminal_gesture_legacy_offsets) ||
        !validate_semantic_sequence_matches_legacy_offsets("FX\\explosion", "cloud", "FX\\Group_102", linear_offsets(0, 8)) ||
        !validate_semantic_sequence_matches_legacy_offsets("Environment\\fish_gulls", "variant_a", "Environment\\Group_206",
            linear_offsets(0, 18)) ||
        !validate_semantic_sequence_matches_legacy_offsets("Environment\\fish_gulls", "variant_b", "Environment\\Group_206",
            linear_offsets(18, 24)) ||
        !validate_semantic_sequence_matches_legacy_offsets("Ships\\flotsam", "wood", "Ships\\Group_153",
            std::vector<int>(std::begin(flotsam_wood_offsets), std::end(flotsam_wood_offsets))) ||
        !validate_semantic_sequence_matches_legacy_offsets("Ships\\flotsam", "neptune_sheep", "Ships\\Group_242",
            std::vector<int>(std::begin(flotsam_wood_offsets), std::end(flotsam_wood_offsets))) ||
        !validate_semantic_sequence_matches_legacy_offsets("Ships\\flotsam", "cargo_a", "Ships\\Group_154",
            std::vector<int>(std::begin(flotsam_cargo_offsets), std::end(flotsam_cargo_offsets))) ||
        !validate_semantic_sequence_matches_legacy_offsets("Ships\\flotsam", "cargo_b", "Ships\\Group_155",
            std::vector<int>(std::begin(flotsam_cargo_offsets), std::end(flotsam_cargo_offsets))) ||
        !validate_semantic_sequence_matches_legacy_offsets("Ships\\flotsam", "debris", "Ships\\Group_156",
            std::vector<int>(std::begin(flotsam_debris_offsets), std::end(flotsam_debris_offsets)))) return false;

    std::cout << "Validated native semantic image selection against committed legacy group formulas.\n" << std::flush;
    return true;
}

bool validate_semantic_figure_graphics_contract()
{
    static const char *const directions[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    std::vector<std::string> wolf_entries;
    std::vector<std::string> zebra_entries;
    std::vector<std::string> sheep_entries;
    std::vector<std::string> hippodrome_entries;
    for (const char *direction : directions) {
        wolf_entries.emplace_back(std::string("move_") + direction);
        wolf_entries.emplace_back(std::string("attack_") + direction);
        wolf_entries.emplace_back(std::string("rest_") + direction);
        for (std::vector<std::string> *entries : { &zebra_entries, &sheep_entries }) {
            entries->emplace_back(std::string("move_") + direction);
            entries->emplace_back(std::string("rest_") + direction);
            entries->emplace_back(std::string("alternate_rest_") + direction);
        }
        for (const char *team : { "blue", "red" }) {
            hippodrome_entries.emplace_back(std::string("horse_") + team + "_" + direction);
            hippodrome_entries.emplace_back(std::string("cart_") + team + "_" + direction);
        }
    }
    wolf_entries.emplace_back("corpse");
    zebra_entries.emplace_back("corpse");
    sheep_entries.emplace_back("corpse");

    if (!validate_named_asset_entries("Environment\\wolf", wolf_entries) ||
        !validate_named_asset_entries("Environment\\zebra", zebra_entries) ||
        !validate_named_asset_entries("Environment\\sheep", sheep_entries) ||
        !validate_named_asset_entries("Environment\\fish_gulls", { "variant_a", "variant_b" }) ||
        !validate_named_asset_entries("Ships\\flotsam", { "wood", "neptune_sheep", "cargo_a", "cargo_b", "debris" }) ||
        !validate_named_asset_entries("Walkers\\hippodrome_horses", hippodrome_entries)) {
        return false;
    }

    std::vector<std::string> missionary_entries;
    for (int frame = 0; frame < 104; frame++) {
        char image[32];
        std::snprintf(image, sizeof(image), "Image_%04d", frame);
        missionary_entries.emplace_back(image);
    }
    if (!validate_named_asset_entries("Walkers\\Group_230", missionary_entries)) return false;

    GraphicsAssetReference reference;
    reference.set_path("Environment\\wolf");
    reference.set_image("move_ne");
    if (!reference.has_logical_asset_path() || !reference.cache_asset_binding() || !reference.cached_entry()) {
        std::cerr << "Shared asset-reference contract failed: a valid logical FigureType asset reference did not bind.\n";
        return false;
    }
    reference.set_image("horse_blue_ne");
    if (reference.cache_asset_binding() || reference.cached_entry()) {
        std::cerr << "Runtime-selected graphics contract failed: entry lookup escaped the referenced XML asset.\n";
        return false;
    }
    reference.set_path("Environment\\wolf.xml");
    if (reference.has_logical_asset_path() || reference.cache_asset_binding()) {
        std::cerr << "Shared asset-reference contract failed: file-extension paths must be rejected.\n";
        return false;
    }

    std::cout << "Validated semantic animal, missionary, gull, flotsam, and two-layer hippodrome assets in every direction.\n" << std::flush;
    return true;
}

bool validate_synthetic_figure_lifecycle_graphics_contract()
{
    int figure_type_count = 0;
    int live_binding_count = 0;
    int corpse_binding_count = 0;
    int specialized_live_count = 0;
    int no_corpse_presentation_count = 0;
    std::vector<std::string> specialized_live_types;
    std::vector<std::string> types_without_corpse_presentation;
    const auto intentionally_non_corporeal = [](figure_type type) {
        switch (type) {
            case FIGURE_FORT_STANDARD:
            case FIGURE_FISHING_BOAT:
            case FIGURE_MAP_FLAG:
            case FIGURE_HIPPODROME_HORSES:
                return true;
            default:
                return false;
        }
    };
    const auto validate_role = [&](const figure_type_registry_impl::FigureTypeDefinition &definition,
                                   const figure_type_registry_impl::FigureGraphics &graphics,
                                   GraphicsTargetRole role,
                                   int &binding_count) {
        const int frame_count = graphics.target_frame_count(role);
        for (int direction = 0; direction < GRAPHICS_DIRECTION8_COUNT; direction++) {
            for (int frame = 1; frame <= frame_count; frame++) {
                const figure_type_registry_impl::GraphicsTargetBinding *binding =
                    graphics.cached_target_binding(role, direction, frame);
                if (!binding || !binding->is_complete() || !binding->is_resolved() ||
                    !binding->resolved_slice().is_valid()) {
                    std::cerr << "Synthetic FigureType lifecycle render failed: figure=" << definition.attr()
                        << " state=" << (role == GraphicsTargetRole::Corpse ? "dead" : "alive")
                        << " direction=" << graphics_direction8_suffix(direction)
                        << " frame=" << frame << ".\n";
                    return false;
                }
                binding_count++;
            }
        }
        return true;
    };

    for (int value = 1; value < FIGURE_TYPE_MAX; value++) {
        const auto type = static_cast<figure_type>(value);
        const figure_type_registry_impl::FigureTypeDefinition *definition =
            figure_type_registry_impl::definition_for(type);
        if (!definition) continue;
        figure_type_count++;
        const figure_type_registry_impl::FigureGraphics &graphics = definition->graphics();
        if (graphics.has_native_payload()) {
            if (!validate_role(*definition, graphics, GraphicsTargetRole::Default, live_binding_count)) return false;
        } else {
            specialized_live_count++;
            specialized_live_types.emplace_back(definition->attr());
        }
        if (graphics.has_corpse_native_payload()) {
            if (!validate_role(*definition, graphics, GraphicsTargetRole::Corpse, corpse_binding_count)) return false;
        } else if (intentionally_non_corporeal(type)) {
            no_corpse_presentation_count++;
            types_without_corpse_presentation.emplace_back(definition->attr());
        } else {
            std::cerr << "Synthetic FigureType lifecycle render failed: corporeal figure has no dead presentation: figure="
                << definition->attr() << " corpse_sources=" << graphics.corpse_source_count()
                << " corpse_path=" << graphics.asset_target(GraphicsTargetRole::Corpse).path() << ".\n";
            return false;
        }
    }

    std::cout << "Synthetically materialized every configured alive/dead FigureType render binding: figure_types="
        << figure_type_count << " live_bindings=" << live_binding_count
        << " corpse_bindings=" << corpse_binding_count
        << " specialized_live_types=" << specialized_live_count
        << " types_without_corpse_presentation=" << no_corpse_presentation_count << ".\n" << std::flush;
    if (!specialized_live_types.empty()) {
        std::cout << "Synthetic lifecycle specialized live renderers:";
        for (const std::string &type : specialized_live_types) std::cout << ' ' << type;
        std::cout << ".\n";
    }
    if (!types_without_corpse_presentation.empty()) {
        std::cout << "Synthetic lifecycle explicitly non-corporeal renderers:";
        for (const std::string &type : types_without_corpse_presentation) std::cout << ' ' << type;
        std::cout << ".\n";
    }
    return true;
}

bool validate_semantic_walker_alias_frame_selection_contract()
{
    struct BindingCase {
        figure_type type;
        GraphicsTargetRole role;
        int direction;
        int frame;
        const char *expected_entry;
    };
    constexpr BindingCase cases[] = {
        { FIGURE_ENGINEER, GraphicsTargetRole::Default, 0, 1, "Image_0000" },
        { FIGURE_ENGINEER, GraphicsTargetRole::Default, 1, 1, "Image_0001" },
        { FIGURE_ENGINEER, GraphicsTargetRole::Default, 0, 2, "Image_0008" },
        { FIGURE_ENGINEER, GraphicsTargetRole::Default, 7, 12, "Image_0095" },
        { FIGURE_ENGINEER, GraphicsTargetRole::Corpse, 0, 1, "Image_0096" },
        { FIGURE_ENGINEER, GraphicsTargetRole::Corpse, 7, 8, "Image_0103" },
        { FIGURE_IMMIGRANT, GraphicsTargetRole::Default, 0, 1, "Image_0000" },
        { FIGURE_IMMIGRANT, GraphicsTargetRole::Default, 1, 1, "Image_0001" },
        { FIGURE_IMMIGRANT, GraphicsTargetRole::Default, 0, 2, "Image_0008" },
        { FIGURE_IMMIGRANT, GraphicsTargetRole::Corpse, 0, 1, "Image_0096" }
    };

    for (const BindingCase &test_case : cases) {
        const figure_type_registry_impl::FigureGraphics *graphics =
            figure_type_registry_impl::graphics_for(test_case.type);
        const figure_type_registry_impl::GraphicsTargetBinding *binding = graphics ?
            graphics->cached_target_binding(test_case.role, test_case.direction, test_case.frame) :
            nullptr;
        if (!binding || binding->image != test_case.expected_entry || !binding->frame_selects_entry) {
            std::cerr << "Semantic walker alias selected the wrong extracted entry: figure_type="
                << static_cast<int>(test_case.type)
                << " direction=" << test_case.direction << " frame=" << test_case.frame
                << " expected=" << test_case.expected_entry
                << " actual=" << (binding ? binding->image : "missing") << ".\n";
            return false;
        }
    }
    std::cout << "Validated semantic walker aliases across direction, animation, and corpse entries.\n" << std::flush;
    return true;
}

struct ExtractionPrerequisite {
    const char *label;
    const char *relative_path;
};

std::filesystem::path executable_directory(const char *argv0)
{
    std::error_code error;
    std::filesystem::path executable = argv0 && *argv0 ?
        std::filesystem::absolute(std::filesystem::path(argv0), error) :
        std::filesystem::current_path(error);
    if (error) {
        executable = std::filesystem::current_path();
    }
    return executable.has_filename() ? executable.parent_path() : executable;
}

std::string quoted(const std::filesystem::path &path)
{
    return "\"" + path.string() + "\"";
}

#if defined(_WIN32)
int run_hidden_process(const std::filesystem::path &executable, const std::wstring &command_line)
{
    std::vector<wchar_t> mutable_command_line(command_line.begin(), command_line.end());
    mutable_command_line.push_back(L'\0');
    STARTUPINFOW startup_info = {};
    startup_info.cb = sizeof(startup_info);
    startup_info.dwFlags = STARTF_USESHOWWINDOW;
    startup_info.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process_info = {};
    if (!CreateProcessW(executable.wstring().c_str(), mutable_command_line.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &startup_info, &process_info)) return -static_cast<int>(GetLastError());
    WaitForSingleObject(process_info.hProcess, INFINITE);
    DWORD child_exit_code = 1;
    GetExitCodeProcess(process_info.hProcess, &child_exit_code);
    CloseHandle(process_info.hThread);
    CloseHandle(process_info.hProcess);
    return static_cast<int>(child_exit_code);
}
#endif

bool run_executable_startup_test(const std::filesystem::path &game_root, const std::filesystem::path &tool_directory, const char *mod_name)
{
#if defined(_WIN32)
    const std::filesystem::path executable = tool_directory / "Vespasian.exe";
#else
    const std::filesystem::path executable = tool_directory / "Vespasian";
#endif
    if (!std::filesystem::is_regular_file(executable)) {
        std::cerr << "Executable startup test requires: " << executable << "\n";
        return false;
    }

    std::cout << "Running executable startup test for " << mod_name << ": " << executable << "\n" << std::flush;
#if defined(_WIN32)
    const std::wstring executable_arg = executable.wstring();
    const std::wstring game_root_arg = game_root.wstring();
    const std::wstring command_line = L"\"" + executable_arg + L"\" --startup-test --no-audio --mod " + std::filesystem::path(mod_name).wstring() + L" \"" + game_root_arg + L"\"";
    const int result = run_hidden_process(executable, command_line);
#else
    const std::string command = quoted(executable) + " --startup-test --no-audio --mod " + mod_name + " " + quoted(game_root);
    const int result = std::system(command.c_str());
#endif
    if (result != 0) {
        std::cerr << "Executable startup test for " << mod_name << " failed with exit code " << result << ".\n";
        return false;
    }
    std::cout << "Executable startup test for " << mod_name << " passed.\n";
    return true;
}

bool run_executable_startup_tests(const std::filesystem::path &game_root, const std::filesystem::path &tool_directory)
{
    constexpr const char *MOD_STACK_TOPS[] = { "Julius", "Augustus", "Vespasian" };
    for (const char *mod_name : MOD_STACK_TOPS) {
        if (!run_executable_startup_test(game_root, tool_directory, mod_name)) return false;
    }
    return true;
}

struct SaveSoakCandidate {
    std::filesystem::path path;
    std::filesystem::file_time_type modified;
    std::string extension;
};

std::string lowercase(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

bool is_required_save_cohort_member(const std::filesystem::path &path)
{
    const std::string name = lowercase(path.stem().string());
    if (name.find("praetor") != std::string::npos || name.find("engineer") != std::string::npos ||
        name.find("quaestor") != std::string::npos || name.find("questor") != std::string::npos) return true;
    int aedile_sequence = 0;
    return std::sscanf(name.c_str(), "aedile 1 %d", &aedile_sequence) == 1 && aedile_sequence >= 3 && aedile_sequence <= 15;
}

std::vector<std::filesystem::path> discover_save_soak_files(const std::filesystem::path &game_root, int requested_count)
{
    const std::filesystem::path save_directory = game_root / "savegames";
    std::error_code error;
    std::vector<SaveSoakCandidate> candidates;
    std::filesystem::recursive_directory_iterator iterator(save_directory, std::filesystem::directory_options::skip_permission_denied, error);
    const std::filesystem::recursive_directory_iterator end;
    while (!error && iterator != end) {
        if (iterator->is_regular_file(error)) {
            std::string extension = iterator->path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
            if (extension == ".svv" || extension == ".sav") candidates.push_back({ std::filesystem::absolute(iterator->path()), iterator->last_write_time(error), extension });
        }
        iterator.increment(error);
    }
    std::sort(candidates.begin(), candidates.end(), [](const SaveSoakCandidate &left, const SaveSoakCandidate &right) {
        if (left.modified != right.modified) return left.modified > right.modified;
        return left.path.string() < right.path.string();
    });

    std::vector<std::filesystem::path> selected;
    if (candidates.empty()) return selected;
    for (const SaveSoakCandidate &candidate : candidates) {
        if (is_required_save_cohort_member(candidate.path)) selected.push_back(candidate.path);
    }
    const auto append_if_missing = [&selected](const std::filesystem::path &path) {
        if (std::find(selected.begin(), selected.end(), path) == selected.end()) selected.push_back(path);
    };
    append_if_missing(candidates.front().path);
    for (const SaveSoakCandidate &candidate : candidates) {
        if (candidate.extension != candidates.front().extension) {
            append_if_missing(candidate.path);
            break;
        }
    }
    int representative_count = 0;
    for (const SaveSoakCandidate &candidate : candidates) {
        if (representative_count >= requested_count) break;
        if (!is_required_save_cohort_member(candidate.path)) {
            append_if_missing(candidate.path);
            representative_count++;
        }
    }
    return selected;
}

bool run_executable_save_soak_test(const std::filesystem::path &executable, const std::filesystem::path &game_root, const std::filesystem::path &save, const char *mod_name, int tick_count)
{
    const std::filesystem::path roundtrip_save = std::filesystem::temp_directory_path() / (std::string("Vespasian-StartupParserTest-") + mod_name + "-roundtrip.svv");
    std::error_code remove_error;
    std::filesystem::remove(roundtrip_save, remove_error);
    std::cout << "Save soak [" << mod_name << "]: " << save << "\n" << std::flush;
#if defined(_WIN32)
    const std::wstring command_line = L"\"" + executable.wstring() + L"\" --load-save-test \"" + save.wstring() + L"\" --save-roundtrip-test \"" + roundtrip_save.wstring() + L"\" --save-soak-ticks " + std::to_wstring(tick_count) + L" --no-audio --mod " + std::filesystem::path(mod_name).wstring() + L" \"" + game_root.wstring() + L"\"";
    const int result = run_hidden_process(executable, command_line);
#else
    const std::string command = quoted(executable) + " --load-save-test " + quoted(save) + " --save-roundtrip-test " + quoted(roundtrip_save) + " --save-soak-ticks " + std::to_string(tick_count) + " --no-audio --mod " + mod_name + " " + quoted(game_root);
    const int result = std::system(command.c_str());
#endif
    if (result != 0) {
        std::cerr << "Save soak for " << mod_name << " failed with exit code " << result << ": " << save << "\n";
        std::cerr << "Roundtrip output preserved for diagnosis: " << roundtrip_save << "\n";
        return false;
    }
    std::filesystem::remove(roundtrip_save, remove_error);
    return true;
}

bool run_dependency_stack_save_soak_tests(const std::filesystem::path &game_root, const std::filesystem::path &tool_directory, int tick_count)
{
#if defined(_WIN32)
    const std::filesystem::path executable = tool_directory / "Vespasian.exe";
#else
    const std::filesystem::path executable = tool_directory / "Vespasian";
#endif
    const std::filesystem::path save = game_root / "savegames" / "Citizen - Julius Only Save.svv";
    if (!std::filesystem::is_regular_file(save)) {
        std::cerr << "Dependency-stack save test requires: " << save << "\n";
        return false;
    }
    return run_executable_save_soak_test(executable, game_root, save, "Julius", tick_count) &&
        run_executable_save_soak_test(executable, game_root, save, "Augustus", tick_count);
}

bool run_executable_save_soak_tests(const std::filesystem::path &game_root, const std::filesystem::path &tool_directory, int save_count, int tick_count)
{
#if defined(_WIN32)
    const std::filesystem::path executable = tool_directory / "Vespasian.exe";
#else
    const std::filesystem::path executable = tool_directory / "Vespasian";
#endif
    const std::vector<std::filesystem::path> saves = discover_save_soak_files(game_root, save_count);
    if (saves.empty()) {
        std::cerr << "Save-soak test requires at least one .svv or .sav file under " << (game_root / "savegames") << ".\n";
        return false;
    }
    std::cout << "Running " << saves.size() << " required and representative Vespasian save soak(s) for " << tick_count << " ticks each at 1000% speed.\n" << std::flush;
    for (const std::filesystem::path &save : saves) {
        if (!run_executable_save_soak_test(executable, game_root, save, "Vespasian", tick_count)) {
            return false;
        }
    }
    std::cout << "Required and representative save soaks passed with zero unallowlisted warnings and errors.\n";
    return true;
}

bool check_extraction_prerequisites(
    const std::filesystem::path &game_root,
    const std::filesystem::path &tool_directory)
{
    const std::vector<ExtractionPrerequisite> prerequisites = {
        { "Julius central graphics extraction", "Mods/Julius/Graphics.legacy_extract.stamp" },
        { "Julius northern graphics extraction", "Mods/Julius/Graphics.legacy_extract_northern.stamp" },
        { "Julius desert graphics extraction", "Mods/Julius/Graphics.legacy_extract_desert.stamp" },
        { "Augustus graphics extraction", "Mods/Augustus/Graphics.graphics_extract.stamp" },
    };

    std::vector<std::filesystem::path> missing;
    for (const ExtractionPrerequisite &prerequisite : prerequisites) {
        std::filesystem::path path = game_root / std::filesystem::path(prerequisite.relative_path);
        std::error_code error;
        if (!std::filesystem::is_regular_file(path, error)) {
            missing.push_back(path);
        }
    }

    if (missing.empty()) {
        return true;
    }

    std::cerr
        << "\nStartupParserTest prerequisite failure: generated graphics extraction stamps are missing.\n"
        << "The XML parser was not run. Do not add parser fallbacks for this failure; run the extractors first.\n"
        << "Deploying Mods replaces generated graphics, so these commands are expected after each fresh deploy.\n\n";

    for (const std::filesystem::path &path : missing) {
        std::cerr << "Missing stamp: " << path.string() << "\n";
    }

    const std::filesystem::path julius_extractor = tool_directory / "JuliusGraphicsExtractor.exe";
    const std::filesystem::path augustus_extractor = tool_directory / "AugustusGraphicsExtractor.exe";
    std::cerr
        << "\nRequired order:\n"
        << "  " << quoted(julius_extractor) << " --game-root " << quoted(game_root) << "\n"
        << "  " << quoted(augustus_extractor) << " --game-root " << quoted(game_root)
        << " --no-force --stamp\n"
        << "  " << quoted(tool_directory / "StartupParserTest.exe") << " --game-root " << quoted(game_root) << "\n";
    return false;
}

void dump_graphics_source_metadata(const building_type_registry_impl::GraphicsTarget &target, std::set<std::string> &emitted)
{
    using namespace building_type_registry_impl;
    const int option_count = target.has_options() ? target.option_count() : 1;
    for (int option_index = 0; option_index < option_count; option_index++) {
        const GraphicsTarget resolved = target.has_options() ? target.resolved_option(static_cast<unsigned char>(option_index)) : target;
        if (resolved.no_draw() || !resolved.has_path() || !image_group_payload_load(resolved.path())) continue;
        const ImageGroupPayload *payload = image_group_payload_get(resolved.path());
        const ImageGroupEntry *entry = payload ? (resolved.has_image() ? payload->entry_for(resolved.image()) : payload->default_entry()) : nullptr;
        if (!entry) continue;
        const std::string image_id = resolved.has_image() ? resolved.image() : payload->default_image_id();
        const std::string key = std::string(resolved.path()) + "\t" + image_id;
        if (emitted.insert(key).second) {
            const render_logical_size logical = entry->fixed_logical_size();
            std::cout << "GRAPHICS_METADATA\t" << resolved.path() << "\t" << image_id << "\t" << entry->source_pixel_width() << "\t" << entry->source_pixel_height() << "\t" << logical.width << "\t" << logical.height << "\t" << (entry->footprint() ? 1 : 0) << "\t" << entry->has_top() << "\t" << entry->has_animation() << "\t" << (entry->has_sprite_offset() ? entry->sprite_offset_x() : 0) << "\t" << (entry->has_sprite_offset() ? entry->sprite_offset_y() : 0) << "\n";
        }
        for (const GraphicsLayer &layer : resolved.layers()) {
            const int layer_option_count = layer.has_options() ? layer.option_count() : 1;
            for (int layer_option = 0; layer_option < layer_option_count; layer_option++) {
                const GraphicsLayer resolved_layer = layer.has_options() ? layer.resolved_option(static_cast<unsigned char>(layer_option)) : layer;
                if (!resolved_layer.has_path() || !resolved_layer.has_image()) continue;
                GraphicsTarget layer_target;
                layer_target.set_path(resolved_layer.path());
                layer_target.set_image(resolved_layer.image());
                dump_graphics_source_metadata(layer_target, emitted);
            }
        }
    }
}

void dump_building_graphics_metadata()
{
    using namespace building_type_registry_impl;
    std::set<std::string> emitted;
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) continue;
        dump_graphics_source_metadata(definition->graphics().default_target(), emitted);
        for (const GraphicsVariant &variant : definition->graphics().variants()) dump_graphics_source_metadata(variant.target, emitted);
        for (const ConstructionPhaseGraphics &phase : definition->graphics().construction_phases()) dump_graphics_source_metadata(phase.target, emitted);
    }
}

} // namespace

int run_startup_parser_test(int argc, char **argv)
{
    Options options;
    const int parse_result = parse_options(argc, argv, options);
    if (parse_result <= 0) {
        return parse_result == 0 ? 0 : 2;
    }

    if (!options.game_root.empty()) {
        std::error_code error;
        std::filesystem::current_path(options.game_root, error);
        if (error) {
            std::cerr << "Unable to change to game root: " << options.game_root.string()
                << " (" << error.message() << ")\n";
            return 2;
        }
    }

    if (!validate_startup_parser_abi_contract() ||
        !validate_building_graphics_generation_contract() ||
        !validate_relationship_contract() ||
        !validate_hippodrome_race_contract()) {
        return 1;
    }

    const std::filesystem::path game_root = std::filesystem::current_path();
    const std::filesystem::path tool_directory = executable_directory(argv[0]);
    if (!check_extraction_prerequisites(game_root, tool_directory)) {
        return 1;
    }
    std::filesystem::path source_graphics = tool_directory.parent_path().parent_path() /
        "Mods" / "Vespasian" / "Graphics";
    if (!std::filesystem::is_directory(source_graphics)) {
        source_graphics = game_root / "Mods" / "Vespasian" / "Graphics";
    }
    if (!validate_vespasian_graphics_source_contract(source_graphics, std::cerr)) {
        return 1;
    }

    StartupEnvironmentSnapshot environment;
    if (!inspect_startup_environment(environment)) {
        return 1;
    }
    std::cout << "Startup parser test game root: " << environment.game_root << "\n";
    std::cout << "Selected mod stack: ";
    const auto &mods = environment.mod_stack;
    for (size_t i = 0; i < mods.size(); ++i) {
        std::cout << (i ? " -> " : "") << mods[i];
    }
    std::cout << "\nSelected mod path: " << environment.mod_path << "\n" << std::flush;

    // These fixture loaders intentionally publish temporary registries. Run
    // them before startup so the ordinary parse replaces them before any
    // BuildingType resolves definition pointers.
    if (!validate_housing_profile_registry_layering_contract(std::cerr) ||
        !validate_foundation_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!run_startup_parse()) {
        std::cerr << "Startup parser test failed.\n";
        return 1;
    }
    if (!validate_mod_metadata_contract(std::cerr)) {
        return 1;
    }
    if (options.dump_building_graphics_metadata) {
        dump_building_graphics_metadata();
        return 0;
    }
    if (!validate_entertainer_graphics_contract()) {
        return 1;
    }
    // Validate configured-stack contracts before layering fixtures replace the
    // live registries with deliberately incomplete definitions.
    if (!validate_building_identity_contract() ||
        !validate_parsed_race_visual_contract() ||
        !validate_parsed_composition_contracts(std::cerr) ||
        !validate_gate_terrain_foundation_contract() ||
        !validate_native_gatehouse_bridge_graphics_contract(std::cerr) ||
        !validate_authoritative_building_graphics_contract(std::cerr)) {
        return 1;
    }
    std::cout << "Validated authoritative native graphics for forts, towers, arches, vacant lots, and rubble.\n";
    if (!validate_native_statue_orientation_graphics_contract(std::cerr) ||
        !validate_native_building_selection_parity(std::cerr) ||
        !validate_native_hippodrome_graphics_contract(std::cerr) ||
        !validate_native_overlay_summary_graphics_contract(std::cerr) ||
        !validate_dock_native_orientation_contract() ||
        !validate_simple_native_building_graphics_contract() ||
        !validate_native_storage_and_fort_base_graphics_contract() ||
         !validate_colosseum_graphics_contract() ||
         !validate_figure_owner_contracts() ||
         !validate_race_betting_ui_asset_contract() ||
         !validate_vespasian_legacy_figure_and_projectile_scale_contract() ||
         !validate_vespasian_race_team_asset_contract() ||
         !validate_semantic_figure_graphics_contract() ||
         !validate_legacy_figure_image_selection_parity() ||
         !validate_semantic_walker_alias_frame_selection_contract() ||
         !validate_synthetic_figure_lifecycle_graphics_contract() ||
         !validate_native_auxiliary_soldier_graphics()) {
        return 1;
    }
    if (!validate_building_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_mod_definition_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_culture_module_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_god_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_resource_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_religion_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_production_method_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_storage_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_distribution_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_water_access_type_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_unit_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_formation_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_font_vector_runtime_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_graphics_extraction_boundary(std::cerr)) {
        return 1;
    }
    if (!validate_figure_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_plague_runtime_contract(std::cerr)) {
        return 1;
    }
    if (!validate_foundation_rotation_contract() ||
        !validate_composition_rotation_contracts(std::cerr) ||
        !validate_housing_transition_planner_contract()) {
        return 1;
    }
    if (!validate_rubble_repair_contract()) {
        return 1;
    }
    if (!validate_animated_isometric_contracts()) {
        return 1;
    }
    if (!validate_road_aqueduct_crossing_rules()) {
        return 1;
    }
    if (!validate_access_ramp_road_connection_rules()) {
        return 1;
    }
    if (!run_executable_startup_tests(game_root, tool_directory) ||
        !run_dependency_stack_save_soak_tests(game_root, tool_directory, options.save_soak_ticks) ||
        !run_executable_save_soak_tests(game_root, tool_directory, options.save_soak_count, options.save_soak_ticks)) {
        return 1;
    }
    std::cout << "Startup parser test passed.\n";
    return 0;
}

#if defined(_WIN32)
static LONG WINAPI report_cli_exception(EXCEPTION_POINTERS *exception)
{
    const EXCEPTION_RECORD *record = exception ? exception->ExceptionRecord : nullptr;
    const unsigned long code = record ? record->ExceptionCode : 0;
    const void *address = record ? record->ExceptionAddress : nullptr;
    const auto module_base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    const auto exception_address = reinterpret_cast<uintptr_t>(address);
    const auto relative_address = exception_address >= module_base ? exception_address - module_base : 0;
    std::fprintf(stderr, "StartupParserTest terminated after Windows exception 0x%08lX at %p (RVA 0x%llX).\n", code, address, static_cast<unsigned long long>(relative_address));
    std::fflush(stderr);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

int main(int argc, char **argv)
{
#if defined(_WIN32)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    SetUnhandledExceptionFilter(report_cli_exception);
    __try {
        return run_startup_parser_test(argc, argv);
    } __except (report_cli_exception(GetExceptionInformation())) {
        return 3;
    }
#else
    return run_startup_parser_test(argc, argv);
#endif
}
