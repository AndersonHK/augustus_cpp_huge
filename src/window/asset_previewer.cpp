#include "asset_previewer.h"
#include "assets/image_group_payload.h"
#include "core/Logger.h"
#include "core/time.h"
#include "game/mod_manager.h"
#include "game/system.h"
#include "graphics/declarative_window.h"
#include "graphics/graphics.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "SDL.h"
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <set>

namespace {
class AssetPreviewController final : public DeclarativeWindowController {
public:
    std::vector<std::string> groups;
    const ImageGroupPayload *payload = nullptr;
    int group_index = 0, entry_index = 0, zoom = 100, background = 0;
    bool bounds = false, playing = true;
    time_millis frame_time = 0;
    int frame = 1;

    const ImageGroupEntry *entry() const { return payload ? payload->entry_at_index(entry_index) : nullptr; }
    bool select_group(int index)
    {
        payload = nullptr; entry_index = 0; frame = 1;
        if (index < 0 || index >= static_cast<int>(groups.size())) return false;
        group_index = index;
        if (!image_group_payload_load(groups[index].c_str())) return false;
        payload = image_group_payload_get(groups[index].c_str());
        if (!payload || payload->entry_count() == 0) {
            payload = nullptr;
            return false;
        }
        return true;
    }
    bool scan()
    {
        std::set<std::string> names;
        for (const auto &root : mod_manager::graphics_paths()) {
            if (!std::filesystem::is_directory(root)) continue;
            for (const auto &file : std::filesystem::recursive_directory_iterator(root)) {
                if (!file.is_regular_file() || file.path().extension() != ".xml") continue;
                auto relative = file.path().lexically_relative(root); relative.replace_extension();
                names.insert(relative.generic_string());
            }
        }
        groups.assign(names.begin(), names.end());
        if (groups.empty()) { Logger::error("Asset previewer has no image groups in the active mods"); return false; }
        return select_group(std::min(group_index, static_cast<int>(groups.size()) - 1));
    }
    std::vector<std::string> choices(std::string_view binding) const override
    {
        if (binding == "group") return groups;
        if (binding == "background") return {"Black", "Stone", "White"};
        if (binding == "zoom") return {"50%", "100%", "200%", "400%"};
        std::vector<std::string> result;
        if (binding == "entry" && payload) for (int i = 0; i < payload->entry_count(); ++i) result.push_back(payload->entry_at_index(i)->id());
        return result;
    }
    std::string text(std::string_view binding, int) const override
    {
        if (binding == "group") return groups.empty() ? "" : groups[group_index];
        if (binding == "entry") return entry() ? entry()->id() : "";
        if (binding == "zoom") return std::to_string(zoom) + "%";
        if (binding == "background") return choices(binding)[background];
        if (binding == "source") return payload ? payload->xml_path() : "";
        if (!entry()) return {};
        if (binding == "size") return std::to_string(entry()->source_pixel_width()) + " x " + std::to_string(entry()->source_pixel_height());
        if (binding == "frames") return std::to_string(entry()->animation().frame_count());
        if (binding == "frame") return std::to_string(frame);
        if (binding == "layers") return entry()->has_top() ? "Footprint + top" : "Single image";
        return {};
    }
    int condition(std::string_view binding, int) const override { return binding == "bounds" ? bounds : binding == "playing" ? playing : 1; }
    void action(std::string_view action, int index) override
    {
        if (action == "group") select_group(index);
        else if (action == "entry" && payload && index >= 0 && index < payload->entry_count()) { entry_index = index; frame = 1; }
        else if (action == "previous" || action == "next") {
            if (payload) { entry_index = (entry_index + (action == "next" ? 1 : payload->entry_count() - 1)) % payload->entry_count(); frame = 1; }
        } else if (action == "zoom" && index >= 0 && index < 4) zoom = 50 << index;
        else if (action == "background" && index >= 0 && index < 3) background = index;
        else if (action == "bounds") bounds = !bounds;
        else if (action == "playing") playing = !playing;
        else if (action == "refresh") {
            // This standalone tool owns no city. Drop its payload pointer before rebuilding
            // the cache; all UI references resolve again through the native group loader.
            payload = nullptr; image_group_payload_clear_all(); scan();
        } else if (action == "quit") system_exit();
        window_invalidate();
    }
    void draw_custom(const DeclarativeWidgetDefinition &, int, int x, int y, int width, int height, bool) const override
    {
        const color_t backgrounds[] = {COLOR_BLACK, 0xff706858, COLOR_WHITE};
        graphics_fill_rect(x, y, width, height, backgrounds[background]);
        const auto *selected = entry();
        if (!selected) return;
        std::vector<RuntimeDrawSlice> slices;
        if (selected->footprint()) slices.push_back(*selected->footprint());
        if (selected->top()) slices.push_back(*selected->top());
        if (selected->has_animation() && selected->animation().has_frames()) slices.push_back(selected->animation().frame_slice_at_offset(frame));
        if (slices.empty()) return;
        int left = INT_MAX, top = INT_MAX, right = INT_MIN, bottom = INT_MIN;
        for (const auto &slice : slices) {
            left = std::min(left, slice.draw_offset_x); top = std::min(top, slice.draw_offset_y);
            right = std::max(right, slice.draw_offset_x + slice.width); bottom = std::max(bottom, slice.draw_offset_y + slice.height);
        }
        const float factor = zoom / 100.0f;
        const float origin_x = x + (width - (right - left) * factor) / 2 - left * factor;
        const float origin_y = y + (height - (bottom - top) * factor) / 2 - top * factor;
        graphics_renderer()->push_state();
        graphics_renderer()->set_clip_rectangle(x, y, width, height);
        for (auto slice : slices) {
            const float xx = origin_x + slice.draw_offset_x * factor, yy = origin_y + slice.draw_offset_y * factor;
            slice.draw_offset_x = slice.draw_offset_y = 0; slice.fixed_logical_size = {};
            runtime_texture_draw_request(slice, xx, yy, slice.width * factor, slice.height * factor, COLOR_MASK_NONE, RENDER_DOMAIN_UI, RENDER_SCALING_POLICY_AUTO);
        }
        if (bounds) {
            const int xx = static_cast<int>(origin_x + left * factor) - 1, yy = static_cast<int>(origin_y + top * factor) - 1;
            const int ww = static_cast<int>((right - left) * factor) + 2, hh = static_cast<int>((bottom - top) * factor) + 2;
            graphics_draw_line(xx, xx + ww, yy, yy, COLOR_RED); graphics_draw_line(xx, xx + ww, yy + hh, yy + hh, COLOR_RED);
            graphics_draw_line(xx, xx, yy, yy + hh, COLOR_RED); graphics_draw_line(xx + ww, xx + ww, yy, yy + hh, COLOR_RED);
        }
        graphics_renderer()->pop_state();
    }
};
AssetPreviewController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
void draw_background() { graphics_clear_screen(); if (runtime) runtime->draw(DeclarativeDrawPhase::Background, screen_width(), screen_height()); }
void draw_foreground()
{
    if (runtime) runtime->draw(DeclarativeDrawPhase::Foreground, screen_width(), screen_height());
    const auto *entry = controller.entry();
    if (controller.playing && entry && entry->animation().frame_count() > 0 && time_get_millis() - controller.frame_time >= 80) {
        controller.frame_time = time_get_millis();
        controller.frame = controller.frame % entry->animation().frame_count() + 1;
        window_invalidate();
    }
}
void handle_input(const mouse *m, const hotkeys *h)
{
    if (h->f5_pressed) controller.action("refresh", 0);
    if (runtime) runtime->handle_mouse(*m, screen_width(), screen_height());
}
void get_tooltip(tooltip_context *context) { if (runtime) runtime->tooltip(*context); }
}

int window_asset_previewer_show()
{
    try {
        const auto *definition = declarative_window_definition("asset_previewer");
        if (!definition) { Logger::error("Asset previewer XML window is missing"); return 0; }
        controller = AssetPreviewController{};
        if (!controller.scan()) return 0;
        runtime = std::make_unique<DeclarativeWindowRuntime>(*definition, controller);
        static const window_type window{WINDOW_ASSET_PREVIEWER, draw_background, draw_foreground, handle_input, get_tooltip};
        window_show(&window);
        return 1;
    } catch (const std::exception &error) { Logger::error("Cannot open asset previewer", error.what()); return 0; }
}

int window_asset_previewer_validate_for_test()
{
    if (!runtime || controller.groups.empty()) { std::fprintf(stderr, "Preview test has no runtime/groups\n"); return 0; }
    const auto group = std::find(controller.groups.begin(), controller.groups.end(), "UI/Empire_Panel_01");
    if (group != controller.groups.end() && !controller.select_group(static_cast<int>(group - controller.groups.begin()))) return 0;
    controller.playing = false;
    const auto *definition = declarative_window_definition("asset_previewer");
    const auto *button = definition ? definition->widget("bounds") : nullptr;
    if (!button || controller.text("size", 0).empty()) { std::fprintf(stderr, "Preview test has no button/size\n"); return 0; }
    window_draw(1);
    mouse click{}; click.x = button->x + 5; click.y = button->y + 5;
    const auto click_bounds = [&]() {
        click.left.went_up = 0; click.left.went_down = 1;
        const int down = runtime->handle_mouse(click, screen_width(), screen_height());
        click.left.went_down = 0; click.left.went_up = 1;
        return down && runtime->handle_mouse(click, screen_width(), screen_height());
    };
    if (!click_bounds() || !controller.bounds) { std::fprintf(stderr, "Preview bounds click failed: %d,%d surface=%d,%d\n", click.x, click.y, screen_width(), screen_height()); return 0; }
    window_draw(0);
    const int width = screen_pixel_width(), height = screen_pixel_height();
    std::vector<color_t> pixels(static_cast<size_t>(width) * height);
    if (!graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width)) return 0;
    const bool border_visible = std::find(pixels.begin(), pixels.end(), COLOR_RED) != pixels.end();
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    if (!surface) return 0;
    const int saved = SDL_SaveBMP(surface, "out/asset-previewer-bounds.bmp"); SDL_FreeSurface(surface);
    if (!border_visible) std::fprintf(stderr, "Preview bounds absent: group=%s size=%s\n", controller.groups[controller.group_index].c_str(), controller.text("size", 0).c_str());
    if (!border_visible || saved || !click_bounds() || controller.bounds) return 0;
    controller.action("refresh", 0); window_draw(0);
    if (!controller.entry()) return 0;
    std::fprintf(stdout, "Native asset previewer dimensions, bounds, refresh and render passed.\n");
    return 1;
}
