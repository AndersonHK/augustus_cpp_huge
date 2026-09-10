#pragma once

#include "game/augustus_save_bridge.h"
#include <span>
#include <set>

namespace augustus_save {

// These offsets describe the foreign producer, never runtime definition IDs.
// The producer's array stride is allocation metadata: houses write an extra
// evolution-text byte into the sequential stream from version 184 onward.
struct BuildingRecord {
    size_t offset = 0;
    size_t size = 0;
    uint16_t type = 0;
    uint8_t state = 0;
    uint16_t evolution_text = 0;
    int16_t construction_phase = 0;
};

struct FigureRecord {
    size_t offset = 0;
    size_t size = 0;
    uint8_t type = 0;
    uint8_t state = 0;
    uint32_t owner = 0;
    uint16_t trader = 0;
};

enum class DefinitionKind { Building, Figure, Terrain, PhasedConstruction };
struct RequiredDefinition {
    DefinitionKind kind;
    std::string name;
    bool operator<(const RequiredDefinition &other) const { return kind != other.kind ? kind < other.kind : name < other.name; }
};

inline uint16_t read_u16(std::span<const uint8_t> bytes, size_t offset)
{
    return static_cast<uint16_t>(bytes[offset] | (uint16_t(bytes[offset + 1]) << 8));
}

inline uint32_t read_u32(std::span<const uint8_t> bytes, size_t offset)
{
    return uint32_t(read_u16(bytes, offset)) | (uint32_t(read_u16(bytes, offset + 2)) << 16);
}

inline bool index_buildings(const AugustusArchive &archive, std::vector<BuildingRecord> &records, std::string &diagnostic)
{
    const int version = archive.origin.save_version;
    if (archive.origin.family != ArchiveFamily::Augustus || version < 175 || version > 189 || archive.origin.resource_version != 5) {
        diagnostic = "Building decoder requires a post-fork Augustus producer with its 22-resource schema.";
        return false;
    }
    const auto found = archive.pieces.find("buildings");
    if (found == archive.pieces.end() || found->second.size() < 4) {
        diagnostic = "Augustus building payload is missing its allocation stride.";
        return false;
    }
    const std::span<const uint8_t> bytes(found->second);
    constexpr size_t common_record_size = 201, type_offset = 10, evolution_offset = 99;
    const uint32_t stride = read_u32(bytes, 0);
    if ((stride != common_record_size && !(version >= 184 && stride == common_record_size + 8)) || (bytes.size() - 4) % stride) {
        diagnostic = "Augustus building allocation does not match a reviewed producer layout.";
        return false;
    }
    const size_t count = (bytes.size() - 4) / stride;
    std::vector<BuildingRecord> indexed;
    indexed.reserve(count);
    size_t offset = 4;
    for (size_t id = 0; id < count; ++id) {
        if (offset > bytes.size() || bytes.size() - offset < common_record_size) {
            diagnostic = "Augustus building stream is truncated at record " + std::to_string(id) + ".";
            return false;
        }
        const uint16_t type = read_u16(bytes, offset + type_offset);
        const bool house = type >= 10 && type <= 29;
        const bool wide_evolution = house && version >= 184;
        const size_t size = common_record_size + wide_evolution;
        if (bytes.size() - offset < size) {
            diagnostic = "Augustus house stream is truncated at record " + std::to_string(id) + ".";
            return false;
        }
        if (type > 212 || bytes[offset] > 7) {
            diagnostic = "Invalid Augustus building identity/state at record " + std::to_string(id) + ".";
            return false;
        }
        const uint16_t evolution = !house ? 0 : wide_evolution ? read_u16(bytes, offset + evolution_offset) : bytes[offset + evolution_offset];
        indexed.push_back({offset, size, type, bytes[offset], evolution, static_cast<int16_t>(read_u16(bytes, offset + 118 + wide_evolution))});
        offset += size;
    }
    // The producer allocated more than it wrote. Its unused tail may contain
    // arbitrary allocator bytes and must never be interpreted as more records.
    records = std::move(indexed);
    diagnostic.clear();
    return true;
}

inline bool index_figures(const AugustusArchive &archive, std::vector<FigureRecord> &records, std::string &diagnostic)
{
    const int version = archive.origin.save_version;
    if (archive.origin.family != ArchiveFamily::Augustus || version < 175 || version > 189) {
        diagnostic = "Figure decoder requires a reviewed post-fork Augustus producer.";
        return false;
    }
    const auto found = archive.pieces.find("figures");
    if (found == archive.pieces.end() || found->second.size() < 4) {
        diagnostic = "Augustus figure payload is missing its allocation stride.";
        return false;
    }
    const std::span<const uint8_t> bytes(found->second);
    const bool wide_trader = version >= 182;
    const uint32_t stride = read_u32(bytes, 0);
    // Both producer schemas reserve fourteen bytes per figure in the array's
    // allocation, but write/read actual records consecutively without padding.
    const size_t record_size = wide_trader ? 157 : 156;
    if (stride != record_size + 14 || (bytes.size() - 4) % stride) {
        diagnostic = "Augustus figure allocation does not match its producer version.";
        return false;
    }
    const size_t count = (bytes.size() - 4) / stride;
    std::vector<FigureRecord> indexed;
    indexed.reserve(count);
    for (size_t id = 0, offset = 4; id < count; ++id, offset += record_size) {
        const uint8_t type = bytes[offset + 14], state = bytes[offset + 18];
        if (type > 99 || state > 2) {
            diagnostic = "Invalid Augustus figure identity/state at record " + std::to_string(id) + ".";
            return false;
        }
        const uint16_t trader = wide_trader ? read_u16(bytes, offset + 127) : bytes[offset + 127];
        indexed.push_back({offset, record_size, type, state, read_u32(bytes, offset + 84), trader});
    }
    records = std::move(indexed);
    diagnostic.clear();
    return true;
}

// Collect post-fork content before touching the live world's registries or ID
// ledgers. These are producer IDs; 211 must never become native clear_trees.
inline bool collect_required_definitions(const AugustusArchive &archive, std::set<RequiredDefinition> &requirements, std::string &diagnostic)
{
    std::vector<BuildingRecord> buildings;
    std::vector<FigureRecord> figures;
    if (!index_buildings(archive, buildings, diagnostic) || !index_figures(archive, figures, diagnostic)) return false;
    std::set<RequiredDefinition> result;
    for (const auto &building : buildings) {
        if (!building.state) continue;
        if (building.type == 211) result.insert({DefinitionKind::Building, "highway_station"});
        if (building.type == 212) result.insert({DefinitionKind::Building, "willow_tree"});
        if (building.type == 56 && archive.origin.save_version >= 180 && building.construction_phase > 0) result.insert({DefinitionKind::PhasedConstruction, "triumphal_arch"});
    }
    for (const auto &figure : figures) {
        if (!figure.state) continue;
        if (figure.type == 97) result.insert({DefinitionKind::Figure, "wandering_citizen"});
        if (figure.type == 98) result.insert({DefinitionKind::Figure, "dog"});
        if (figure.type == 99) {
            // Outstanding station cargo uses the generic resource delivery carrier.
            result.insert({DefinitionKind::Figure, "resource_delivery"});
            result.insert({DefinitionKind::Building, "highway_station"});
        }
    }
    const auto found = archive.pieces.find("terrain_grid");
    if (found == archive.pieces.end() || found->second.size() != 162 * 162 * 4) {
        diagnostic = "Augustus terrain payload does not match its producer grid.";
        return false;
    }
    const std::span<const uint8_t> terrain(found->second);
    for (size_t offset = 0; offset < terrain.size(); offset += 4) {
        const uint32_t value = read_u32(terrain, offset);
        if (value & ~((uint32_t{1} << 22) - 1)) {
            diagnostic = "Augustus terrain contains an unknown producer flag at tile " + std::to_string(offset / 4) + ".";
            return false;
        }
        if (value & (uint32_t{1} << 21)) result.insert({DefinitionKind::Terrain, "shallow_water"});
    }
    requirements = std::move(result);
    diagnostic.clear();
    return true;
}

template<class IsAvailable>
bool validate_required_definitions(const std::set<RequiredDefinition> &requirements, IsAvailable available, std::string &diagnostic)
{
    std::string missing;
    for (const auto &requirement : requirements) {
        if (available(requirement)) continue;
        const char *kind = requirement.kind == DefinitionKind::Building ? "building" : requirement.kind == DefinitionKind::Figure ? "figure" : requirement.kind == DefinitionKind::Terrain ? "terrain" : "phased construction";
        if (!missing.empty()) missing += ", ";
        missing += std::string(kind) + " '" + requirement.name + "'";
    }
    if (!missing.empty()) {
        diagnostic = "Enable the mod providing the following saved content before importing: " + missing + ". The source save has not been changed.";
        return false;
    }
    diagnostic.clear();
    return true;
}

} // namespace augustus_save
