#include "startup/startup_parser.h"

#include "building/RubbleState.h"
#include "building/building_type_registry_internal.h"

#include <filesystem>
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
    if (farm->placement_width(0) != 3 || farm->placement_height(0) != 3 ||
        hippodrome->placement_width(0) != 15 || hippodrome->placement_height(0) != 5 ||
        hippodrome->placement_width(1) != 5 || hippodrome->placement_height(1) != 15) {
        std::cerr << "Rubble repair contract failed: BuildingType placement dimensions are incorrect.\n";
        return false;
    }

    std::cout << "Validated rubble repair contracts (type-derived dimensions and pointer-backed origin identity).\n";
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

    std::cout << "Startup parser test passed.\n";
    return 0;
}
