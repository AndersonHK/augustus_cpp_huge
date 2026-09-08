#pragma once

#include "game/augustus_record_bridge.h"
#include <algorithm>
#include <functional>

namespace augustus_save {

// The shared reader's scalar wire layout is a bridge detail, not a save version
// assigned to the foreign archive. Keep source-only fields alongside the records
// until the owning runtime modules have been hydrated.
struct CommonRecords {
    std::vector<uint8_t> buildings, figures;
    std::vector<BuildingRecord> source_buildings;
    std::vector<FigureRecord> source_figures;
    std::vector<size_t> invalid_trader_references;
};

inline void write_u16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
{
    bytes[offset] = static_cast<uint8_t>(value);
    bytes[offset + 1] = static_cast<uint8_t>(value >> 8);
}

inline void write_u32(std::vector<uint8_t> &bytes, size_t offset, uint32_t value)
{
    write_u16(bytes, offset, static_cast<uint16_t>(value));
    write_u16(bytes, offset + 2, static_cast<uint16_t>(value >> 16));
}

// Bind only newly introduced figure identities here; the shared fixed-enum
// bridge owns the older identities. Never let source 97/98/99 alias native IDs.
inline bool decode_common_records(const AugustusArchive &archive, const std::function<int(const char *)> &bind_figure, CommonRecords &records, std::string &diagnostic)
{
    CommonRecords decoded;
    if (!index_buildings(archive, decoded.source_buildings, diagnostic) || !index_figures(archive, decoded.source_figures, diagnostic)) return false;
    const auto &buildings = archive.pieces.at("buildings"), &figures = archive.pieces.at("figures");
    constexpr size_t building_size = 201, figure_size = 156, figure_allocation = 170;
    decoded.buildings.resize(4 + building_size * decoded.source_buildings.size());
    decoded.figures.resize(4 + figure_allocation * decoded.source_figures.size());
    write_u32(decoded.buildings, 0, building_size);
    write_u32(decoded.figures, 0, figure_allocation);
    for (size_t id = 0; id < decoded.source_buildings.size(); ++id) {
        const auto &record = decoded.source_buildings[id];
        const auto begin = buildings.begin() + record.offset;
        const size_t at = 4 + id * building_size;
        if (record.size == building_size) std::copy_n(begin, building_size, decoded.buildings.begin() + at);
        else {
            // Evolve-text is derived presentation state, recomputed after import.
            // Remove its high byte, not the final byte of the whole house record.
            std::copy_n(begin, 100, decoded.buildings.begin() + at);
            decoded.buildings[at + 99] = 0;
            std::copy_n(begin + 101, building_size - 100, decoded.buildings.begin() + at + 100);
        }
        if (record.type == 56 && archive.origin.save_version < 180) write_u16(decoded.buildings, at + 118, uint16_t(-1));
    }
    constexpr const char *new_figures[] = {"wandering_citizen", "dog", "resource_delivery"};
    for (size_t id = 0; id < decoded.source_figures.size(); ++id) {
        const auto &record = decoded.source_figures[id];
        const auto begin = figures.begin() + record.offset;
        const size_t at = 4 + id * figure_size;
        if (record.size == figure_size) std::copy_n(begin, figure_size, decoded.figures.begin() + at);
        else {
            std::copy_n(begin, 128, decoded.figures.begin() + at);
            std::copy_n(begin + 129, figure_size - 128, decoded.figures.begin() + at + 128);
        }
        // Both producers still have exactly 100 trader slots. A wider serialized
        // value is not permission to index past that table or silently truncate.
        if (record.trader >= 100) {
            decoded.figures[at + 127] = 0;
            if (record.state && (record.type == 19 || record.type == 20 || record.type == 21 || record.type == 38 || record.type == 58)) {
                // The runtime importer must repair these references and log the
                // repair before any trader accesses the table. Keep the original
                // wide value in source_figures for that diagnosis.
                decoded.invalid_trader_references.push_back(id);
            }
        }
        if (record.type >= 97) {
            const int type = record.state ? bind_figure(new_figures[record.type - 97]) : 0;
            if (record.state && (type <= 0 || type > 255)) {
                diagnostic = "No native figure binding for '" + std::string(new_figures[record.type - 97]) + "'.";
                return false;
            }
            decoded.figures[at + 14] = static_cast<uint8_t>(type);
        }
    }
    records = std::move(decoded);
    diagnostic.clear();
    return true;
}

} // namespace augustus_save
