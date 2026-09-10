#pragma once

#include "game/augustus_record_bridge.h"
#include "game/augustus_city_layout.generated.h"

namespace augustus_save {

struct CityRecord {
    // Reviewed common-ancestor wire fields only. New source fields live beside
    // them; passing the producer version into a native reader is never valid.
    std::vector<uint8_t> common;
    int immigration_percent = 100, emigration_percent = 100;
    int fourth_religion = 0, fifth_religion = 0;
    size_t missing_tail_bytes = 0;
};

inline bool decode_city(const AugustusArchive &archive, CityRecord &city, std::string &diagnostic)
{
    const int version = archive.origin.save_version;
    const auto found = archive.pieces.find("city_data");
    if (archive.origin.family != ArchiveFamily::Augustus || version < 175 || version > 189 || archive.origin.resource_version != 5 || found == archive.pieces.end()) {
        diagnostic = "Missing reviewed Augustus city schema.";
        return false;
    }
    const std::span<const uint8_t> bytes(found->second);
    // Before version 186 the allocation was not enlarged when fields were
    // added. The writer silently lost the final 28/36/44 bytes respectively.
    const size_t allocation = version >= 186 ? augustus_city_layout::current_size : augustus_city_layout::current_size - 44;
    if (bytes.size() != allocation) { diagnostic = "Unexpected Augustus city payload size."; return false; }
    CityRecord decoded;
    const bool migration = version >= 184, religions = version >= 185;
    if (migration) {
        decoded.immigration_percent = static_cast<int32_t>(read_u32(bytes, augustus_city_layout::migration_percentages));
        decoded.emigration_percent = static_cast<int32_t>(read_u32(bytes, augustus_city_layout::migration_percentages + 4));
        if (decoded.immigration_percent < 0 || decoded.immigration_percent > 1000000 || decoded.emigration_percent < 0 || decoded.emigration_percent > 1000000) {
            diagnostic = "Invalid Augustus scenario migration percentages.";
            return false;
        }
    }
    if (religions) {
        decoded.fourth_religion = static_cast<int32_t>(read_u32(bytes, augustus_city_layout::extra_religions));
        decoded.fifth_religion = static_cast<int32_t>(read_u32(bytes, augustus_city_layout::extra_religions + 4));
    }
    decoded.common.reserve(augustus_city_layout::common_size);
    for (size_t offset = 0; offset < bytes.size(); ++offset) {
        if (migration && offset >= augustus_city_layout::migration_percentages && offset < augustus_city_layout::migration_percentages + 8) continue;
        if (religions && offset >= augustus_city_layout::extra_religions && offset < augustus_city_layout::extra_religions + 8) continue;
        decoded.common.push_back(bytes[offset]);
    }
    decoded.missing_tail_bytes = augustus_city_layout::common_size - decoded.common.size();
    decoded.common.resize(augustus_city_layout::common_size, 0);
    city = std::move(decoded);
    diagnostic.clear();
    return true;
}

} // namespace augustus_save
