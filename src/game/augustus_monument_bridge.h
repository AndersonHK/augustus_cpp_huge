#pragma once
#include "game/augustus_common_records.h"
#include "game/augustus_monument_defaults.generated.h"

namespace augustus_save {
struct ConstructionException { std::string building; int phase, resource, amount; };

inline bool decode_construction_exceptions(const AugustusArchive &archive, std::vector<ConstructionException> &exceptions, std::string &diagnostic)
{
    if (archive.origin.family != ArchiveFamily::Augustus || archive.origin.save_version < 175 || archive.origin.save_version > 189) {
        diagnostic = "Unreviewed Augustus construction schema."; return false;
    }
    if (archive.origin.save_version < 184) { exceptions.clear(); return true; }
    const auto found = archive.pieces.find("monument_stages");
    constexpr size_t stride = 4 + 6 * 22 * 4;
    if (found == archive.pieces.end() || found->second.size() != std::size(augustus_monument_defaults::entries) * stride) {
        diagnostic = "Invalid Augustus construction snapshot size."; return false;
    }
    std::vector<ConstructionException> decoded;
    for (size_t building = 0; building < std::size(augustus_monument_defaults::entries); ++building) {
        const auto &baseline = augustus_monument_defaults::entries[building];
        const size_t start = building * stride;
        if (read_u32(found->second, start) != static_cast<uint32_t>(baseline.phases)) { diagnostic = "Unrecognized Augustus construction phase count."; return false; }
        for (int phase = 0; phase < baseline.phases - 1; ++phase) for (int resource = 0; resource < 22; ++resource) {
            const int value = static_cast<int32_t>(read_u32(found->second, start + 4 + 4 * (phase * 22 + resource)));
            if (value == baseline.resources[phase][resource]) continue;
            if (value < 0) { diagnostic = "Invalid Augustus construction amount."; return false; }
            decoded.push_back({baseline.name, phase + 1, resource, value});
        }
    }
    exceptions = std::move(decoded); diagnostic.clear(); return true;
}
}
