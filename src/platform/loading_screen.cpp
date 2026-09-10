#include "core/Logger.h"
#include "platform/loading_screen.h"
#include "platform/renderer.h"
#include "core/loading_progress.h"
#include "core/png_read.h"
#include "core/file.h"
#include "spng/spng.h"
#include "game/mod_content.h"
#include "graphics/font.h"
#include "core/encoding.h"
#include <SDL_ttf.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
struct Screen {
    SDL_Texture *background = nullptr;
    struct Stage { SDL_Texture *background = nullptr; std::string title; };
    std::map<std::string, Stage> stages;
    std::string stage_id, stage_title;
    TTF_Font *font = nullptr;
    std::string font_path, title, theme;
    SDL_Color accent = {205, 169, 99, 255};
    int font_size = 0;
    bool active = false, cancelled = false;
    Uint32 last_draw = 0;
    std::string last_label;
    std::string capture;
    std::vector<uint8_t> captured_pixels;
    struct Glyph { SDL_Texture *texture; int width, height, advance, x_offset, y_offset; };
    std::map<int, Glyph> glyphs;
    bool legacy_font = false, legacy_font_ready = false, ttf_initialized = false;
} screen;

void receive_glyph(const loading_progress::FontGlyph &glyph)
{
    SDL_Texture *texture = SDL_CreateTexture(platform_renderer_get_sdl(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, glyph.width, glyph.height);
    if (!texture || SDL_UpdateTexture(texture, nullptr, glyph.pixels, glyph.row_width * sizeof(uint32_t))) {
        SDL_DestroyTexture(texture);
        throw std::runtime_error(SDL_GetError());
    }
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    auto existing = screen.glyphs.find(glyph.id);
    if (existing != screen.glyphs.end()) SDL_DestroyTexture(existing->second.texture);
    screen.glyphs[glyph.id] = {texture, glyph.width, glyph.height, glyph.advance, glyph.x_offset, glyph.y_offset};
}

void font_ready()
{
    screen.legacy_font_ready = true;
    screen.last_label.clear();
    loading_progress::font_glyph_observer = nullptr;
    loading_progress::font_ready_observer = nullptr;
}

void release_theme()
{
    for (auto &stage : screen.stages) SDL_DestroyTexture(stage.second.background);
    screen.stages.clear();
    screen.background = nullptr;
    if (screen.font) TTF_CloseFont(screen.font);
    screen.font = nullptr;
    screen.font_size = 0;
}

SDL_Texture *load_background(const std::string &background)
{
    int width = 0, height = 0;
    if (!png_load_from_file(background.c_str(), 0) || !png_get_image_size(&width, &height) || width <= 0 || height <= 0 || width > 8192 || height > 8192) {
        png_unload();
        throw std::runtime_error("Invalid loading background: " + background);
    }
    std::vector<color_t> pixels(static_cast<size_t>(width) * height);
    const bool decoded = png_read(pixels.data(), 0, 0, width, height, 0, 0, width, 0) != 0;
    png_unload();
    if (!decoded) throw std::runtime_error("Cannot decode loading background: " + background);
    SDL_Texture *texture = SDL_CreateTexture(platform_renderer_get_sdl(), SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, width, height);
    if (!texture || SDL_UpdateTexture(texture, nullptr, pixels.data(), width * sizeof(color_t))) {
        SDL_DestroyTexture(texture);
        throw std::runtime_error(SDL_GetError());
    }
    return texture;
}

void select_stage(const char *id)
{
    const auto stage = screen.stages.find(id);
    if (stage == screen.stages.end()) throw std::runtime_error("Missing loading stage " + std::string(id) + " in " + screen.theme);
    screen.stage_id = id;
    screen.stage_title = stage->second.title;
    screen.background = stage->second.background;
    screen.last_label.clear();
}

void select_theme(const char *filename)
{
    if (!filename || screen.theme == filename || !std::filesystem::is_regular_file(mod_content::utf8_path(filename))) return;
    const auto root = mod_content::parse(mod_content::read(mod_content::utf8_path(filename)));
    if (root.name != "loading_screen") throw std::runtime_error("Expected loading_screen root in " + std::string(filename));
    const auto directory = mod_content::utf8_path(filename).parent_path();
    release_theme();
    for (const auto &child : root.children) {
        const auto id = child.attribute("id");
        if (child.name != "stage" || id.empty() || screen.stages.count(id)) throw std::runtime_error("Invalid or duplicate loading stage");
        const auto background = mod_content::path_text((directory / mod_content::utf8_path(child.attribute("background"))).lexically_normal());
        screen.stages.emplace(id, Screen::Stage{load_background(background), child.attribute("title")});
    }
    if (screen.stages.empty()) throw std::runtime_error("Loading screen has no stages");
    screen.title = root.attribute("title");
    screen.legacy_font = root.attribute("font") == "game";
    screen.font_path = screen.legacy_font ? "" : mod_content::path_text((directory / mod_content::utf8_path(root.attribute("font"))).lexically_normal());
    if (!screen.legacy_font && !screen.ttf_initialized) {
        if (TTF_Init()) throw std::runtime_error(TTF_GetError());
        screen.ttf_initialized = true;
    }
    screen.theme = filename;
    const auto accent = root.attribute("accent", "#cda963");
    if (accent.size() != 7 || accent[0] != '#' || accent.find_first_not_of("0123456789abcdefABCDEF", 1) != std::string::npos) throw std::runtime_error("Invalid loading accent");
    const auto rgb = std::stoul(accent.substr(1), nullptr, 16);
    screen.accent = {static_cast<Uint8>(rgb >> 16), static_cast<Uint8>(rgb >> 8), static_cast<Uint8>(rgb), 255};
    select_stage(root.attribute("initial_stage").c_str());
}

void text(SDL_Renderer *renderer, const std::string &value, int center_x, int y, int max_width, SDL_Color color, bool heading = false)
{
    if (screen.legacy_font) {
        if (!screen.legacy_font_ready) return;
        const auto *definition = font_original_definition_for(heading ? FONT_LARGE_PLAIN : FONT_NORMAL_WHITE);
        std::vector<uint8_t> encoded(value.size() * 2 + 1);
        encoding_from_utf8(value.c_str(), encoded.data(), static_cast<int>(encoded.size()));
        struct Letter { const Screen::Glyph *glyph; int x, y; };
        std::vector<Letter> letters;
        int width = 0;
        for (const auto *character = encoded.data(); *character;) {
            int bytes = 1;
            const int id = font_original_letter_id(definition, character, &bytes);
            const auto glyph = screen.glyphs.find(id);
            if (glyph == screen.glyphs.end()) width += definition->space_width;
            else {
                letters.push_back({&glyph->second, width + glyph->second.x_offset, glyph->second.y_offset - definition->image_y_offset(*character, glyph->second.height + glyph->second.y_offset, definition->line_height)});
                width += glyph->second.advance + definition->letter_spacing;
            }
            character += std::max(1, bytes);
        }
        int output_width, output_height;
        SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
        const double scale = std::min(double(max_width) / std::max(1, width), std::max(1.0, output_height / 720.0));
        const int left = center_x - int(width * scale) / 2;
        for (const auto &letter : letters) {
            SDL_Rect rect = {left + int(letter.x * scale), y + int(letter.y * scale), int(letter.glyph->width * scale), int(letter.glyph->height * scale)};
            SDL_SetTextureColorMod(letter.glyph->texture, color.r, color.g, color.b);
            SDL_RenderCopy(renderer, letter.glyph->texture, nullptr, &rect);
        }
        return;
    }
    SDL_Surface *surface = TTF_RenderUTF8_Blended(screen.font, value.c_str(), color);
    if (!surface) throw std::runtime_error(TTF_GetError());
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    const int width = std::min(surface->w, max_width);
    const int height = std::max(1, surface->h * width / std::max(1, surface->w));
    SDL_FreeSurface(surface);
    if (!texture) throw std::runtime_error(SDL_GetError());
    SDL_Rect rect = {center_x - width / 2, y, width, height};
    SDL_RenderCopy(renderer, texture, nullptr, &rect);
    SDL_DestroyTexture(texture);
}

void draw(const char *label, std::size_t completed, std::size_t total)
{
    if (!screen.active) return;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT || (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE)) screen.cancelled = true;
    }
    if (screen.cancelled) throw std::runtime_error("Loading cancelled");
    const Uint32 now = SDL_GetTicks();
    if (screen.capture.empty() && screen.last_label == label && now - screen.last_draw < 33 && (!total || completed < total)) return;
    screen.last_label = label;
    screen.last_draw = now;
    SDL_Renderer *renderer = platform_renderer_get_sdl();
    if (!renderer) return;
    // Restore every SDL state touched here. Engine draw queues and city state
    // are never executed while definitions or a save are incomplete.
    SDL_Texture *target = SDL_GetRenderTarget(renderer);
    SDL_Rect viewport, clip;
    SDL_RenderGetViewport(renderer, &viewport);
    SDL_RenderGetClipRect(renderer, &clip);
    const SDL_bool clipped = SDL_RenderIsClipEnabled(renderer);
    float sx, sy;
    SDL_RenderGetScale(renderer, &sx, &sy);
    Uint8 r, g, b, a;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    SDL_BlendMode blend;
    SDL_GetRenderDrawBlendMode(renderer, &blend);
    struct Restore {
        SDL_Renderer *renderer; SDL_Texture *target; SDL_Rect viewport, clip; SDL_bool clipped;
        float sx, sy; Uint8 r, g, b, a; SDL_BlendMode blend;
        ~Restore() {
            SDL_SetRenderTarget(renderer, target);
            SDL_RenderSetScale(renderer, sx, sy);
            SDL_RenderSetViewport(renderer, &viewport);
            SDL_RenderSetClipRect(renderer, clipped ? &clip : nullptr);
            SDL_SetRenderDrawColor(renderer, r, g, b, a);
            SDL_SetRenderDrawBlendMode(renderer, blend);
        }
    } restore{renderer, target, viewport, clip, clipped, sx, sy, r, g, b, a, blend};
    SDL_SetRenderTarget(renderer, nullptr);
    SDL_RenderSetViewport(renderer, nullptr);
    SDL_RenderSetScale(renderer, 1, 1);
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    int width, height;
    SDL_GetRendererOutputSize(renderer, &width, &height);
    SDL_SetRenderDrawColor(renderer, 22, 18, 14, 255);
    SDL_RenderClear(renderer);
    if (screen.background) {
        int bw, bh;
        SDL_QueryTexture(screen.background, nullptr, nullptr, &bw, &bh);
        const double scale = std::max(double(width) / bw, double(height) / bh);
        SDL_Rect rect = {(width - int(bw * scale)) / 2, (height - int(bh * scale)) / 2, int(std::ceil(bw * scale)), int(std::ceil(bh * scale))};
        SDL_RenderCopy(renderer, screen.background, nullptr, &rect);
    }
    const int unit = std::max(1, height / 540);
    const int bar_width = width * 3 / 5;
    SDL_Rect bar = {(width - bar_width) / 2, height * 87 / 100, bar_width, std::max(8, height / 70)};
    SDL_SetRenderDrawColor(renderer, 12, 10, 8, 220);
    SDL_RenderFillRect(renderer, &bar);
    SDL_SetRenderDrawColor(renderer, screen.accent.r, screen.accent.g, screen.accent.b, 255);
    SDL_RenderDrawRect(renderer, &bar);
    if (total) {
        SDL_Rect fill = {bar.x + unit, bar.y + unit, int((bar.w - 2 * unit) * double(std::min(completed, total)) / total), bar.h - 2 * unit};
        SDL_RenderFillRect(renderer, &fill);
    }
    if ((screen.legacy_font && screen.legacy_font_ready) || !screen.font_path.empty()) {
        const int font_size = std::max(14, height / 36);
        if (!screen.legacy_font && screen.font_size != font_size) {
            if (screen.font) TTF_CloseFont(screen.font);
            screen.font = TTF_OpenFont(screen.font_path.c_str(), font_size);
            if (!screen.font) throw std::runtime_error(TTF_GetError());
            screen.font_size = font_size;
        }
        text(renderer, screen.title + " - " + screen.stage_title, width / 2, height * 73 / 100, width * 4 / 5, screen.accent, true);
        std::string status = label;
        if (total) status += "  " + std::to_string(std::min(completed, total)) + " / " + std::to_string(total);
        text(renderer, status, width / 2, height * 81 / 100, width * 9 / 10, {239, 230, 209, 255});
    }
    if (!screen.capture.empty()) {
        screen.captured_pixels.resize(static_cast<size_t>(width) * height * 4);
        if (SDL_RenderReadPixels(renderer, nullptr, SDL_PIXELFORMAT_RGBA32, screen.captured_pixels.data(), width * 4)) throw std::runtime_error(SDL_GetError());
        spng_ctx *ctx = spng_ctx_new(SPNG_CTX_ENCODER);
        FILE *file = file_open(screen.capture.c_str(), "wb");
        spng_ihdr header = {};
        header.width = width; header.height = height; header.bit_depth = 8; header.color_type = SPNG_COLOR_TYPE_TRUECOLOR_ALPHA;
        const bool okay = ctx && file && !spng_set_png_file(ctx, file) && !spng_set_ihdr(ctx, &header) &&
            !spng_encode_image(ctx, screen.captured_pixels.data(), screen.captured_pixels.size(), SPNG_FMT_PNG, SPNG_ENCODE_FINALIZE);
        if (ctx) spng_ctx_free(ctx);
        if (file) file_close(file);
        screen.capture.clear();
        if (!okay) throw std::runtime_error("Cannot capture loading-screen validation image");
    }
    SDL_RenderPresent(renderer);
}
}

void platform_loading_screen_begin()
{
    if (screen.active) return;
    screen = {};
    screen.active = true;
    loading_progress::observer = draw;
    loading_progress::theme_observer = select_theme;
    loading_progress::stage_observer = select_stage;
    loading_progress::font_glyph_observer = receive_glyph;
    loading_progress::font_ready_observer = font_ready;
}

void platform_loading_screen_end()
{
    if (!screen.active) return;
    loading_progress::observer = nullptr;
    loading_progress::theme_observer = nullptr;
    loading_progress::stage_observer = nullptr;
    loading_progress::font_glyph_observer = nullptr;
    loading_progress::font_ready_observer = nullptr;
    release_theme();
    for (const auto &glyph : screen.glyphs) SDL_DestroyTexture(glyph.second.texture);
    screen.glyphs.clear();
    if (screen.ttf_initialized) TTF_Quit();
    screen.active = false;
}

bool platform_loading_screen_cancelled() { return screen.cancelled; }

void platform_loading_screen_validate(const char *output_directory)
{
    const auto output = mod_content::utf8_path(output_directory);
    std::filesystem::create_directories(output);
    const auto theme = screen.theme, stage = screen.stage_id;
    auto *renderer = platform_renderer_get_sdl();
    auto *target = SDL_GetRenderTarget(renderer);
    SDL_Rect viewport;
    SDL_RenderGetViewport(renderer, &viewport);
    for (const char *mod : {"Julius", "Augustus", "Vespasian"}) {
        select_theme((std::string("Mods/") + mod + "/UI/loading.xml").c_str());
        if (screen.legacy_font) {
            if (!screen.legacy_font_ready || screen.glyphs.empty() || screen.font || !screen.font_path.empty()) throw std::runtime_error("Original loading font was not supplied independently of a typeface file");
            select_stage("graphics");
            screen.legacy_font_ready = false;
            screen.capture = mod_content::path_text(output / (std::string(mod) + "-font-pending.png"));
            draw("Fonts are not ready", 1, 4);
            const auto pending = screen.captured_pixels;
            screen.capture = mod_content::path_text(output / (std::string(mod) + "-font-pending-alternate-label.png"));
            draw("This label must not be rendered yet", 1, 4);
            if (pending != screen.captured_pixels) throw std::runtime_error("Loading text appeared before original font readiness");
            screen.capture = mod_content::path_text(output / (std::string(mod) + "-font-pending-progress.png"));
            draw("This label must not be rendered yet", 3, 4);
            if (pending == screen.captured_pixels) throw std::runtime_error("Loading bar did not advance while fonts were pending");
            const auto progressed = screen.captured_pixels;
            font_ready();
            screen.capture = mod_content::path_text(output / (std::string(mod) + "-font-ready.png"));
            draw("This label must not be rendered yet", 3, 4);
            if (progressed == screen.captured_pixels) throw std::runtime_error("Original loading labels did not appear after font readiness");
        }
        std::vector<uint8_t> previous_background;
        for (const char *id : {"graphics", "mod_data"}) {
            select_stage(id);
            screen.capture = mod_content::path_text(output / (std::string(mod) + "-" + id + ".png"));
            draw("Preparing the next stage", 3, 4);
            if (screen.captured_pixels.empty() || (!previous_background.empty() && previous_background == screen.captured_pixels)) throw std::runtime_error("Loading stages did not render distinct backgrounds");
            previous_background = screen.captured_pixels;
            SDL_Rect after;
            SDL_RenderGetViewport(renderer, &after);
            if (SDL_GetRenderTarget(renderer) != target || after.x != viewport.x || after.y != viewport.y || after.w != viewport.w || after.h != viewport.h) throw std::runtime_error("Loading screen changed game render state");
        }
    }
    select_theme(theme.c_str());
    select_stage(stage.c_str());
    {
        SDL_Texture *fixture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, 128, 128);
        if (!fixture) throw std::runtime_error(SDL_GetError());
        SDL_Rect original_clip;
        SDL_RenderGetClipRect(renderer, &original_clip);
        const auto original_clipped = SDL_RenderIsClipEnabled(renderer);
        float original_sx, original_sy;
        SDL_RenderGetScale(renderer, &original_sx, &original_sy);
        Uint8 original_r, original_g, original_b, original_a;
        SDL_GetRenderDrawColor(renderer, &original_r, &original_g, &original_b, &original_a);
        SDL_BlendMode original_blend;
        SDL_GetRenderDrawBlendMode(renderer, &original_blend);
        SDL_SetRenderTarget(renderer, fixture);
        SDL_RenderSetScale(renderer, 2, 3);
        SDL_Rect test_viewport = {3, 5, 27, 29}, test_clip = {2, 3, 11, 13};
        SDL_RenderSetViewport(renderer, &test_viewport);
        SDL_RenderSetClipRect(renderer, &test_clip);
        SDL_SetRenderDrawColor(renderer, 11, 22, 33, 44);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        draw("Scaled render target restoration", 1, 1);
        SDL_Rect after_viewport, after_clip;
        SDL_RenderGetViewport(renderer, &after_viewport);
        SDL_RenderGetClipRect(renderer, &after_clip);
        float after_sx, after_sy;
        SDL_RenderGetScale(renderer, &after_sx, &after_sy);
        Uint8 after_r, after_g, after_b, after_a;
        SDL_GetRenderDrawColor(renderer, &after_r, &after_g, &after_b, &after_a);
        SDL_BlendMode after_blend;
        SDL_GetRenderDrawBlendMode(renderer, &after_blend);
        const auto same_rect = [](const SDL_Rect &a, const SDL_Rect &b) { return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; };
        const bool restored = SDL_GetRenderTarget(renderer) == fixture && after_sx == 2 && after_sy == 3 &&
            same_rect(after_viewport, test_viewport) && SDL_RenderIsClipEnabled(renderer) && same_rect(after_clip, test_clip) &&
            after_r == 11 && after_g == 22 && after_b == 33 && after_a == 44 && after_blend == SDL_BLENDMODE_ADD;
        SDL_SetRenderTarget(renderer, target);
        SDL_RenderSetScale(renderer, original_sx, original_sy);
        SDL_RenderSetViewport(renderer, &viewport);
        SDL_RenderSetClipRect(renderer, original_clipped ? &original_clip : nullptr);
        SDL_SetRenderDrawColor(renderer, original_r, original_g, original_b, original_a);
        SDL_SetRenderDrawBlendMode(renderer, original_blend);
        SDL_DestroyTexture(fixture);
        if (!restored) throw std::runtime_error("Loading screen changed a scaled, clipped render target");
    }
    SDL_Event quit = {};
    quit.type = SDL_QUIT;
    SDL_PushEvent(&quit);
    bool rejected = false;
    try { draw("Cancellation test", 0, 1); }
    catch (const std::runtime_error &) { rejected = screen.cancelled; }
    screen.cancelled = false;
    if (!rejected) throw std::runtime_error("Loading screen ignored the close event");
    Logger::infof("Loading-screen validation passed: deferred original fonts, progress without text, six stage backgrounds, renderer state restoration and cancellation");
}
