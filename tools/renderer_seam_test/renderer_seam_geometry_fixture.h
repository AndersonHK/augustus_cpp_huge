#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include <SDL.h>

struct RendererSeamGeometryAssertionResult {
    int coverage_no_background = 1;
    int no_black_gap = 1;
    int same_surface_delta = 1;
    int sampled_pixels = 0;
    std::string failure_detail;

    bool failed() const;
    void record_failure(const std::string &detail);
};

struct RendererSeamGeometryFixtureArtifacts {
    std::filesystem::path screenshot_path;
    std::filesystem::path seam_mask_path;
    std::filesystem::path failure_overlay_path;

    static RendererSeamGeometryFixtureArtifacts for_case(
        const std::filesystem::path &artifacts_root,
        const std::string &case_id);
};

struct RendererSeamGeometryFixtureRunResult {
    std::string result = "pass";
    std::string skip_reason;
    std::string artifact_status = "pixel_buffer_created";
    std::string capture_source = "sdl_canonical_isometric_fixture";
    RendererSeamGeometryFixtureArtifacts artifacts;
    int coverage_no_background = 1;
    int no_black_gap = 1;
    int same_surface_delta = 1;
    int sampled_pixels = 0;
    std::string failure_detail;

    bool failed() const;
    void mark_failure(const std::string &detail);
};

class RendererSeamGeometryFixture {
public:
    struct SurfaceSize {
        int width = 0;
        int height = 0;
    };

    struct RunConfig {
        int city_scale_percent = 100;
    };

    RendererSeamGeometryFixture() = default;

    SurfaceSize surface_size(const RunConfig &config) const;
    void draw_fixture(SDL_Renderer &renderer, const RunConfig &config) const;
    void draw_seam_mask(SDL_Renderer &renderer, const RunConfig &config) const;
    void draw_failure_overlay(SDL_Renderer &renderer, const RunConfig &config) const;
    void assert_pixels(
        SDL_Surface &surface,
        RendererSeamGeometryAssertionResult &result,
        const RunConfig &config) const;
    RendererSeamGeometryFixtureRunResult run(
        const RendererSeamGeometryFixtureArtifacts &artifacts,
        const RunConfig &config) const;

private:
    struct PixelCoordinate {
        int x = 0;
        int y = 0;
    };

    struct TileBounds {
        PixelCoordinate top;
        PixelCoordinate right;
        PixelCoordinate bottom;
        PixelCoordinate left;
    };

    struct SharedEdge {
        PixelCoordinate start;
        PixelCoordinate end;
    };

    static constexpr int kTiles = 8;
    static constexpr int kTileWidth = 60;
    static constexpr int kTileHalfWidth = kTileWidth / 2;
    static constexpr int kTileHalfHeight = 15;
    static constexpr int kOriginX = 32;
    static constexpr int kOriginY = 32;
    static constexpr int kSurfaceWidth = 512;
    static constexpr int kSurfaceHeight = 320;
    static constexpr int kMaxNearestDelta = 1;

    PixelCoordinate unscaled_tile_top(int tile_x, int tile_y) const;
    PixelCoordinate scale_coordinate(PixelCoordinate coordinate, const RunConfig &config) const;
    TileBounds tile_bounds(int tile_x, int tile_y, const RunConfig &config) const;
    PixelCoordinate tile_top(int tile_x, int tile_y, const RunConfig &config) const;

    void draw_tile(SDL_Renderer &renderer, int tile_x, int tile_y, const RunConfig &config) const;
    void draw_shared_edge_mask(SDL_Renderer &renderer, const RunConfig &config) const;

    void assert_east_west_shared_edge(
        SDL_Surface &surface,
        RendererSeamGeometryAssertionResult &result,
        int tile_x,
        int tile_y,
        const RunConfig &config) const;
    void assert_north_south_shared_edge(
        SDL_Surface &surface,
        RendererSeamGeometryAssertionResult &result,
        int tile_x,
        int tile_y,
        const RunConfig &config) const;
    void assert_shared_edge_geometry(
        RendererSeamGeometryAssertionResult &result,
        SharedEdge first,
        SharedEdge second,
        const char *seam_name) const;
    void record_shared_edge_geometry_failure(
        RendererSeamGeometryAssertionResult &result,
        SharedEdge first,
        SharedEdge second,
        const char *seam_name) const;
    void sample_same_surface_pair(
        SDL_Surface &surface,
        RendererSeamGeometryAssertionResult &result,
        PixelCoordinate first,
        PixelCoordinate second,
        const char *seam_name) const;
    void sample_covered_edge_pixel(
        SDL_Surface &surface,
        RendererSeamGeometryAssertionResult &result,
        PixelCoordinate coordinate,
        const char *seam_name) const;

    RendererSeamGeometryFixtureRunResult make_initial_run_result(
        const RendererSeamGeometryFixtureArtifacts &artifacts) const;
    RendererSeamGeometryFixtureRunResult setup_failure_result(
        const RendererSeamGeometryFixtureArtifacts &artifacts,
        const std::string &detail) const;
    void apply_assertion_result(
        RendererSeamGeometryFixtureRunResult &result,
        const RendererSeamGeometryAssertionResult &assertion_result) const;
    int save_bmp(
        SDL_Surface &surface,
        const std::filesystem::path &path,
        RendererSeamGeometryFixtureRunResult &result,
        const char *label) const;
    void update_artifact_status(
        RendererSeamGeometryFixtureRunResult &result,
        int screenshot_saved,
        int mask_saved,
        int failure_overlay_saved) const;

    uint32_t pixel_at(const SDL_Surface &surface, PixelCoordinate coordinate) const;
    int scale_round_nearest(int value, const RunConfig &config) const;
    int round_divide(int numerator, int denominator) const;
    int interpolate_x_for_y(PixelCoordinate start, PixelCoordinate end, int y) const;
    std::string pixel_coordinate_text(PixelCoordinate coordinate) const;
    std::string shared_edge_text(SharedEdge edge) const;
    int max_rgb_delta(SDL_PixelFormat &format, uint32_t left, uint32_t right) const;
    bool is_background_pixel(SDL_PixelFormat &format, uint32_t pixel) const;
    bool is_opaque_black_pixel(SDL_PixelFormat &format, uint32_t pixel) const;
};
