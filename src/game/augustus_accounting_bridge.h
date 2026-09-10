#pragma once

#include "game/augustus_record_bridge.h"
#include <array>
#include <stdexcept>

namespace augustus_save {

class PayloadReader {
public:
    explicit PayloadReader(std::span<const uint8_t> bytes) : bytes_(bytes) {}
    uint8_t u8() { require(1); return bytes_[offset_++]; }
    uint16_t u16() { require(2); const auto value = read_u16(bytes_, offset_); offset_ += 2; return value; }
    uint32_t u32() { require(4); const auto value = read_u32(bytes_, offset_); offset_ += 4; return value; }
    int32_t i32() { return static_cast<int32_t>(u32()); }
    size_t remaining() const { return bytes_.size() - offset_; }
    void require(size_t bytes) const { if (bytes > remaining()) throw std::runtime_error("Truncated Augustus accounting payload"); }
private:
    std::span<const uint8_t> bytes_;
    size_t offset_ = 0;
};

struct AccountingTransaction {
    int32_t price = 0;
    uint16_t city = 0, trader = 0;
    uint8_t storage = 0, month = 0, resource = 0;
    int8_t quantity = 0;
};

struct AccountingYear {
    int32_t year = 0, transaction_count = 0;
    // Producer units: stock/import/export/production are loads, consumption is
    // resource units, balance is denarii. These source ordinals never escape import.
    std::array<std::array<int32_t, 22>, 6> resources{};
};

struct AccountingArchive {
    uint16_t history_years = 0;
    std::array<AccountingYear, 8> years{};
    std::array<std::vector<AccountingTransaction>, 2> transactions;
    uint8_t finance_years = 0;
    std::array<std::array<int32_t, 15>, 6> finances{};
};

inline bool decode_accounting(const AugustusArchive &archive, AccountingArchive &accounting, std::string &diagnostic)
{
    try {
        AccountingArchive decoded;
        if (archive.origin.family != ArchiveFamily::Augustus || archive.origin.save_version < 175 || archive.origin.save_version > 189) throw std::runtime_error("Unknown Augustus accounting schema");
        if (archive.origin.save_version < 182) { accounting = {}; diagnostic.clear(); return true; }
        const auto found = archive.pieces.find("finance_ledger");
        if (found == archive.pieces.end()) throw std::runtime_error("Missing Augustus accounting payload");
        PayloadReader source(found->second);
        if (source.u32() != found->second.size()) throw std::runtime_error("Invalid Augustus accounting payload size");
        decoded.history_years = source.u16();
        if (decoded.history_years > 7) throw std::runtime_error("Invalid Augustus accounting year count");
        for (auto &year : decoded.years) {
            year.year = source.i32(); year.transaction_count = source.i32();
            for (auto &field : year.resources) for (auto &value : field) value = source.i32();
        }
        for (auto &transactions : decoded.transactions) {
            const uint32_t count = source.u32();
            if (count > source.remaining() / 12) throw std::runtime_error("Invalid Augustus accounting transaction count");
            transactions.reserve(count);
            for (uint32_t i = 0; i < count; ++i) {
                AccountingTransaction entry;
                entry.price = source.i32(); entry.city = source.u16(); entry.storage = source.u8(); entry.month = source.u8();
                entry.resource = source.u8(); entry.trader = source.u16(); entry.quantity = static_cast<int8_t>(source.u8());
                if (entry.resource >= 22 || entry.month >= 12) throw std::runtime_error("Invalid Augustus transaction identity or month");
                transactions.push_back(entry);
            }
        }
        if (archive.origin.save_version >= 183) {
            decoded.finance_years = source.u8();
            if (decoded.finance_years > 6) throw std::runtime_error("Invalid Augustus finance history length");
            for (auto &year : decoded.finances) for (auto &field : year) field = source.i32();
        }
        if (source.remaining()) throw std::runtime_error("Unexplained Augustus accounting payload tail");
        accounting = std::move(decoded);
        diagnostic.clear();
        return true;
    } catch (const std::exception &error) {
        diagnostic = error.what();
        return false;
    }
}

struct TradeRouteRecord {
    std::array<std::array<int32_t, 22>, 4> values{}; // import limit/traded, export limit/traded
    bool open = false;
};

struct TradeRouteArchive {
    std::vector<TradeRouteRecord> current;
    std::array<std::vector<TradeRouteRecord>, 7> history;
    uint8_t years = 0;
};

inline bool decode_trade_routes(const AugustusArchive &archive, TradeRouteArchive &routes, std::string &diagnostic)
{
    try {
        if (archive.origin.family != ArchiveFamily::Augustus || archive.origin.save_version < 175 || archive.origin.save_version > 189) throw std::runtime_error("Unknown Augustus trade route schema");
        const bool has_history = archive.origin.save_version >= 182;
        auto read_routes = [has_history](PayloadReader &source, std::vector<TradeRouteRecord> &result) {
            const uint32_t count = source.u32();
            if (count > source.remaining() / (22 * 4 * 4 + has_history)) throw std::runtime_error("Invalid Augustus trade route count");
            result.resize(count);
            for (auto &route : result) {
                for (size_t direction = 0; direction < 2; ++direction) for (size_t resource = 0; resource < 22; ++resource) {
                    route.values[direction * 2][resource] = source.i32();
                    route.values[direction * 2 + 1][resource] = source.i32();
                }
                if (has_history) route.open = source.u8() != 0;
            }
        };
        TradeRouteArchive decoded;
        const auto current = archive.pieces.find("trade_routes");
        if (current == archive.pieces.end()) throw std::runtime_error("Missing Augustus trade routes");
        PayloadReader source(current->second);
        read_routes(source, decoded.current);
        if (source.remaining()) throw std::runtime_error("Unexplained Augustus route payload tail");
        if (has_history) {
            const auto history = archive.pieces.find("trade_history");
            if (history == archive.pieces.end()) throw std::runtime_error("Missing Augustus trade history");
            PayloadReader past(history->second);
            decoded.years = past.u8();
            if (decoded.years > 7) throw std::runtime_error("Invalid Augustus route history length");
            for (auto &year : decoded.history) read_routes(past, year);
            if (past.remaining()) throw std::runtime_error("Unexplained Augustus route history tail");
        }
        routes = std::move(decoded);
        diagnostic.clear();
        return true;
    } catch (const std::exception &error) {
        diagnostic = error.what();
        return false;
    }
}

} // namespace augustus_save
