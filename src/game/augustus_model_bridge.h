#pragma once

#include "game/augustus_record_bridge.h"
#include "game/augustus_model_defaults.generated.h"

namespace augustus_save {

struct ModelException {
    bool housing = false;
    std::string target;
    int field = 0, value = 0;
};

// Decode producer snapshots into exceptions only. Never publish a producer's
// defaults into mod definitions, and never interpret its ordinal as a native ID.
inline bool decode_model_exceptions(const AugustusArchive &archive, std::vector<ModelException> &exceptions, std::string &diagnostic)
{
    const int version = archive.origin.save_version;
    const auto found = archive.pieces.find("model_data");
    if (archive.origin.family != ArchiveFamily::Augustus || version < 175 || version > 189 || found == archive.pieces.end()) {
        diagnostic = "Missing reviewed Augustus model schema.";
        return false;
    }
    const std::span<const uint8_t> bytes(found->second);
    size_t building_bytes = bytes.size(), house_bytes = version >= 184 ? 20 * 17 * 4 : 0, prefix = 0;
    if (version >= 187) {
        if (bytes.size() < 8) { diagnostic = "Truncated Augustus model sizes."; return false; }
        prefix = 8;
        building_bytes = read_u32(bytes, 0); house_bytes = read_u32(bytes, 4);
    } else {
        if (building_bytes < house_bytes) { diagnostic = "Truncated Augustus house models."; return false; }
        building_bytes -= house_bytes;
    }
    if (house_bytes != (version >= 184 ? 20 * 17 * 4 : 0) || building_bytes % 24 || building_bytes / 24 < 211 ||
        building_bytes / 24 > 213 || prefix + building_bytes + house_bytes != bytes.size()) {
        diagnostic = "Augustus model payload sizes do not match a reviewed producer.";
        return false;
    }
    std::vector<const augustus_model_defaults::Baseline *> candidates;
    for (const auto &baseline : augustus_model_defaults::candidates) {
        if (baseline.version == version && baseline.building_count == building_bytes / 24) candidates.push_back(&baseline);
    }
    if (candidates.empty()) { diagnostic = "No matching Augustus model baseline."; return false; }
    std::vector<ModelException> result;
    for (int housing = 0; housing < 2; ++housing) {
        const size_t count = housing ? house_bytes / (17 * 4) : building_bytes / 24;
        const size_t fields = housing ? 17 : 6;
        const size_t start = prefix + (housing ? building_bytes : 0);
        for (size_t model = 0; model < count; ++model) {
            const size_t building = housing ? model + 10 : model;
            for (size_t field = 0; field < fields; ++field) {
                const int value = static_cast<int32_t>(read_u32(bytes, start + 4 * (model * fields + field)));
                bool default_value = false;
                for (const auto *baseline : candidates) {
                    if (value == (housing ? baseline->houses[model][field] : baseline->buildings[model][field])) { default_value = true; break; }
                }
                if (default_value) continue;
                const char *name = candidates.back()->names[building];
                if (!*name) { diagnostic = "Authored model exception has no producer identity at " + std::to_string(building) + "."; return false; }
                if (housing && field == 3 && (value < 0 || value > 3)) { diagnostic = "Invalid Augustus housing water requirement."; return false; }
                // Native Fountain retains value 2 for existing SVV overrides;
                // source 2 means latrine OR fountain, source 3 means fountain.
                const int mapped = housing && field == 3 ? (value == 2 ? 3 : value == 3 ? 2 : value) : value;
                result.push_back({housing != 0, name, static_cast<int>(field), mapped});
            }
        }
    }
    exceptions = std::move(result);
    diagnostic.clear();
    return true;
}

} // namespace augustus_save
