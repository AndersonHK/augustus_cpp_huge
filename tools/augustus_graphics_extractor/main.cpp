#include "assets/augustus_asset_extractor.h"
#include "core/image.h"
#include "scenario/property.h"

#include <filesystem>
#include <iostream>
#include <string>

extern "C" void augustus_graphics_extractor_shims_set_game_root(const char *path);
extern "C" void augustus_graphics_extractor_shims_set_augustus_graphics_path(const char *path);
extern "C" void augustus_graphics_extractor_shims_set_julius_graphics_path(const char *path);
extern "C" void augustus_graphics_extractor_shims_install_renderer(void);

namespace {

struct CliOptions {
    std::string game_root;
    std::string source_graphics;
    std::string output_graphics;
    std::string julius_graphics;
    bool force = true;
    bool write_stamp = false;
    bool extract_julius_first = false;
};

std::string absolute_path(const std::string &path)
{
    if (path.empty()) {
        return {};
    }
    return std::filesystem::absolute(std::filesystem::path(path)).string();
}

std::string append_path(const std::string &base, const char *relative)
{
    return (std::filesystem::path(base) / relative).string();
}

void print_usage()
{
    std::cout
        << "Usage: AugustusGraphicsExtractor [options]\n"
        << "  --game-root <path>          Caesar 3 folder. Defaults to current directory.\n"
        << "  --source-graphics <path>    Source packaged assets. Defaults to <game-root>\\assets\\Graphics.\n"
        << "  --output <path>             Extract output. Defaults to <game-root>\\Mods\\Augustus\\Graphics.\n"
        << "  --julius-graphics <path>    Julius template graphics. Defaults to <game-root>\\Mods\\Julius\\Graphics.\n"
        << "  --extract-julius-first      Run the legacy Julius extractor before Augustus extraction.\n"
        << "  --no-force                  Reuse matching stamped output when possible.\n"
        << "  --stamp                     Write/check the extraction stamp.\n";
}

bool parse_args(int argc, char **argv, CliOptions &options)
{
    options.game_root = std::filesystem::current_path().string();

    for (int index = 1; index < argc; ++index) {
        const std::string arg = argv[index];
        auto require_value = [&](std::string &target) -> bool {
            if (index + 1 >= argc) {
                std::cerr << "Missing value for " << arg << "\n";
                return false;
            }
            target = argv[++index];
            return true;
        };

        if (arg == "--game-root") {
            if (!require_value(options.game_root)) {
                return false;
            }
        } else if (arg == "--source-graphics") {
            if (!require_value(options.source_graphics)) {
                return false;
            }
        } else if (arg == "--output") {
            if (!require_value(options.output_graphics)) {
                return false;
            }
        } else if (arg == "--julius-graphics") {
            if (!require_value(options.julius_graphics)) {
                return false;
            }
        } else if (arg == "--extract-julius-first") {
            options.extract_julius_first = true;
        } else if (arg == "--no-force") {
            options.force = false;
        } else if (arg == "--stamp") {
            options.write_stamp = true;
        } else if (arg == "--help" || arg == "-h" || arg == "/?") {
            print_usage();
            return false;
        } else {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage();
            return false;
        }
    }

    options.game_root = absolute_path(options.game_root);
    if (options.source_graphics.empty()) {
        options.source_graphics = append_path(options.game_root, "assets\\Graphics");
    }
    if (options.output_graphics.empty()) {
        options.output_graphics = append_path(options.game_root, "Mods\\Augustus\\Graphics");
    }
    if (options.julius_graphics.empty()) {
        options.julius_graphics = append_path(options.game_root, "Mods\\Julius\\Graphics");
    }

    options.source_graphics = absolute_path(options.source_graphics);
    options.output_graphics = absolute_path(options.output_graphics);
    options.julius_graphics = absolute_path(options.julius_graphics);
    return true;
}

} // namespace

int main(int argc, char **argv)
{
    CliOptions options;
    if (!parse_args(argc, argv, options)) {
        return 2;
    }

    std::cout << "Game root: " << options.game_root << "\n";
    std::cout << "Source graphics: " << options.source_graphics << "\n";
    std::cout << "Output graphics: " << options.output_graphics << "\n";
    std::cout << "Julius graphics: " << options.julius_graphics << "\n";

    augustus_graphics_extractor_shims_set_game_root(options.game_root.c_str());
    augustus_graphics_extractor_shims_set_augustus_graphics_path(options.output_graphics.c_str());
    augustus_graphics_extractor_shims_set_julius_graphics_path(options.julius_graphics.c_str());

    if (options.extract_julius_first) {
        std::cout << "Extracting Julius graphics first through image_load_climate.\n";
        augustus_graphics_extractor_shims_install_renderer();
        if (!image_load_climate(CLIMATE_CENTRAL, 0, 1, 1, 1)) {
            std::cerr << "Julius graphics extraction failed.\n";
            return 1;
        }
    }

    augustus_asset_extractor_config config = {};
    config.game_root_path = options.game_root.c_str();
    config.source_graphics_path = options.source_graphics.c_str();
    config.output_graphics_path = options.output_graphics.c_str();
    config.julius_graphics_path = options.julius_graphics.c_str();
    config.force = options.force ? 1 : 0;
    config.write_stamp = options.write_stamp ? 1 : 0;

    const int result = augustus_asset_extractor_extract_with_config(&config);
    return result ? 0 : 1;
}
