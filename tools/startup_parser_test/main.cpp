#include "building/building_runtime.h"
#include "building/building_type_registry.h"
#include "building/building_type_startup_bridge.h"
#include "building/properties.h"
#include "core/config.h"
#include "figure/figure_type_registry.h"
#include "figure/formation_type.h"
#include "figure/unit_type.h"
#include "game/defines.h"
#include "game/mod_manager.h"
#include "game/resource.h"
#include "translation/translation.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
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

bool has_case_insensitive_extension(const std::filesystem::path &path, const char *extension)
{
    std::string actual = path.extension().string();
    std::string expected = extension ? extension : "";
    std::transform(actual.begin(), actual.end(), actual.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return actual == expected;
}

int count_xml_files(const std::filesystem::path &directory)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        std::cerr << "Missing XML directory: " << directory.string() << "\n";
        return -1;
    }

    int count = 0;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            std::cerr << "Unable to enumerate XML directory: " << directory.string() << "\n";
            return -1;
        }
        if (entry.is_regular_file(error) && has_case_insensitive_extension(entry.path(), ".xml")) {
            ++count;
        }
    }
    return count;
}

bool run_step(const char *label, int (*step)(), const char *failure_reason = nullptr)
{
    std::cout << "Loading " << label << "... ";
    if (step()) {
        std::cout << "ok\n";
        return true;
    }
    std::cout << "failed\n";
    if (failure_reason && *failure_reason) {
        std::cerr << failure_reason << "\n";
    }
    return false;
}

bool load_resources()
{
    const std::filesystem::path resource_path = std::filesystem::path(mod_manager::mod_path()) / "Resources";
    const int expected_resources = count_xml_files(resource_path);
    if (expected_resources <= 0) {
        return false;
    }

    std::cout << "Loading Resources... ";
    resource_init();
    const int loaded_resources = resource_loaded_count();
    if (loaded_resources != expected_resources) {
        std::cout << "failed\n";
        std::cerr << "Loaded " << loaded_resources << " resources, expected "
            << expected_resources << " XML files from " << resource_path.string() << ".\n";
        return false;
    }
    std::cout << "ok (" << loaded_resources << " definitions)\n";
    return true;
}

bool run_startup_parse()
{
    config_load();

    if (!building_type_startup_bridge_validate_mod()) {
        std::cerr << "Selected mod data is missing. Expected BuildingType folder: "
            << building_type_startup_bridge_get_building_type_path() << "\n";
        return false;
    }

    if (!run_step("localization", []() { return lang_load(0); })) {
        return false;
    }

    model_reset();
    if (!load_resources()) {
        return false;
    }
    if (!run_step("game defines", game_defines_load, game_defines_get_failure_reason())) {
        return false;
    }

    building_properties_init();
    figure_type_registry_reset();
    unit_type_registry_reset();
    formation_type_registry_reset();

    if (!run_step("FigureType definitions", figure_type_registry_load, figure_type_registry_get_failure_reason())) {
        return false;
    }
    if (!run_step("UnitType definitions", unit_type_registry_load, unit_type_registry_get_failure_reason())) {
        return false;
    }
    if (!run_step("FormationType definitions", formation_type_registry_load, formation_type_registry_get_failure_reason())) {
        return false;
    }
    if (!run_step("BuildingType definitions", building_type_registry_load)) {
        return false;
    }
    if (!run_step("FigureType building references", figure_type_registry_resolve_building_references,
        figure_type_registry_get_failure_reason())) {
        return false;
    }

    building_runtime_reset();
    return true;
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

    std::cout << "Startup parser test game root: " << std::filesystem::current_path().string() << "\n";
    std::cout << "Selected mod stack: ";
    const auto &mods = mod_manager::mod_names();
    for (size_t i = 0; i < mods.size(); ++i) {
        std::cout << (i ? " -> " : "") << mods[i];
    }
    std::cout << "\nSelected mod path: " << mod_manager::mod_path() << "\n";

    if (!run_startup_parse()) {
        std::cerr << "Startup parser test failed.\n";
        return 1;
    }

    std::cout << "Startup parser test passed.\n";
    return 0;
}
