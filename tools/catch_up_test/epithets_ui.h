#pragma once
#include <stdexcept>

void window_epithets_validate_ui_for_test()
{
    const auto &tabs = *definition->widget("religions");
    bool exercised_scroll = false;
    for (int i = 0; i < controller.repeat_count("epithets.religions"); ++i) {
        mouse pointer{}; pointer.x = tabs.x + i * tabs.repeat_spacing_x + 10; pointer.y = tabs.y + 10;
        pointer.left.went_down = 1; runtime->handle_mouse(pointer, 640, 432);
        pointer.left.went_down = 0; pointer.left.went_up = 1; runtime->handle_mouse(pointer, 640, 432);
        if (controller.selected != i || controller.image("religion.portrait", i).width() <= 0) throw std::runtime_error("God portrait tab did not select a drawable portrait");
        window_draw(1);
        mouse wheel{}; wheel.x = 200; wheel.y = 250; wheel.scrolled = SCROLL_DOWN;
        const auto &bar = *definition->widget("scrollbar");
        rich_text_set_scrollbar_appearance(&bar.scrollbar_appearance);
        rich_text_handle_mouse(&wheel);
        if (rich_text_scroll_position() > 0) {
            exercised_scroll = true;
            mouse drag{}; drag.x = bar.x + 10; drag.y = bar.y + bar.height - 30; drag.left.is_down = 1;
            rich_text_handle_mouse(&drag);
            if (rich_text_scroll_position() <= 0) throw std::runtime_error("Skinned scrollbar drag lost its position");
            drag.left.is_down = 0; rich_text_handle_mouse(&drag);
        }
        rich_text_set_scrollbar_appearance(nullptr);
    }
    if (!exercised_scroll) throw std::runtime_error("God powers fixture did not exercise scrolling");
    controller.select(0); window_draw(1);
    std::fprintf(stdout, "God portrait tabs, skinned scrollbar wheel and drag passed.\n");
}
