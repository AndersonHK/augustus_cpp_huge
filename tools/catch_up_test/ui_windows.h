#pragma once
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/empire.h"
#include "window/config.h"
#include "widget/sidebar/extra.h"
#include "core/config.h"
#include "SDL.h"
#include <algorithm>
#include <filesystem>
#include <stdexcept>
#include <vector>

inline void validate_in_game_ui_windows()
{
    const std::filesystem::path output("out/ui-window-review");
    std::filesystem::create_directories(output);
    auto capture = [&](const char *name, bool ratings = false) {
        window_draw(1); window_draw(1);
        if (ratings) {
            screen_set_ui_render_scale();
            const int previous = config_get(CONFIG_UI_SIDEBAR_INFO);
            config_set(CONFIG_UI_SIDEBAR_INFO, 1);
            const int rendered_height = sidebar_extra_draw_background(20, 20, 160, 300, 0, SIDEBAR_EXTRA_DISPLAY_RATINGS);
            config_set(CONFIG_UI_SIDEBAR_INFO, previous);
            if (rendered_height <= 0) throw std::runtime_error("Sidebar ratings XML did not render");
        }
        int width = screen_pixel_width(), height = screen_pixel_height();
        std::vector<color_t> pixels(static_cast<size_t>(width) * height);
        if (!graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width)) throw std::runtime_error("UI window capture failed");
        if (std::all_of(pixels.begin(), pixels.end(), [&](color_t value) { return value == pixels.front(); })) throw std::runtime_error("UI window rendered blank");
        SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
        if (!surface) throw std::runtime_error("UI window capture allocation failed");
        int result = SDL_SaveBMP(surface, (output / (std::string(name) + ".bmp")).string().c_str());
        SDL_FreeSurface(surface);
        if (result) throw std::runtime_error("UI window capture write failed");
    };
    window_city_show(); capture("sidebar-ratings", true);
    window_city_show(); window_advisors_set_advisor(ADVISOR_FINANCIAL); window_advisors_show(); capture("financial");
    window_city_show(); window_advisors_set_advisor(ADVISOR_RELIGION); window_advisors_show(); capture("religion");
    window_city_show(); window_empire_show(); capture("empire");
    window_city_show(); window_config_show(CONFIG_PAGE_GAMEPLAY_CHANGES, 0, 0); capture("difficulty-settings");
    window_city_show(); window_config_show(CONFIG_PAGE_CITY_MANAGEMENT_CHANGES, 3, 0); capture("housing-settings");
    window_city_show();
}
