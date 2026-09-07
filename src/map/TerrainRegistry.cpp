#include "map/TerrainRegistry.h"

#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include <cctype>
#include <set>
#include <stdexcept>

namespace {
TerrainRegistry registry;
std::string failure_reason;

std::vector<std::string> tokens(const std::string &text)
{
    std::vector<std::string> result;
    std::string token;
    for (const char ch : text) {
        if (ch == '|' || ch == ',' || std::isspace(static_cast<unsigned char>(ch))) {
            if (!token.empty()) { result.push_back(token); token.clear(); }
        } else token += ch;
    }
    if (!token.empty()) result.push_back(token);
    return result;
}

struct Declaration {
    std::string name;
    TerrainTraits traits;
    std::array<TerrainWaterImage, static_cast<size_t>(WaterShoreShape::Count)> water_images;
    std::vector<std::string> includes, aliases;
    std::string source;
    int graphics_priority = 0;
    bool disabled = false;
    bool root = false;
    bool traversal = false;
    bool placement = false;
};
Declaration parsed;

bool read_bool(const char *name, bool &out, bool required = true)
{
    if (!xml_parser_has_attribute(name)) return !required;
    int value = 0;
    if (!xml_value::parse_bool(xml_parser_get_attribute_string(name), &value)) return false;
    out = value != 0;
    return true;
}

int root()
{
    if (parsed.root || !xml_parser_has_attribute("text_id") || xml_parser_has_attribute("number_id")) return 0;
    parsed.name = xml_definition::normalize_path(xml_parser_get_attribute_string("text_id"));
    if (xml_parser_has_attribute("graphics_priority") && !xml_value::parse_int_strict(xml_parser_get_attribute_string("graphics_priority"), &parsed.graphics_priority)) return 0;
    parsed.root = true;
    return !parsed.name.empty() && parsed.name != "none" && parsed.name != "highway" && parsed.name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_/-.") == std::string::npos && read_bool("disabled", parsed.disabled, false);
}

int traversal()
{
    if (parsed.disabled || parsed.traversal) return 0;
    parsed.traversal = true;
    auto &t = parsed.traits;
    return read_bool("land", t.land) && read_bool("sea", t.sea) && read_bool("enemy", t.enemy) &&
        read_bool("herd", t.herd) && read_bool("earthquake", t.earthquake) && read_bool("water", t.water);
}

int placement()
{
    if (parsed.disabled || parsed.placement) return 0;
    parsed.placement = true;
    auto &t = parsed.traits;
    return read_bool("blocking", t.blocks_construction) && read_bool("clearable", t.clearable) && read_bool("editable", t.editable) && read_bool("paintable", t.paintable);
}

int includes()
{
    if (parsed.disabled || !xml_parser_has_attribute("terrain")) return 0;
    const std::string name = xml_definition::normalize_path(xml_parser_get_attribute_string("terrain"));
    if (name.empty()) return 0;
    parsed.includes.push_back(name);
    return 1;
}

int alias()
{
    if (parsed.disabled || !xml_parser_has_attribute("name")) return 0;
    const std::string name = xml_definition::normalize_path(xml_parser_get_attribute_string("name"));
    if (name.empty()) return 0;
    parsed.aliases.push_back(name);
    return 1;
}

int water_image()
{
    if (parsed.disabled || !xml_parser_has_attribute("shape") || !xml_parser_has_attribute("group") || !xml_parser_has_attribute("images")) return 0;
    const std::string shape = xml_parser_get_attribute_string("shape");
    const char *names[] = {"other", "open", "north", "east", "south", "west"};
    size_t index = 0;
    while (index < std::size(names) && shape != names[index]) ++index;
    if (index == std::size(names) || !parsed.water_images[index].images.empty()) return 0;
    auto &image = parsed.water_images[index];
    image.group = xml_parser_get_attribute_string("group");
    image.images = tokens(xml_parser_get_attribute_string("images"));
    return !image.group.empty() && !image.images.empty();
}

const xml_parser_element elements[] = {
    {"terrain", root, nullptr, nullptr, nullptr},
    {"traversal", traversal, nullptr, "terrain", nullptr},
    {"placement", placement, nullptr, "terrain", nullptr},
    {"includes", includes, nullptr, "terrain", nullptr},
    {"load_alias", alias, nullptr, "terrain", nullptr},
    {"water_image", water_image, nullptr, "terrain", nullptr}
};
}

TerrainRegistry &terrain_registry() { return registry; }
const TerrainTypes &terrain_types() { return registry.types(); }

void TerrainRegistry::clear()
{
    types_ = {};
    load_aliases_.clear();
    definitions_.clear();
}

const Terrain *TerrainRegistry::find(const std::string &name) const
{
    const auto found = definitions_.find(xml_definition::normalize_path(name.c_str()));
    return found == definitions_.end() ? nullptr : found->second.get();
}

const Terrain &TerrainRegistry::require(const std::string &name, const std::string &source) const
{
    if (const auto *terrain = find(name)) return *terrain;
    throw std::runtime_error(source + " references unavailable terrain '" + name + "'");
}

const Terrain *TerrainRegistry::load_alias(const std::string &name) const
{
    if (const auto *exact = find(name)) return exact;
    const auto alias = load_aliases_.find(name);
    return alias == load_aliases_.end() ? nullptr : alias->second;
}

TerrainSet TerrainRegistry::bind(const std::string &names, const std::string &source) const
{
    TerrainSet result;
    for (const auto &name : tokens(names)) {
        if (name == "none") continue;
        if (name == "highway") result |= types_.highway;
        else result.add(require(name, source));
    }
    return result;
}

bool TerrainRegistry::load(const std::vector<mod_definition::DefinitionLayer> &layers, std::string &failure)
{
    try {
        std::map<std::string, Declaration> winners;
        mod_definition::DefinitionOverlayTracker overlay;
        if (!mod_definition::for_each_definition_file(layers, {"Terrain-Types"}, "Terrain", true, [&](const auto &source) {
            parsed = {};
            parsed.source = source.describe();
            if (!xml_definition::parse_file(source.full_path.c_str(), "Terrain", elements, static_cast<int>(std::size(elements))) ||
                !parsed.root || (!parsed.disabled && (!parsed.traversal || !parsed.placement))) {
                failure = "Invalid or incomplete terrain declaration: " + source.describe();
                return false;
            }
            if (parsed.name != source.normalized_definition_path) {
                failure = "Terrain text_id must match its file path: " + source.describe();
                return false;
            }
            if (!overlay.apply(parsed.name, parsed.disabled, source)) { failure = overlay.failure_reason(); return false; }
            winners[parsed.name] = std::move(parsed);
            return true;
        }, nullptr, &failure)) return false;

        TerrainRegistry staged;
        for (const auto &[name, declaration] : winners) {
            if (!declaration.disabled) staged.definitions_.emplace(name, std::make_unique<Terrain>(name, declaration.traits));
        }
        std::set<int> graphics_priorities;
        for (const auto &[name, declaration] : winners) {
            if (declaration.disabled) continue;
            auto &terrain = *staged.definitions_.at(name);
            terrain.water_images_ = declaration.water_images;
            terrain.graphics_priority_ = declaration.graphics_priority;
            if (terrain.has_water_images() && !graphics_priorities.insert(terrain.graphics_priority()).second) throw std::runtime_error("Terrain water-image definitions require distinct graphics_priority values: " + declaration.source);
            for (const auto &included : declaration.includes) terrain.includes_.add(staged.require(included, declaration.source));
            for (const auto &alias : declaration.aliases) {
                if (!staged.load_aliases_.emplace(alias, &terrain).second) throw std::runtime_error("Duplicate terrain load alias '" + alias + "': " + declaration.source);
            }
        }
        std::set<const Terrain *> resolving, resolved;
        std::function<void(Terrain &)> resolve = [&](Terrain &terrain) {
            if (resolved.count(&terrain)) return;
            if (!resolving.insert(&terrain).second) throw std::runtime_error("Terrain includes cycle: " + terrain.name());
            const TerrainSet direct = terrain.includes_;
            for (const auto *included : direct.entries()) {
                resolve(*staged.definitions_.at(included->name()));
                terrain.includes_ |= included->includes();
            }
            resolving.erase(&terrain);
            resolved.insert(&terrain);
        };
        for (auto &[name, terrain] : staged.definitions_) resolve(*terrain);

        auto &t = staged.types_;
        const auto optional = [&](const char *name) { const auto *terrain = staged.find(name); return terrain ? TerrainSet(*terrain) : TerrainSet(); };
        t.tree = staged.require("tree", "Native tree system");
        t.rock = staged.require("rock", "Native rock system");
        t.water = staged.require("water", "Native water system");
        t.building = staged.require("building", "Building occupancy");
        t.shrub = staged.require("shrub", "Native shrub system");
        t.garden = staged.require("garden", "Garden system");
        t.road = staged.require("road", "Road system");
        t.reservoir_range = staged.require("reservoir_range", "Reservoir coverage");
        t.aqueduct = staged.require("aqueduct", "Aqueduct system");
        t.elevation = staged.require("elevation", "Map elevation");
        t.access_ramp = staged.require("access_ramp", "Elevation access");
        t.meadow = staged.require("meadow", "Meadow system");
        t.rubble = staged.require("rubble", "Rubble system");
        t.fountain_range = staged.require("fountain_range", "Fountain coverage");
        t.wall = staged.require("wall", "Wall system");
        t.gatehouse = staged.require("gatehouse", "Wall access");
        t.originally_tree = staged.require("originally_tree", "Tree regrowth");
        t.highway_top_left = optional("highway_top_left");
        t.highway_bottom_left = optional("highway_bottom_left");
        t.highway_top_right = optional("highway_top_right");
        t.highway_bottom_right = optional("highway_bottom_right");
        t.shallow_water = optional("shallow_water");
        t.highway_quadrants = {t.highway_top_left, t.highway_bottom_left, t.highway_top_right, t.highway_bottom_right};
        t.highway_directions = {t.highway_top_right | t.highway_bottom_right, t.highway_bottom_left | t.highway_bottom_right, t.highway_top_left | t.highway_bottom_left, t.highway_top_left | t.highway_top_right};
        t.highway = t.highway_top_left | t.highway_bottom_left | t.highway_top_right | t.highway_bottom_right;
        t.wall_or_gatehouse = t.wall | t.gatehouse;
        t.elevation_rock = t.elevation | t.rock;
        t.map_edge = t.tree | t.water;
        for (const auto &[name, terrain] : staged.definitions_) {
            if (!terrain->allows_land()) t.impassable.add(*terrain);
            if (!terrain->allows_enemy()) t.impassable_enemy.add(*terrain);
            if (!terrain->allows_herd()) t.impassable_herd.add(*terrain);
            if (!terrain->allows_earthquake()) t.impassable_earthquake.add(*terrain);
            if (terrain->blocks_construction()) t.not_clear.add(*terrain);
            if (terrain->clearable()) t.clearable.add(*terrain);
            if (terrain->editable()) t.all.add(*terrain);
            if (terrain->paintable()) t.paintable.add(*terrain);
        }
        t.not_clear_except_road = t.not_clear - t.road;
        *this = std::move(staged);
        failure.clear();
        return true;
    } catch (const std::exception &error) {
        failure = error.what();
        return false;
    }
}

int terrain_registry_load()
{
    std::vector<mod_definition::DefinitionLayer> layers;
    if (!mod_definition::configured_layers(layers, &failure_reason) || !registry.load(layers, failure_reason)) {
        log_error("Unable to load Terrain definitions", failure_reason.c_str(), 0);
        return 0;
    }
    return 1;
}

const char *terrain_registry_failure_reason() { return failure_reason.c_str(); }
