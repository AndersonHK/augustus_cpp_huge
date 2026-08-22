#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>

#include <SDL.h>

#include "renderer_seam_report_writer.h"

class RendererSeamAssetFixture {
public:
    RendererSeamAssetFixture(std::filesystem::path graphics_root, std::filesystem::path artifacts_root);
    ~RendererSeamAssetFixture();

    RendererSeamAssetFixture(const RendererSeamAssetFixture &) = delete;
    RendererSeamAssetFixture &operator=(const RendererSeamAssetFixture &) = delete;

    bool ready() const;
    const std::string &failure_detail() const;
    RendererSeamMatrixCaseResult run(const RendererSeamMatrixCase &test_case);

private:
    struct ClimateAssets {
        SDL_Surface *terrain = nullptr;
        SDL_Surface *grass = nullptr;
        SDL_Surface *water = nullptr;
    };

    struct TilePoint {
        int x = 0;
        int y = 0;
    };

    std::filesystem::path graphics_root_;
    std::filesystem::path artifacts_root_;
    std::map<std::string, ClimateAssets> climates_;
    std::string failure_detail_;
    bool sdl_initialized_ = false;

    void load_assets();
    bool load_climate(const char *climate);
    SDL_Surface *load_png(const std::filesystem::path &path);
    void free_assets();
    SDL_Texture *create_texture(SDL_Renderer &renderer, SDL_Surface &source, bool atlas_fallback, SDL_Rect &source_rect) const;
    void duplicate_edges(SDL_Surface &surface, const SDL_Rect &content) const;
    void draw_scene(SDL_Renderer &renderer, const RendererSeamMatrixCase &test_case, const ClimateAssets &assets) const;
    void draw_tile(SDL_Renderer &renderer, SDL_Texture &texture, const SDL_Rect *source_rect, int tile_x, int tile_y, int elevation, const RendererSeamMatrixCase &test_case) const;
    void draw_grid(SDL_Renderer &renderer, const RendererSeamMatrixCase &test_case) const;
    void assert_seams(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, const RendererSeamMatrixCase &test_case) const;
    void sample_shared_edge(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, TilePoint start, TilePoint end) const;
    void sample_tile_interiors(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, const RendererSeamMatrixCase &test_case, int elevation) const;
    void record_failure(RendererSeamMatrixCaseResult &result, const std::string &detail) const;
    bool save_failure_artifacts(SDL_Surface &surface, RendererSeamMatrixCaseResult &result) const;
    const ClimateAssets *assets_for(const std::string &climate) const;
    SDL_ScaleMode scale_mode_for(const std::string &filter) const;
    TilePoint tile_top(int tile_x, int tile_y, int elevation, const RendererSeamMatrixCase &test_case) const;
    TilePoint scale_point(TilePoint point, const RendererSeamMatrixCase &test_case) const;
    uint32_t pixel_at(const SDL_Surface &surface, int x, int y) const;
    bool is_background(SDL_PixelFormat &format, uint32_t pixel) const;
    bool is_black(SDL_PixelFormat &format, uint32_t pixel) const;
    int round_scaled(int value, const RendererSeamMatrixCase &test_case) const;
};
