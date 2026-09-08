#include "financial.h"
#include "city/data_private.h"
#include "city/finance.h"
#include "city/trade_ledger.h"
#include "graphics/lang_text.h"
#include <algorithm>
#include "graphics/declarative_window.h"
#include "graphics/window.h"
#include "window/trade_ledger.h"
#include <memory>

namespace {
class FinancialController final : public DeclarativeWindowController {
public:
    int years[2] = {1, 0};
    static std::string tr(const char *key) { return reinterpret_cast<const char *>(translation_for_key(key)); }
    std::vector<std::string> choices(std::string_view) const override
    {
        std::vector<std::string> result{tr("TR_UI_CURRENT_YEAR"), tr("TR_UI_LAST_YEAR")};
        const auto &history = city_trade_ledger_periods();
        for (size_t i = 2; i < history.size(); ++i) result.push_back(std::to_string(history[i].year));
        return result;
    }
    std::string text(std::string_view binding, int) const override
    {
        if (binding == "finance.treasury_line") {
            const int value = city_finance_treasury();
            return tr(value < 0 ? "main_strings.60.3" : "main_strings.60.2") + " " + std::to_string(std::abs(value)) + " " + tr(current_string_amount_key(8, 0, std::abs(value)).c_str());
        }
        if (binding == "finance.tax_line") return std::to_string(city_finance_tax_percentage()) + "% " + tr("main_strings.60.4") + " " + std::to_string(city_finance_estimated_tax_income()) + " " + tr(current_string_amount_key(8, 0, city_finance_estimated_tax_income()).c_str());
        if (binding == "finance.taxpayers_line") return std::to_string(city_finance_percentage_taxed_people()) + "% " + tr("main_strings.60.5");
        if (binding == "previous.year" || binding == "current.year") {
            const auto labels = choices(binding); const int year = years[binding == "previous.year" ? 0 : 1];
            return year < labels.size() ? labels[year] : labels[1];
        }
        if (binding == "finance.treasury") return std::to_string(city_finance_treasury());
        if (binding == "finance.tax") return std::to_string(city_finance_tax_percentage()) + "%";
        if (binding == "finance.estimate") return std::to_string(city_finance_estimated_tax_income());
        if (binding == "finance.taxpayers") return std::to_string(city_finance_percentage_taxed_people()) + "%";
        const bool previous = binding.substr(0, 9) == "previous.";
        if (!previous && binding.substr(0, 8) != "current.") return {};
        binding.remove_prefix(previous ? 9 : 8);
        const int year = years[previous ? 0 : 1];
        const auto &history = city_trade_ledger_periods();
        const auto &f = year < history.size() ? history[year].finance : *city_finance_overview_last_year();
        if (binding == "taxes") return std::to_string(f.income.taxes);
        if (binding == "exports") return std::to_string(f.income.exports);
        if (binding == "tourism") return std::to_string(year < history.size() ? history[year].miscellaneous_income : city_data.finance.misc_last_year);
        if (binding == "donated") return std::to_string(f.income.donated);
        if (binding == "income") return std::to_string(f.income.total);
        if (binding == "imports") return std::to_string(f.expenses.imports);
        if (binding == "wages") return std::to_string(f.expenses.wages);
        if (binding == "construction") return std::to_string(f.expenses.construction);
        if (binding == "levies") return std::to_string(f.expenses.levies);
        if (binding == "salary") return std::to_string(f.expenses.salary);
        if (binding == "sundries") return std::to_string(f.expenses.sundries);
        if (binding == "interest") return std::to_string(f.expenses.interest);
        if (binding == "tribute") return std::to_string(f.expenses.tribute);
        if (binding == "interest_tribute") return std::to_string(f.expenses.interest + f.expenses.tribute);
        if (binding == "expenses") return std::to_string(f.expenses.total);
        if (binding == "net") return std::to_string(f.net_in_out);
        if (binding == "balance") return std::to_string(f.balance);
        return {};
    }
    void action(std::string_view action, int index) override
    {
        if (action == "previous.year" || action == "current.year") { if (index >= 0 && index < choices(action).size()) years[action == "previous.year" ? 0 : 1] = index; window_invalidate(); return; }
        if (action == "ledger.show") { window_trade_ledger_show(); return; }
        if (action != "tax.less" && action != "tax.more") return;
        city_finance_change_tax_percentage(action == "tax.less" ? -1 : 1);
        city_finance_estimate_taxes();
        city_finance_calculate_totals();
        window_invalidate();
    }
};
FinancialController controller;
std::unique_ptr<DeclarativeWindowRuntime> runtime;
const DeclarativeWindowDefinition *definition;
int draw_background() { if (runtime) runtime->draw(DeclarativeDrawPhase::Background, definition->base_width(), definition->base_height()); return definition ? (definition->base_height() + 15) / 16 : 27; }
void draw_foreground() { if (runtime) runtime->draw(DeclarativeDrawPhase::Foreground, definition->base_width(), definition->base_height()); }
int handle_mouse(const mouse *m) { return runtime ? runtime->handle_mouse(*m, definition->base_width(), definition->base_height()) : 0; }
void tooltip(advisor_tooltip_result *) {}
}
const advisor_window_type *window_advisor_financial()
{
    definition = declarative_window_definition("advisor_financial");
    runtime = definition ? std::make_unique<DeclarativeWindowRuntime>(*definition, controller) : nullptr;
    static const advisor_window_type window = {draw_background, draw_foreground, handle_mouse, tooltip};
    return &window;
}
