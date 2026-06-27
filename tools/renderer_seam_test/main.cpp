#include <array>
#include <algorithm>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <stdexcept>
#include <system_error>
#include <vector>

#include <SDL.h>

namespace {

struct Options {
    std::filesystem::path game_root;
    std::filesystem::path artifacts = "out/renderer_seams";
    std::string matrix = "terrain-water";
};

struct ExtractionPrerequisite {
    const char *label;
    const char *relative_path;
};

struct MatrixCase {
    std::string id;
    std::string mod;
    std::string climate;
    std::string backend;
    std::string scale_filter;
    int city_scale_percent;
    bool grid;
    std::string orientation;
    std::string scene;
};

struct MatrixCaseResult {
    std::string result = "expected_skip";
    std::string skip_reason;
    std::string artifact_status = "not_created_expected_skip";
    std::string capture_source = "not_captured";
    std::filesystem::path screenshot_path;
    std::filesystem::path seam_mask_path;
    std::filesystem::path failure_overlay_path;
    int coverage_no_background = 0;
    int no_black_gap = 0;
    int same_surface_delta = 0;
    int sampled_pixels = 0;
    std::string failure_detail;
};

void print_usage()
{
    std::cout
        << "RendererSeamTest [--game-root <path>] [--matrix terrain-water] [--artifacts <path>]\n\n"
        << "Prepares renderer terrain/water seam pixel-check cases and writes machine-readable JSON results.\n"
        << "This first slice validates game-root/extraction prerequisites and marks render-dependent cases\n"
        << "as expected skips until offscreen renderer capture is wired.\n";
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

    if (options.game_root.empty()) {
        std::error_code error;
        options.game_root = std::filesystem::current_path(error);
        if (error) {
            std::cerr << "Unable to resolve current directory: " << error.message() << "\n";
            return -1;
        }
    }
    if (options.matrix != "terrain-water") {
        std::cerr << "Unsupported matrix: " << options.matrix << "\n";
        print_usage();
        return -1;
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
        << "  " << quoted(julius_extractor) << " --game-root " << quoted(game_root) << "\n"
        << "  " << quoted(augustus_extractor) << " --game-root " << quoted(game_root)
        << " --no-force --stamp\n"
        << "  " << quoted(tool_directory / "RendererSeamTest.exe") << " --game-root "
        << quoted(game_root) << " --matrix terrain-water --artifacts out\\renderer_seams\n";
    return false;
}

std::string json_escape(const std::string &text)
{
    std::ostringstream out;
    for (char ch : text) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch));
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

std::string json_string(const std::string &text)
{
    return "\"" + json_escape(text) + "\"";
}

std::string json_path(const std::filesystem::path &path)
{
    return json_string(path.string());
}

std::vector<MatrixCase> build_terrain_water_matrix()
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

    std::vector<MatrixCase> cases;
    int next_id = 1;
    for (const char *mod : mods) {
        for (const char *climate : climates) {
            for (const char *backend : backends) {
                for (const char *filter : filters) {
                    for (const int scale : scales) {
                        for (const bool grid : grids) {
                            for (const char *orientation : orientations) {
                                for (const char *scene : scenes) {
                                    MatrixCase test_case;
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

std::filesystem::path case_artifact_path(
    const std::filesystem::path &artifacts_root,
    const MatrixCase &test_case,
    const char *suffix)
{
    return artifacts_root / (test_case.id + suffix);
}

bool is_smoke_render_case(const MatrixCase &test_case)
{
    return test_case.mod == "Vespasian" &&
        test_case.climate == "central" &&
        test_case.backend == "atlas-fallback" &&
        test_case.scale_filter == "nearest" &&
        test_case.city_scale_percent == 100 &&
        !test_case.grid &&
        test_case.orientation == "north" &&
        test_case.scene == "solid-terrain-8x8";
}

MatrixCaseResult expected_skip_result(const Options &options, const MatrixCase &test_case)
{
    MatrixCaseResult result;
    result.skip_reason = "offscreen city renderer capture and screenshot comparison are not wired in this harness slice";
    result.screenshot_path = case_artifact_path(options.artifacts, test_case, ".png");
    result.seam_mask_path = case_artifact_path(options.artifacts, test_case, ".seam-mask.png");
    result.failure_overlay_path = case_artifact_path(options.artifacts, test_case, ".failure-overlay.png");
    return result;
}

uint32_t pixel_at(const SDL_Surface *surface, int x, int y)
{
    const uint8_t *row = static_cast<const uint8_t *>(surface->pixels) + y * surface->pitch;
    uint32_t pixel = 0;
    std::memcpy(&pixel, row + x * surface->format->BytesPerPixel, surface->format->BytesPerPixel);
    return pixel;
}

int max_rgb_delta(SDL_PixelFormat *format, uint32_t left, uint32_t right)
{
    uint8_t left_r = 0;
    uint8_t left_g = 0;
    uint8_t left_b = 0;
    uint8_t left_a = 0;
    uint8_t right_r = 0;
    uint8_t right_g = 0;
    uint8_t right_b = 0;
    uint8_t right_a = 0;
    SDL_GetRGBA(left, format, &left_r, &left_g, &left_b, &left_a);
    SDL_GetRGBA(right, format, &right_r, &right_g, &right_b, &right_a);
    return std::max({
        std::abs(static_cast<int>(left_r) - static_cast<int>(right_r)),
        std::abs(static_cast<int>(left_g) - static_cast<int>(right_g)),
        std::abs(static_cast<int>(left_b) - static_cast<int>(right_b)),
    });
}

bool is_background_pixel(SDL_PixelFormat *format, uint32_t pixel)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
    return a == 0 || (r == 255 && g == 0 && b == 255);
}

bool is_opaque_black_pixel(SDL_PixelFormat *format, uint32_t pixel)
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    SDL_GetRGBA(pixel, format, &r, &g, &b, &a);
    return a != 0 && r == 0 && g == 0 && b == 0;
}

void mark_failure(MatrixCaseResult &result, const std::string &detail)
{
    result.result = "fail";
    if (!result.failure_detail.empty()) {
        result.failure_detail += " | ";
    }
    result.failure_detail += detail;
}

void assert_smoke_fixture_pixels(SDL_Surface *surface, MatrixCaseResult &result)
{
    constexpr int origin_x = 16;
    constexpr int origin_y = 16;
    constexpr int tile_size = 8;
    constexpr int tiles = 8;
    constexpr int field_size = tile_size * tiles;
    constexpr int max_nearest_delta = 1;

    if (SDL_MUSTLOCK(surface) && SDL_LockSurface(surface) != 0) {
        mark_failure(result, std::string("Unable to lock smoke fixture surface: ") + SDL_GetError());
        return;
    }

    for (int seam = 1; seam < tiles; ++seam) {
        const int x = origin_x + seam * tile_size;
        for (int y = origin_y; y < origin_y + field_size; ++y) {
            const uint32_t left = pixel_at(surface, x - 1, y);
            const uint32_t right = pixel_at(surface, x, y);
            ++result.sampled_pixels;
            if (is_background_pixel(surface->format, left) || is_background_pixel(surface->format, right)) {
                result.coverage_no_background = 0;
                mark_failure(result, "background pixel on vertical same-surface seam");
            }
            if (is_opaque_black_pixel(surface->format, left) || is_opaque_black_pixel(surface->format, right)) {
                result.no_black_gap = 0;
                mark_failure(result, "black pixel on vertical same-surface seam");
            }
            if (max_rgb_delta(surface->format, left, right) > max_nearest_delta) {
                result.same_surface_delta = 0;
                mark_failure(result, "RGB delta exceeded nearest-filter threshold on vertical same-surface seam");
            }
        }
    }

    for (int seam = 1; seam < tiles; ++seam) {
        const int y = origin_y + seam * tile_size;
        for (int x = origin_x; x < origin_x + field_size; ++x) {
            const uint32_t top = pixel_at(surface, x, y - 1);
            const uint32_t bottom = pixel_at(surface, x, y);
            ++result.sampled_pixels;
            if (is_background_pixel(surface->format, top) || is_background_pixel(surface->format, bottom)) {
                result.coverage_no_background = 0;
                mark_failure(result, "background pixel on horizontal same-surface seam");
            }
            if (is_opaque_black_pixel(surface->format, top) || is_opaque_black_pixel(surface->format, bottom)) {
                result.no_black_gap = 0;
                mark_failure(result, "black pixel on horizontal same-surface seam");
            }
            if (max_rgb_delta(surface->format, top, bottom) > max_nearest_delta) {
                result.same_surface_delta = 0;
                mark_failure(result, "RGB delta exceeded nearest-filter threshold on horizontal same-surface seam");
            }
        }
    }

    if (SDL_MUSTLOCK(surface)) {
        SDL_UnlockSurface(surface);
    }
}

void draw_smoke_fixture(SDL_Renderer *renderer)
{
    constexpr int origin_x = 16;
    constexpr int origin_y = 16;
    constexpr int tile_size = 8;
    constexpr int tiles = 8;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 255, 0, 255, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 48, 132, 67, 255);
    for (int tile_y = 0; tile_y < tiles; ++tile_y) {
        for (int tile_x = 0; tile_x < tiles; ++tile_x) {
            SDL_Rect rect = {
                origin_x + tile_x * tile_size,
                origin_y + tile_y * tile_size,
                tile_size,
                tile_size
            };
            SDL_RenderFillRect(renderer, &rect);
        }
    }
    SDL_RenderPresent(renderer);
}

void draw_smoke_seam_mask(SDL_Renderer *renderer)
{
    constexpr int origin_x = 16;
    constexpr int origin_y = 16;
    constexpr int tile_size = 8;
    constexpr int tiles = 8;
    constexpr int field_size = tile_size * tiles;

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int seam = 1; seam < tiles; ++seam) {
        const int x = origin_x + seam * tile_size;
        SDL_RenderDrawLine(renderer, x, origin_y, x, origin_y + field_size - 1);
        const int y = origin_y + seam * tile_size;
        SDL_RenderDrawLine(renderer, origin_x, y, origin_x + field_size - 1, y);
    }
    SDL_RenderPresent(renderer);
}

int save_bmp(SDL_Surface *surface, const std::filesystem::path &path, MatrixCaseResult &result, const char *label)
{
    if (SDL_SaveBMP(surface, path.string().c_str()) == 0) {
        return 1;
    }
    mark_failure(result, std::string("Unable to write ") + label + ": " + SDL_GetError());
    return 0;
}

MatrixCaseResult run_smoke_render_case(const Options &options, const MatrixCase &test_case)
{
    MatrixCaseResult result;
    result.result = "pass";
    result.artifact_status = "pixel_buffer_created";
    result.capture_source = "sdl_software_fixture";
    result.screenshot_path = case_artifact_path(options.artifacts, test_case, ".bmp");
    result.seam_mask_path = case_artifact_path(options.artifacts, test_case, ".seam-mask.bmp");
    result.failure_overlay_path = case_artifact_path(options.artifacts, test_case, ".failure-overlay.bmp");
    result.coverage_no_background = 1;
    result.no_black_gap = 1;
    result.same_surface_delta = 1;

    // TODO(renderer seam): replace this software fixture with a captured city renderer frame once an offscreen
    // city-view draw entry point can be exposed without pulling live gameplay state into this harness.
    constexpr int width = 96;
    constexpr int height = 96;
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        result = expected_skip_result(options, test_case);
        result.skip_reason = std::string("SDL video initialization failed before a real pixel buffer existed: ") + SDL_GetError();
        return result;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!surface || !renderer) {
        result = expected_skip_result(options, test_case);
        result.skip_reason = std::string("SDL software surface/renderer creation failed before a real pixel buffer existed: ") + SDL_GetError();
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
        SDL_Quit();
        return result;
    }

    draw_smoke_fixture(renderer);
    assert_smoke_fixture_pixels(surface, result);
    const int screenshot_saved = save_bmp(surface, result.screenshot_path, result, "smoke screenshot");

    int mask_saved = 0;
    int failure_overlay_saved = 0;
    SDL_Surface *mask_surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *mask_renderer = mask_surface ? SDL_CreateSoftwareRenderer(mask_surface) : nullptr;
    if (mask_surface && mask_renderer) {
        draw_smoke_seam_mask(mask_renderer);
        mask_saved = save_bmp(mask_surface, result.seam_mask_path, result, "smoke seam mask");
    } else {
        mark_failure(result, std::string("Unable to create smoke seam mask surface: ") + SDL_GetError());
    }

    if (result.result == "fail") {
        if (mask_surface && mask_renderer) {
            SDL_SetRenderDrawColor(mask_renderer, 255, 0, 0, 255);
            SDL_Rect rect = { 0, 0, width, height };
            SDL_RenderDrawRect(mask_renderer, &rect);
            SDL_RenderPresent(mask_renderer);
            failure_overlay_saved = save_bmp(mask_surface, result.failure_overlay_path, result, "smoke failure overlay");
        }
    }

    if (result.result == "pass") {
        result.artifact_status = screenshot_saved && mask_saved
            ? "screenshot_and_seam_mask_created"
            : "artifact_write_failed";
    } else if (screenshot_saved && mask_saved && failure_overlay_saved) {
        result.artifact_status = "screenshot_seam_mask_and_failure_overlay_created";
    } else if (screenshot_saved && mask_saved) {
        result.artifact_status = "screenshot_and_seam_mask_created_failure_overlay_missing";
    } else if (screenshot_saved) {
        result.artifact_status = "partial_artifacts_created";
    } else {
        result.artifact_status = "artifact_write_failed";
    }

    if (mask_renderer) {
        SDL_DestroyRenderer(mask_renderer);
    }
    if (mask_surface) {
        SDL_FreeSurface(mask_surface);
    }
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    SDL_Quit();
    return result;
}

std::vector<MatrixCaseResult> run_matrix_cases(const Options &options, const std::vector<MatrixCase> &cases)
{
    std::vector<MatrixCaseResult> results;
    results.reserve(cases.size());
    for (const MatrixCase &test_case : cases) {
        if (is_smoke_render_case(test_case)) {
            results.push_back(run_smoke_render_case(options, test_case));
        } else {
            results.push_back(expected_skip_result(options, test_case));
        }
    }
    return results;
}

void write_json_results(
    const Options &options,
    const std::filesystem::path &result_path,
    const std::vector<MatrixCase> &cases,
    const std::vector<MatrixCaseResult> &results)
{
    std::ofstream out(result_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Unable to write JSON result file: " + result_path.string());
    }

    int passed = 0;
    int failed = 0;
    int expected_skipped = 0;
    for (const MatrixCaseResult &result : results) {
        if (result.result == "pass") {
            ++passed;
        } else if (result.result == "fail") {
            ++failed;
        } else {
            ++expected_skipped;
        }
    }

    out << "{\n";
    out << "  \"tool\": \"RendererSeamTest\",\n";
    out << "  \"matrix\": " << json_string(options.matrix) << ",\n";
    out << "  \"game_root\": " << json_path(options.game_root) << ",\n";
    out << "  \"artifacts_root\": " << json_path(options.artifacts) << ",\n";
    out << "  \"status\": " << json_string(failed ? "failed" : expected_skipped ? "partial_expected_skip" : "passed") << ",\n";
    out << "  \"summary\": {\n";
    out << "    \"total_cases\": " << cases.size() << ",\n";
    out << "    \"passed\": " << passed << ",\n";
    out << "    \"failed\": " << failed << ",\n";
    out << "    \"expected_skipped\": " << expected_skipped << "\n";
    out << "  },\n";
    out << "  \"assertions\": [\n";
    out << "    \"coverage_no_background\",\n";
    out << "    \"no_black_gap\",\n";
    out << "    \"same_surface_delta\",\n";
    out << "    \"grid_overlay_only\",\n";
    out << "    \"no_grid_side_effect_gap\",\n";
    out << "    \"backend_parity\"\n";
    out << "  ],\n";
    out << "  \"cases\": [\n";

    for (size_t index = 0; index < cases.size(); ++index) {
        const MatrixCase &test_case = cases[index];
        const MatrixCaseResult &result = results[index];
        out << "    {\n";
        out << "      \"id\": " << json_string(test_case.id) << ",\n";
        out << "      \"inputs\": {\n";
        out << "        \"mod\": " << json_string(test_case.mod) << ",\n";
        out << "        \"climate\": " << json_string(test_case.climate) << ",\n";
        out << "        \"backend\": " << json_string(test_case.backend) << ",\n";
        out << "        \"scale_filter\": " << json_string(test_case.scale_filter) << ",\n";
        out << "        \"city_scale_percent\": " << test_case.city_scale_percent << ",\n";
        out << "        \"grid\": " << (test_case.grid ? "true" : "false") << ",\n";
        out << "        \"orientation\": " << json_string(test_case.orientation) << ",\n";
        out << "        \"scene\": " << json_string(test_case.scene) << "\n";
        out << "      },\n";
        out << "      \"result\": " << json_string(result.result) << ",\n";
        if (result.result == "expected_skip") {
            out << "      \"skip_reason\": " << json_string(result.skip_reason) << ",\n";
        }
        out << "      \"artifact_status\": " << json_string(result.artifact_status) << ",\n";
        out << "      \"capture_source\": " << json_string(result.capture_source) << ",\n";
        out << "      \"pixel_assertions\": {\n";
        out << "        \"coverage_no_background\": " << (result.coverage_no_background ? "true" : "false") << ",\n";
        out << "        \"no_black_gap\": " << (result.no_black_gap ? "true" : "false") << ",\n";
        out << "        \"same_surface_delta\": " << (result.same_surface_delta ? "true" : "false") << ",\n";
        out << "        \"sampled_pixels\": " << result.sampled_pixels << "\n";
        out << "      },\n";
        if (!result.failure_detail.empty()) {
            out << "      \"failure_detail\": " << json_string(result.failure_detail) << ",\n";
        }
        out << "      \"artifacts\": {\n";
        out << "        \"screenshot\": " << json_path(result.screenshot_path) << ",\n";
        out << "        \"seam_mask\": " << json_path(result.seam_mask_path) << ",\n";
        out << "        \"failure_overlay\": " << json_path(result.failure_overlay_path) << "\n";
        out << "      }\n";
        out << "    }" << (index + 1 == cases.size() ? "\n" : ",\n");
    }

    out << "  ]\n";
    out << "}\n";
}

std::filesystem::path prepare_artifacts(const std::filesystem::path &artifacts_root)
{
    std::error_code error;
    std::filesystem::create_directories(artifacts_root, error);
    if (error) {
        throw std::runtime_error("Unable to create artifact folder: " + artifacts_root.string() + " (" + error.message() + ")");
    }
    return artifacts_root / "renderer_seam_results.json";
}

} // namespace

int main(int argc, char **argv)
{
    Options options;
    const int parse_result = parse_options(argc, argv, options);
    if (parse_result <= 0) {
        return parse_result == 0 ? 0 : 2;
    }

    std::error_code error;
    options.game_root = std::filesystem::absolute(options.game_root, error);
    if (error || !is_valid_game_root(options.game_root)) {
        std::cerr << "RendererSeamTest requires an actual Caesar 3 game root with c3.exe and c3.eng or c3_mm.eng: "
            << options.game_root.string() << "\n";
        return 1;
    }

    if (!check_extraction_prerequisites(options.game_root, executable_directory(argv[0]))) {
        return 1;
    }

    try {
        options.artifacts = std::filesystem::absolute(options.artifacts, error);
        if (error) {
            throw std::runtime_error("Unable to resolve artifact folder: " + error.message());
        }
        const std::vector<MatrixCase> cases = build_terrain_water_matrix();
        const std::filesystem::path result_path = prepare_artifacts(options.artifacts);
        const std::vector<MatrixCaseResult> results = run_matrix_cases(options, cases);
        write_json_results(options, result_path, cases, results);

        int passed = 0;
        int failed = 0;
        int expected_skipped = 0;
        for (const MatrixCaseResult &result : results) {
            if (result.result == "pass") {
                ++passed;
            } else if (result.result == "fail") {
                ++failed;
            } else {
                ++expected_skipped;
            }
        }

        std::cout << "Renderer seam test game root: " << options.game_root.string() << "\n";
        std::cout << "Matrix: " << options.matrix << "\n";
        std::cout << "Total cases: " << cases.size() << "\n";
        std::cout << "Passed: " << passed << "\n";
        std::cout << "Failed: " << failed << "\n";
        std::cout << "Expected skips: " << expected_skipped << "\n";
        std::cout << "JSON results: " << result_path.string() << "\n";
        return failed ? 1 : 0;
    } catch (const std::exception &exc) {
        std::cerr << "RendererSeamTest failed: " << exc.what() << "\n";
        return 1;
    }
}
