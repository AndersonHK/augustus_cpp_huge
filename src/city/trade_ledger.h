#pragma once

#include "city/finance.h"
#include "core/buffer.h"
#include "game/resource.h"
#include <cstdint>
#include <map>
#include <string>
#include <vector>

class Figure;

struct ResourceAccounts {
    int64_t produced = 0, consumed = 0, imported = 0, exported = 0, income = 0, expense = 0, stock = 0;
    // Imported history may retain a net balance without its gross cash flows.
    int64_t balance_adjustment = 0;
    bool complete_cash_flows = true;
    int64_t balance() const { return income - expense + balance_adjustment; }
};

struct TradeTransaction {
    std::string resource;
    uint64_t visit = 0;
    int city = 0, storage = 0, month = 0, price = 0;
    bool imported = false;
    int64_t units = 0;
    bool direction_known = true;
    int source_trader = -1, source_storage = -1; // foreign references, never native visit/building identities
};

struct RouteResourceAccounts {
    int import_limit = 0, export_limit = 0, imported = 0, exported = 0;
};

struct RouteAccounts {
    bool open = false, sea = false;
    int cost = 0;
    std::map<std::string, RouteResourceAccounts> resources;
};

struct AccountingPeriod {
    int year = 0, start_month = 0, start_day = 0;
    bool partial = false;
    finance_overview finance = {};
    int miscellaneous_income = 0;
    std::map<std::string, ResourceAccounts> resources;
    std::vector<TradeTransaction> transactions;
    // City identities are stable within the scenario; resources use their authored identities.
    std::map<int, RouteAccounts> routes;
};

// A synchronous exchange scope carries the actual caravan/ship through storage APIs.
class TradeLedgerContext {
public:
    explicit TradeLedgerContext(const Figure *trader);
    ~TradeLedgerContext();
    TradeLedgerContext(const TradeLedgerContext &) = delete;
    TradeLedgerContext &operator=(const TradeLedgerContext &) = delete;
private:
    uint64_t previous_visit_;
    int previous_city_;
};

void city_trade_ledger_forget_figure(int id);
void city_trade_ledger_reset(bool imported_save = false);

void city_trade_ledger_exchange(resource_type resource, int loads, int unit_price, bool imported, int storage);
void city_trade_ledger_produced(resource_type resource, int units);
void city_trade_ledger_consumed(resource_type resource, int units);
void city_trade_ledger_revert_consumed(resource_type resource, int units, int year);
void city_trade_ledger_year_change();
const std::vector<AccountingPeriod> &city_trade_ledger_periods();
void city_trade_ledger_save(buffer *buf);
void city_trade_ledger_load(buffer *buf, bool has_route_history = true, bool has_balance_adjustments = true);
void city_trade_ledger_import(std::vector<AccountingPeriod> history);
