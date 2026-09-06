#include "window/trade_ledger.h"
#include "city/trade_ledger.h"
#include "empire/city.h"
#include "game/time.h"
#include "graphics/declarative_window.h"
#include "graphics/graphics.h"
#include "graphics/window.h"
#include "input/input.h"
#include <algorithm>
#include <memory>
#include <numeric>
#include <set>

namespace {
std::string tr(const char *key) { return reinterpret_cast<const char *>(translation_for_key(key)); }
std::string resource_name(const std::string &key)
{
    const int resource = resource_type_from_text_id(key.c_str());
    const auto *data = resource != RESOURCE_NONE ? resource_get_data(resource) : nullptr;
    return data && data->text ? reinterpret_cast<const char *>(data->text) : key;
}
std::string city_name(int id)
{
    const auto *city = id > 0 && id < empire_city_get_array_size() ? empire_city_get(id) : nullptr;
    return city && city->in_use ? reinterpret_cast<const char *>(empire_city_get_name(city)) : tr("TR_LEDGER_UNKNOWN");
}
std::string quantity(int64_t units)
{
    const int load = resource_units_per_load();
    const int64_t remainder = std::abs(units % load);
    return std::to_string(units / load) + (remainder ? "." + (remainder < 10 ? std::string("0") : std::string()) + std::to_string(remainder) : "");
}
class LedgerController final : public DeclarativeWindowController {
public:
    size_t period_index = 0, period_count = 0;
    int tab = 0, page = 0, page_size = 14, direction = -1, city = -1, month = -1, storage = -1;
    uint64_t visit = 0;
    std::string resource, sort = "resource";
    bool descending = false;
    AccountingPeriod period;
    std::vector<std::pair<std::string, ResourceAccounts>> resources;
    std::vector<TradeTransaction> transactions;
    void refresh()
    {
        const auto &history = city_trade_ledger_periods();
        period_count = history.size();
        period_index = std::min(period_index, history.size() - 1);
        period = history[period_index];
        resources.assign(period.resources.begin(), period.resources.end());
        transactions.clear();
        for (const auto &t : period.transactions) {
            if ((!resource.empty() && t.resource != resource) || (city >= 0 && t.city != city) || (direction >= 0 && t.imported != bool(direction)) || (month >= 0 && t.month != month) || (storage >= 0 && t.storage != storage) || (visit && t.visit != visit)) continue;
            transactions.push_back(t);
        }
        auto resource_value = [this](const ResourceAccounts &a) {
            if (sort == "produced") return a.produced;
            if (sort == "consumed") return a.consumed;
            if (sort == "imported") return a.imported;
            if (sort == "exported") return a.exported;
            if (sort == "stock") return a.stock;
            if (sort == "income") return a.income;
            if (sort == "expense") return a.expense;
            return a.income - a.expense;
        };
        std::stable_sort(resources.begin(), resources.end(), [&](const auto &a, const auto &b) {
            const bool less = sort == "resource" ? resource_name(a.first) < resource_name(b.first) : resource_value(a.second) < resource_value(b.second);
            const bool greater = sort == "resource" ? resource_name(b.first) < resource_name(a.first) : resource_value(b.second) < resource_value(a.second);
            return descending ? greater : less;
        });
        std::stable_sort(transactions.begin(), transactions.end(), [&](const auto &a, const auto &b) {
            const auto value = [this](const TradeTransaction &t) -> int64_t {
                if (sort == "month") return t.month;
                if (sort == "city") return t.city;
                if (sort == "storage") return t.storage;
                if (sort == "visit") return t.visit;
                if (sort == "price") return t.price;
                if (sort == "value") return t.units * t.price / resource_units_per_load();
                return t.units;
            };
            const bool less = sort == "resource" ? resource_name(a.resource) < resource_name(b.resource) : value(a) < value(b);
            const bool greater = sort == "resource" ? resource_name(b.resource) < resource_name(a.resource) : value(b) < value(a);
            return descending ? greater : less;
        });
        page = std::min(page, std::max(0, (row_count() - 1) / page_size));
    }
    int row_count() const { return tab == 0 ? static_cast<int>(resources.size()) : tab == 1 ? static_cast<int>(transactions.size()) : 16; }
    int repeat_count(std::string_view source) const override { return source == "ledger.rows" ? std::max(0, std::min(page_size, row_count() - page * page_size)) : 0; }
    std::string text(std::string_view binding, int index) const override
    {
        if (binding == "period") return std::to_string(period.year) + (period.partial ? " - " + tr("TR_LEDGER_PARTIAL") + " " + std::to_string(period.start_month + 1) + "/" + std::to_string(period.start_day + 1) : "");
        if (binding == "page") return std::to_string(page + 1) + " / " + std::to_string(std::max(1, (row_count() + page_size - 1) / page_size));
        if (binding == "filter.resource") return tr("TR_LEDGER_RESOURCE") + ": " + (resource.empty() ? tr("main_strings.52.19") : resource_name(resource));
        if (binding == "filter.city") return tr("main_strings.44.9") + ": " + (city < 0 ? tr("main_strings.52.19") : city_name(city));
        if (binding == "filter.direction") return direction < 0 ? tr("TR_LEDGER_ALL_DIRECTIONS") : tr(direction ? "TR_LEDGER_IMPORT" : "TR_LEDGER_EXPORT");
        if (binding == "filter.month") return tr("main_strings.8.4") + ": " + (month < 0 ? tr("main_strings.52.19") : std::to_string(month + 1));
        if (binding == "filter.storage") return tr("TR_LEDGER_STORAGE") + ": " + (storage < 0 ? tr("main_strings.52.19") : std::to_string(storage));
        if (binding == "filter.visit") return tr("TR_LEDGER_VISIT") + ": " + (!visit ? tr("main_strings.52.19") : std::to_string(visit));
        const int row = page * page_size + index;
        if (row < 0 || row >= row_count()) return {};
        if (tab == 0) {
            const auto &[name, a] = resources[row];
            if (binding == "row.resource") return resource_name(name);
            if (binding == "row.produced") return quantity(a.produced);
            if (binding == "row.consumed") return quantity(a.consumed);
            if (binding == "row.imported") return quantity(a.imported);
            if (binding == "row.exported") return quantity(a.exported);
            if (binding == "row.stock") return quantity(a.stock);
            if (binding == "row.income") return std::to_string(a.income);
            if (binding == "row.expense") return std::to_string(a.expense);
            if (binding == "row.balance") return std::to_string(a.income - a.expense);
        } else if (tab == 1) {
            const auto &t = transactions[row];
            if (binding == "row.resource") return resource_name(t.resource);
            if (binding == "row.city") return city_name(t.city);
            if (binding == "row.month") return std::to_string(t.month + 1);
            if (binding == "row.direction") return tr(t.imported ? "TR_LEDGER_IMPORT" : "TR_LEDGER_EXPORT");
            if (binding == "row.storage") return std::to_string(t.storage);
            if (binding == "row.visit") return std::to_string(t.visit);
            if (binding == "row.units") return quantity(t.units);
            if (binding == "row.price") return std::to_string(t.price);
            if (binding == "row.value") return std::to_string(t.units * t.price / resource_units_per_load());
        } else {
            const auto &f = period.finance;
            const char *labels[] = {"main_strings.60.8", "main_strings.60.9", "main_strings.60.20", "TR_WINDOW_ADVISOR_TOURISM", "main_strings.60.10", "main_strings.60.11", "main_strings.60.12", "main_strings.60.13", "main_strings.60.14", "main_strings.60.15", "main_strings.60.16", "main_strings.60.21", "TR_ADVISOR_FINANCE_LEVIES", "main_strings.60.17", "main_strings.60.18", "main_strings.60.19"};
            const int values[] = {f.income.taxes, f.income.exports, f.income.donated, period.miscellaneous_income, f.income.total, f.expenses.imports, f.expenses.wages, f.expenses.construction, f.expenses.interest, f.expenses.salary, f.expenses.sundries, f.expenses.tribute, f.expenses.levies, f.expenses.total, f.net_in_out, f.balance};
            if (binding == "row.finance_name") return tr(labels[row]);
            if (binding == "row.finance_value") return std::to_string(values[row]);
        }
        return {};
    }
    int condition(std::string_view binding, int) const override
    {
        if (binding == "tab.resources") return tab == 0;
        if (binding == "tab.transactions") return tab == 1;
        if (binding == "tab.finance") return tab == 2;
        if (binding == "period.older") return period_index + 1 < period_count;
        if (binding == "period.newer") return period_index > 0;
        if (binding == "page.previous") return page > 0;
        if (binding == "page.next") return (page + 1) * page_size < row_count();
        if (binding == "empty") return row_count() == 0;
        return 0;
    }
    template<class T> void cycle(T &selected, std::set<T> values, T all)
    {
        auto next = values.upper_bound(selected);
        selected = next == values.end() ? all : *next;
    }
    void action(std::string_view action, int) override
    {
        if (action == "close") { window_go_back(); return; }
        if (action == "tab.resources") { tab = 0; page = 0; }
        else if (action == "tab.transactions") { tab = 1; page = 0; }
        else if (action == "tab.finance") { tab = 2; page = 0; }
        else if (action == "period.older" && condition(action, 0)) { ++period_index; page = 0; }
        else if (action == "period.newer" && period_index) { --period_index; page = 0; }
        else if (action == "page.previous" && page) --page;
        else if (action == "page.next" && condition(action, 0)) ++page;
        else if (action.substr(0, 5) == "sort.") { const std::string next(action.substr(5)); descending = sort == next ? !descending : false; sort = next; }
        else if (action == "filter.direction") direction = direction == 1 ? -1 : direction + 1;
        else if (action == "filter.month") month = month == 11 ? -1 : month + 1;
        else if (action == "filter.clear") { city = storage = month = direction = -1; resource.clear(); visit = 0; }
        else if (action == "filter.resource") { std::set<std::string> values; for (const auto &t : period.transactions) values.insert(t.resource); cycle(resource, values, std::string()); }
        else if (action == "filter.city") { std::set<int> values; for (const auto &t : period.transactions) values.insert(t.city); cycle(city, values, -1); }
        else if (action == "filter.storage") { std::set<int> values; for (const auto &t : period.transactions) values.insert(t.storage); cycle(storage, values, -1); }
        else if (action == "filter.visit") { std::set<uint64_t> values; for (const auto &t : period.transactions) values.insert(t.visit); cycle(visit, values, uint64_t(0)); }
        if (action.substr(0, 7) == "filter.") page = 0;
        refresh();
        window_invalidate();
    }
};
LedgerController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
const DeclarativeWindowDefinition *definition;
void background() { window_draw_underlying_window(); graphics_in_dialog_with_size(definition->base_width(), definition->base_height()); runtime->draw(DeclarativeDrawPhase::Background, definition->base_width(), definition->base_height()); graphics_reset_dialog(); }
void foreground() { graphics_in_dialog_with_size(definition->base_width(), definition->base_height()); runtime->draw(DeclarativeDrawPhase::Foreground, definition->base_width(), definition->base_height()); graphics_reset_dialog(); }
void input(const mouse *m, const hotkeys *keys)
{
    const mouse *dialog = mouse_in_dialog_with_size(m, definition->base_width(), definition->base_height());
    if (runtime->handle_mouse(*dialog, definition->base_width(), definition->base_height())) return;
    if (m->scrolled == SCROLL_UP) controller.action("page.previous", 0);
    if (m->scrolled == SCROLL_DOWN) controller.action("page.next", 0);
    if (input_go_back_requested(m, keys)) window_go_back();
}
}
void window_trade_ledger_show(int city, int resource)
{
    definition = declarative_window_definition("trade_ledger");
    if (!definition || !definition->widget("resource_rows")) return;
    controller = LedgerController();
    controller.city = city;
    if (resource > RESOURCE_NONE) controller.resource = resource_text_id(static_cast<resource_type>(resource));
    if (city >= 0 || resource > RESOURCE_NONE) controller.tab = 1;
    controller.page_size = std::max(1, definition->widget("resource_rows")->height / std::max(1, definition->widget("resource_rows")->repeat_spacing_y));
    controller.refresh();
    runtime = std::make_unique<DeclarativeWindowRuntime>(*definition, controller);
    window_type window = {WINDOW_TRADE_LEDGER, background, foreground, input};
    window_show(&window);
}
