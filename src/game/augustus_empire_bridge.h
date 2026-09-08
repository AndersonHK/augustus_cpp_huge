#pragma once
#include "game/augustus_common_records.h"

namespace augustus_save {

struct EmpireException {
    int route = 0;
    bool hidden = false;
    std::array<uint32_t, 22> costs{};
};

struct EmpireRecords {
    std::vector<uint8_t> common;
    std::vector<EmpireException> exceptions;
};

inline bool decode_empire(const AugustusArchive &archive, EmpireRecords &result, std::string &diagnostic)
{
    const auto found = archive.pieces.find("custom_empire");
    if (found == archive.pieces.end() || found->second.size() < 4) { diagnostic = "Missing Augustus custom empire."; return false; }
    const auto &bytes = found->second;
    const uint32_t count = read_u32(bytes, 0);
    if (count > (bytes.size() - 4) / 2) { diagnostic = "Invalid Augustus empire object count."; return false; }
    EmpireRecords decoded;
    decoded.common.assign(bytes.begin(), bytes.begin() + 4);
    size_t offset = 4;
    const bool extended = archive.origin.save_version >= 184;
    for (uint32_t id = 0; id < count; ++id) {
        if (bytes.size() - offset < 2) { diagnostic = "Truncated Augustus empire object."; return false; }
        const bool city = bytes[offset] == 1, active = bytes[offset + 1] != 0;
        const size_t stride = !active ? 2 : 87 + (extended ? 1 : 0) + (city ? (extended ? 252 : 84) : 0);
        if (stride > bytes.size() - offset) { diagnostic = "Truncated Augustus empire payload."; return false; }
        if (!active) { decoded.common.insert(decoded.common.end(), bytes.begin() + offset, bytes.begin() + offset + 2); offset += 2; continue; }
        decoded.common.insert(decoded.common.end(), bytes.begin() + offset, bytes.begin() + offset + 28);
        size_t at = offset + 28;
        EmpireException overlay;
        overlay.route = bytes[offset + 22];
        if (city) {
            // Reviewed common native format has 23 slots per array and 32-bit
            // quantities. Slots 22/23 are special resources, absent upstream.
            for (int direction = 0; direction < 2; ++direction) for (int resource = 1; resource < 24; ++resource) {
                const int64_t value = resource >= 22 ? 0 : extended ? int64_t(read_u32(bytes, at)) : int16_t(read_u16(bytes, at));
                if (value < 0 || value > INT32_MAX) { diagnostic = "Invalid Augustus empire resource quantity."; return false; }
                const size_t target = decoded.common.size(); decoded.common.resize(target + 4);
                write_u32(decoded.common, target, static_cast<uint32_t>(value));
                if (resource < 22) at += extended ? 4 : 2;
            }
            if (extended) for (int resource = 1; resource < 22; ++resource, at += 4) {
                overlay.costs[resource] = read_u32(bytes, at);
                if (overlay.costs[resource] > INT32_MAX) { diagnostic = "Invalid Augustus route price."; return false; }
            }
        }
        decoded.common.insert(decoded.common.end(), bytes.begin() + at, bytes.begin() + at + 59);
        if (extended) overlay.hidden = bytes[at + 59] != 0;
        if (overlay.hidden || std::any_of(overlay.costs.begin(), overlay.costs.end(), [](auto value) { return value != 0; })) decoded.exceptions.push_back(overlay);
        offset += stride;
    }
    if (offset != bytes.size()) { diagnostic = "Unexpected Augustus empire trailing bytes."; return false; }
    result = std::move(decoded); diagnostic.clear(); return true;
}

} // namespace augustus_save
