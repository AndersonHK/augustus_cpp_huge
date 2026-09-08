#include "window/advisor/financial.h"
#include "window/advisor/religion.h"
#include "window/epithets.h"
#include "platform/screen.h"
#pragma once

static void capture_reference_frame(const char *name)
{
    std::filesystem::create_directories("out/empire-ui-review");
    const int width = screen_pixel_width(), height = screen_pixel_height();
    std::vector<color_t> frame(static_cast<size_t>(width) * height);
    if (!graphics_renderer()->save_screen_buffer(frame.data(), 0, 0, width, height, width)) throw std::runtime_error("Cannot capture reference window");
    auto *surface = SDL_CreateRGBSurfaceFrom(frame.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    if (!surface) throw std::runtime_error("Cannot create reference screenshot");
    const std::string path = std::string("out/empire-ui-review/") + name + ".bmp";
    const int result = SDL_SaveBMP(surface, path.c_str()); SDL_FreeSurface(surface);
    if (result != 0) throw std::runtime_error("Cannot write reference screenshot");
}

// Included by the empire window to exercise its actual XML runtimes and input.
void window_empire_validate_ui_for_test()
{
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    window_empire_show(); window_draw(1); window_draw(1);
    const int initial_width = screen_pixel_width(), initial_height = screen_pixel_height();
    auto restore_screen = std::shared_ptr<void>(nullptr, [&](void *) { platform_screen_resize(initial_width, initial_height, 0); });
    platform_screen_resize(3840, 2160, 0);
    window_empire_show(); window_draw(1); window_draw(1);
    require(sidebar_definition && details_definition, "Empire windows are incomplete");
    capture_reference_frame("empire-initial");
    const bool classic = sidebar_definition->has_widget("city_list");
    require(data.sidebar.width > 0, "City sidebar has no width");
    for (const auto &card : empire_city_list.cards) {
        const auto *city = card->trade_city();
        const auto *object = empire_object_get(city->empire_object_id);
        const auto icon = empire_city_icon(object->empire_city_icon == EMPIRE_CITY_ICON_DEFAULT ? EMPIRE_CITY_ICON_TRADE_CITY : object->empire_city_icon);
        require(icon.width() > 0 && icon.height() > 0, "Trading city has no drawable icon");
    }
    if (classic) {
        const int original_sort = window_empire_sidebar_sort_get_current_sorting();
        const int original_reverse = window_empire_sidebar_sort_get_sorting_reversed();
        const auto click_control = [&](const char *id, int choice = -1) {
            const auto *widget = sidebar_definition->widget(id);
            require(widget != nullptr, "Classic city list lost a sorting/filtering control");
            mouse click{}; click.x = widget->resolved_x(data.sidebar.width, sidebar_definition->base_width()) + 4; click.y = widget->resolved_y(data.sidebar.height, sidebar_definition->base_height()) + 4;
            click.left.went_down = 1; empire_sidebar_ui->handle_mouse(click, data.sidebar.width, data.sidebar.height);
            click.left.went_down = 0; click.left.went_up = 1; empire_sidebar_ui->handle_mouse(click, data.sidebar.width, data.sidebar.height);
            if (widget->type == DeclarativeWidgetType::Dropdown) {
                const auto options = empire_controller.choices(widget->binding);
                const int menu_height = static_cast<int>(options.size()) * 24 + 8;
                click.x = widget->resolved_x(data.sidebar.width, sidebar_definition->base_width()) + 8;
                click.y = std::max(0, std::min(widget->resolved_y(data.sidebar.height, sidebar_definition->base_height()) + widget->height + 2, data.sidebar.height - menu_height)) + 8 + std::max(0, choice) * 24;
                empire_sidebar_ui->handle_mouse(click, data.sidebar.width, data.sidebar.height);
            }
            window_draw(1);
        };
        click_control("sort", (original_sort + 1) % MAX_SORTING_KEY);
        require(window_empire_sidebar_sort_get_current_sorting() == (original_sort + 1) % MAX_SORTING_KEY, "Classic sorting button does not change sorting");
        click_control("reverse");
        require(window_empire_sidebar_sort_get_sorting_reversed() != original_reverse, "Classic sort direction button does not work");
        for (int i = 0; i < MAX_FILTER_KEY; ++i) {
            click_control("filter", i);
            for (const auto &entry : empire_city_list.cards) require(window_empire_sidebar_sort_city_matches_current_filter(entry->trade_city()), "Classic list includes a city excluded by its filter");
        }
        require(window_empire_sidebar_sort_get_current_filtering() == FILTER_NONE, "Classic filter button does not cycle through every filter");
        if (sidebar_definition->has_widget("year") && city_trade_ledger_periods().size() > 1) {
            click_control("year", 1);
            require(empire_display_period_index == 1, "History year dropdown did not select the archive");
            for (const auto &entry : empire_city_list.cards) {
                const auto *card_definition = declarative_window_definition("empire_city_card");
                mouse historical{}; historical.left.went_up = 1; historical.y = 12;
                for (const char *id : {"open", "sells", "buys"}) {
                    const auto &w = *card_definition->widget(id);
                    for (int x = 0; x < w.width; x += 8) { historical.x = x; entry->handle_custom(w, -1, historical, w.width, w.height); }
                    require(window_is(WINDOW_EMPIRE), "Historical trade controls allowed a live edit");
                }
            }
            click_control("year", 0);
            require(empire_display_period_index == 0, "Current-year selection did not restore live trade");
        }
        if (sidebar_definition->has_widget("history")) {
            click_control("history"); require(window_is(WINDOW_TRADE_LEDGER), "Trade History did not open the ledger");
            window_empire_show(); window_draw(1);
            require(!empire_city_list.cards.empty(), "Trade History return lost the city list");
            auto &entry = *empire_city_list.cards.front();
            const auto &badge = *declarative_window_definition("empire_city_card")->widget("badge");
            mouse m{}; m.x = badge.x + 5; m.y = badge.y + 5; m.left.went_down = 1;
            entry.runtime.handle_mouse(m, entry.draw_width, 140); m.left.went_down = 0; m.left.went_up = 1; entry.runtime.handle_mouse(m, entry.draw_width, 140);
            require(window_is(WINDOW_TRADE_LEDGER), "City name badge did not open its ledger");
            window_empire_show(); window_draw(1);
        }
        window_empire_sidebar_sort_set_current_sorting(original_sort);
        window_empire_sidebar_sort_set_sorting_reversed(original_reverse);
        empire_city_list.initialize(); window_draw(1);
        require(!empire_city_list.cards.empty(), "Empire UI fixture needs trading cities");
        auto &card = *empire_city_list.cards.front();
        mouse title{}; title.x = 35; title.y = 40; title.left.went_up = 1;
        empire_city_list.handle(title);
        require(data.selected_city == card.city_id, "Clicking the city card did not select its map object");
        if (empire_city_list.cards.size() > empire_city_list.grid.scrollbar.elements_in_view) {
            const auto before = grid_box_get_scroll_position(&empire_city_list.grid);
            mouse wheel{}; wheel.x = 35; wheel.y = 15; wheel.scrolled = SCROLL_DOWN;
            empire_city_list.handle(wheel);
            require(grid_box_get_scroll_position(&empire_city_list.grid) > before, "Empire city list does not scroll");
        }
        bool checked_resource = false, checked_open = false;
        for (const auto &entry : empire_city_list.cards) {
            auto *city = empire_city_get(entry->city_id);
            empire_select_object_by_id(city->empire_object_id); process_selection();
            const auto *definition = declarative_window_definition("empire_city_card");
            if (!city->is_open && !checked_open) {
                const auto &widget = *definition->widget("open");
                mouse click{}; click.left.went_up = 1;
                entry->handle_custom(widget, -1, click, widget.width, widget.height);
                require(window_is(WINDOW_POPUP_DIALOG), "Route button did not open confirmation");
                checked_open = true;
                // Do not rebuild the card list while iterating it.
                window_go_back();
            }
            if (!checked_resource) {
                const int was_open = city->is_open;
                city->is_open = 1;
                for (const char *key : {"sells", "buys"}) {
                    const auto &widget = *definition->widget(key);
                    for (int x = 0; x < widget.width && !checked_resource; ++x) {
                        mouse click{}; click.x = x; click.y = 12; click.left.went_up = 1;
                        entry->handle_custom(widget, -1, click, widget.width, widget.height);
                        if (window_is(WINDOW_RESOURCE_SETTINGS)) { checked_resource = true; window_go_back(); }
                    }
                }
                city->is_open = was_open;
            }
        }
        require(checked_resource, "Fixture did not exercise a resource settings button");
        std::fprintf(stdout, "Empire city cards: selection, scroll, resource buttons checked; closed-route button=%d.\n", checked_open);
    }
    window_draw(1); window_draw(1);
    std::filesystem::create_directories("out/empire-ui-review");
    const int width = screen_pixel_width(), height = screen_pixel_height();
    std::vector<color_t> pixels(static_cast<size_t>(width) * height);
    require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width), "Cannot capture empire window");
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    require(surface != nullptr, "Cannot create empire screenshot");
    for (const auto &widget : details_definition->widgets()) {
        if (widget.type != DeclarativeWidgetType::ImageButton) continue;
        std::fprintf(stdout, "Empire button %s: x=%d y=%d image=%s/%s\n", widget.id.c_str(), widget.resolved_x(data.panel.x_max - data.panel.x_min, details_definition->base_width()) + data.panel.x_min, widget.y + data.y_max - BOTTOM_PANEL_HEIGHT, widget.assetlist_name.c_str(), widget.image_name.c_str());
    }
    const int saved = SDL_SaveBMP(surface, "out/empire-ui-review/empire.bmp"); SDL_FreeSurface(surface);
    require(saved == 0, "Cannot save empire screenshot");
    auto click = [&](const char *id) {
        const auto *widget = details_definition->widget(id);
        require(widget != nullptr, "Required empire navigation button missing");
        const int panel_width = data.panel.x_max - data.panel.x_min;
        mouse pointer{};
        pointer.x = widget->resolved_x(panel_width, details_definition->base_width()) + 5;
        pointer.y = widget->y + 5; pointer.left.went_down = 1;
        empire_details_ui->handle_mouse(pointer, panel_width, BOTTOM_PANEL_HEIGHT);
        pointer.left.went_down = 0; pointer.left.went_up = 1;
        empire_details_ui->handle_mouse(pointer, panel_width, BOTTOM_PANEL_HEIGHT);
    };
    click("prices"); require(window_is(WINDOW_TRADE_PRICES), "Prices button did not navigate");
    window_empire_show(); window_draw(1);
    click("advisor"); require(window_is(WINDOW_ADVISORS), "Advisor button did not navigate");
    window_empire_show(); window_draw(1);
    if (details_definition->has_widget("ledger")) {
        click("ledger"); require(window_is(WINDOW_TRADE_LEDGER), "Ledger button did not navigate");
        window_empire_show(); window_draw(1);
    }
    click("close"); require(window_is(WINDOW_CITY), "Return-to-city button did not navigate");
    window_advisors_show_advisor(ADVISOR_FINANCIAL); window_draw(1); capture_reference_frame("finance");
    const auto *finance = declarative_window_definition("advisor_financial");
    const auto *financial = window_advisor_financial();
    const int tax_before = city_finance_tax_percentage();
    const auto finance_click = [&](const char *id) {
        const auto &w = *finance->widget(id);
        mouse m{}; m.x = w.x + 4; m.y = w.y + 4; m.left.went_down = 1;
        financial->handle_mouse(&m); m.left.went_down = 0; m.left.went_up = 1; financial->handle_mouse(&m);
    };
    finance_click(tax_before < 25 ? "more" : "less");
    require(city_finance_tax_percentage() != tax_before, "Finance tax arrow did not change the tax rate");
    finance_click(tax_before < 25 ? "less" : "more");
    require(city_finance_tax_percentage() == tax_before, "Finance tax arrows did not restore the rate");
    if (finance->widget("previous") && finance->widget("previous")->type == DeclarativeWidgetType::Dropdown) {
        finance_click("previous"); window_draw(1); capture_reference_frame("finance-years");
        mouse choice{}; choice.x = finance->widget("previous")->x + 8; choice.y = finance->widget("previous")->y + 30; choice.left.went_up = 1;
        require(financial->handle_mouse(&choice), "Finance year menu did not consume selection");
        window_draw(1);
        // A long data-provided list must remain selectable in a compact window.
        class Choices final : public DeclarativeWindowController {
        public:
            int selected = -1;
            std::vector<std::string> choices(std::string_view) const override { return std::vector<std::string>(100, "Choice"); }
            void action(std::string_view, int index) override { selected = index; }
        } choices;
        DeclarativeWindowRuntime menu(*finance, choices);
        mouse m{}; m.x = finance->widget("previous")->x + 4; m.y = finance->widget("previous")->y + 4; m.left.went_down = 1;
        menu.handle_mouse(m, 640, 432); m.left.went_down = 0; m.left.went_up = 1; menu.handle_mouse(m, 640, 432);
        m.left.went_up = 0; m.scrolled = SCROLL_DOWN;
        for (int i = 0; i < 100; ++i) menu.handle_mouse(m, 640, 432);
        m.scrolled = SCROLL_NONE; m.x = finance->widget("previous")->x + 8; m.y = 408; m.left.went_up = 1;
        menu.handle_mouse(m, 640, 432);
        require(choices.selected == 99, "Long dropdown did not scroll to its final entry");
    }
    window_advisors_show_advisor(ADVISOR_RELIGION); window_draw(1); capture_reference_frame("religion");
    if (window_epithets_available()) { window_epithets_show(); require(window_is(WINDOW_EPITHETS), "Powers of the Gods did not open"); window_draw(1); capture_reference_frame("epithets"); window_epithets_validate_ui_for_test(); window_go_back(); }
    window_city_show();
    std::fprintf(stdout, "Empire XML navigation and image contracts passed (%s layout).\n", classic ? "city-list" : "accounting");
}
