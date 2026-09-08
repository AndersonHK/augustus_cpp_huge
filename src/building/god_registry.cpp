#include "building/god_registry.h"

#include "city/god.h"
#include "core/Logger.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <array>
#include <set>
#include <stdexcept>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace building_type_registry_impl {

namespace {

std::unordered_map<std::string, std::unique_ptr<God>> g_gods;
std::vector<const God *> g_runtime_gods;
mod_definition::DefinitionOverlayTracker g_definition_sources;
std::string g_failure_reason;

god_type parse_god_type(const char *value)
{
    const std::string text = xml_value::trim_copy(value ? value : "");
    if (text == "ceres") {
        return GOD_CERES;
    }
    if (text == "neptune") {
        return GOD_NEPTUNE;
    }
    if (text == "mercury") {
        return GOD_MERCURY;
    }
    if (text == "mars") {
        return GOD_MARS;
    }
    if (text == "venus") {
        return GOD_VENUS;
    }
    return GOD_ALL;
}

struct ParsedDefinition {
    std::unique_ptr<God> definition;
    bool disabled = false;
};

bool parse_definition_buffer(
    const char *filename,
    const char *definition_path,
    const std::vector<char> &buffer,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    Logger::Scope error_scope("god_registry.parse_definition", filename);
    const std::string stable_id = xml_definition::normalize_path(definition_path);
    try {
        std::string xml(buffer.begin(), buffer.end());
        while (!xml.empty() && !xml.back()) xml.pop_back();
        const auto root = mod_content::parse(xml);
        if (root.name != "god" || stable_id.empty()) throw std::runtime_error("Invalid God root or identity");
        const auto legacy = parse_god_type(root.attribute("legacy").c_str());
        if (legacy == GOD_ALL) throw std::runtime_error("God requires a valid legacy save slot");
        int disabled = 0;
        if (root.attributes.count("disabled") && !xml_value::parse_bool(root.attribute("disabled").c_str(), &disabled)) throw std::runtime_error("Invalid God disabled value");
        for (const auto &attribute : root.attributes) if (attribute.first != "legacy" && attribute.first != "disabled") throw std::runtime_error("Unknown God attribute " + attribute.first);
        result.disabled = disabled != 0;
        result.definition = std::make_unique<God>(stable_id);
        result.definition->set_legacy_type(legacy);
        if (disabled && !root.children.empty()) throw std::runtime_error("Disabled God contains definition data");
        std::set<std::string> sections;
        for (const auto &child : root.children) {
            if (!sections.insert(child.name).second) throw std::runtime_error("Duplicate God section " + child.name);
            if (child.name == "wrath") {
                auto &rules = result.definition->wrath;
                const std::map<std::string, int *> fields{{"neutral", &rules.neutral}, {"threshold", &rules.threshold}, {"mild_threshold", &rules.mild_threshold}, {"severe_threshold", &rules.severe_threshold}, {"mild_amount", &rules.mild_amount}, {"moderate_amount", &rules.moderate_amount}, {"severe_amount", &rules.severe_amount}, {"maximum", &rules.maximum}, {"favor_decay", &rules.favor_decay}};
                if (!child.children.empty() || child.attributes.size() != fields.size()) throw std::runtime_error("Incomplete wrath rules");
                for (const auto &field : fields) if (!xml_value::parse_int_strict(child.attribute(field.first).c_str(), field.second) || *field.second < 0 || *field.second > 100) throw std::runtime_error("Invalid wrath rule");
                if (rules.neutral < rules.threshold || rules.threshold < rules.mild_threshold || rules.mild_threshold < rules.severe_threshold) throw std::runtime_error("Wrath thresholds must be descending");
            } else if (child.name == "favor") {
                auto &rules = result.definition->favor;
                if (child.attributes.empty() && child.children.empty()) continue;
                rules.enabled = true;
                const std::map<std::string, int *> fields{{"neutral", &rules.neutral}, {"happiness_divisor", &rules.happiness_divisor}, {"base_chance", &rules.base_chance}, {"festival_months", &rules.festival_months}, {"festival_divisor", &rules.festival_divisor}, {"maximum", &rules.maximum}};
                if (!child.children.empty() || child.attributes.size() != fields.size()) throw std::runtime_error("Incomplete favor rules");
                for (const auto &field : fields) if (!xml_value::parse_int_strict(child.attribute(field.first).c_str(), field.second) || *field.second < 0 || *field.second > 100) throw std::runtime_error("Invalid favor rule");
                if (!rules.happiness_divisor || !rules.festival_divisor) throw std::runtime_error("Favor divisor must be positive");
            } else {
                const auto trigger = child.name == "blessings" ? religion::Trigger::Blessing : child.name == "minor_curses" ? religion::Trigger::MinorCurse : religion::Trigger::MajorCurse;
                if (child.name != "blessings" && child.name != "minor_curses" && child.name != "major_curses") throw std::runtime_error("Unknown God section " + child.name);
                if (!child.attributes.empty()) throw std::runtime_error("Unexpected effect section attributes");
                for (const auto &effect : child.children) {
                    if (effect.name != "effect") throw std::runtime_error("Expected religion effect");
                    result.definition->effects.push_back(religion::parse_effect(effect, trigger));
                }
            }
        }
    } catch (const std::exception &error) {
        const std::string detail = std::string(filename) + ": " + error.what();
        Logger::error("Unable to parse God xml", detail.c_str(), 0);
        if (failure_reason) *failure_reason = detail;
        return false;
    }
    return true;
}

bool parse_definition_file(
    const mod_definition::DefinitionSource &source,
    ParsedDefinition &result,
    std::string *failure_reason)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "God")) {
        if (failure_reason) {
            *failure_reason = xml_definition::format_failure_reason(
                "Unable to load God xml.", source.full_path.c_str());
        }
        return false;
    }
    return parse_definition_buffer(
        source.full_path.c_str(),
        source.normalized_definition_path.c_str(),
        buffer,
        result,
        failure_reason);
}

struct StableIdentity {
    god_type legacy_type = GOD_ALL;
    mod_definition::DefinitionSource source;
};

struct LayeredDefinition {
    ParsedDefinition parsed;
    mod_definition::DefinitionSource source;
};

struct StagedRegistry {
    mod_definition::DefinitionOverlayTracker overlay;
    std::map<std::string, StableIdentity> identities;
    std::map<std::string, LayeredDefinition> winners;
    std::unordered_map<std::string, std::unique_ptr<God>> definitions;
    std::vector<const God *> runtime_gods;
};

bool stage_definition(
    StagedRegistry &staged,
    ParsedDefinition parsed,
    const mod_definition::DefinitionSource &source,
    std::string *failure_reason)
{
    const std::string stable_id = parsed.definition ? parsed.definition->path() : "";
    const god_type legacy_type = parsed.definition ? parsed.definition->legacy_type() : GOD_ALL;
    const auto identity = staged.identities.find(stable_id);
    if (identity != staged.identities.end() && identity->second.legacy_type != legacy_type) {
        const std::string detail = "God '" + stable_id + "' changes legacy identity from " +
            std::to_string(identity->second.legacy_type) + " in " + identity->second.source.describe() +
            " to " + std::to_string(legacy_type) + " in " + source.describe() + '.';
        Logger::error("God replacement changes stable legacy identity", detail.c_str(), 0);
        Logger::error("God replacement changes stable legacy identity.", detail.c_str());
        if (failure_reason) {
            *failure_reason = detail;
        }
        return false;
    }
    if (identity == staged.identities.end()) {
        staged.identities.emplace(stable_id, StableIdentity{legacy_type, source});
    }

    if (!staged.overlay.apply(stable_id, parsed.disabled, source)) {
        Logger::error("Unable to layer God definition", staged.overlay.failure_reason().c_str(), 0);
        Logger::error("Unable to layer God definition.", staged.overlay.failure_reason().c_str());
        if (failure_reason) {
            *failure_reason = staged.overlay.failure_reason();
        }
        return false;
    }
    staged.winners[stable_id] = {std::move(parsed), source};
    return true;
}

bool materialize_winners(StagedRegistry &staged, std::string *failure_reason)
{
    std::array<const LayeredDefinition *, GOD_ALL> legacy_owners = {};
    for (const auto &entry : staged.winners) {
        const LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        const god_type legacy_type = winner.parsed.definition->legacy_type();
        const LayeredDefinition *existing = legacy_owners[static_cast<std::size_t>(legacy_type)];
        if (existing) {
            const std::string detail = "God legacy identity " + std::to_string(legacy_type) +
                " is claimed by both " + existing->source.describe() + " and " + winner.source.describe() + '.';
            Logger::error("Duplicate active God legacy identity", detail.c_str(), legacy_type);
            Logger::error("Duplicate active God legacy identity.", detail.c_str());
            if (failure_reason) {
                *failure_reason = detail;
            }
            return false;
        }
        legacy_owners[static_cast<std::size_t>(legacy_type)] = &winner;
    }

    int runtime_id = 0;
    for (auto &entry : staged.winners) {
        LayeredDefinition &winner = entry.second;
        if (winner.parsed.disabled) {
            continue;
        }
        winner.parsed.definition->set_runtime_id(runtime_id++);
        God *definition = winner.parsed.definition.get();
        staged.runtime_gods.push_back(definition);
        staged.definitions.emplace(entry.first, std::move(winner.parsed.definition));
    }
    return true;
}

bool build_layered_registry(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    StagedRegistry &staged,
    std::string *failure_reason)
{
    if (!mod_definition::for_each_definition_file(
            layers,
            {"Gods"},
            "God",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                ParsedDefinition parsed;
                return parse_definition_file(source, parsed, failure_reason) &&
                    stage_definition(staged, std::move(parsed), source, failure_reason);
            },
            nullptr,
            failure_reason)) {
        return false;
    }
    return materialize_winners(staged, failure_reason);
}

} // namespace

const God *find_god_definition(const char *path)
{
    const std::string normalized = xml_definition::normalize_path(path);
    const auto found = g_gods.find(normalized);
    return found != g_gods.end() ? found->second.get() : nullptr;
}

const God *find_god_definition(god_type legacy_type)
{
    for (const auto &entry : g_gods) {
        if (entry.second && entry.second->legacy_type() == legacy_type) {
            return entry.second.get();
        }
    }
    return nullptr;
}

const God *find_god_definition_by_runtime_id(int runtime_id)
{
    return runtime_id >= 0 && runtime_id < static_cast<int>(g_runtime_gods.size())
        ? g_runtime_gods[static_cast<std::size_t>(runtime_id)]
        : nullptr;
}

const God *god_definition_at_runtime_index(int index)
{
    return find_god_definition_by_runtime_id(index);
}

int god_definition_count(void)
{
    return static_cast<int>(g_runtime_gods.size());
}

} // namespace building_type_registry_impl

int god_registry_load(void)
{
    std::vector<mod_definition::DefinitionLayer> layers;
    std::string failure_reason;
    if (!mod_definition::configured_layers(layers, &failure_reason)) {
        Logger::error("Unable to configure God definition layers", failure_reason.c_str(), 0);
        building_type_registry_impl::g_failure_reason = failure_reason;
        return 0;
    }
    return god_registry_load_layers(layers, &failure_reason);
}

int god_registry_load_layers(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    std::string local_failure;
    std::string *out_failure = failure_reason ? failure_reason : &local_failure;
    if (!build_layered_registry(layers, staged, out_failure)) {
        g_failure_reason = *out_failure;
        return 0;
    }
    g_gods = std::move(staged.definitions);
    g_runtime_gods = std::move(staged.runtime_gods);
    g_definition_sources = std::move(staged.overlay);
    g_failure_reason.clear();
    return 1;
}

const char *god_registry_get_failure_reason(void)
{
    return building_type_registry_impl::g_failure_reason.c_str();
}

const char *god_definition_source_path(const char *path)
{
    const std::string stable_id = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_definition_sources.find(stable_id);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int god_definition_is_suppressed(const char *path)
{
    const std::string stable_id = xml_definition::normalize_path(path);
    const mod_definition::DefinitionOverlayEntry *entry =
        building_type_registry_impl::g_definition_sources.find(stable_id);
    return entry && entry->disabled;
}

#ifdef STARTUP_PARSER_TEST
namespace {

void fill_layer_test_result(
    const building_type_registry_impl::StagedRegistry &staged,
    const char *query_path,
    god_layer_test_result *result)
{
    if (!result) {
        return;
    }
    *result = {};
    result->active_count = static_cast<int>(staged.overlay.active_count());
    result->suppressed_count = static_cast<int>(staged.overlay.suppressed_count());
    result->queried_source_layer = -1;
    result->queried_legacy_type = GOD_ALL;
    result->queried_runtime_id = -1;
    result->runtime_count = static_cast<int>(staged.runtime_gods.size());

    const std::string stable_id = xml_definition::normalize_path(query_path);
    const mod_definition::DefinitionOverlayEntry *overlay = staged.overlay.find(stable_id);
    if (overlay) {
        result->queried_disabled = overlay->disabled ? 1 : 0;
        result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
    }
    const auto definition = staged.definitions.find(stable_id);
    if (definition != staged.definitions.end() && definition->second) {
        result->queried_legacy_type = definition->second->legacy_type();
        result->queried_runtime_id = definition->second->runtime_id();
        for (const auto &effect : definition->second->effects) for (const auto &action : effect.actions) {
            if (action.callback == religion::Callback::TradeBonus) result->queried_trade_bonus_months = action.arguments[0];
        }
        return;
    }
    const auto identity = staged.identities.find(stable_id);
    if (identity != staged.identities.end()) {
        result->queried_legacy_type = identity->second.legacy_type;
    }
}

} // namespace

int god_layered_definition_buffers_are_valid_for_test(
    const god_layer_test_input *inputs,
    int input_count,
    const char *query_path,
    god_layer_test_result *result)
{
    using namespace building_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedRegistry staged;
    std::string failure_reason;
    for (int index = 0; index < input_count; ++index) {
        const god_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        mod_definition::DefinitionSource source;
        source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        source.mod_name = input.mod_name ? input.mod_name : "";
        source.category = "Gods";
        source.full_path = input.source_path ? input.source_path : "GodLayerTest.xml";
        source.file_name = source.full_path;
        source.normalized_definition_path = xml_definition::normalize_path(input.definition_path);
        source.registry_relative_path = "Gods\\" + source.normalized_definition_path;

        ParsedDefinition parsed;
        if (!parse_definition_buffer(
                source.full_path.c_str(),
                source.normalized_definition_path.c_str(),
                buffer,
                parsed,
                &failure_reason) ||
            !stage_definition(staged, std::move(parsed), source, &failure_reason)) {
            return 0;
        }
    }
    if (!materialize_winners(staged, &failure_reason)) {
        return 0;
    }
    fill_layer_test_result(staged, query_path, result);
    return 1;
}

int god_layered_definition_files_are_valid_for_test(
    const std::vector<mod_definition::DefinitionLayer> &layers,
    const char *query_path,
    god_layer_test_result *result,
    std::string *failure_reason)
{
    using namespace building_type_registry_impl;
    StagedRegistry staged;
    if (!build_layered_registry(layers, staged, failure_reason)) {
        return 0;
    }
    fill_layer_test_result(staged, query_path, result);
    return 1;
}
#endif
