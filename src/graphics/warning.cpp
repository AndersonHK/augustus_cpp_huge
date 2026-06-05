extern "C" {
#include "warning.h"
#include "city/warning.h"
#include "game/state.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
}

#include "graphics/image.h"

static const int TOP_OFFSETS[] = { 30, 55, 80, 105, 130 };

static int determine_width(const uint8_t *text)
{
    int width = text_get_width(text, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    if (width <= 100) {
        return 200;
    } else if (width <= 200) {
        return 300;
    } else if (width <= 300) {
        return 400;
    } else {
        return 460;
    }
}

void warning_draw(void)
{
    if (!window_is(WINDOW_BUILDING_INFO) && !window_is(WINDOW_CITY) && !window_is(WINDOW_EDITOR_MAP)) {
        city_warning_clear_all();
        return;
    }

    int center = (screen_width() - 180) / 2;
    for (int i = 0; i < 5; i++) {
        const uint8_t *text = city_warning_get(i);
        if (!text) {
            continue;
        }
        int top_offset = TOP_OFFSETS[i];
        if (game_state_is_paused()) {
            top_offset += 70;
        }
        int box_width = determine_width(text);
        label_draw(center - box_width / 2 + 1, top_offset, box_width / BLOCK_SIZE + 1, 1);
        if (box_width < 460) {
            // ornaments at the side
            Image::from_id(Image::group(GROUP_CONTEXT_ICONS) + 15).draw(center - box_width / 2 + 2, top_offset + 2, COLOR_MASK_NONE, SCALE_NONE);
            Image::from_id(Image::group(GROUP_CONTEXT_ICONS) + 15).draw(center + box_width / 2 - 30, top_offset + 2, COLOR_MASK_NONE, SCALE_NONE);
        }
        text_draw_centered(text, center - box_width / 2 + 1, top_offset + 4, box_width, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }
    city_warning_clear_outdated();
}
