#include "game/mod_manager.h"
#include "startup/startup_parser.h"

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
