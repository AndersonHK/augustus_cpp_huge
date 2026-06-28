#include "renderer_seam_geometry_fixture.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

bool RendererSeamGeometryAssertionResult::failed() const
{
    return !failure_detail.empty() ||
        !coverage_no_background ||
        !no_black_gap ||
        !same_surface_delta;
}

void RendererSeamGeometryAssertionResult::record_failure(const std::string &detail)
{
    if (!failure_detail.empty()) {
        failure_detail += " | ";
    }
    failure_detail += detail;
}

RendererSeamGeometryFixtureArtifacts RendererSeamGeometryFixtureArtifacts::for_case(
    const std::filesystem::path &artifacts_root,
    const std::string &case_id)
{
    return {
        artifacts_root / (case_id + ".bmp"),
        artifacts_root / (case_id + ".seam-mask.bmp"),
        artifacts_root / (case_id + ".failure-overlay.bmp"),
    };
}

bool RendererSeamGeometryFixtureRunResult::failed() const
{
    return result == "fail";
}

void RendererSeamGeometryFixtureRunResult::mark_failure(const std::string &detail)
{
    result = "fail";
    if (!failure_detail.empty()) {
        failure_detail += " | ";
    }
    failure_detail += detail;
}

RendererSeamGeometryFixture::SurfaceSize RendererSeamGeometryFixture::surface_size(const RunConfig &config) const
{
    return {
        scale_round_nearest(kSurfaceWidth, config),
        scale_round_nearest(kSurfaceHeight, config),
    };
}

void RendererSeamGeometryFixture::draw_fixture(SDL_Renderer &renderer, const RunConfig &config) const
{
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(&renderer, 255, 0, 255, 255);
    SDL_RenderClear(&renderer);
    SDL_SetRenderDrawColor(&renderer, 48, 132, 67, 255);
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            draw_tile(renderer, tile_x, tile_y, config);
        }
    }
    SDL_RenderPresent(&renderer);
}

void RendererSeamGeometryFixture::draw_seam_mask(SDL_Renderer &renderer, const RunConfig &config) const
{
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(&renderer, 0, 0, 0, 255);
    SDL_RenderClear(&renderer);
    SDL_SetRenderDrawColor(&renderer, 255, 255, 255, 255);
    draw_shared_edge_mask(renderer, config);
    SDL_RenderPresent(&renderer);
}

void RendererSeamGeometryFixture::draw_failure_overlay(SDL_Renderer &renderer, const RunConfig &config) const
{
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(&renderer, 0, 0, 0, 255);
    SDL_RenderClear(&renderer);
    SDL_SetRenderDrawColor(&renderer, 255, 255, 255, 255);
    draw_shared_edge_mask(renderer, config);
    SDL_SetRenderDrawColor(&renderer, 255, 0, 0, 255);
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            const TileBounds bounds = tile_bounds(tile_x, tile_y, config);
            SDL_RenderDrawLine(&renderer, bounds.top.x, bounds.top.y, bounds.right.x, bounds.right.y);
            SDL_RenderDrawLine(&renderer, bounds.right.x, bounds.right.y, bounds.bottom.x, bounds.bottom.y);
            SDL_RenderDrawLine(&renderer, bounds.bottom.x, bounds.bottom.y, bounds.left.x, bounds.left.y);
            SDL_RenderDrawLine(&renderer, bounds.left.x, bounds.left.y, bounds.top.x, bounds.top.y);
        }
    }
    SDL_RenderPresent(&renderer);
}

void RendererSeamGeometryFixture::assert_pixels(
    SDL_Surface &surface,
    RendererSeamGeometryAssertionResult &result,
    const RunConfig &config) const
{
    if (SDL_MUSTLOCK(&surface) && SDL_LockSurface(&surface) != 0) {
        result.record_failure(std::string("Unable to lock geometry fixture surface: ") + SDL_GetError());
        return;
    }

    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            if (tile_x + 1 < kTiles) {
                assert_east_west_shared_edge(surface, result, tile_x, tile_y, config);
            }
            if (tile_y + 1 < kTiles) {
                assert_north_south_shared_edge(surface, result, tile_x, tile_y, config);
            }
        }
    }

    if (SDL_MUSTLOCK(&surface)) {
        SDL_UnlockSurface(&surface);
    }
}

RendererSeamGeometryFixtureRunResult RendererSeamGeometryFixture::run(
    const RendererSeamGeometryFixtureArtifacts &artifacts,
    const RunConfig &config) const
{
    const SurfaceSize size = surface_size(config);
    RendererSeamGeometryFixtureRunResult result = make_initial_run_result(artifacts);

    // TODO(renderer seam): replace this software fixture with a captured city renderer frame once an offscreen
    // city-view draw entry point can be exposed without pulling live gameplay state into this harness.
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        return setup_failure_result(
            artifacts,
            std::string("SDL video initialization failed before a geometry fixture pixel buffer existed: ") +
                SDL_GetError());
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(
        0, size.width, size.height, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!surface || !renderer) {
        result.artifact_status = "fixture_setup_failed";
        result.mark_failure(
            std::string("SDL software surface/renderer creation failed before a geometry fixture pixel buffer existed: ") +
                SDL_GetError());
        if (renderer) {
            SDL_DestroyRenderer(renderer);
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
        SDL_Quit();
        return result;
    }

    draw_fixture(*renderer, config);
    RendererSeamGeometryAssertionResult assertion_result;
    assert_pixels(*surface, assertion_result, config);
    apply_assertion_result(result, assertion_result);
    const int screenshot_saved = save_bmp(
        *surface, result.artifacts.screenshot_path, result, "geometry fixture screenshot");

    int mask_saved = 0;
    int failure_overlay_saved = 0;
    SDL_Surface *mask_surface = SDL_CreateRGBSurfaceWithFormat(
        0, size.width, size.height, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *mask_renderer = mask_surface ? SDL_CreateSoftwareRenderer(mask_surface) : nullptr;
    if (mask_surface && mask_renderer) {
        draw_seam_mask(*mask_renderer, config);
        mask_saved = save_bmp(*mask_surface, result.artifacts.seam_mask_path, result, "geometry fixture seam mask");
    } else {
        result.mark_failure(std::string("Unable to create geometry fixture seam mask surface: ") + SDL_GetError());
    }

    if (result.failed() && mask_surface && mask_renderer) {
        draw_failure_overlay(*mask_renderer, config);
        failure_overlay_saved = save_bmp(
            *mask_surface, result.artifacts.failure_overlay_path, result, "geometry fixture failure overlay");
    }

    update_artifact_status(result, screenshot_saved, mask_saved, failure_overlay_saved);

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

RendererSeamGeometryFixture::PixelCoordinate RendererSeamGeometryFixture::unscaled_tile_top(
    int tile_x,
    int tile_y) const
{
    return {
        kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth,
        kOriginY + (tile_x + tile_y) * kTileHalfHeight,
    };
}

RendererSeamGeometryFixture::PixelCoordinate RendererSeamGeometryFixture::scale_coordinate(
    PixelCoordinate coordinate,
    const RunConfig &config) const
{
    return {
        scale_round_nearest(coordinate.x, config),
        scale_round_nearest(coordinate.y, config),
    };
}

RendererSeamGeometryFixture::TileBounds RendererSeamGeometryFixture::tile_bounds(
    int tile_x,
    int tile_y,
    const RunConfig &config) const
{
    const PixelCoordinate top = unscaled_tile_top(tile_x, tile_y);
    return {
        tile_top(tile_x, tile_y, config),
        scale_coordinate({ top.x + kTileHalfWidth, top.y + kTileHalfHeight }, config),
        scale_coordinate({ top.x, top.y + 2 * kTileHalfHeight }, config),
        scale_coordinate({ top.x - kTileHalfWidth, top.y + kTileHalfHeight }, config),
    };
}

RendererSeamGeometryFixture::PixelCoordinate RendererSeamGeometryFixture::tile_top(
    int tile_x,
    int tile_y,
    const RunConfig &config) const
{
    return scale_coordinate(unscaled_tile_top(tile_x, tile_y), config);
}

void RendererSeamGeometryFixture::draw_tile(
    SDL_Renderer &renderer,
    int tile_x,
    int tile_y,
    const RunConfig &config) const
{
    const TileBounds bounds = tile_bounds(tile_x, tile_y, config);
    for (int y = bounds.top.y; y <= bounds.bottom.y; ++y) {
        const bool top_half = y <= bounds.left.y;
        const int left_x = top_half
            ? interpolate_x_for_y(bounds.top, bounds.left, y)
            : interpolate_x_for_y(bounds.left, bounds.bottom, y);
        const int right_x = top_half
            ? interpolate_x_for_y(bounds.top, bounds.right, y)
            : interpolate_x_for_y(bounds.right, bounds.bottom, y);
        SDL_RenderDrawLine(&renderer, left_x, y, right_x, y);
    }
}

void RendererSeamGeometryFixture::draw_shared_edge_mask(SDL_Renderer &renderer, const RunConfig &config) const
{
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            const TileBounds bounds = tile_bounds(tile_x, tile_y, config);
            if (tile_x + 1 < kTiles) {
                SDL_RenderDrawLine(&renderer, bounds.right.x, bounds.right.y, bounds.bottom.x, bounds.bottom.y);
            }
            if (tile_y + 1 < kTiles) {
                SDL_RenderDrawLine(&renderer, bounds.left.x, bounds.left.y, bounds.bottom.x, bounds.bottom.y);
            }
        }
    }
}

void RendererSeamGeometryFixture::assert_east_west_shared_edge(
    SDL_Surface &surface,
    RendererSeamGeometryAssertionResult &result,
    int tile_x,
    int tile_y,
    const RunConfig &config) const
{
    const TileBounds bounds = tile_bounds(tile_x, tile_y, config);
    for (int edge_y = bounds.right.y + 1; edge_y < bounds.bottom.y; ++edge_y) {
        const int edge_x = interpolate_x_for_y(bounds.right, bounds.bottom, edge_y);
        sample_covered_edge_pixel(
            surface,
            result,
            { edge_x, edge_y },
            "east-west canonical isometric seam");
        sample_same_surface_pair(
            surface,
            result,
            { edge_x - 1, edge_y },
            { edge_x + 1, edge_y },
            "east-west canonical isometric seam");
    }
}

void RendererSeamGeometryFixture::assert_north_south_shared_edge(
    SDL_Surface &surface,
    RendererSeamGeometryAssertionResult &result,
    int tile_x,
    int tile_y,
    const RunConfig &config) const
{
    const TileBounds bounds = tile_bounds(tile_x, tile_y, config);
    for (int edge_y = bounds.left.y + 1; edge_y < bounds.bottom.y; ++edge_y) {
        const int edge_x = interpolate_x_for_y(bounds.left, bounds.bottom, edge_y);
        sample_covered_edge_pixel(
            surface,
            result,
            { edge_x, edge_y },
            "north-south canonical isometric seam");
        sample_same_surface_pair(
            surface,
            result,
            { edge_x + 1, edge_y },
            { edge_x - 1, edge_y },
            "north-south canonical isometric seam");
    }
}

void RendererSeamGeometryFixture::sample_same_surface_pair(
    SDL_Surface &surface,
    RendererSeamGeometryAssertionResult &result,
    PixelCoordinate first,
    PixelCoordinate second,
    const char *seam_name) const
{
    const uint32_t first_pixel = pixel_at(surface, first);
    const uint32_t second_pixel = pixel_at(surface, second);
    ++result.sampled_pixels;
    if (is_background_pixel(*surface.format, first_pixel) || is_background_pixel(*surface.format, second_pixel)) {
        result.coverage_no_background = 0;
        result.record_failure(std::string("background pixel on ") + seam_name);
    }
    if (is_opaque_black_pixel(*surface.format, first_pixel) || is_opaque_black_pixel(*surface.format, second_pixel)) {
        result.no_black_gap = 0;
        result.record_failure(std::string("black pixel on ") + seam_name);
    }
    if (max_rgb_delta(*surface.format, first_pixel, second_pixel) > kMaxNearestDelta) {
        result.same_surface_delta = 0;
        result.record_failure(std::string("RGB delta exceeded nearest-filter threshold on ") + seam_name);
    }
}

void RendererSeamGeometryFixture::sample_covered_edge_pixel(
    SDL_Surface &surface,
    RendererSeamGeometryAssertionResult &result,
    PixelCoordinate coordinate,
    const char *seam_name) const
{
    const uint32_t pixel = pixel_at(surface, coordinate);
    ++result.sampled_pixels;
    if (is_background_pixel(*surface.format, pixel)) {
        result.coverage_no_background = 0;
        result.record_failure(std::string("background pixel on ") + seam_name);
    }
    if (is_opaque_black_pixel(*surface.format, pixel)) {
        result.no_black_gap = 0;
        result.record_failure(std::string("black pixel on ") + seam_name);
    }
}

RendererSeamGeometryFixtureRunResult RendererSeamGeometryFixture::make_initial_run_result(
    const RendererSeamGeometryFixtureArtifacts &artifacts) const
{
    RendererSeamGeometryFixtureRunResult result;
    result.artifacts = artifacts;
    return result;
}

RendererSeamGeometryFixtureRunResult RendererSeamGeometryFixture::setup_failure_result(
    const RendererSeamGeometryFixtureArtifacts &artifacts,
    const std::string &detail) const
{
    RendererSeamGeometryFixtureRunResult result = make_initial_run_result(artifacts);
    result.artifact_status = "fixture_setup_failed";
    result.mark_failure(detail);
    return result;
}

void RendererSeamGeometryFixture::apply_assertion_result(
    RendererSeamGeometryFixtureRunResult &result,
    const RendererSeamGeometryAssertionResult &assertion_result) const
{
    result.coverage_no_background = assertion_result.coverage_no_background;
    result.no_black_gap = assertion_result.no_black_gap;
    result.same_surface_delta = assertion_result.same_surface_delta;
    result.sampled_pixels = assertion_result.sampled_pixels;
    if (assertion_result.failed()) {
        result.mark_failure(assertion_result.failure_detail);
    }
}

int RendererSeamGeometryFixture::save_bmp(
    SDL_Surface &surface,
    const std::filesystem::path &path,
    RendererSeamGeometryFixtureRunResult &result,
    const char *label) const
{
    if (SDL_SaveBMP(&surface, path.string().c_str()) == 0) {
        return 1;
    }
    result.mark_failure(std::string("Unable to write ") + label + ": " + SDL_GetError());
    return 0;
}

void RendererSeamGeometryFixture::update_artifact_status(
    RendererSeamGeometryFixtureRunResult &result,
    int screenshot_saved,
    int mask_saved,
    int failure_overlay_saved) const
{
    if (!result.failed()) {
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
}

uint32_t RendererSeamGeometryFixture::pixel_at(const SDL_Surface &surface, PixelCoordinate coordinate) const
{
    const uint8_t *row = static_cast<const uint8_t *>(surface.pixels) + coordinate.y * surface.pitch;
    uint32_t pixel = 0;
    std::memcpy(&pixel, row + coordinate.x * surface.format->BytesPerPixel, surface.format->BytesPerPixel);
    return pixel;
}

int RendererSeamGeometryFixture::scale_round_nearest(int value, const RunConfig &config) const
{
    return round_divide(value * config.city_scale_percent, 100);
}

int RendererSeamGeometryFixture::round_divide(int numerator, int denominator) const
{
    if (denominator < 0) {
        numerator = -numerator;
        denominator = -denominator;
    }
    if (denominator == 0) {
        return numerator;
    }
    const int adjustment = denominator / 2;
    return numerator >= 0 ?
        (numerator + adjustment) / denominator :
        -((-numerator + adjustment) / denominator);
}

int RendererSeamGeometryFixture::interpolate_x_for_y(PixelCoordinate start, PixelCoordinate end, int y) const
{
    const int denominator = end.y - start.y;
    if (denominator == 0) {
        return start.x;
    }
    const int numerator = (end.x - start.x) * (y - start.y);
    return start.x + round_divide(numerator, denominator);
}

int RendererSeamGeometryFixture::max_rgb_delta(SDL_PixelFormat &format, uint32_t left, uint32_t right) const
{
    uint8_t left_r = 0;
    uint8_t left_g = 0;
    uint8_t left_b = 0;
    uint8_t left_a = 0;
    uint8_t right_r = 0;
    uint8_t right_g = 0;
    uint8_t right_b = 0;
    uint8_t right_a = 0;
    SDL_GetRGBA(left, &format, &left_r, &left_g, &left_b, &left_a);
    SDL_GetRGBA(right, &format, &right_r, &right_g, &right_b, &right_a);
    return std::max({
        std::abs(static_cast<int>(left_r) - static_cast<int>(right_r)),
        std::abs(static_cast<int>(left_g) - static_cast<int>(right_g)),
        std::abs(static_cast<int>(left_b) - static_cast<int>(right_b)),
    });
}

bool RendererSeamGeometryFixture::is_background_pixel(SDL_PixelFormat &format, uint32_t pixel) const
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    SDL_GetRGBA(pixel, &format, &r, &g, &b, &a);
    return a == 0 || (r == 255 && g == 0 && b == 255);
}

bool RendererSeamGeometryFixture::is_opaque_black_pixel(SDL_PixelFormat &format, uint32_t pixel) const
{
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 0;
    SDL_GetRGBA(pixel, &format, &r, &g, &b, &a);
    return a != 0 && r == 0 && g == 0 && b == 0;
}
