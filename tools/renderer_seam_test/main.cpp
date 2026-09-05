#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <stdexcept>
#include <system_error>
#include <unordered_map>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

#include "renderer_seam_asset_fixture.h"
#include "renderer_seam_geometry_fixture.h"
#include "renderer_seam_report_writer.h"

namespace {

struct Options {
    std::filesystem::path game_root;
    std::filesystem::path graphics_root;
    std::filesystem::path artifacts = "out/renderer_seams";
    std::string matrix = "terrain-water";
};

struct ExtractionPrerequisite {
    const char *label;
    const char *relative_path;
};

void print_usage()
{
    std::cout
        << "RendererSeamTest [--game-root <path>] [--graphics-root <path>] [--matrix terrain-water|city-tile-geometry] [--artifacts <path>]\n\n"
        << "Prepares renderer terrain/water seam pixel-check cases and writes machine-readable JSON results.\n"
        << "The city-tile-geometry matrix is asset-free and validates canonical isometric shared-edge masks.\n"
        << "The terrain-water matrix renders climate-specific extracted terrain/water pixels through managed-native\n"
        << "and padded-atlas paths, then requires every matrix case and backend parity check to pass.\n";
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
        if (arg == "--graphics-root") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --graphics-root.\n";
                return -1;
            }
            options.graphics_root = argv[++i];
            continue;
        }
        if (arg == "--matrix") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --matrix.\n";
                return -1;
            }
            options.matrix = argv[++i];
            continue;
        }
        if (arg == "--artifacts") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --artifacts.\n";
                return -1;
            }
            options.artifacts = argv[++i];
            continue;
        }
        std::cerr << "Unsupported argument: " << arg << "\n";
        print_usage();
        return -1;
    }

    if (options.matrix != "terrain-water" && options.matrix != "city-tile-geometry") {
        std::cerr << "Unsupported matrix: " << options.matrix << "\n";
        print_usage();
        return -1;
    }
    if (options.game_root.empty() && options.matrix == "terrain-water") {
        std::error_code error;
        options.game_root = std::filesystem::current_path(error);
        if (error) {
            std::cerr << "Unable to resolve current directory: " << error.message() << "\n";
            return -1;
        }
    }
    if (options.graphics_root.empty() && options.matrix == "terrain-water") {
        options.graphics_root = options.game_root / "Mods" / "Julius" / "Graphics";
    }
    return 1;
}

std::string lowercase_ascii(std::string text)
{
    for (char &ch : text) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return text;
}

bool directory_contains_file(const std::filesystem::path &directory, const char *name)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        return false;
    }

    const std::string expected = lowercase_ascii(name ? name : "");
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            return false;
        }
        if (entry.is_regular_file(error) && lowercase_ascii(entry.path().filename().string()) == expected) {
            return true;
        }
    }
    return false;
}

bool is_valid_game_root(const std::filesystem::path &game_root)
{
    return directory_contains_file(game_root, "c3.exe") &&
        (directory_contains_file(game_root, "c3.eng") || directory_contains_file(game_root, "c3_mm.eng"));
}

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
    const std::filesystem::path &graphics_root,
    const std::filesystem::path &tool_directory)
{
    const std::array<ExtractionPrerequisite, 4> prerequisites = {{
        { "Julius central graphics extraction", "Mods/Julius/Graphics.legacy_extract.stamp" },
        { "Julius northern graphics extraction", "Mods/Julius/Graphics.legacy_extract_northern.stamp" },
        { "Julius desert graphics extraction", "Mods/Julius/Graphics.legacy_extract_desert.stamp" },
        { "Augustus graphics extraction", "Mods/Augustus/Graphics.graphics_extract.stamp" },
    }};

    std::vector<std::filesystem::path> missing;
    for (const ExtractionPrerequisite &prerequisite : prerequisites) {
        const std::filesystem::path stamp_path = game_root / std::filesystem::path(prerequisite.relative_path);
        std::error_code error;
        if (!std::filesystem::is_regular_file(stamp_path, error)) {
            missing.push_back(stamp_path);
        }
    }

    for (const char *climate : { "central", "northern", "desert" }) {
        for (const char *group : { "Flat_Tile", "Grass_1", "Water" }) {
            const std::filesystem::path snapshot = graphics_root / "Renderer_Seam_Climate" / climate / "Terrain_Maps" / group / "Image_0000.png";
            std::error_code error;
            if (!std::filesystem::is_regular_file(snapshot, error)) missing.push_back(snapshot);
        }
    }

    if (missing.empty()) {
        return true;
    }

    std::cerr
        << "\nRendererSeamTest prerequisite failure: generated graphics extraction stamps are missing.\n"
        << "Renderer seam pixels depend on generated terrain/water art. Do not add parser/XML fallbacks for this failure;\n"
        << "run the extractors first, then rerun this test.\n\n";

    for (const std::filesystem::path &path : missing) {
        std::cerr << "Missing stamp: " << path.string() << "\n";
    }

    const std::filesystem::path julius_extractor = tool_directory / "JuliusGraphicsExtractor.exe";
    const std::filesystem::path augustus_extractor = tool_directory / "AugustusGraphicsExtractor.exe";
    std::cerr
        << "\nRequired order:\n"
        << "  " << quoted(julius_extractor) << " --game-root " << quoted(game_root) << " --output " << quoted(graphics_root) << "\n"
        << "  " << quoted(augustus_extractor) << " --game-root " << quoted(game_root)
        << " --no-force --stamp\n"
        << "  " << quoted(tool_directory / "RendererSeamTest.exe") << " --game-root "
        << quoted(game_root) << " --matrix terrain-water --artifacts out\\renderer_seams\n";
    return false;
}

std::vector<RendererSeamMatrixCase> build_terrain_water_matrix()
{
    const std::array<const char *, 1> mods = { "Vespasian" };
    const std::array<const char *, 3> climates = { "central", "northern", "desert" };
    const std::array<const char *, 2> backends = { "atlas-fallback", "managed-native" };
    const std::array<const char *, 3> filters = { "nearest", "linear", "best" };
    const std::array<int, 8> scales = { 100, 125, 150, 175, 200, 250, 275, 300 };
    const std::array<bool, 2> grids = { false, true };
    const std::array<const char *, 4> orientations = { "north", "east", "south", "west" };
    const std::array<const char *, 4> scenes = {
        "solid-terrain-8x8",
        "water-8x8",
        "terrain-water-cross",
        "mixed-elevation-smoke",
    };

    std::vector<RendererSeamMatrixCase> cases;
    int next_id = 1;
    for (const char *mod : mods) {
        for (const char *climate : climates) {
            for (const char *backend : backends) {
                for (const char *filter : filters) {
                    for (const int scale : scales) {
                        for (const bool grid : grids) {
                            for (const char *orientation : orientations) {
                                for (const char *scene : scenes) {
                                    RendererSeamMatrixCase test_case;
                                    std::ostringstream id;
                                    id << "terrain-water-" << std::setw(5) << std::setfill('0') << next_id++;
                                    test_case.id = id.str();
                                    test_case.mod = mod;
                                    test_case.climate = climate;
                                    test_case.backend = backend;
                                    test_case.scale_filter = filter;
                                    test_case.city_scale_percent = scale;
                                    test_case.grid = grid;
                                    test_case.orientation = orientation;
                                    test_case.scene = scene;
                                    cases.push_back(test_case);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    return cases;
}

std::vector<RendererSeamMatrixCase> build_city_tile_geometry_matrix()
{
    const std::array<int, 8> scales = { 100, 125, 150, 175, 200, 250, 275, 300 };

    std::vector<RendererSeamMatrixCase> cases;
    int next_id = 1;
    for (const int scale : scales) {
        RendererSeamMatrixCase test_case;
        std::ostringstream id;
        id << "city-tile-geometry-" << std::setw(5) << std::setfill('0') << next_id++;
        test_case.id = id.str();
        test_case.mod = "asset-free-fixture";
        test_case.climate = "none";
        test_case.backend = "software-fixture";
        test_case.scale_filter = "nearest";
        test_case.city_scale_percent = scale;
        test_case.grid = false;
        test_case.orientation = "north";
        test_case.scene = "solid-isometric-terrain-8x8";
        cases.push_back(test_case);
    }
    return cases;
}

bool is_software_fixture_case(const RendererSeamMatrixCase &test_case)
{
    return test_case.backend == "software-fixture" &&
        test_case.scene == "solid-isometric-terrain-8x8";
}

RendererSeamMatrixCaseResult run_software_fixture_case(
    const Options &options,
    const RendererSeamMatrixCase &test_case)
{
    RendererSeamGeometryFixture fixture;
    RendererSeamGeometryFixture::RunConfig config;
    config.city_scale_percent = test_case.city_scale_percent;
    const RendererSeamGeometryFixtureArtifacts artifacts =
        RendererSeamGeometryFixtureArtifacts::for_case(options.artifacts, test_case.id);
    const RendererSeamGeometryFixtureRunResult fixture_result = fixture.run(artifacts, config);
    return RendererSeamMatrixCaseResult::from_geometry_fixture(fixture_result);
}

std::string backend_parity_key(const RendererSeamMatrixCase &test_case)
{
    std::ostringstream key;
    key << test_case.mod << '|' << test_case.climate << '|' << test_case.scale_filter << '|' << test_case.city_scale_percent << '|'
        << test_case.grid << '|' << test_case.orientation << '|' << test_case.scene;
    return key.str();
}

std::string grid_parity_key(const RendererSeamMatrixCase &test_case)
{
    std::ostringstream key;
    key << test_case.mod << '|' << test_case.climate << '|' << test_case.backend << '|' << test_case.scale_filter << '|'
        << test_case.city_scale_percent << '|' << test_case.orientation << '|' << test_case.scene;
    return key.str();
}

void validate_backend_parity(const std::vector<RendererSeamMatrixCase> &cases, std::vector<RendererSeamMatrixCaseResult> &results)
{
    std::unordered_map<std::string, uint64_t> atlas_signatures;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        if (results[index].result != "pass") continue;
        const std::string key = backend_parity_key(cases[index]);
        if (cases[index].backend == "atlas-fallback") {
            atlas_signatures[key] = results[index].seam_signature;
            continue;
        }
        const auto found = atlas_signatures.find(key);
        if (found == atlas_signatures.end() || found->second == results[index].seam_signature) continue;
        results[index].result = "fail";
        results[index].same_surface_delta = 0;
        results[index].backend_parity = 0;
        results[index].failure_detail = "managed-native and padded-atlas seam signatures differ";
    }
}

void validate_grid_is_overlay_only(const std::vector<RendererSeamMatrixCase> &cases, std::vector<RendererSeamMatrixCaseResult> &results)
{
    std::unordered_map<std::string, uint64_t> grid_off_signatures;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        if (results[index].result != "pass") continue;
        const std::string key = grid_parity_key(cases[index]);
        if (!cases[index].grid) {
            grid_off_signatures[key] = results[index].interior_signature;
            continue;
        }
        const auto found = grid_off_signatures.find(key);
        if (found == grid_off_signatures.end() || found->second == results[index].interior_signature) continue;
        results[index].result = "fail";
        results[index].grid_overlay_only = 0;
        results[index].failure_detail = "grid rendering changed pixels in tile interiors outside the grid-line mask";
    }
}

std::vector<RendererSeamMatrixCaseResult> run_matrix_cases(
    const Options &options,
    const std::vector<RendererSeamMatrixCase> &cases)
{
    std::vector<RendererSeamMatrixCaseResult> results;
    results.reserve(cases.size());
    std::unique_ptr<RendererSeamAssetFixture> asset_fixture;
    if (options.matrix == "terrain-water") asset_fixture = std::make_unique<RendererSeamAssetFixture>(options.graphics_root, options.artifacts);
    for (const RendererSeamMatrixCase &test_case : cases) {
        if (is_software_fixture_case(test_case)) {
            results.push_back(run_software_fixture_case(options, test_case));
        } else {
            results.push_back(asset_fixture->run(test_case));
        }
    }
    validate_grid_is_overlay_only(cases, results);
    validate_backend_parity(cases, results);
    return results;
}

} // namespace

int run_renderer_seam_test(int argc, char **argv)
{
    Options options;
    const int parse_result = parse_options(argc, argv, options);
    if (parse_result <= 0) {
        return parse_result == 0 ? 0 : 2;
    }

    std::error_code error;
    if (options.matrix == "terrain-water") {
        options.game_root = std::filesystem::absolute(options.game_root, error);
        if (error || !is_valid_game_root(options.game_root)) {
            std::cerr << "RendererSeamTest requires an actual Caesar 3 game root with c3.exe and c3.eng or c3_mm.eng: "
                << options.game_root.string() << "\n";
            return 1;
        }

        options.graphics_root = std::filesystem::absolute(options.graphics_root, error);
        if (error) {
            std::cerr << "Unable to resolve graphics root: " << error.message() << "\n";
            return 1;
        }
        if (!check_extraction_prerequisites(options.game_root, options.graphics_root, executable_directory(argv[0]))) {
            return 1;
        }
    } else if (!options.game_root.empty()) {
        options.game_root = std::filesystem::absolute(options.game_root, error);
        if (error) {
            std::cerr << "Unable to resolve game root: " << error.message() << "\n";
            return 1;
        }
    }

    try {
        options.artifacts = std::filesystem::absolute(options.artifacts, error);
        if (error) {
            throw std::runtime_error("Unable to resolve artifact folder: " + error.message());
        }
        std::vector<RendererSeamMatrixCase> cases;
        if (options.matrix == "city-tile-geometry") {
            cases = build_city_tile_geometry_matrix();
        } else {
            cases = build_terrain_water_matrix();
        }
        const RendererSeamReportWriter report_writer({ options.matrix, options.game_root, options.artifacts });
        const std::filesystem::path result_path = report_writer.prepare_artifacts();
        const std::vector<RendererSeamMatrixCaseResult> results = run_matrix_cases(options, cases);
        const RendererSeamReportSummary summary = report_writer.summarize(cases, results);
        report_writer.write(result_path, cases, results);

        std::cout << "Renderer seam test game root: " << options.game_root.string() << "\n";
        std::cout << "Matrix: " << options.matrix << "\n";
        std::cout << "Total cases: " << summary.total_cases << "\n";
        std::cout << "Passed: " << summary.passed << "\n";
        std::cout << "Failed: " << summary.failed << "\n";
        std::cout << "Expected skips: " << summary.expected_skipped << "\n";
        std::cout << "JSON results: " << result_path.string() << "\n";
        return summary.failed ? 1 : 0;
    } catch (const std::exception &exc) {
        std::cerr << "RendererSeamTest failed: " << exc.what() << "\n";
        return 1;
    }
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
    std::fprintf(stderr, "RendererSeamTest terminated after Windows exception 0x%08lX at %p (RVA 0x%llX).\n", code, address, static_cast<unsigned long long>(relative_address));
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
        return run_renderer_seam_test(argc, argv);
    } __except (report_cli_exception(GetExceptionInformation())) {
        return 3;
    }
#else
    return run_renderer_seam_test(argc, argv);
#endif
}
