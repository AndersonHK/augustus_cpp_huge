#include "startup/startup_parser.h"

#include "assets/image_group_payload.h"
#include "building/RubbleState.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/type.h"
#include "map/road_aqueduct_rules.h"
#include "map/terrain.h"

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

void print_step(const startup_parser::StartupParseStep &step)
{
    std::cout << "Loading " << step.label << "... ";
    if (step.succeeded) {
        std::cout << "ok";
        if (!step.detail.empty()) {
            std::cout << " (" << step.detail << ")";
        }
        std::cout << "\n";
        return;
    }
    std::cout << "failed\n";
    if (!step.detail.empty()) {
        std::cerr << step.detail << "\n";
    }
}

bool run_startup_parse()
{
    const startup_parser::StartupParseResult result = startup_parser::parse_startup_definitions();
    for (const startup_parser::StartupParseStep &step : result.steps) {
        print_step(step);
    }
    return result.succeeded != 0;
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
        aqueduct->declared_model_size() != 1 ||
        !rubble || !rubble->has_rubble() || !rubble->rubble().is_rubble() || rubble->tool().is_aqueduct() ||
        rubble->declared_model_size() != 1 ||
        !burning_ruin || !burning_ruin->has_rubble() || !burning_ruin->rubble().is_burning() ||
        burning_ruin->declared_model_size() != 1 ||
        burning_ruin->rubble().decay_type != rubble ||
        aqueduct->type() == rubble->type() || aqueduct->type() == burning_ruin->type()) {
        std::cerr << "Rubble repair contract failed: aqueduct/rubble BuildingType XML identities overlap or are incomplete.\n";
        return false;
    }
    if (!farm || !farm->has_composition() ||
        farm->composition().footprint_width() != 3 ||
        farm->composition().footprint_height() != 3 ||
        farm->composition().parts().size() != 5) {
        std::cerr << "Rubble repair contract failed: fruit farm composition is not the expected 3x3/5-part shape.\n";
        return false;
    }
    if (!hippodrome || !hippodrome->has_composition() ||
        hippodrome->composition().footprint_width() != 15 ||
        hippodrome->composition().footprint_height() != 5 ||
        hippodrome->composition().parts().size() != 2) {
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

    const std::filesystem::path game_root = std::filesystem::current_path();
    if (!check_extraction_prerequisites(game_root, executable_directory(argv[0]))) {
        return 1;
    }

    const startup_parser::StartupEnvironment environment = startup_parser::inspect_startup_environment();
    std::cout << "Startup parser test game root: " << environment.game_root << "\n";
    std::cout << "Selected mod stack: ";
    const auto &mods = environment.mod_stack;
    for (size_t i = 0; i < mods.size(); ++i) {
        std::cout << (i ? " -> " : "") << mods[i];
    }
    std::cout << "\nSelected mod path: " << environment.mod_path << "\n";

    if (!run_startup_parse()) {
        std::cerr << "Startup parser test failed.\n";
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
    if (!validate_colosseum_graphics_contract()) {
        return 1;
    }
    if (!validate_figure_owner_contracts()) {
        return 1;
    }

    std::cout << "Startup parser test passed.\n";
    return 0;
}
