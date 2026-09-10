#include "scrollbar.h"

#include "core/calc.h"
#include "core/image_group.h"
#include "graphics/image_button.h"
#include "graphics/screen.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/window.h"
#include "graphics/graphics.h"
#include "graphics/ui_primitives.h"
#include "graphics/runtime_texture.h"
#include <algorithm>

enum {
    TOUCH_DRAG_NONE = 0,
    TOUCH_DRAG_PENDING = 1,
    TOUCH_DRAG_IN_PROGRESS = 2
};

#define SCROLL_BUTTON_HEIGHT 26
#define SCROLL_BUTTON_WIDTH 39
#define SCROLL_DOT_SIZE 25
#define TOTAL_BUTTON_HEIGHT (2 * SCROLL_BUTTON_HEIGHT + SCROLL_DOT_SIZE)

static void text_scroll(int is_down, int num_lines);

static image_button image_button_scroll_up = {
    0, 0, SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT, IB_SCROLL,
    GROUP_OK_CANCEL_SCROLL_BUTTONS, 8, text_scroll, button_none, 0, 1, 1
};
static image_button image_button_scroll_down = {
    0, 0, SCROLL_BUTTON_WIDTH, SCROLL_BUTTON_HEIGHT, IB_SCROLL,
    GROUP_OK_CANCEL_SCROLL_BUTTONS, 12, text_scroll, button_none, 1, 1, 1
};

static scrollbar_type *current;

static int button_height(const scrollbar_type *bar) { return bar->appearance ? bar->appearance->up.height() : SCROLL_BUTTON_HEIGHT; }
static int button_width(const scrollbar_type *bar) { return bar->appearance ? bar->appearance->up.width() : SCROLL_BUTTON_WIDTH; }
static int thumb_height(const scrollbar_type *bar)
{
    if (!bar->appearance) return SCROLL_DOT_SIZE;
    const int track = std::max(1, bar->height - 2 * button_height(bar) - 2 * bar->dot_padding);
    const int ends = bar->appearance->top.height() + bar->appearance->bottom.height();
    const unsigned int total = bar->elements_in_view + bar->max_scroll_position;
    return std::clamp(total ? static_cast<int>(static_cast<int64_t>(track) * bar->elements_in_view / total) : track, std::min(track, ends + 8), track);
}


void scrollbar_init(scrollbar_type *scrollbar, unsigned int scroll_position, unsigned int total_elements)
{
    unsigned int max_scroll_position;
    if (total_elements <= scrollbar->elements_in_view) {
        max_scroll_position = 0;
    } else {
        max_scroll_position = total_elements - scrollbar->elements_in_view;
    }
    scrollbar->scroll_position = calc_bound(scroll_position, 0, max_scroll_position);
    scrollbar->max_scroll_position = max_scroll_position;
    scrollbar->is_dragging_scrollbar_dot = 0;
    scrollbar->scrollbar_dot_drag_offset = 0;
    scrollbar->scrollbar_dot_mouse_offset = 0;
    scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
}

void scrollbar_reset(scrollbar_type *scrollbar, unsigned int scroll_position)
{
    scrollbar->scroll_position = calc_bound(scroll_position, 0, scrollbar->max_scroll_position);
    scrollbar->is_dragging_scrollbar_dot = 0;
    scrollbar->scrollbar_dot_drag_offset = 0;
    scrollbar->scrollbar_dot_mouse_offset = 0;
    scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
}

void scrollbar_update_total_elements(scrollbar_type *scrollbar, unsigned int total_elements)
{
    unsigned int max_scroll_position;
    if (total_elements <= scrollbar->elements_in_view) {
        max_scroll_position = 0;
    } else {
        max_scroll_position = total_elements - scrollbar->elements_in_view;
    }
    if (scrollbar->max_scroll_position != max_scroll_position) scrollbar->is_dragging_scrollbar_dot = 0;
    scrollbar->max_scroll_position = max_scroll_position;
    if (!max_scroll_position) scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    if (scrollbar->scroll_position > max_scroll_position) {
        scrollbar->scroll_position = max_scroll_position;
    }
}

void scrollbar_draw(scrollbar_type *scrollbar)
{
    if (scrollbar->max_scroll_position > 0 || scrollbar->always_visible) {
        if (const auto *skin = scrollbar->appearance) {
            const int x = scrollbar->x, y = scrollbar->y, width = button_width(scrollbar), bh = button_height(scrollbar);
            skin->up.draw(x, y); skin->down.draw(x, y + scrollbar->height - bh);
            graphics_draw_inset_rect(x, y + bh, width, scrollbar->height - 2 * bh, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
            const int thumb = thumb_height(scrollbar);
            const int travel = std::max(0, scrollbar->height - 2 * bh - 2 * scrollbar->dot_padding - thumb);
            const int offset = scrollbar->max_scroll_position ? static_cast<int>(static_cast<int64_t>(travel) * scrollbar->scroll_position / scrollbar->max_scroll_position) : 0;
            const int top = y + bh + scrollbar->dot_padding + offset;
            skin->top.draw(x, top);
            UiPrimitives().draw_tiled_slice(skin->middle.runtime_slice(), x, top + skin->top.height(), width, thumb - skin->top.height() - skin->bottom.height());
            skin->bottom.draw(x, top + thumb - skin->bottom.height());
            if (skin->grip.is_bound()) skin->grip.draw(x, top + (thumb - skin->grip.height()) / 2);
            return;
        }
        image_buttons_draw(scrollbar->x, scrollbar->y, &image_button_scroll_up, 1);
        image_buttons_draw(scrollbar->x, scrollbar->y + scrollbar->height - SCROLL_BUTTON_HEIGHT,
            &image_button_scroll_down, 1);
        ui_runtime_draw_scrollbar_dot(scrollbar);
    }
}

static int touch_inside_scrollable_area(const scrollbar_type *scrollbar, const touch *t, int in_dialog)
{
    int x = t->start_point.x;
    int y = t->start_point.y;
    if (in_dialog) {
        x -= screen_dialog_offset_x();
        y -= screen_dialog_offset_y();
    }
    return scrollbar->max_scroll_position > 0 &&
        x >= scrollbar->x - scrollbar->scrollable_width && x <= scrollbar->x - 2 &&
        y >= scrollbar->y && y < scrollbar->y + scrollbar->height;
}

static int handle_touch(scrollbar_type *scrollbar, const touch *t, int in_dialog)
{
    unsigned int old_position = scrollbar->scroll_position;
    int active = scrollbar->touch_drag_state == TOUCH_DRAG_IN_PROGRESS;

    if (t->has_started && touch_inside_scrollable_area(scrollbar, t, in_dialog)) {
        scrollbar->touch_drag_state = TOUCH_DRAG_PENDING;
        scrollbar->position_on_touch = scrollbar->scroll_position;
    }
    if (t->has_moved && scrollbar->touch_drag_state != TOUCH_DRAG_NONE) {
        scrollbar->touch_drag_state = TOUCH_DRAG_IN_PROGRESS;
        if (!scrollbar->elements_in_view) return 0;
        int element_height = (scrollbar->height - 8 * scrollbar->has_y_margin) / scrollbar->elements_in_view;
        if (element_height <= 0) return 0;
        const int dialog_y = in_dialog ? screen_dialog_offset_y() : 0;
        const int current_touch_y = t->current_point.y - dialog_y;
        const int start_touch_y = t->start_point.y - dialog_y;
        int current_y = current_touch_y - ((current_touch_y - (scrollbar->y + 8 * scrollbar->has_y_margin)) % element_height);
        int start_y = start_touch_y - ((start_touch_y - (scrollbar->y + 8 * scrollbar->has_y_margin)) % element_height);
        int touch_scrolled = (current_y - start_y) / element_height;
        scrollbar->scroll_position = calc_bound(scrollbar->position_on_touch - touch_scrolled, 0, scrollbar->max_scroll_position);
        active = 1;
    }
    if (t->has_ended) {
        scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    }
    if (old_position != scrollbar->scroll_position) {
        if (scrollbar->on_scroll_callback) scrollbar->on_scroll_callback();
        window_invalidate();
    }
    return active;
}

static int handle_scrollbar_dot(scrollbar_type *scrollbar, const mouse *m)
{
    if (scrollbar->max_scroll_position <= 0 || !m->left.is_down) {
        return 0;
    }
    int track_height = scrollbar->height - (2 * button_height(scrollbar) + thumb_height(scrollbar)) - 2 * scrollbar->dot_padding;
    if (track_height <= 0) return 0;
    const int track_y = scrollbar->y + button_height(scrollbar) + scrollbar->dot_padding;
    if (!scrollbar->is_dragging_scrollbar_dot) {
        if (m->x < scrollbar->x || m->x >= scrollbar->x + button_width(scrollbar) ||
            m->y < track_y || m->y > scrollbar->y + scrollbar->height - button_height(scrollbar) - scrollbar->dot_padding) return 0;
        const int offset = calc_adjust_with_percentage(track_height, calc_percentage(scrollbar->scroll_position, scrollbar->max_scroll_position));
        const int within_dot = m->y - track_y - offset;
        scrollbar->scrollbar_dot_mouse_offset = within_dot >= 0 && within_dot < thumb_height(scrollbar) ? within_dot : thumb_height(scrollbar) / 2;
        scrollbar->is_dragging_scrollbar_dot = 1;
        // Capture the pointer without quantizing the position on the initial
        // press. Clicking the track still moves the thumb to that location.
        if (within_dot >= 0 && within_dot < thumb_height(scrollbar)) {
            scrollbar->scrollbar_dot_drag_offset = offset;
            return 1;
        }
    }
    int dot_offset = m->y - track_y - scrollbar->scrollbar_dot_mouse_offset;
    if (dot_offset < 0) {
        dot_offset = 0;
    }
    if (dot_offset > track_height) {
        dot_offset = track_height;
    }
    int pct_scrolled = calc_percentage(dot_offset, track_height);
    scrollbar->scroll_position = calc_adjust_with_percentage(
        scrollbar->max_scroll_position, pct_scrolled);
    scrollbar->is_dragging_scrollbar_dot = 1;
    scrollbar->scrollbar_dot_drag_offset = dot_offset;
    if (scrollbar->scrollbar_dot_drag_offset < 0) {
        scrollbar->scrollbar_dot_drag_offset = 0;
    }
    if (scrollbar->on_scroll_callback) {
        scrollbar->on_scroll_callback();
    }
    window_invalidate();
    return 1;
}

int scrollbar_handle_mouse(scrollbar_type *scrollbar, const mouse *m, int in_dialog)
{
    if (!m->left.is_down) scrollbar->is_dragging_scrollbar_dot = 0;
    if (scrollbar->max_scroll_position <= 0) {
        return 0;
    }
    current = scrollbar;
    image_button_scroll_up.width = image_button_scroll_down.width = static_cast<short>(button_width(scrollbar));
    image_button_scroll_up.height = image_button_scroll_down.height = static_cast<short>(button_height(scrollbar));
    if (scrollbar->is_dragging_scrollbar_dot) return handle_scrollbar_dot(scrollbar, m);
    if (!m->is_touch) {
        scrollbar->touch_drag_state = TOUCH_DRAG_NONE;
    }
    if (scrollbar->touch_drag_state != TOUCH_DRAG_IN_PROGRESS) {
        if (m->scrolled == SCROLL_DOWN) {
            text_scroll(1, 3);
        } else if (m->scrolled == SCROLL_UP) {
            text_scroll(0, 3);
        }

        if (image_buttons_handle_mouse(m,
            scrollbar->x, scrollbar->y, &image_button_scroll_up, 1, 0)) {
            return 1;
        }
        if (image_buttons_handle_mouse(m,
            scrollbar->x, scrollbar->y + scrollbar->height - button_height(scrollbar),
            &image_button_scroll_down, 1, 0)) {
            return 1;
        }
    }
    if (m->is_touch && handle_touch(scrollbar, touch_get_earliest(), in_dialog)) {
        return 1;
    }
    return handle_scrollbar_dot(scrollbar, m);
}

static void text_scroll(int is_down, int num_lines)
{
    scrollbar_type *scrollbar = current;
    if (is_down) {
        scrollbar->scroll_position += num_lines;
        if (scrollbar->scroll_position > scrollbar->max_scroll_position) {
            scrollbar->scroll_position = scrollbar->max_scroll_position;
        }
    } else {
        if (scrollbar->scroll_position <= (unsigned int) num_lines) {
            scrollbar->scroll_position = 0;
        } else {
            scrollbar->scroll_position -= num_lines;
        }
    }
    scrollbar->is_dragging_scrollbar_dot = 0;
    if (scrollbar->on_scroll_callback) {
        scrollbar->on_scroll_callback();
    }
    window_invalidate();
}
