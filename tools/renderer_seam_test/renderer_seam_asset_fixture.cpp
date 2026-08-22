#include "renderer_seam_asset_fixture.h"

#include "platform/render_destination_geometry.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <sstream>
#include <utility>

#include <objbase.h>
#include <wincodec.h>
#include <wrl/client.h>

namespace {

constexpr int kTiles = 8;
constexpr int kTileWidth = 60;
constexpr int kTileHalfWidth = 30;
constexpr int kTileHeight = 30;
constexpr int kTileHalfHeight = 15;
constexpr int kImageWidth = 58;
constexpr int kImageHeight = 30;
constexpr int kOriginX = 32;
constexpr int kOriginY = 32;
constexpr int kSurfaceWidth = 512;
constexpr int kSurfaceHeight = 320;
constexpr uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr uint64_t kFnvPrime = 1099511628211ULL;

std::string hresult_text(HRESULT result)
{
    std::ostringstream out;
    out << "HRESULT 0x" << std::hex << static_cast<unsigned long>(result);
    return out.str();
}

} // namespace

RendererSeamAssetFixture::RendererSeamAssetFixture(std::filesystem::path graphics_root, std::filesystem::path artifacts_root)
    : graphics_root_(std::move(graphics_root)), artifacts_root_(std::move(artifacts_root))
{
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        failure_detail_ = std::string("SDL video initialization failed: ") + SDL_GetError();
        return;
    }
    sdl_initialized_ = true;
    load_assets();
}

RendererSeamAssetFixture::~RendererSeamAssetFixture()
{
    free_assets();
    if (sdl_initialized_) {
        SDL_Quit();
    }
}

bool RendererSeamAssetFixture::ready() const
{
    return failure_detail_.empty();
}

const std::string &RendererSeamAssetFixture::failure_detail() const
{
    return failure_detail_;
}

RendererSeamMatrixCaseResult RendererSeamAssetFixture::run(const RendererSeamMatrixCase &test_case)
{
    RendererSeamMatrixCaseResult result;
    result.result = "pass";
    result.artifact_status = "pixel_buffer_validated";
    result.capture_source = "sdl_extracted_climate_terrain_water_fixture";
    result.screenshot_path = artifacts_root_ / (test_case.id + ".bmp");
    result.seam_mask_path = artifacts_root_ / (test_case.id + ".seam-mask.bmp");
    result.failure_overlay_path = artifacts_root_ / (test_case.id + ".failure-overlay.bmp");
    result.coverage_no_background = 1;
    result.no_black_gap = 1;
    result.same_surface_delta = 1;
    result.grid_overlay_only = 1;
    result.no_grid_side_effect_gap = 1;
    result.backend_parity = 1;

    const ClimateAssets *assets = assets_for(test_case.climate);
    if (!ready() || !assets) {
        record_failure(result, ready() ? "No extracted climate assets were loaded for " + test_case.climate : failure_detail_);
        result.artifact_status = "fixture_setup_failed";
        return result;
    }

    const int width = round_scaled(kSurfaceWidth, test_case);
    const int height = round_scaled(kSurfaceHeight, test_case);
    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, width, height, 32, SDL_PIXELFORMAT_ARGB8888);
    SDL_Renderer *renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    if (!surface || !renderer) {
        record_failure(result, std::string("Unable to create extracted-asset seam pixel buffer: ") + SDL_GetError());
        result.artifact_status = "fixture_setup_failed";
        if (renderer) SDL_DestroyRenderer(renderer);
        if (surface) SDL_FreeSurface(surface);
        return result;
    }

    draw_scene(*renderer, test_case, *assets);
    assert_seams(*surface, result, test_case);
    if (result.result == "fail") {
        save_failure_artifacts(*surface, result);
    }
    SDL_DestroyRenderer(renderer);
    SDL_FreeSurface(surface);
    return result;
}

void RendererSeamAssetFixture::load_assets()
{
    for (const char *climate : { "central", "northern", "desert" }) {
        if (!load_climate(climate)) {
            return;
        }
    }
}

bool RendererSeamAssetFixture::load_climate(const char *climate)
{
    const std::filesystem::path root = graphics_root_ / "Renderer_Seam_Climate" / climate / "Terrain_Maps";
    ClimateAssets assets;
    assets.terrain = load_png(root / "Flat_Tile" / "Image_0000.png");
    assets.grass = load_png(root / "Grass_1" / "Image_0000.png");
    assets.water = load_png(root / "Water" / "Image_0000.png");
    if (!assets.terrain || !assets.grass || !assets.water) {
        if (assets.terrain) SDL_FreeSurface(assets.terrain);
        if (assets.grass) SDL_FreeSurface(assets.grass);
        if (assets.water) SDL_FreeSurface(assets.water);
        return false;
    }
    climates_.emplace(climate, assets);
    return true;
}

SDL_Surface *RendererSeamAssetFixture::load_png(const std::filesystem::path &path)
{
    using Microsoft::WRL::ComPtr;
    const HRESULT initialize_result = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(initialize_result) && initialize_result != RPC_E_CHANGED_MODE) {
        failure_detail_ = "Unable to initialize COM for PNG decoding: " + hresult_text(initialize_result);
        return nullptr;
    }

    ComPtr<IWICImagingFactory> factory;
    HRESULT result = CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    ComPtr<IWICBitmapDecoder> decoder;
    if (SUCCEEDED(result)) result = factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder);
    ComPtr<IWICBitmapFrameDecode> frame;
    if (SUCCEEDED(result)) result = decoder->GetFrame(0, &frame);
    ComPtr<IWICFormatConverter> converter;
    if (SUCCEEDED(result)) result = factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(result)) result = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);
    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(result)) result = converter->GetSize(&width, &height);
    if (FAILED(result) || width != kImageWidth || height != kImageHeight) {
        failure_detail_ = "Unable to decode expected 58x30 renderer seam PNG " + path.string() + ": " + hresult_text(result);
        return nullptr;
    }

    SDL_Surface *surface = SDL_CreateRGBSurfaceWithFormat(0, static_cast<int>(width), static_cast<int>(height), 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        failure_detail_ = std::string("Unable to allocate decoded renderer seam surface: ") + SDL_GetError();
        return nullptr;
    }
    result = converter->CopyPixels(nullptr, static_cast<UINT>(surface->pitch), static_cast<UINT>(surface->pitch * surface->h), static_cast<BYTE *>(surface->pixels));
    if (FAILED(result)) {
        failure_detail_ = "Unable to copy decoded renderer seam PNG pixels " + path.string() + ": " + hresult_text(result);
        SDL_FreeSurface(surface);
        return nullptr;
    }
    return surface;
}

void RendererSeamAssetFixture::free_assets()
{
    for (auto &[climate, assets] : climates_) {
        if (assets.terrain) SDL_FreeSurface(assets.terrain);
        if (assets.grass) SDL_FreeSurface(assets.grass);
        if (assets.water) SDL_FreeSurface(assets.water);
    }
    climates_.clear();
}

SDL_Texture *RendererSeamAssetFixture::create_texture(SDL_Renderer &renderer, SDL_Surface &source, bool atlas_fallback, SDL_Rect &source_rect) const
{
    source_rect = { 0, 0, source.w, source.h };
    if (!atlas_fallback) {
        return SDL_CreateTextureFromSurface(&renderer, &source);
    }

    SDL_Surface *atlas = SDL_CreateRGBSurfaceWithFormat(0, source.w + 2, source.h + 2, 32, SDL_PIXELFORMAT_RGBA32);
    if (!atlas) {
        return nullptr;
    }
    SDL_FillRect(atlas, nullptr, SDL_MapRGBA(atlas->format, 0, 0, 0, 0));
    source_rect = { 1, 1, source.w, source.h };
    SDL_BlitSurface(&source, nullptr, atlas, &source_rect);
    duplicate_edges(*atlas, source_rect);
    SDL_Texture *texture = SDL_CreateTextureFromSurface(&renderer, atlas);
    SDL_FreeSurface(atlas);
    return texture;
}

void RendererSeamAssetFixture::duplicate_edges(SDL_Surface &surface, const SDL_Rect &content) const
{
    const int bytes = surface.format->BytesPerPixel;
    auto *pixels = static_cast<uint8_t *>(surface.pixels);
    for (int y = content.y; y < content.y + content.h; ++y) {
        uint8_t *row = pixels + y * surface.pitch;
        std::memcpy(row + (content.x - 1) * bytes, row + content.x * bytes, bytes);
        std::memcpy(row + (content.x + content.w) * bytes, row + (content.x + content.w - 1) * bytes, bytes);
    }
    std::memcpy(pixels + (content.y - 1) * surface.pitch, pixels + content.y * surface.pitch, surface.pitch);
    std::memcpy(pixels + (content.y + content.h) * surface.pitch, pixels + (content.y + content.h - 1) * surface.pitch, surface.pitch);
}

void RendererSeamAssetFixture::draw_scene(SDL_Renderer &renderer, const RendererSeamMatrixCase &test_case, const ClimateAssets &assets) const
{
    const bool atlas_fallback = test_case.backend == "atlas-fallback";
    SDL_Rect terrain_source;
    SDL_Rect grass_source;
    SDL_Rect water_source;
    SDL_Texture *terrain = create_texture(renderer, *assets.terrain, atlas_fallback, terrain_source);
    SDL_Texture *grass = create_texture(renderer, *assets.grass, atlas_fallback, grass_source);
    SDL_Texture *water = create_texture(renderer, *assets.water, atlas_fallback, water_source);
    if (!terrain || !grass || !water) {
        if (terrain) SDL_DestroyTexture(terrain);
        if (grass) SDL_DestroyTexture(grass);
        if (water) SDL_DestroyTexture(water);
        return;
    }
    const SDL_ScaleMode scale_mode = scale_mode_for(test_case.scale_filter);
    SDL_SetTextureScaleMode(terrain, scale_mode);
    SDL_SetTextureScaleMode(grass, scale_mode);
    SDL_SetTextureScaleMode(water, scale_mode);
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_NONE);
    SDL_SetRenderDrawColor(&renderer, 255, 0, 255, 255);
    SDL_RenderClear(&renderer);

    for (int diagonal = 0; diagonal <= 2 * (kTiles - 1); ++diagonal) {
        for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
            const int tile_x = diagonal - tile_y;
            if (tile_x < 0 || tile_x >= kTiles) continue;
            int scene_x = tile_x;
            int scene_y = tile_y;
            if (test_case.orientation == "east") {
                scene_x = kTiles - 1 - tile_y;
                scene_y = tile_x;
            } else if (test_case.orientation == "south") {
                scene_x = kTiles - 1 - tile_x;
                scene_y = kTiles - 1 - tile_y;
            } else if (test_case.orientation == "west") {
                scene_x = tile_y;
                scene_y = kTiles - 1 - tile_x;
            }
            SDL_Texture *selected = terrain;
            SDL_Rect *selected_source = &terrain_source;
            if (test_case.scene == "water-8x8") {
                selected = water;
                selected_source = &water_source;
            } else if (test_case.scene == "terrain-water-cross" && (scene_x == scene_y || scene_x + scene_y == kTiles - 1)) {
                selected = water;
                selected_source = &water_source;
            } else if (test_case.scene == "mixed-elevation-smoke" && ((scene_x + scene_y) & 1)) {
                selected = grass;
                selected_source = &grass_source;
            }
            const int elevation = test_case.scene == "mixed-elevation-smoke" ? 4 : 0;
            draw_tile(renderer, *selected, selected_source, tile_x, tile_y, elevation, test_case);
        }
    }
    if (test_case.grid) draw_grid(renderer, test_case);
    SDL_RenderPresent(&renderer);
    SDL_DestroyTexture(terrain);
    SDL_DestroyTexture(grass);
    SDL_DestroyTexture(water);
}

void RendererSeamAssetFixture::draw_tile(SDL_Renderer &renderer, SDL_Texture &texture, const SDL_Rect *source_rect, int tile_x, int tile_y, int elevation, const RendererSeamMatrixCase &test_case) const
{
    const int logical_left = kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth - kImageWidth / 2;
    const int logical_top = kOriginY + (tile_x + tile_y) * kTileHalfHeight - elevation;
    const float scale = test_case.city_scale_percent / 100.0f;
    const render_destination_rect rounded = render_shared_city_tile_destination({ logical_left * scale, logical_top * scale, kImageWidth * scale, kImageHeight * scale });
    SDL_Rect destination = { static_cast<int>(rounded.x), static_cast<int>(rounded.y), static_cast<int>(rounded.width), static_cast<int>(rounded.height) };
    SDL_RenderCopy(&renderer, &texture, source_rect, &destination);
}

void RendererSeamAssetFixture::draw_grid(SDL_Renderer &renderer, const RendererSeamMatrixCase &test_case) const
{
    const int elevation = test_case.scene == "mixed-elevation-smoke" ? 4 : 0;
    SDL_SetRenderDrawBlendMode(&renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(&renderer, 238, 238, 238, 180);
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            const TilePoint top = tile_top(tile_x, tile_y, elevation, test_case);
            const TilePoint right = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth + kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHalfHeight - elevation }, test_case);
            const TilePoint bottom = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHeight - elevation }, test_case);
            const TilePoint left = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth - kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHalfHeight - elevation }, test_case);
            SDL_RenderDrawLine(&renderer, top.x, top.y, right.x, right.y);
            SDL_RenderDrawLine(&renderer, right.x, right.y, bottom.x, bottom.y);
            SDL_RenderDrawLine(&renderer, bottom.x, bottom.y, left.x, left.y);
            SDL_RenderDrawLine(&renderer, left.x, left.y, top.x, top.y);
        }
    }
}

void RendererSeamAssetFixture::assert_seams(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, const RendererSeamMatrixCase &test_case) const
{
    result.seam_signature = kFnvOffset;
    const int elevation = test_case.scene == "mixed-elevation-smoke" ? 4 : 0;
    if (SDL_MUSTLOCK(&surface) && SDL_LockSurface(&surface) != 0) {
        record_failure(result, std::string("Unable to lock extracted-asset seam surface: ") + SDL_GetError());
        return;
    }
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            const TilePoint right = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth + kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHalfHeight - elevation }, test_case);
            const TilePoint bottom = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHeight - elevation }, test_case);
            const TilePoint left = scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth - kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight + kTileHalfHeight - elevation }, test_case);
            if (tile_x + 1 < kTiles) sample_shared_edge(surface, result, right, bottom);
            if (tile_y + 1 < kTiles) sample_shared_edge(surface, result, left, bottom);
        }
    }
    sample_tile_interiors(surface, result, test_case, elevation);
    if (SDL_MUSTLOCK(&surface)) SDL_UnlockSurface(&surface);
}

void RendererSeamAssetFixture::sample_shared_edge(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, TilePoint start, TilePoint end) const
{
    const int steps = std::max(std::abs(end.x - start.x), std::abs(end.y - start.y));
    for (int step = 1; step < steps; ++step) {
        const int center_x = start.x + (end.x - start.x) * step / steps;
        const int center_y = start.y + (end.y - start.y) * step / steps;
        std::array<uint32_t, 3> samples = { pixel_at(surface, center_x - 1, center_y), pixel_at(surface, center_x, center_y), pixel_at(surface, center_x + 1, center_y) };
        int background_count = 0;
        for (uint32_t pixel : samples) {
            ++result.sampled_pixels;
            result.seam_signature = (result.seam_signature ^ pixel) * kFnvPrime;
            if (is_background(*surface.format, pixel)) ++background_count;
            if (is_black(*surface.format, pixel)) {
                result.no_black_gap = 0;
                record_failure(result, "opaque black pixel found on a terrain/water shared edge");
            }
        }
        if (background_count == static_cast<int>(samples.size())) {
            result.coverage_no_background = 0;
            record_failure(result, "uncovered background span found on a terrain/water shared edge");
        }
    }
}

void RendererSeamAssetFixture::sample_tile_interiors(SDL_Surface &surface, RendererSeamMatrixCaseResult &result, const RendererSeamMatrixCase &test_case, int elevation) const
{
    result.interior_signature = kFnvOffset;
    for (int tile_y = 0; tile_y < kTiles; ++tile_y) {
        for (int tile_x = 0; tile_x < kTiles; ++tile_x) {
            const TilePoint top = tile_top(tile_x, tile_y, elevation, test_case);
            const TilePoint center = { top.x, top.y + round_scaled(kTileHalfHeight, test_case) };
            for (int offset_y = -2; offset_y <= 2; ++offset_y) {
                for (int offset_x = -2; offset_x <= 2; ++offset_x) {
                    const uint32_t pixel = pixel_at(surface, center.x + offset_x, center.y + offset_y);
                    result.interior_signature = (result.interior_signature ^ pixel) * kFnvPrime;
                }
            }
        }
    }
}

void RendererSeamAssetFixture::record_failure(RendererSeamMatrixCaseResult &result, const std::string &detail) const
{
    result.result = "fail";
    if (result.failure_detail.find(detail) != std::string::npos) return;
    if (!result.failure_detail.empty()) result.failure_detail += " | ";
    result.failure_detail += detail;
}

bool RendererSeamAssetFixture::save_failure_artifacts(SDL_Surface &surface, RendererSeamMatrixCaseResult &result) const
{
    if (SDL_SaveBMP(&surface, result.screenshot_path.string().c_str()) == 0) {
        result.artifact_status = "failure_screenshot_created";
        return true;
    }
    result.artifact_status = "failure_artifact_write_failed";
    record_failure(result, std::string("Unable to save extracted-asset seam failure screenshot: ") + SDL_GetError());
    return false;
}

const RendererSeamAssetFixture::ClimateAssets *RendererSeamAssetFixture::assets_for(const std::string &climate) const
{
    const auto found = climates_.find(climate);
    return found == climates_.end() ? nullptr : &found->second;
}

SDL_ScaleMode RendererSeamAssetFixture::scale_mode_for(const std::string &filter) const
{
    if (filter == "linear") return SDL_ScaleModeLinear;
    if (filter == "best") return SDL_ScaleModeBest;
    return SDL_ScaleModeNearest;
}

RendererSeamAssetFixture::TilePoint RendererSeamAssetFixture::tile_top(int tile_x, int tile_y, int elevation, const RendererSeamMatrixCase &test_case) const
{
    return scale_point({ kOriginX + (kTiles - 1 + tile_x - tile_y) * kTileHalfWidth, kOriginY + (tile_x + tile_y) * kTileHalfHeight - elevation }, test_case);
}

RendererSeamAssetFixture::TilePoint RendererSeamAssetFixture::scale_point(TilePoint point, const RendererSeamMatrixCase &test_case) const
{
    return { round_scaled(point.x, test_case), round_scaled(point.y, test_case) };
}

uint32_t RendererSeamAssetFixture::pixel_at(const SDL_Surface &surface, int x, int y) const
{
    if (x < 0 || y < 0 || x >= surface.w || y >= surface.h) return 0;
    const uint8_t *row = static_cast<const uint8_t *>(surface.pixels) + y * surface.pitch;
    uint32_t pixel = 0;
    std::memcpy(&pixel, row + x * surface.format->BytesPerPixel, surface.format->BytesPerPixel);
    return pixel;
}

bool RendererSeamAssetFixture::is_background(SDL_PixelFormat &format, uint32_t pixel) const
{
    uint8_t r = 0, g = 0, b = 0, a = 0;
    SDL_GetRGBA(pixel, &format, &r, &g, &b, &a);
    return a == 0 || (r == 255 && g == 0 && b == 255);
}

bool RendererSeamAssetFixture::is_black(SDL_PixelFormat &format, uint32_t pixel) const
{
    uint8_t r = 0, g = 0, b = 0, a = 0;
    SDL_GetRGBA(pixel, &format, &r, &g, &b, &a);
    return a != 0 && r == 0 && g == 0 && b == 0;
}

int RendererSeamAssetFixture::round_scaled(int value, const RendererSeamMatrixCase &test_case) const
{
    return (value * test_case.city_scale_percent + 50) / 100;
}
