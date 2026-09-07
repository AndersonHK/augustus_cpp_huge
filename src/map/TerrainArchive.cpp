#include "map/TerrainSaveBridge.h"
#include "map/TerrainRegistry.h"
#include "core/log.h"
#include <map>
#include <charconv>
#include <set>
#include <sstream>
#include <stdexcept>

namespace terrain_save {
namespace {
const char *legacy_names[] = {
    "tree", "rock", "water", "building", "shrub", "garden", "road", "reservoir_range",
    "aqueduct", "elevation", "access_ramp", "meadow", "rubble", "fountain_range", "wall", "gatehouse",
    "originally_tree", "highway_top_left", "highway_bottom_left", "highway_top_right", "highway_bottom_right", "shallow_water"
};
bool native_ledger = false;
std::map<TerrainSet, uint32_t> saved_sets;
std::map<uint32_t, TerrainSet> loaded_sets;
std::set<std::string> reported_repairs;

uint32_t archive_number(const std::string &token)
{
    uint32_t value = 0;
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (token.empty() || parsed.ec != std::errc() || parsed.ptr != token.data() + token.size()) throw std::runtime_error("Invalid terrain archive number");
    return value;
}

const Terrain *resolve(const std::string &name)
{
    if (const auto *exact = terrain_registry().find(name)) return exact;
    const auto *replacement = terrain_registry().load_alias(name);
    if (reported_repairs.insert(name).second) {
        const std::string detail = name + (replacement ? " -> " + replacement->name() : " (removed)");
        log_warning("Repairing imported terrain absent from the active mod stack", detail.c_str(), 0);
    }
    return replacement;
}
}

void reset()
{
    saved_sets.clear();
    loaded_sets.clear();
    reported_repairs.clear();
    native_ledger = false;
}

void prepare()
{
    saved_sets.clear();
    saved_sets.emplace(TerrainSet(), 0);
}

uint32_t encode(const TerrainSet &terrain)
{
    const auto found = saved_sets.find(terrain);
    if (found != saved_sets.end()) return found->second;
    if (saved_sets.empty()) prepare();
    const uint32_t id = static_cast<uint32_t>(saved_sets.size());
    saved_sets.emplace(terrain, id);
    return id;
}

TerrainSet decode_legacy(uint32_t mask)
{
    TerrainSet terrain;
    for (size_t bit = 0; bit < std::size(legacy_names); ++bit) {
        if (mask & (1u << bit)) if (const auto *entry = resolve(legacy_names[bit])) terrain.add(*entry);
    }
    const uint32_t unknown = mask & ~((1u << std::size(legacy_names)) - 1);
    if (unknown && reported_repairs.insert("unknown legacy bits").second) log_warning("Removing unknown legacy terrain bits", nullptr, static_cast<int>(unknown));
    return terrain;
}

uint32_t encode_legacy(const TerrainSet &terrain)
{
    uint32_t mask = 0;
    for (const auto *entry : terrain.entries()) {
        bool found = false;
        for (size_t bit = 0; bit < std::size(legacy_names); ++bit) {
            if (entry->name() == legacy_names[bit]) { mask |= 1u << bit; found = true; break; }
        }
        if (!found) throw std::runtime_error("Terrain cannot be represented by the legacy format: " + entry->name());
    }
    return mask;
}

TerrainSet decode(uint32_t value)
{
    if (!native_ledger) return decode_legacy(value);
    const auto found = loaded_sets.find(value);
    if (found == loaded_sets.end()) throw std::runtime_error("Terrain grid references absent archive set " + std::to_string(value));
    return found->second;
}

TerrainSet read_at(buffer *source, int offset, bool wide)
{
    if (!source || offset < 0 || static_cast<size_t>(offset + 1) * (wide ? 4 : 2) > source->size) throw std::runtime_error("Truncated terrain grid");
    buffer copy = *source;
    buffer_set(&copy, static_cast<size_t>(offset) * (wide ? 4 : 2));
    return decode(wide ? buffer_read_u32(&copy) : buffer_read_u16(&copy));
}

void write_ledger(buffer *destination)
{
    std::map<const Terrain *, uint32_t> ids;
    std::ostringstream text;
    text << "terrain-ledger\t1\n";
    for (const auto &[name, terrain] : terrain_registry().definitions()) {
        const uint32_t id = static_cast<uint32_t>(ids.size() + 1);
        ids.emplace(terrain.get(), id);
        text << "terrain\t" << id << '\t' << name << '\n';
    }
    std::map<uint32_t, const TerrainSet *> ordered;
    for (const auto &[set, id] : saved_sets) ordered.emplace(id, &set);
    for (const auto &[id, set] : ordered) {
        text << "set\t" << id;
        std::vector<uint32_t> references;
        for (const auto *terrain : set->entries()) references.push_back(ids.at(terrain));
        std::sort(references.begin(), references.end());
        for (const auto reference : references) text << '\t' << reference;
        text << '\n';
    }
    text << "end-terrain-ledger\n";
    const std::string payload = text.str();
    buffer_init_dynamic(destination, payload.size());
    buffer_write_raw(destination, payload.data(), payload.size());
}

bool load_ledger(buffer *source, bool has_ledger)
{
    native_ledger = has_ledger;
    loaded_sets.clear();
    reported_repairs.clear();
    if (!has_ledger) return true;
    try {
        if (!source) throw std::runtime_error("Missing terrain ledger");
        buffer copy = *source;
        const size_t payload_size = buffer_load_dynamic(&copy);
        if (copy.overflow || copy.index > copy.size || payload_size != copy.size - copy.index) throw std::runtime_error("Truncated terrain ledger");
        std::istringstream text(std::string(reinterpret_cast<const char *>(copy.data + copy.index), copy.size - copy.index));
        std::string line;
        if (!std::getline(text, line) || line != "terrain-ledger\t1") throw std::runtime_error("Unsupported terrain ledger");
        std::map<uint32_t, const Terrain *> definitions;
        std::set<std::string> names;
        bool ended = false;
        while (std::getline(text, line)) {
            if (line == "end-terrain-ledger") { ended = true; break; }
            std::istringstream row(line);
            std::string kind;
            std::string number;
            if (!(row >> kind >> number)) throw std::runtime_error("Malformed terrain ledger row");
            const uint32_t id = archive_number(number);
            if (kind == "terrain") {
                std::string name, extra;
                if (!(row >> name) || (row >> extra) || !id || definitions.count(id) || !names.insert(name).second) throw std::runtime_error("Duplicate or malformed terrain identity");
                definitions.emplace(id, resolve(name));
            } else if (kind == "set") {
                if (loaded_sets.count(id)) throw std::runtime_error("Duplicate terrain set identity");
                TerrainSet terrain;
                std::set<uint32_t> references;
                while (row >> number) {
                    const uint32_t reference = archive_number(number);
                    if (!references.insert(reference).second) throw std::runtime_error("Duplicate terrain reference in set");
                    const auto found = definitions.find(reference);
                    if (found == definitions.end()) throw std::runtime_error("Terrain set references absent definition");
                    if (found->second) terrain.add(*found->second);
                }
                if (!row.eof() || (!id && !terrain.empty())) throw std::runtime_error("Malformed terrain reference list");
                loaded_sets.emplace(id, std::move(terrain));
            } else throw std::runtime_error("Unknown terrain ledger record");
        }
        if (!ended || !loaded_sets.count(0) || std::getline(text, line)) throw std::runtime_error("Incomplete terrain ledger");
        return true;
    } catch (const std::exception &error) {
        log_error("Unable to load terrain ledger", error.what(), 0);
        return false;
    }
}
}

