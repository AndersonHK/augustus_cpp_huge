#include "startup/startup_parser_abi.h"

#include "assets/image_group_payload.h"
#include "building/BuildingGraphicsState.h"
#include "building/RubbleState.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/type.h"
#include "map/access_ramp_rules.h"
#include "map/road_aqueduct_rules.h"
#include "map/terrain.h"
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
#include "plague_runtime_contract_test.h"
#include "production_method_layering_test.h"
#include "resource_layering_test.h"
#include "religion_layering_test.h"
#include "storage_type_registry_layering_test.h"
#include "water_access_type_layering_test.h"
#include "unit_type_registry_layering_test.h"
#include "formation_type_registry_layering_test.h"
#include "figure_type_registry_layering_test.h"

#include <filesystem>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <system_error>

namespace {

struct Options {
    std::filesystem::path game_root;
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

void print_usage()
{
    std::cout
        << "StartupParserTest [--game-root <path>]\n\n"
        << "Runs the headless startup XML/registry parse sequence from the installed game folder.\n";
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
        std::cout << "\n";
        return;
    }
    std::cout << "failed\n";
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
    const figure_type_registry_impl::FigureTypeProfile *beggar =
        figure_type_registry_impl::profile_for(FIGURE_BEGGAR, "unemployment_wanderer");
    const figure_type_registry_impl::FigureTypeProfile *homeless =
        figure_type_registry_impl::profile_for(FIGURE_HOMELESS, "legacy");
    const figure_type_registry_impl::FigureTypeProfile *fishing_boat =
        figure_type_registry_impl::profile_for(FIGURE_FISHING_BOAT, "fish_fetch");
    const figure_type_registry_impl::FigureTypeProfile *prefect =
        figure_type_registry_impl::profile_for(FIGURE_PREFECT, "service");
    if (!beggar || beggar->requires_owner()) {
        std::cerr << "Figure owner contract failed: beggars must be ownerless after spawning.\n";
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
    std::cout << "Validated ownerless and required-owner FigureType profile contracts.\n";
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

bool validate_native_soldier_corpse_graphics()
{
    static const char *const prefixes[] = { "auxinf", "auxarch" };
    for (const char *prefix : prefixes) {
        for (int frame = 1; frame <= 8; frame++) {
            char group[64];
            char image[32];
            std::snprintf(image, sizeof(image), "%s_death_%02d", prefix, frame);
            std::snprintf(group, sizeof(group), "Warriors\\%s", image);
            if (!image_group_payload_load(group)) {
                std::cerr << "Soldier corpse graphics contract failed: could not load " << group << ".\n";
                return false;
            }
            const ImageGroupPayload *payload = image_group_payload_get(group);
            const ImageGroupEntry *entry = payload ? payload->entry_for(image) : nullptr;
            const RuntimeDrawSlice *slice = entry ? entry->footprint() : nullptr;
            if (!slice || !slice->is_valid() || slice->width > 64 || slice->height > 64) {
                std::cerr << "Soldier corpse graphics contract failed: " << group
                    << " did not materialize a valid figure-sized footprint.\n";
                return false;
            }
        }
    }
    std::cout << "Validated every native auxiliary soldier corpse frame.\n";
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

} // namespace

int main(int argc, char **argv)
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
        !validate_building_graphics_generation_contract()) {
        return 1;
    }

    const std::filesystem::path game_root = std::filesystem::current_path();
    if (!check_extraction_prerequisites(game_root, executable_directory(argv[0]))) {
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
    std::cout << "\nSelected mod path: " << environment.mod_path << "\n";

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
    if (!validate_entertainer_graphics_contract()) {
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
    if (!validate_figure_type_registry_layering_contract(std::cerr)) {
        return 1;
    }
    if (!validate_building_identity_contract()) {
        return 1;
    }
    if (!validate_plague_runtime_contract(std::cerr)) {
        return 1;
    }
    if (!validate_foundation_rotation_contract() ||
        !validate_composition_rotation_contracts(std::cerr) ||
        !validate_parsed_composition_contracts(std::cerr) ||
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
    if (!validate_gate_terrain_foundation_contract()) {
        return 1;
    }
    if (!validate_native_gatehouse_bridge_graphics_contract(std::cerr)) {
        return 1;
    }
    if (!validate_authoritative_building_graphics_contract(std::cerr)) {
        return 1;
    }
    std::cout << "Validated authoritative native graphics for forts, towers, arches, vacant lots, and rubble.\n";
    if (!validate_native_statue_orientation_graphics_contract(std::cerr)) {
        return 1;
    }
    if (!validate_native_hippodrome_graphics_contract(std::cerr)) {
        return 1;
    }
    if (!validate_native_overlay_summary_graphics_contract(std::cerr)) {
        return 1;
    }
    if (!validate_dock_native_orientation_contract()) {
        return 1;
    }
    if (!validate_simple_native_building_graphics_contract()) {
        return 1;
    }
    if (!validate_native_storage_and_fort_base_graphics_contract()) {
        return 1;
    }
    if (!validate_colosseum_graphics_contract()) {
        return 1;
    }
    if (!validate_figure_owner_contracts()) {
        return 1;
    }
    if (!validate_native_soldier_corpse_graphics()) {
        return 1;
    }

    std::cout << "Startup parser test passed.\n";
    return 0;
}
