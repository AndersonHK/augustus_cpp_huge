#pragma once

// Empire-specific content for XML windows. Layout, decorations and ordinary
// buttons live in the mod; these widgets lay out variable-length trade data.
namespace {
const AccountingPeriod *empire_display_period = nullptr;
size_t empire_display_period_index = 0;
class EmpireTradeController : public DeclarativeWindowController {
public:
    virtual const empire_city *trade_city() const = 0;
    virtual int trade_city_id() const = 0;
    mutable empire_city display_copy{};
    const empire_city *display_city() const
    {
        const auto *city = trade_city();
        if (!city || !empire_display_period_index || !empire_display_period) return city;
        const auto found = empire_display_period->routes.find(trade_city_id());
        if (found == empire_display_period->routes.end()) return nullptr;
        display_copy = *city;
        display_copy.is_open = found->second.open; display_copy.is_sea_trade = found->second.sea; display_copy.cost_to_open = found->second.cost;
        for (int i = 0; i < resource_loaded_count(); ++i) {
            const auto r = resource_get_loaded(i); const auto entry = found->second.resources.find(resource_text_id(r));
            display_copy.sells_resource[r] = entry != found->second.resources.end() && entry->second.import_limit > 0;
            display_copy.buys_resource[r] = entry != found->second.resources.end() && entry->second.export_limit > 0;
        }
        return &display_copy;
    }
    int trade_amount(resource_type resource, bool sells, bool limit) const
    {
        if (empire_display_period_index && empire_display_period) {
            const auto route = empire_display_period->routes.find(trade_city_id());
            if (route == empire_display_period->routes.end()) return 0;
            const auto entry = route->second.resources.find(resource_text_id(resource));
            if (entry == route->second.resources.end()) return 0;
            return limit ? (sells ? entry->second.import_limit : entry->second.export_limit) : (sells ? entry->second.imported : entry->second.exported);
        }
        return limit ? trade_route_limit(trade_city()->route_id, resource, !sells) : trade_route_traded(trade_city()->route_id, resource, !sells);
    }

    int trade_row(const DeclarativeWidgetDefinition &widget, const empire_city &city, bool sells, int x, int y, int width, bool draw, const mouse *pointer = nullptr) const
    {
        const font_t font = widget.font;
        const int font_height = screen_ui_to_pixel(font_definition_for(font)->line_height);
        const bool compact = widget.repeat_columns > 1;
        const char *label = city.is_open ? (sells ? "main_strings.47.10" : "main_strings.47.9") : (sells ? "main_strings.47.5" : "main_strings.47.4");
        std::vector<resource_type> resources;
        for (int i = 0; i < resource_loaded_count(); ++i) {
            const auto r = resource_get_loaded(i);
            if (resource_is_storable(r) && (sells ? city.sells_resource[r] : city.buys_resource[r])) resources.push_back(r);
        }
        if (resources.empty()) return 0;
        const int label_width = lang_text_get_width(label, font, font_height);
        const int indent = city.is_open ? std::max(lang_text_get_width("main_strings.47.10", font, font_height), lang_text_get_width("main_strings.47.9", font, font_height)) : label_width;
        if (draw) lang_text_draw(label, x, y + widget.text_offset_y, font, font_height);
        int cursor = indent + widget.padding_x;
        for (const auto r : resources) {
            const int limit = trade_amount(r, sells, true);
            const int traded = trade_amount(r, sells, false);
            const std::string amount = city.is_open ? std::to_string(traded) + " " + reinterpret_cast<const char *>(translation_for_key("main_strings.47.11")) + " " + std::to_string(limit) : std::to_string(limit);
            const int amount_width = text_get_width(reinterpret_cast<const uint8_t *>(amount.c_str()), font, font_height);
            const int segment = RESOURCE_ICON_WIDTH + (compact ? 2 : 6) + amount_width + (compact ? 4 : 12);
            if (cursor + segment > width) {
                if (draw) text_draw_ellipsized(reinterpret_cast<const uint8_t *>("(...)"), x + cursor, y + widget.text_offset_y, std::max(0, width - cursor), font, font_height, 0);
                break;
            }
            if (draw) {
                graphics_draw_inset_rect(x + cursor - 1, y - 1, 26, 26, COLOR_INSET_DARK, COLOR_INSET_LIGHT);
                resource_graphics(r).empire_icon().draw(x + cursor, y);
                // The original three quota badges belong to Julius too.
                if (limit == 15 || limit == 25 || limit == 40) {
                    const int badge = limit == 15 ? 0 : limit == 25 ? 1 : 2;
                    Image::from_id(Image::group(GROUP_TRADE_AMOUNT) + badge).draw(x + cursor + 21 - 4 * badge, y - 1);
                }
                text_draw(reinterpret_cast<const uint8_t *>(amount.c_str()), x + cursor + RESOURCE_ICON_WIDTH + (compact ? 2 : 6), y + widget.text_offset_y, font, font_height, 0);
                if (data.focus_resource == r) button_border_draw(x + cursor - 2, y - 2, segment, 29, 1);
            }
            if (pointer && pointer->x >= cursor && pointer->x < cursor + segment) {
                data.focus_resource = r;
                if (pointer->left.went_up && city.is_open && !empire_display_period_index) window_resource_settings_show(r);
            }
            cursor += segment;
        }
        return cursor;
    }

    void draw_custom(const DeclarativeWidgetDefinition &widget, int item, int x, int y, int width, int height, bool focused) const override
    {
        const auto *city = display_city();
        if (!city) return;
        if (widget.binding == "trade.sells" || widget.binding == "trade.buys") {
            trade_row(widget, *city, widget.binding == "trade.sells", x, y, width, true);
        } else if (widget.binding == "trade.closed") {
            const int used = trade_row(widget, *city, true, x, y, width, true) + widget.padding_y;
            trade_row(widget, *city, false, x + used, y, std::max(0, width - used), true);
        } else if (widget.binding == "trade.open") {
            EmpireTradeRouteButtonSpec spec{};
            spec.cost_text.content_type = UiTextContentType::Amount;
            spec.cost_text.text_group = 8; spec.cost_text.text_id = 0; spec.cost_text.value = city->cost_to_open;
            spec.cost_text.x = x + widget.padding_x; spec.cost_text.y = y + widget.text_offset_y; spec.cost_text.font = widget.font;
            const int cost_width = lang_text_get_amount_width(current_string_amount_key(8, 0, city->cost_to_open), city->cost_to_open, widget.font, screen_ui_to_pixel(font_definition_for(widget.font)->line_height));
            spec.draw_label = width > cost_width + 160;
            spec.label_text.content_type = UiTextContentType::Language;
            spec.label_text.text_group = 47; spec.label_text.text_id = 6;
            spec.label_text.x = spec.cost_text.x + cost_width; spec.label_text.y = spec.cost_text.y; spec.label_text.font = widget.font;
            spec.icon_image_id = Image::group(GROUP_EMPIRE_TRADE_ROUTE_TYPE) + 1 - city->is_sea_trade;
            spec.icon_x = x + width - 38; spec.icon_y = y + 2 + 2 * city->is_sea_trade;
            EmpireTradeRouteButtonWidget(shared_ui_runtime().primitives(), x, y, width, height, focused, spec).draw();
        } else {
            DeclarativeWindowController::draw_custom(widget, item, x, y, width, height, focused);
        }
    }

    int handle_custom(const DeclarativeWidgetDefinition &widget, int, const mouse &local, int width, int) override
    {
        const auto *city = display_city();
        if (!city) return 0;
        if (widget.binding == "trade.open") {
            if (local.left.went_up && !city->is_open && !empire_display_period_index) button_open_trade_by_route(city->route_id);
            return 1;
        }
        if (widget.binding == "trade.closed") {
            const int used = trade_row(widget, *city, true, 0, 0, width, false, &local) + widget.padding_y;
            mouse second = local; second.x -= used;
            trade_row(widget, *city, false, 0, 0, std::max(0, width - used), false, &second);
        } else {
            trade_row(widget, *city, widget.binding == "trade.sells", 0, 0, width, false, &local);
        }
        window_invalidate();
        return 1;
    }
};

class EmpireCityCard final : public EmpireTradeController {
public:
    int city_id;
    int draw_width = 0;
    DeclarativeWindowRuntime runtime;
    EmpireCityCard(int id, const DeclarativeWindowDefinition &definition) : city_id(id), runtime(definition, *this) {}
    const empire_city *trade_city() const override { return empire_city_get(city_id); }
    int trade_city_id() const override { return city_id; }
    std::string text(std::string_view binding, int) const override
    {
        return binding == "city.name" ? reinterpret_cast<const char *>(empire_city_get_name(trade_city())) : "";
    }
    ImageGroupEntryRef image(std::string_view binding, int) const override
    {
        if (binding == "city.route_type") return ImageGroupEntryRef::from_group("PaperMap\\Empire_Trade_Route_Type", trade_city()->is_sea_trade ? "Image_0000" : "Image_0001");
        return {};
    }
    int condition(std::string_view binding, int) const override
    {
        if (binding == "city.badge_fits") return draw_width >= 272;
        if (binding == "city.open") return display_city() && display_city()->is_open;
        if (binding == "city.closed") return display_city() && !display_city()->is_open;
        if (binding == "city.selected") return city_id == data.selected_city;
        return 0;
    }
    const char *tooltip(std::string_view binding, int) const override { return binding == "city.ledger" ? "TR_UI_TOOLTIP_OPEN_TRADE_LEDGER" : nullptr; }
    void action(std::string_view binding, int) override
    {
        if (binding == "city.ledger") { window_trade_ledger_show(city_id, -1, static_cast<int>(empire_display_period_index)); return; }
        if (binding == "city.select") {
            empire_select_object_by_id(trade_city()->empire_object_id);
            process_selection(); window_invalidate();
        }
    }
};

class EmpireCityList {
public:
    grid_box_type grid{};
    bool hovered = false;
    std::vector<std::unique_ptr<EmpireCityCard>> cards;
    static EmpireCityList *drawing;
    void initialize(const std::vector<int> *ordered = nullptr)
    {
        cards.clear(); grid = {};
        const auto *definition = declarative_window_definition("empire_city_card");
        if (!definition) { Logger::error("Missing required empire_city_card window."); return; }
        for (int i = 1; i < empire_city_get_array_size(); ++i) {
            const auto *city = empire_city_get(i);
            if (city->in_use && city->type == EMPIRE_CITY_TRADE && window_empire_sidebar_sort_city_matches_current_filter(city)) cards.push_back(std::make_unique<EmpireCityCard>(i, *definition));
        }
        std::stable_sort(cards.begin(), cards.end(), [](const auto &left, const auto &right) {
            sidebar_city_entry a{}, b{};
            a.city_id = left->city_id; b.city_id = right->city_id;
            return window_empire_sidebar_sort_sidebar_city_sorter(&a, &b) < 0;
        });
        if (ordered) {
            cards.clear();
            for (int id : *ordered) cards.push_back(std::make_unique<EmpireCityCard>(id, *definition));
        }
        grid.item_height = definition->base_height(); grid.num_columns = 1;
        grid.item_margin.vertical = 5; grid.extend_to_hidden_scrollbar = 1; grid.decorate_scrollbar = 1;
        grid.draw_item = [](const grid_box_item *item) {
            if (!drawing || item->index >= drawing->cards.size()) return;
            auto &card = *drawing->cards[item->index];
            card.draw_width = item->width;
            graphics_set_clip_rectangle(item->x, item->y, item->width, item->height);
            if (item->is_focused) data.hovered_object = card.trade_city()->empire_object_id + 1;
            for (auto phase : {DeclarativeDrawPhase::Background, DeclarativeDrawPhase::Foreground}) card.runtime.draw(phase, item->width, item->height, item->x, item->y);
            graphics_set_clip_rectangle(drawing->grid.x, drawing->grid.y, drawing->grid.width, drawing->grid.height);
        };
        grid_box_init(&grid, static_cast<unsigned int>(cards.size()));
    }
    void draw(int x, int y, int width, int height)
    {
        drawing = this;
        grid_box_set_bounds(&grid, x, y, std::max(1, width), std::max(1, height));
        graphics_set_clip_rectangle(x, y, width, height);
        grid_box_request_refresh(&grid);
        grid_box_draw(&grid);
        graphics_reset_clip_rectangle();
        drawing = nullptr;
    }
    void tooltip(tooltip_context &context) const
    {
        const auto &item = grid.focused_item;
        if (hovered && item.index < cards.size()) cards[item.index]->runtime.tooltip(context);
    }
    int handle(const mouse &local)
    {
        hovered = true;
        mouse global = local; global.x += grid.x; global.y += grid.y;
        grid_box_handle_input(&grid, &global, 1);
        const auto &item = grid.focused_item;
        if (item.index < cards.size()) {
            mouse card_mouse = global; card_mouse.x -= item.x; card_mouse.y -= item.y;
            auto &card = *cards[item.index];
            if (!card.runtime.handle_mouse(card_mouse, item.width, item.height) && local.left.went_up) card.action("city.select", -1);
        }
        window_invalidate();
        return 1;
    }
};
EmpireCityList *EmpireCityList::drawing = nullptr;
EmpireCityList empire_city_list;
}
