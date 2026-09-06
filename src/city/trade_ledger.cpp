#include "city/data_private.h"
#include "city/trade_ledger.h"
#include "city/resource.h"
#include "core/log.h"
#include "figure/figure.h"
#include "game/time.h"
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <tuple>

namespace {
constexpr size_t history_years = 8;
std::vector<AccountingPeriod> periods;
struct VisitIdentity { uint32_t sequence = 0; uint64_t visit = 0; };
std::map<int, VisitIdentity> visits;
uint64_t next_visit = 1, current_visit = 0;
int current_city = 0;
bool imported_history = false;
using TransactionKey = std::tuple<std::string, uint64_t, int, int, int, int, bool>;
std::map<TransactionKey, size_t> transaction_index;

TransactionKey key(const TradeTransaction &t) { return {t.resource, t.visit, t.city, t.storage, t.month, t.price, t.imported}; }
std::string identity(resource_type resource)
{
    const char *id = resource_text_id(resource);
    return id ? id : "";
}

AccountingPeriod &current()
{
    if (periods.empty()) {
        AccountingPeriod period;
        period.year = game_time_year();
        period.start_month = game_time_month();
        period.start_day = game_time_day();
        period.partial = imported_history || period.start_month || period.start_day;
        periods.push_back(std::move(period));
    }
    return periods.front();
}

void snapshot(AccountingPeriod &period)
{
    for (int i = 0; i < resource_loaded_count(); ++i) {
        const resource_type resource = resource_get_loaded(i);
        if (resource == RESOURCE_NONE || resource_is_special(resource)) continue;
        const auto name = identity(resource);
        if (name.empty()) continue;
        period.resources[name].stock = static_cast<int64_t>(city_resource_get_total_amount(resource, 0)) * resource_units_per_load();
    }
}

void write64(buffer *buf, uint64_t value) { buffer_write_u32(buf, static_cast<uint32_t>(value)); buffer_write_u32(buf, static_cast<uint32_t>(value >> 32)); }
uint64_t read64(buffer *buf) { const uint64_t low = buffer_read_u32(buf); return low | (static_cast<uint64_t>(buffer_read_u32(buf)) << 32); }
void write_string(buffer *buf, const std::string &value) { buffer_write_u16(buf, static_cast<uint16_t>(value.size())); buffer_write_raw(buf, value.data(), value.size()); }
std::string read_string(buffer *buf)
{
    const uint16_t size = buffer_read_u16(buf);
    if (!size || size > 4096 || buf->index > buf->size || size > buf->size - buf->index) throw std::runtime_error("Invalid accounting resource identity");
    std::string result(size, '\0');
    buffer_read_raw(buf, result.data(), size);
    if (result.find('\0') != std::string::npos) throw std::runtime_error("Invalid accounting resource identity");
    return result;
}

// Explicit field order: never serialize the native struct or process-local resource IDs.
template<class F> void finance_fields(finance_overview &f, F field)
{
    field(f.income.taxes); field(f.income.exports); field(f.income.donated); field(f.income.total);
    field(f.expenses.imports); field(f.expenses.wages); field(f.expenses.construction); field(f.expenses.interest);
    field(f.expenses.salary); field(f.expenses.sundries); field(f.expenses.tribute); field(f.expenses.total);
    field(f.expenses.levies); field(f.net_in_out); field(f.balance);
}
template<class F> void resource_fields(ResourceAccounts &r, F field)
{
    field(r.produced); field(r.consumed); field(r.imported); field(r.exported); field(r.income); field(r.expense); field(r.stock);
}
}

TradeLedgerContext::TradeLedgerContext(const Figure *trader) : previous_visit_(current_visit), previous_city_(current_city)
{
    current_visit = 0;
    current_city = 0;
    if (trader) {
        auto &identity = visits[trader->id()];
        if (!identity.visit || identity.sequence != trader->created_sequence) identity = {trader->created_sequence, next_visit++};
        current_visit = identity.visit;
        current_city = trader->empire_city_id;
    }
}
TradeLedgerContext::~TradeLedgerContext() { current_visit = previous_visit_; current_city = previous_city_; }

void city_trade_ledger_forget_figure(int id) { visits.erase(id); }

void city_trade_ledger_reset(bool imported_save)
{
    periods.clear(); visits.clear(); transaction_index.clear();
    next_visit = 1; current_visit = 0; current_city = 0; imported_history = imported_save;
}

void city_trade_ledger_exchange(resource_type resource, int loads, int price, bool imported, int storage)
{
    if (resource == RESOURCE_NONE || loads <= 0) return;
    const std::string name = identity(resource);
    if (name.empty()) return;
    const int64_t units = static_cast<int64_t>(loads) * resource_units_per_load();
    auto &period = current();
    auto &totals = period.resources[name];
    if (imported) { totals.imported += units; totals.expense += static_cast<int64_t>(loads) * price; }
    else { totals.exported += units; totals.income += static_cast<int64_t>(loads) * price; }
    TradeTransaction transaction{name, current_visit, current_city, storage, game_time_month(), price, imported, units};
    const auto found = transaction_index.find(key(transaction));
    if (found != transaction_index.end()) period.transactions[found->second].units += units;
    else {
        transaction_index.emplace(key(transaction), period.transactions.size());
        period.transactions.push_back(std::move(transaction));
    }
}
void city_trade_ledger_produced(resource_type resource, int units)
{
    if (resource != RESOURCE_NONE && !resource_is_special(resource) && units > 0) current().resources[identity(resource)].produced += units;
}
void city_trade_ledger_consumed(resource_type resource, int units)
{
    if (resource != RESOURCE_NONE && !resource_is_special(resource) && units > 0) current().resources[identity(resource)].consumed += units;
}

void city_trade_ledger_year_change()
{
    const bool no_activity = periods.empty();
    auto &previous = current();
    if (no_activity) previous.year = game_time_year() - 1;
    snapshot(previous);
    previous.finance = *city_finance_overview_last_year();
    previous.miscellaneous_income = city_data.finance.misc_last_year;
    AccountingPeriod period;
    period.year = game_time_year();
    periods.insert(periods.begin(), std::move(period));
    if (periods.size() > history_years) periods.resize(history_years);
    transaction_index.clear();
}
const std::vector<AccountingPeriod> &city_trade_ledger_periods()
{
    auto &period = current();
    period.finance = *city_finance_overview_this_year();
    period.miscellaneous_income = city_data.finance.misc_this_year;
    snapshot(period);
    return periods;
}

void city_trade_ledger_save(buffer *buf)
{
    city_trade_ledger_periods();
    write64(buf, next_visit);
    buffer_write_u32(buf, static_cast<uint32_t>(visits.size()));
    for (const auto &[slot, visit] : visits) { buffer_write_i32(buf, slot); buffer_write_u32(buf, visit.sequence); write64(buf, visit.visit); }
    buffer_write_u32(buf, static_cast<uint32_t>(periods.size()));
    for (auto &period : periods) {
        buffer_write_i32(buf, period.year); buffer_write_i32(buf, period.start_month); buffer_write_i32(buf, period.start_day);
        buffer_write_u8(buf, period.partial);
        finance_fields(period.finance, [buf](int &value) { buffer_write_i32(buf, value); });
        buffer_write_i32(buf, period.miscellaneous_income);
        buffer_write_u32(buf, static_cast<uint32_t>(period.resources.size()));
        for (auto &[resource, totals] : period.resources) {
            write_string(buf, resource);
            resource_fields(totals, [buf](int64_t &value) { write64(buf, static_cast<uint64_t>(value)); });
        }
        buffer_write_u32(buf, static_cast<uint32_t>(period.transactions.size()));
        for (const auto &t : period.transactions) {
            write_string(buf, t.resource); write64(buf, t.visit); buffer_write_i32(buf, t.city); buffer_write_i32(buf, t.storage);
            buffer_write_i32(buf, t.month); buffer_write_i32(buf, t.price); buffer_write_u8(buf, t.imported); write64(buf, t.units);
        }
    }
}

void city_trade_ledger_load(buffer *buf)
{
    city_trade_ledger_reset();
    try {
        next_visit = read64(buf);
        const uint32_t visit_count = buffer_read_u32(buf);
        if (!next_visit || visit_count > 1000000) throw std::runtime_error("Invalid accounting trader visits");
        for (uint32_t i = 0; i < visit_count; ++i) {
            const int slot = buffer_read_i32(buf);
            const uint32_t sequence = buffer_read_u32(buf);
            const uint64_t visit = read64(buf);
            if (slot <= 0 || slot > 1000000 || !visit || visit >= next_visit || visits.count(slot)) throw std::runtime_error("Invalid accounting trader visit");
            visits[slot] = {sequence, visit};
        }
        const uint32_t count = buffer_read_u32(buf);
        if (!count || count > history_years) throw std::runtime_error("Invalid accounting history length");
        for (uint32_t i = 0; i < count; ++i) {
            AccountingPeriod period;
            period.year = buffer_read_i32(buf); period.start_month = buffer_read_i32(buf); period.start_day = buffer_read_i32(buf);
            period.partial = buffer_read_u8(buf) != 0;
            if (period.start_month < 0 || period.start_month >= 12 || period.start_day < 0 || period.start_day >= 31) throw std::runtime_error("Invalid accounting period");
            finance_fields(period.finance, [buf](int &value) { value = buffer_read_i32(buf); });
            period.miscellaneous_income = buffer_read_i32(buf);
            const uint32_t resources = buffer_read_u32(buf);
            if (resources > 4096) throw std::runtime_error("Invalid accounting resource count");
            for (uint32_t r = 0; r < resources; ++r) {
                auto name = read_string(buf);
                if (period.resources.count(name)) throw std::runtime_error("Duplicate accounting resource");
                resource_fields(period.resources[name], [buf](int64_t &value) { value = static_cast<int64_t>(read64(buf)); });
            }
            const uint32_t transactions = buffer_read_u32(buf);
            if (transactions > (buf->size - std::min(buf->index, buf->size)) / 36) throw std::runtime_error("Invalid accounting transaction count");
            for (uint32_t t = 0; t < transactions; ++t) {
                TradeTransaction transaction;
                transaction.resource = read_string(buf); transaction.visit = read64(buf);
                transaction.city = buffer_read_i32(buf); transaction.storage = buffer_read_i32(buf);
                transaction.month = buffer_read_i32(buf); transaction.price = buffer_read_i32(buf);
                transaction.imported = buffer_read_u8(buf) != 0; transaction.units = static_cast<int64_t>(read64(buf));
                if (transaction.month < 0 || transaction.month >= 12 || transaction.units <= 0) throw std::runtime_error("Invalid accounting transaction");
                period.transactions.push_back(std::move(transaction));
            }
            periods.push_back(std::move(period));
        }
        if (buf->overflow) throw std::runtime_error("Truncated accounting history");
        for (size_t i = 0; i < periods.front().transactions.size(); ++i) transaction_index.emplace(key(periods.front().transactions[i]), i);
    } catch (const std::exception &error) {
        // Accounting is auxiliary history: losing damaged history must not strand the city.
        log_warning("Repaired unreadable accounting history", error.what(), 0);
        city_trade_ledger_reset(true);
    }
}
