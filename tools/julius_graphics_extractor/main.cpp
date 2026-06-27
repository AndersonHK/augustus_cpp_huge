#include "core/image.h"
#include "scenario/property.h"

#include <filesystem>
#include <iostream>
#include <string>

void augustus_graphics_extractor_shims_set_game_root(const char *path);
void augustus_graphics_extractor_shims_set_julius_graphics_path(const char *path);
void augustus_graphics_extractor_shims_install_renderer(void);

namespace {

class HarnessCli {
public:
    bool parse(int argc, char **argv)
    {
        game_root_ = std::filesystem::current_path().string();

        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--game-root") {
                if (!read_value(argc, argv, index, arg, game_root_)) {
                    return false;
                }
            } else if (arg == "--output") {
                if (!read_value(argc, argv, index, arg, output_graphics_)) {
                    return false;
                }
            } else if (arg == "--help" || arg == "-h" || arg == "/?") {
                show_help_ = true;
                print_usage();
                return true;
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                print_usage();
                return false;
            }
        }

        game_root_ = absolute_path(game_root_);
        if (output_graphics_.empty()) {
            output_graphics_ = append_path(game_root_, "Mods\\Julius\\Graphics");
        }
        output_graphics_ = absolute_path(output_graphics_);
        return true;
    }

    bool show_help() const { return show_help_; }
    const std::string &game_root() const { return game_root_; }
    const std::string &output_graphics() const { return output_graphics_; }

private:
    static std::string absolute_path(const std::string &path)
    {
        if (path.empty()) {
            return {};
        }
        return std::filesystem::absolute(std::filesystem::path(path)).string();
    }

    static std::string append_path(const std::string &base, const char *relative)
    {
        return (std::filesystem::path(base) / relative).string();
    }

    static void print_usage()
    {
        std::cout
            << "Usage: JuliusGraphicsExtractor [options]\n"
            << "  --game-root <path>       Caesar 3 folder. Defaults to current directory.\n"
            << "  --output <path>          Extract output. Defaults to <game-root>\\Mods\\Julius\\Graphics.\n";
    }

    static bool read_value(int argc, char **argv, int &index, const std::string &arg, std::string &target)
    {
        if (index + 1 >= argc) {
            std::cerr << "Missing value for " << arg << "\n";
            return false;
        }
        target = argv[++index];
        return true;
    }

    std::string game_root_;
    std::string output_graphics_;
    bool show_help_ = false;
};

bool extract_climate(int climate_id, const char *label)
{
    std::cout << "Extracting Julius " << label << " graphics.\n";
    if (image_load_climate(climate_id, 0, 1, 1, 1)) {
        return true;
    }

    std::cerr << "Julius " << label << " graphics extraction failed.\n";
    return false;
}

} // namespace

int main(int argc, char **argv)
{
    HarnessCli cli;
    if (!cli.parse(argc, argv)) {
        return 2;
    }
    if (cli.show_help()) {
        return 0;
    }

    std::cout << "Game root: " << cli.game_root() << "\n";
    std::cout << "Output graphics: " << cli.output_graphics() << "\n";

    augustus_graphics_extractor_shims_set_game_root(cli.game_root().c_str());
    augustus_graphics_extractor_shims_set_julius_graphics_path(cli.output_graphics().c_str());
    augustus_graphics_extractor_shims_install_renderer();

    return extract_climate(CLIMATE_CENTRAL, "central") &&
            extract_climate(CLIMATE_NORTHERN, "northern") &&
            extract_climate(CLIMATE_DESERT, "desert") ? 0 : 1;
}
