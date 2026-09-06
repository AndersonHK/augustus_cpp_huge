#include "window/epithets.h"
#include "building/building_type_registry_internal.h"
#include "city/god.h"
#include "graphics/declarative_window.h"
#include "graphics/graphics.h"
#include "graphics/rich_text.h"
#include "graphics/window.h"
#include "input/input.h"
#include <algorithm>
#include <memory>

namespace {
using building_type_registry_impl::Religion;

std::vector<const Religion *> active_religions()
{
    std::vector<const Religion *> result;
    for (const auto &type : building_type_registry_impl::g_building_types) {
        const Religion *religion = type ? type->religion() : nullptr;
        if (religion && religion->presentation().is_complete() && !religion->epithets().empty() &&
            std::find(result.begin(), result.end(), religion) == result.end()) result.push_back(religion);
    }
    std::stable_sort(result.begin(), result.end(), [](const Religion *a, const Religion *b) { return a->presentation().module_index() < b->presentation().module_index(); });
    return result;
}

std::string translated(translation_key key) { return reinterpret_cast<const char *>(translation_for(key)); }

class EpithetsController final : public DeclarativeWindowController {
public:
    std::vector<const Religion *> religions;
    int selected = 0;
    int page = 0;
    int page_size = 6;
    std::string body;

    void select(int index)
    {
        if (index < 0 || index >= religions.size()) return;
        selected = index;
        const Religion &religion = *religions[selected];
        body = "@H" + translated(religion.presentation().name_key()) + "@L" + translated(religion.presentation().bonus_key());
        for (const auto &epithet : religion.epithets()) body += "@P@H" + translated(epithet.name_key) + "@L" + translated(epithet.description_key);
        rich_text_reset(0);
        window_invalidate();
    }
    int repeat_count(std::string_view source) const override { return source == "epithets.religions" ? std::min(page_size, static_cast<int>(religions.size()) - page * page_size) : 0; }
    std::string text(std::string_view binding, int index) const override
    {
        const int id = page * page_size + index;
        if (binding != "religion.name" || index < 0 || id >= religions.size()) return {};
        const Religion &religion = *religions[id];
        if (religion.gods().size() == 1 && !religion.has_all_gods()) {
            const God &god = *religion.gods()[0];
            return god.legacy_type() < GOD_ALL ? translated(current_string_key(59, 11 + god.legacy_type())) : god.path();
        }
        return translated(religion.presentation().name_key());
    }
    int condition(std::string_view binding, int index) const override
    {
        if (binding == "religion.selected") return selected == page * page_size + index;
        if (binding == "page.previous") return page > 0;
        if (binding == "page.next") return (page + 1) * page_size < religions.size();
        return 0;
    }
    void action(std::string_view action, int index) override
    {
        if (action == "religion.select") select(page * page_size + index);
        else if (action == "window.close") window_go_back();
        else if (action == "page.previous" && page > 0) { --page; select(page * page_size); }
        else if (action == "page.next" && (page + 1) * page_size < religions.size()) { ++page; select(page * page_size); }
    }
};

EpithetsController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
const DeclarativeWindowDefinition *definition;

void draw_background()
{
    window_draw_underlying_window();
    graphics_in_dialog_with_size(definition->base_width(), definition->base_height());
    runtime->draw(DeclarativeDrawPhase::Background, definition->base_width(), definition->base_height());
    graphics_reset_dialog();
}

void draw_foreground()
{
    graphics_in_dialog_with_size(definition->base_width(), definition->base_height());
    runtime->draw(DeclarativeDrawPhase::Foreground, definition->base_width(), definition->base_height());
    const auto &body = *definition->widget("body");
    const auto &scrollbar = *definition->widget("scrollbar");
    rich_text_set_fonts(body.font, FONT_NORMAL_GREEN, body.font, body.line_spacing);
    rich_text_set_font_size_delta(body.font_size_delta);
    rich_text_set_paragraph_spacing(body.paragraph_spacing);
    const auto *text = reinterpret_cast<const uint8_t *>(controller.body.c_str());
    rich_text_init(text, body.x, body.y, body.width / BLOCK_SIZE, body.height / BLOCK_SIZE, 0);
    rich_text_set_scrollbar_bounds(scrollbar.x, scrollbar.y, scrollbar.height, body.width);
    const int line_height = std::max(1, rich_text_get_line_height());
    rich_text_draw(text, body.x, body.y, body.width, body.height / line_height, 0);
    rich_text_draw_scrollbar();
    graphics_reset_dialog();
}

void handle_input(const mouse *m, const hotkeys *keys)
{
    const mouse *dialog = mouse_in_dialog_with_size(m, definition->base_width(), definition->base_height());
    if (rich_text_handle_mouse(dialog) || runtime->handle_mouse(*dialog, definition->base_width(), definition->base_height())) return;
    if (input_go_back_requested(m, keys)) window_go_back();
}
}

int window_epithets_available() { return !active_religions().empty(); }

void window_epithets_show()
{
    definition = declarative_window_definition("epithets");
    if (!definition || !definition->widget("body") || !definition->widget("scrollbar") || !definition->widget("religions")) return;
    controller.religions = active_religions();
    if (controller.religions.empty()) return;
    controller.page_size = std::max(1, definition->widget("religions")->repeat_columns);
    controller.page = 0;
    controller.select(0);
    runtime = std::make_unique<DeclarativeWindowRuntime>(*definition, controller);
    window_type window = {WINDOW_EPITHETS, draw_background, draw_foreground, handle_input};
    window_show(&window);
}
