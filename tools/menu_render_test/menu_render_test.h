#pragma once

#include "graphics/graphics.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "window/main_menu.h"
#include "window/popup_dialog.h"
#include "graphics/image.h"
#include "game/mod_manager.h"

#include <cstdio>
#include <vector>

// Read the menu header above the quit dialog: it includes the Vespasian card
// and banner, and must survive both an underlying redraw and dialog dismissal.
inline bool run_menu_render_test()
{
    for (const std::string &mod : mod_manager::mod_names()) {
        if (mod != "Augustus") continue;
        const auto banner = ImageGroupEntryRef::from_group("UI\\HoldGames_Banner", "HoldGames Banner");
        if (!banner.runtime_slice().is_valid()) {
            std::fprintf(stderr, "Hold Games banner could not resolve its external message illustration.\n");
            return false;
        }
    }
    window_main_menu_show(0);
    window_draw(1);
    window_draw(1);
    const int x = screen_ui_to_pixel(screen_dialog_offset_x());
    const int y = screen_ui_to_pixel(screen_dialog_offset_y());
    const int width = screen_ui_to_pixel(640);
    const int height = screen_ui_to_pixel(76);
    std::vector<color_t> expected(static_cast<size_t>(width) * height);
    std::vector<color_t> actual(expected.size());
    auto capture = [&](std::vector<color_t> &pixels) {
        return graphics_renderer()->save_screen_buffer(pixels.data(), x, y, width, height, width) != 0;
    };
    if (!capture(expected)) {
        std::fprintf(stderr, "Menu render test could not read the menu header.\n");
        return false;
    }
    window_popup_dialog_show(POPUP_DIALOG_QUIT, [](int, int) {}, 1);
    for (int frame = 0; frame < 3; frame++) {
        window_draw(1);
        if (!window_is(WINDOW_POPUP_DIALOG) || !capture(actual) || actual != expected) {
            std::fprintf(stderr, "Menu header changed beneath the quit dialog on frame %d.\n", frame);
            window_go_back();
            return false;
        }
    }
    window_go_back();
    window_draw(1);
    if (!window_is(WINDOW_MAIN_MENU) || !capture(actual) || actual != expected) {
        std::fprintf(stderr, "Menu header changed after dismissing the quit dialog.\n");
        return false;
    }
    std::fprintf(stdout, "Menu quit-dialog render test passed.\n");
    return true;
}
