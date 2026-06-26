#include "assets/augustus_asset_extractor.h"
#include "core/image.h"
#include "scenario/property.h"

#include <filesystem>
#include <iostream>
#include <string>

void augustus_graphics_extractor_shims_set_game_root(const char *path);
void augustus_graphics_extractor_shims_set_augustus_graphics_path(const char *path);
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
            } else if (arg == "--source-graphics") {
                if (!read_value(argc, argv, index, arg, source_graphics_)) {
                    return false;
                }
            } else if (arg == "--output") {
                if (!read_value(argc, argv, index, arg, output_graphics_)) {
                    return false;
                }
            } else if (arg == "--julius-graphics") {
                if (!read_value(argc, argv, index, arg, julius_graphics_)) {
                    return false;
                }
            } else if (arg == "--extract-julius-first") {
                extract_julius_first_ = true;
            } else if (arg == "--no-force") {
                force_ = false;
            } else if (arg == "--stamp") {
                write_stamp_ = true;
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
        if (source_graphics_.empty()) {
            source_graphics_ = append_path(game_root_, "assets\\Graphics");
        }
        if (output_graphics_.empty()) {
            output_graphics_ = append_path(game_root_, "Mods\\Augustus\\Graphics");
        }
        if (julius_graphics_.empty()) {
            julius_graphics_ = append_path(game_root_, "Mods\\Julius\\Graphics");
        }

        source_graphics_ = absolute_path(source_graphics_);
        output_graphics_ = absolute_path(output_graphics_);
        julius_graphics_ = absolute_path(julius_graphics_);
        return true;
    }

    bool show_help() const
    {
        return show_help_;
    }

    bool extract_julius_first() const
    {
        return extract_julius_first_;
    }

    const std::string &game_root() const
    {
        return game_root_;
    }

    const std::string &source_graphics() const
    {
        return source_graphics_;
    }

    const std::string &output_graphics() const
    {
        return output_graphics_;
    }

    const std::string &julius_graphics() const
    {
        return julius_graphics_;
    }

    vespasian::graphics::extraction::ExtractorPaths extractor_paths() const
    {
        return vespasian::graphics::extraction::ExtractorPaths(
            game_root_,
            source_graphics_,
            output_graphics_,
            julius_graphics_);
    }

    vespasian::graphics::extraction::ExtractorOptions extractor_options() const
    {
        return vespasian::graphics::extraction::ExtractorOptions(
            force_ && !extract_julius_first_,
            write_stamp_ || extract_julius_first_);
    }

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
            << "Usage: AugustusGraphicsExtractor [options]\n"
            << "  --game-root <path>          Caesar 3 folder. Defaults to current directory.\n"
            << "  --source-graphics <path>    Source packaged assets. Defaults to <game-root>\\assets\\Graphics.\n"
            << "  --output <path>             Extract output. Defaults to <game-root>\\Mods\\Augustus\\Graphics.\n"
            << "  --julius-graphics <path>    Julius template graphics. Defaults to <game-root>\\Mods\\Julius\\Graphics.\n"
            << "  --extract-julius-first      Simulate runtime: run Julius, then Augustus bootstrap.\n"
            << "  --no-force                  Reuse matching stamped output when possible.\n"
            << "  --stamp                     Write/check the extraction stamp.\n";
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
    std::string source_graphics_;
    std::string output_graphics_;
    std::string julius_graphics_;
    bool force_ = true;
    bool write_stamp_ = false;
    bool extract_julius_first_ = false;
    bool show_help_ = false;
};

} // namespace

static bool extract_julius_climate(int climate_id, const char *label)
{
    std::cout << "Extracting Julius " << label << " graphics through image_load_climate.\n";
    if (image_load_climate(climate_id, 0, 1, 1, 1)) {
        return true;
    }

    std::cerr << "Julius " << label << " graphics extraction failed.\n";
    return false;
}

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
    std::cout << "Source graphics: " << cli.source_graphics() << "\n";
    std::cout << "Output graphics: " << cli.output_graphics() << "\n";
    std::cout << "Julius graphics: " << cli.julius_graphics() << "\n";

    augustus_graphics_extractor_shims_set_game_root(cli.game_root().c_str());
    augustus_graphics_extractor_shims_set_augustus_graphics_path(cli.output_graphics().c_str());
    augustus_graphics_extractor_shims_set_julius_graphics_path(cli.julius_graphics().c_str());

    if (cli.extract_julius_first()) {
        augustus_graphics_extractor_shims_install_renderer();
        if (!extract_julius_climate(CLIMATE_CENTRAL, "central") ||
            !extract_julius_climate(CLIMATE_NORTHERN, "northern") ||
            !extract_julius_climate(CLIMATE_DESERT, "desert")) {
            return 1;
        }
    }

    vespasian::graphics::extraction::AugustusExtractor extractor;
    return extractor.extract(cli.extractor_paths(), cli.extractor_options()).succeeded() ? 0 : 1;
}
