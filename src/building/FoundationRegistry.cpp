#include "building/FoundationRegistry.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"
#include "map/terrain.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdio>
#include <memory>
#include <iterator>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

struct ParseState {
    std::unique_ptr<FoundationDef> definition;
    std::unordered_map<char, FoundationCellDefinition> profiles;
    std::vector<std::string> rows;
    std::string expected_path;
    int disabled = 0;
    int saw_root = 0;
    int error = 0;
};

std::vector<std::unique_ptr<FoundationDef>> g_owned_foundations;
std::vector<const FoundationDef *> g_foundations;
mod_definition::DefinitionOverlayTracker g_foundation_overlays;
ParseState g_parse_state;

void fail(const char *message, const char *detail = nullptr)
{
    log_error(message, detail, 0);
    g_parse_state.error = 1;
}

std::vector<std::string> tokens(const char *text)
{
    std::vector<std::string> result;
    std::string token;
    const std::string value = text ? text : "";
    for (char ch : value) {
        if (std::isspace(static_cast<unsigned char>(ch)) || ch == '|' || ch == ',') {
            if (!token.empty()) {
                result.push_back(token);
                token.clear();
            }
        } else {
            token.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
    }
    if (!token.empty()) {
        result.push_back(token);
    }
    return result;
}

int parse_map_terrain_mask(const char *text, uint32_t *out)
{
    struct Entry { const char *name; uint32_t value; };
    static const Entry ENTRIES[] = {
        {"tree", TERRAIN_TREE}, {"rock", TERRAIN_ROCK}, {"water", TERRAIN_WATER},
        {"building", TERRAIN_BUILDING}, {"shrub", TERRAIN_SHRUB}, {"garden", TERRAIN_GARDEN},
        {"road", TERRAIN_ROAD}, {"reservoir_range", TERRAIN_RESERVOIR_RANGE},
        {"aqueduct", TERRAIN_AQUEDUCT}, {"elevation", TERRAIN_ELEVATION},
        {"access_ramp", TERRAIN_ACCESS_RAMP}, {"meadow", TERRAIN_MEADOW},
        {"rubble", TERRAIN_RUBBLE}, {"fountain_range", TERRAIN_FOUNTAIN_RANGE},
        {"wall", TERRAIN_WALL}, {"gatehouse", TERRAIN_GATEHOUSE},
        {"originally_tree", TERRAIN_ORIGINALLY_TREE}, {"highway", TERRAIN_HIGHWAY},
        {"highway_top_left", TERRAIN_HIGHWAY_TOP_LEFT},
        {"highway_bottom_left", TERRAIN_HIGHWAY_BOTTOM_LEFT},
        {"highway_top_right", TERRAIN_HIGHWAY_TOP_RIGHT},
        {"highway_bottom_right", TERRAIN_HIGHWAY_BOTTOM_RIGHT}
    };
    *out = 0;
    for (const std::string &token : tokens(text)) {
        if (token == "none") {
            continue;
        }
        const auto found = std::find_if(std::begin(ENTRIES), std::end(ENTRIES), [&token](const Entry &entry) {
            return token == entry.name;
        });
        if (found == std::end(ENTRIES)) {
            return 0;
        }
        *out |= found->value;
    }
    return 1;
}

int parse_site_mask(const char *text, uint32_t *out)
{
    struct Entry { const char *name; uint32_t value; };
    static const Entry ENTRIES[] = {
        {"meadow", FOUNDATION_SITE_MEADOW}, {"rock", FOUNDATION_SITE_ROCK},
        {"tree", FOUNDATION_SITE_TREE}, {"water", FOUNDATION_SITE_WATER},
        {"wall", FOUNDATION_SITE_WALL}, {"distant_water", FOUNDATION_SITE_DISTANT_WATER}
    };
    *out = 0;
    for (const std::string &token : tokens(text)) {
        if (token == "none") {
            continue;
        }
        const auto found = std::find_if(std::begin(ENTRIES), std::end(ENTRIES), [&token](const Entry &entry) {
            return token == entry.name;
        });
        if (found == std::end(ENTRIES)) {
            return 0;
        }
        *out |= found->value;
    }
    return 1;
}

int parse_permissions(const char *text, uint16_t *out)
{
    struct Entry { const char *name; int bit; };
    static const Entry ENTRIES[] = {
        {"maintenance", 1}, {"priest", 2}, {"market", 3}, {"entertainer", 4},
        {"education", 5}, {"medicine", 6}, {"tax_collector", 7}, {"labor_seeker", 8},
        {"missionary", 9}, {"watchman", 10}
    };
    *out = 0;
    for (const std::string &token : tokens(text)) {
        if (token == "none") {
            continue;
        }
        if (token == "all") {
            *out = UINT16_MAX;
            continue;
        }
        const auto found = std::find_if(std::begin(ENTRIES), std::end(ENTRIES), [&token](const Entry &entry) {
            return token == entry.name;
        });
        if (found == std::end(ENTRIES)) {
            return 0;
        }
        *out |= static_cast<uint16_t>(1u << found->bit);
    }
    return 1;
}

int parse_requirement(const char *text, FoundationCellDefinition *cell)
{
    const std::string requirement = xml_value::trim_copy(text ? text : "");
    if (requirement == "land" || requirement == "clear") {
        return 1;
    }
    if (requirement == "land_or_aqueduct") {
        cell->permitted_blocking_terrain = TERRAIN_AQUEDUCT;
    } else if (requirement == "water") {
        cell->required_terrain = TERRAIN_WATER;
        cell->permitted_blocking_terrain = TERRAIN_WATER;
    } else if (requirement == "road") {
        cell->required_terrain = TERRAIN_ROAD;
        cell->permitted_blocking_terrain = TERRAIN_ROAD;
    } else if (requirement == "road_or_land") {
        cell->permitted_blocking_terrain = TERRAIN_ROAD | TERRAIN_HIGHWAY;
    } else if (requirement == "road_wall_or_land" || requirement == "road_or_wall_or_land") {
        cell->permitted_blocking_terrain = TERRAIN_ROAD | TERRAIN_HIGHWAY | TERRAIN_WALL;
    } else if (requirement == "wall") {
        cell->required_terrain = TERRAIN_WALL;
        cell->permitted_blocking_terrain = TERRAIN_WALL;
    } else if (requirement == "aqueduct") {
        cell->required_terrain = TERRAIN_AQUEDUCT;
        cell->permitted_blocking_terrain = TERRAIN_AQUEDUCT;
    } else if (requirement == "meadow") {
        cell->required_terrain = TERRAIN_MEADOW;
    } else if (requirement == "any") {
        cell->permitted_blocking_terrain = TERRAIN_ALL;
    } else {
        return 0;
    }
    return 1;
}

int parse_root()
{
    std::string type;
    int disabled = 0;
    int width = 0;
    int height = 0;
    int rotates = 0;
    if (g_parse_state.saw_root ||
        !xml_definition::parse_required_nonempty_string_attribute("type", &type) ||
        (xml_parser_has_attribute("disabled") &&
            !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled))) {
        fail("Foundation xml is missing or has invalid identity attributes");
        return 0;
    }
    type = xml_definition::normalize_path(type.c_str());
    if (type != g_parse_state.expected_path) {
        fail("Foundation type does not match its definition path", type.c_str());
        return 0;
    }

    g_parse_state.definition = std::make_unique<FoundationDef>(type);
    g_parse_state.disabled = disabled;
    g_parse_state.saw_root = 1;
    if (disabled) {
        if (xml_parser_has_attribute("width") || xml_parser_has_attribute("height") ||
            xml_parser_has_attribute("rotates") || xml_parser_has_attribute("default_permissions") ||
            xml_parser_has_attribute("configurable_permissions") || xml_parser_has_attribute("site_requires")) {
            fail("Disabled Foundation tombstone must contain only type and disabled", type.c_str());
            return 0;
        }
        return 1;
    }

    if (!xml_definition::parse_required_positive_int_attribute("width", &width) ||
        !xml_definition::parse_required_positive_int_attribute("height", &height) ||
        !xml_parser_has_attribute("rotates") ||
        !xml_value::parse_bool(xml_parser_get_attribute_string("rotates"), &rotates)) {
        fail("Foundation xml is missing or has invalid required attributes");
        return 0;
    }
    if (width > 64 || height > 64) {
        fail("Foundation dimensions exceed 64 cells", type.c_str());
        return 0;
    }

    uint16_t default_permissions = 0;
    uint16_t configurable_permissions = 0;
    if ((xml_parser_has_attribute("default_permissions") &&
            !parse_permissions(xml_parser_get_attribute_string("default_permissions"), &default_permissions)) ||
        (xml_parser_has_attribute("configurable_permissions") &&
            !parse_permissions(xml_parser_get_attribute_string("configurable_permissions"), &configurable_permissions))) {
        fail("Foundation has invalid road permissions", type.c_str());
        return 0;
    }
    if (default_permissions & ~configurable_permissions) {
        fail("Foundation default permissions must be configurable", type.c_str());
        return 0;
    }

    uint32_t site_requirements = 0;
    if (xml_parser_has_attribute("site_requires") &&
        !parse_site_mask(xml_parser_get_attribute_string("site_requires"), &site_requirements)) {
        fail("Foundation has invalid site requirements", type.c_str());
        return 0;
    }

    g_parse_state.definition->set_dimensions(width, height);
    g_parse_state.definition->set_rotates(rotates);
    g_parse_state.definition->set_permissions(default_permissions, configurable_permissions);
    g_parse_state.definition->set_site_requirements(site_requirements);
    return 1;
}

int parse_profile()
{
    if (g_parse_state.disabled) {
        fail("Disabled Foundation tombstone contains profile data");
        return 0;
    }
    if (!g_parse_state.definition || !xml_parser_has_attribute("symbol") ||
        !xml_parser_has_attribute("requires")) {
        fail("Foundation profile is missing symbol or requires");
        return 0;
    }
    const std::string symbol_text = xml_value::trim_copy(xml_parser_get_attribute_string("symbol"));
    if (symbol_text.size() != 1 || symbol_text[0] == '.' ||
        std::isspace(static_cast<unsigned char>(symbol_text[0])) || g_parse_state.profiles.count(symbol_text[0])) {
        fail("Foundation profile has invalid or duplicate symbol", symbol_text.c_str());
        return 0;
    }

    FoundationCellDefinition profile;
    profile.symbol = symbol_text[0];
    if (!parse_requirement(xml_parser_get_attribute_string("requires"), &profile)) {
        fail("Foundation profile has invalid terrain requirement", xml_parser_get_attribute_string("requires"));
        return 0;
    }
    uint32_t mask = 0;
    if (xml_parser_has_attribute("permits")) {
        if (!parse_map_terrain_mask(xml_parser_get_attribute_string("permits"), &mask)) {
            fail("Foundation profile has invalid permits mask", symbol_text.c_str());
            return 0;
        }
        profile.permitted_blocking_terrain |= mask;
    }
    if (xml_parser_has_attribute("adds")) {
        if (!parse_map_terrain_mask(xml_parser_get_attribute_string("adds"), &profile.added_terrain)) {
            fail("Foundation profile has invalid adds mask", symbol_text.c_str());
            return 0;
        }
    }
    if (xml_parser_has_attribute("removes")) {
        if (!parse_map_terrain_mask(xml_parser_get_attribute_string("removes"), &profile.removed_terrain)) {
            fail("Foundation profile has invalid removes mask", symbol_text.c_str());
            return 0;
        }
    }
    if (profile.added_terrain & profile.removed_terrain) {
        fail("Foundation profile adds and removes the same terrain", symbol_text.c_str());
        return 0;
    }
    if (xml_parser_has_attribute("binds") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("binds"), &profile.binds_building)) {
        fail("Foundation profile has invalid binds value", symbol_text.c_str());
        return 0;
    }
    if (xml_parser_has_attribute("passage")) {
        const char *passage = xml_parser_get_attribute_string("passage");
        if (xml_value::equals(passage, "uncontrolled")) {
            profile.passage = FoundationPassage::Uncontrolled;
        } else if (xml_value::equals(passage, "owner_controlled")) {
            profile.passage = FoundationPassage::OwnerControlled;
        } else if (!xml_value::equals(passage, "none")) {
            fail("Foundation profile has invalid passage", passage);
            return 0;
        }
    }
    if (profile.passage != FoundationPassage::None && !(profile.added_terrain & TERRAIN_ROAD)) {
        fail("Foundation passage profile must add road terrain", symbol_text.c_str());
        return 0;
    }
    if (profile.binds_building != ((profile.added_terrain & TERRAIN_BUILDING) != 0)) {
        fail("Foundation profile building terrain must match binds", symbol_text.c_str());
        return 0;
    }
    if (profile.passage == FoundationPassage::OwnerControlled &&
        (!profile.binds_building || !(profile.added_terrain & TERRAIN_BUILDING))) {
        fail("Owner-controlled passage must bind its building", symbol_text.c_str());
        return 0;
    }
    if (profile.passage == FoundationPassage::Uncontrolled &&
        (profile.binds_building || (profile.added_terrain & TERRAIN_BUILDING))) {
        fail("Uncontrolled passage cannot bind a building", symbol_text.c_str());
        return 0;
    }
    g_parse_state.profiles.emplace(profile.symbol, profile);
    return 1;
}

int parse_row()
{
    if (g_parse_state.disabled) {
        fail("Disabled Foundation tombstone contains row data");
        return 0;
    }
    if (!g_parse_state.definition || !xml_parser_has_attribute("cells")) {
        fail("Foundation row is missing cells");
        return 0;
    }
    g_parse_state.rows.emplace_back(xml_parser_get_attribute_string("cells"));
    return 1;
}

int parse_rows()
{
    if (g_parse_state.disabled) {
        fail("Disabled Foundation tombstone contains rows data");
        return 0;
    }
    return 1;
}

void parse_foundation_text(const char *text)
{
    if (g_parse_state.disabled && !xml_value::trim_copy(text ? text : "").empty()) {
        fail("Disabled Foundation tombstone contains text data");
    }
}

const xml_parser_element XML_ELEMENTS[] = {
    {"foundation", parse_root, nullptr, nullptr, parse_foundation_text},
    {"profile", parse_profile, nullptr, "foundation", nullptr},
    {"rows", parse_rows, nullptr, "foundation", nullptr},
    {"row", parse_row, nullptr, "rows", nullptr}
};

int finish_definition()
{
    if (!g_parse_state.definition || !g_parse_state.saw_root || g_parse_state.error) {
        return 0;
    }
    if (g_parse_state.disabled) {
        return g_parse_state.profiles.empty() && g_parse_state.rows.empty();
    }
    if (static_cast<int>(g_parse_state.rows.size()) != g_parse_state.definition->height()) {
        fail("Foundation row count does not match height", g_parse_state.definition->path());
        return 0;
    }
    for (int y = 0; y < g_parse_state.definition->height(); ++y) {
        const std::string &row = g_parse_state.rows[y];
        if (static_cast<int>(row.size()) != g_parse_state.definition->width()) {
            fail("Foundation row width does not match width", g_parse_state.definition->path());
            return 0;
        }
        for (int x = 0; x < g_parse_state.definition->width(); ++x) {
            const char symbol = row[x];
            if (symbol == '.') {
                continue;
            }
            const auto profile = g_parse_state.profiles.find(symbol);
            if (profile == g_parse_state.profiles.end()) {
                fail("Foundation row references an unknown profile", g_parse_state.definition->path());
                return 0;
            }
            FoundationCellDefinition cell = profile->second;
            cell.x = x;
            cell.y = y;
            g_parse_state.definition->add_cell(std::move(cell));
        }
    }
    if (g_parse_state.definition->cells().empty()) {
        fail("Foundation contains no active cells", g_parse_state.definition->path());
        return 0;
    }
    const bool has_controlled_passage = std::any_of(
        g_parse_state.definition->cells().begin(), g_parse_state.definition->cells().end(),
        [](const FoundationCellDefinition &cell) { return cell.passage == FoundationPassage::OwnerControlled; });
    if ((g_parse_state.definition->default_permissions() ||
            g_parse_state.definition->configurable_permissions()) && !has_controlled_passage) {
        fail("Foundation road permissions require an owner-controlled passage",
            g_parse_state.definition->path());
        return 0;
    }
    return 1;
}

struct ParsedDefinition {
    std::unique_ptr<FoundationDef> definition;
    int disabled = 0;
};

int take_parsed_definition(
    int parsed,
    const char *source_name,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    if (!parsed || !finish_definition()) {
        log_error("Unable to parse Foundation xml", source_name, 0);
        if (failure_reason) {
            *failure_reason = std::string("Unable to parse Foundation xml: ") +
                (source_name ? source_name : "<unknown>");
        }
        return 0;
    }
    result.definition = std::move(g_parse_state.definition);
    result.disabled = g_parse_state.disabled;
    return 1;
}

int parse_definition_file(
    const char *filename,
    const char *definition_path,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    ErrorContextScope error_scope("foundation_registry.parse_definition", filename);
    g_parse_state = {};
    g_parse_state.expected_path = definition_path ? definition_path : "";
    const int parsed = xml_definition::parse_file(
        filename, "Foundation", XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    return take_parsed_definition(parsed, filename, result, failure_reason);
}

int parse_definition_buffer(
    const char *xml,
    const char *definition_path,
    const char *source_name,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    g_parse_state = {};
    g_parse_state.expected_path = definition_path ? definition_path : "";
    const char *source = xml ? xml : "";
    std::vector<char> buffer(source, source + std::strlen(source));
    const int parsed = xml_definition::parse_buffer(
        source_name ? source_name : "FoundationTestFixture.xml",
        "Foundation",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    return take_parsed_definition(parsed, source_name, result, failure_reason);
}

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

using LayeredDefinitions = std::map<std::string, LayeredDefinition>;

struct StagedRegistry {
    std::vector<std::unique_ptr<FoundationDef>> owned;
    std::vector<const FoundationDef *> definitions;
    mod_definition::DefinitionOverlayTracker overlays;
};

bool stage_definition(
    LayeredDefinitions &winners,
    mod_definition::DefinitionOverlayTracker &overlays,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->path() : "";
    if (!overlays.apply(stable_id, parsed.disabled != 0, source)) {
        log_error("Unable to layer Foundation definition", overlays.failure_reason().c_str(), 0);
        error_context_report_error("Unable to layer Foundation definition.", overlays.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = overlays.failure_reason();
        }
        return false;
    }
    winners[stable_id] = {std::move(parsed), source};
    return true;
}

void materialize_winners(
    LayeredDefinitions &winners,
    mod_definition::DefinitionOverlayTracker overlays,
    StagedRegistry &staged)
{
    staged.owned.reserve(winners.size());
    staged.definitions.reserve(winners.size());
    for (auto &entry : winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        staged.owned.push_back(std::move(winner.parsed.definition));
        staged.definitions.push_back(staged.owned.back().get());
    }
    staged.overlays = std::move(overlays);
}

bool build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedRegistry &staged,
    std::string *failure_reason)
{
    LayeredDefinitions winners;
    mod_definition::DefinitionOverlayTracker overlays;
    if (!mod_definition::for_each_definition_file(
            layers,
            {"Foundations"},
            "Foundation",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                return parse_definition_file(
                           source.full_path.c_str(),
                           source.normalized_definition_path.c_str(),
                           parsed,
                           failure_reason) &&
                    stage_definition(winners, overlays, std::move(parsed), source, failure_reason);
            },
            nullptr,
            failure_reason)) {
        return false;
    }
    materialize_winners(winners, std::move(overlays), staged);
    return true;
}

} // namespace

const FoundationDef *find_foundation_definition(const char *path)
{
    if (!path || !*path) {
        return nullptr;
    }
    const std::string normalized = xml_definition::normalize_path(path);
    for (const FoundationDef *definition : g_foundations) {
        if (definition && normalized == definition->path()) {
            return definition;
        }
    }
    return nullptr;
}

const std::vector<const FoundationDef *> &foundation_definitions() { return g_foundations; }

const mod_definition::DefinitionOverlayEntry *find_foundation_definition_overlay(const char *path)
{
    if (!path || !*path) {
        return nullptr;
    }
    return g_foundation_overlays.find(xml_definition::normalize_path(path));
}

} // namespace building_type_registry_impl

int foundation_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        log_error("Unable to configure Foundation definition layers", failure_reason.c_str(), 0);
        return 0;
    }
    return foundation_registry_load_layers(layers, &failure_reason);
}

int foundation_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    g_owned_foundations = std::move(staged.owned);
    g_foundations = std::move(staged.definitions);
    g_foundation_overlays = std::move(staged.overlays);
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int foundation_definition_buffer_is_valid_for_test(const char *xml, const char *definition_path)
{
    using namespace building_type_registry_impl;
    ParsedDefinition parsed;
    return parse_definition_buffer(
        xml, definition_path, "FoundationTestFixture.xml", parsed, nullptr);
}

int foundation_layered_definition_buffers_are_valid_for_test(
    const foundation_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    foundation_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    LayeredDefinitions winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const foundation_layer_test_input &input = inputs[index];
        mod_definition::DefinitionSource source;
        source.layer_index = static_cast<std::size_t>(std::max(0, input.layer));
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.full_path = input.source_path ? input.source_path : "";
        source.normalized_definition_path =
            xml_definition::normalize_path(input.definition_path ? input.definition_path : "");
        source.registry_relative_path = "Foundations\\" + source.normalized_definition_path;

        ParsedDefinition parsed;
        if (!parse_definition_buffer(
                input.xml,
                source.normalized_definition_path.c_str(),
                source.full_path.c_str(),
                parsed,
                &failure_reason) ||
            !stage_definition(winners, overlays, std::move(parsed), source, &failure_reason)) {
            return 0;
        }
    }

    StagedRegistry staged;
    materialize_winners(winners, std::move(overlays), staged);
    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        const std::string normalized_query = xml_definition::normalize_path(query_path);
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(normalized_query);
        if (overlay) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
        }
        for (const FoundationDef *definition : staged.definitions) {
            if (definition && normalized_query == definition->path()) {
                result->queried_width = definition->width();
                break;
            }
        }
    }
    return 1;
}
#endif
