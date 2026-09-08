#pragma once

#include "game/augustus_common_records.h"
#include <map>

namespace augustus_save {

enum class ActionOrder { Unspecified, HouseThenLock, LockThenHouse };

struct ScenarioRecords {
    std::vector<uint8_t> actions, formulas;
    std::map<uint32_t, std::string> texts;
    std::vector<uint32_t> assumed_lock_actions;
};

inline bool decode_scenario_records(const AugustusArchive &archive, ActionOrder order, ScenarioRecords &records, std::string &diagnostic)
{
    if (archive.origin.family != ArchiveFamily::Augustus || archive.origin.save_version < 175 || archive.origin.save_version > 189) {
        diagnostic = "Unreviewed Augustus scenario record schema.";
        return false;
    }
    const int version = archive.origin.save_version;
    ScenarioRecords decoded;
    auto array = [&](const char *name, size_t stride, bool reserved_zero, std::vector<uint8_t> &output) {
        const auto found = archive.pieces.find(name);
        if (found == archive.pieces.end() || found->second.size() < 16) { diagnostic = std::string("Missing Augustus ") + name + " array."; return false; }
        const auto &bytes = found->second;
        const auto count = read_u32(bytes, 8);
        if (read_u32(bytes, 0) != bytes.size() || read_u32(bytes, 4) != 0 || read_u32(bytes, 12) != stride ||
            count > (bytes.size() - 16) / stride || 16 + count * stride != bytes.size() || (reserved_zero && !count)) {
            diagnostic = std::string("Invalid Augustus ") + name + " array bounds.";
            return false;
        }
        // Upstream counts the reserved zero slot in allocation metadata, but
        // never writes it. Its final allocation-sized record is not authored data.
        const size_t written = count - (reserved_zero ? 1 : 0);
        output.assign(bytes.begin(), bytes.begin() + 16 + written * stride);
        write_u32(output, 0, static_cast<uint32_t>(output.size()));
        write_u32(output, 8, static_cast<uint32_t>(written));
        if (reserved_zero) for (size_t id = 1; id <= written; ++id) {
            if (read_u32(output, 16 + (id - 1) * stride) != id) {
                diagnostic = std::string("Invalid authored identity in Augustus ") + name + ".";
                return false;
            }
        }
        return true;
    };
    if (!array("scenario_actions", 28, false, decoded.actions) || !array("scenario_formulas", 118, true, decoded.formulas)) return false;
    for (size_t offset = 16; offset < decoded.actions.size(); offset += 28) {
        const int type = read_u16(decoded.actions, offset + 6);
        if (type > 56 || (version < 184 && type > 44)) { diagnostic = "Unknown Augustus scenario action identity."; return false; }
        if (type != 44 && type != 45) continue;
        // Version 189 began on August 22; b4f123b82 restored lock=44 on
        // August 23, with no subsequent version change in the reviewed history.
        // D18 chooses that longer-lived layout when the producer is unknown.
        const auto effective = version < 184 ? ActionOrder::LockThenHouse : version < 189 ? ActionOrder::HouseThenLock :
            order == ActionOrder::Unspecified ? ActionOrder::LockThenHouse : order;
        if (version == 189 && order == ActionOrder::Unspecified && type == 44) decoded.assumed_lock_actions.push_back(read_u32(decoded.actions, offset + 2));
        if (effective == ActionOrder::HouseThenLock) write_u16(decoded.actions, offset + 6, static_cast<uint16_t>(89 - type));
    }
    if (version >= 184) {
        std::vector<uint8_t> texts;
        if (!array("scenario_texts", 132, true, texts)) return false;
        for (size_t offset = 16; offset < texts.size(); offset += 132) {
            const auto start = texts.begin() + offset + 4;
            const auto end = std::find(start, start + 128, uint8_t{0});
            if (end == start + 128) { diagnostic = "Unterminated Augustus scenario text."; return false; }
            decoded.texts.emplace(read_u32(texts, offset), std::string(start, end));
        }
    }
    records = std::move(decoded);
    diagnostic.clear();
    return true;
}

} // namespace augustus_save
