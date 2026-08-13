#include "figure/formation_layout.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_definition_loader.h"

#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace formation_layout_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<FormationLayoutDef>> g_layouts;
mod_definition::DefinitionOverlayTracker g_overlays;

namespace {

constexpr int AUTHORED_POSITION_COUNT = 16;
constexpr int ARMY_ORIENTATION_COUNT = 4;
constexpr int AUTHORED_ARMY_POSITION_COUNT = 7;

struct RequiredLayoutIdentity {
    const char *key;
    int legacy_id;
};

constexpr RequiredLayoutIdentity REQUIRED_LAYOUTS[] = {
    {"column", formation_layout_legacy::COLUMN},
    {"double_line_1", formation_layout_legacy::DOUBLE_LINE_1},
    {"double_line_2", formation_layout_legacy::DOUBLE_LINE_2},
    {"single_line_1", formation_layout_legacy::SINGLE_LINE_1},
    {"single_line_2", formation_layout_legacy::SINGLE_LINE_2},
    {"tortoise", formation_layout_legacy::TORTOISE},
    {"mop_up", formation_layout_legacy::MOP_UP},
    {"at_rest", formation_layout_legacy::AT_REST},
    {"enemy_mob", formation_layout_legacy::ENEMY_MOB},
    {"herd", formation_layout_legacy::HERD},
    {"enemy_double_line", formation_layout_legacy::ENEMY_DOUBLE_LINE},
    {"enemy_wide_column", formation_layout_legacy::ENEMY_WIDE_COLUMN},
    {"enemy_12", formation_layout_legacy::ENEMY_12}
};
static_assert(sizeof(REQUIRED_LAYOUTS) / sizeof(REQUIRED_LAYOUTS[0]) == formation_layout_legacy::COUNT);

struct ParseState {
    std::string key;
    int legacy_id = -1;
    std::vector<FormationLayoutPosition> positions;
    FormationLayoutDef::ArmyOffsets army_offsets;
    std::array<bool, ARMY_ORIENTATION_COUNT> saw_army_orientation = {};
    std::string army_offsets_reference;
    int current_army_orientation = -1;
    bool disabled = false;
    bool saw_root = false;
    bool error = false;
};

struct StagedLayout {
    std::string key;
    int legacy_id = -1;
    std::vector<FormationLayoutPosition> positions;
    FormationLayoutDef::ArmyOffsets army_offsets;
    std::string army_offsets_reference;
    std::unique_ptr<FormationLayoutDef> definition;
    mod_definition::DefinitionSource source;
    bool disabled = false;
};

struct StagedLayouts {
    std::map<std::string, StagedLayout> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

ParseState g_parse_state;

bool parse_signed_attribute(const char *attribute, int *value)
{
    return xml_parser_has_attribute(attribute) &&
        xml_value::parse_int_strict(xml_parser_get_attribute_string(attribute), value);
}

int parse_root()
{
    if (g_parse_state.saw_root) {
        log_error("FormationLayout contains duplicate root nodes", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    if (!xml_definition::parse_required_nonempty_string_attribute("key", &g_parse_state.key)) {
        log_error("FormationLayout xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("FormationLayout has invalid Boolean attribute 'disabled'", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.disabled = disabled != 0;

    if (!g_parse_state.disabled &&
        (!xml_definition::parse_required_nonnegative_int_attribute("legacy_id", &g_parse_state.legacy_id) ||
            g_parse_state.legacy_id >= formation_layout_legacy::COUNT)) {
        log_error("FormationLayout requires legacy_id in the supported save range", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.disabled && xml_parser_has_attribute("legacy_id")) {
        log_error("Disabled FormationLayout cannot declare legacy_id", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (xml_parser_has_attribute("army_from")) {
        if (g_parse_state.disabled ||
            !xml_definition::parse_required_nonempty_string_attribute(
                "army_from", &g_parse_state.army_offsets_reference)) {
            log_error("FormationLayout has invalid army_from reference", g_parse_state.key.c_str(), 0);
            g_parse_state.error = true;
            return 0;
        }
    }

    g_parse_state.saw_root = true;
    return 1;
}

int parse_position()
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        log_error("Disabled FormationLayout definition must contain only its root identity", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    FormationLayoutPosition position = {};
    if (!parse_signed_attribute("x", &position.x) || !parse_signed_attribute("y", &position.y)) {
        log_error("FormationLayout position requires signed integer x and y", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (g_parse_state.current_army_orientation >= 0) {
        g_parse_state.army_offsets[static_cast<size_t>(g_parse_state.current_army_orientation)]
            .push_back(position);
    } else {
        g_parse_state.positions.push_back(position);
    }
    return 1;
}

int parse_army()
{
    if (!g_parse_state.saw_root || g_parse_state.disabled ||
        !g_parse_state.army_offsets_reference.empty() || g_parse_state.current_army_orientation >= 0) {
        log_error("FormationLayout army offsets cannot be nested or combined with army_from", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    int orientation = -1;
    if (!xml_definition::parse_required_nonnegative_int_attribute("orientation", &orientation) ||
        orientation >= ARMY_ORIENTATION_COUNT || g_parse_state.saw_army_orientation[orientation]) {
        log_error("FormationLayout army requires a unique orientation from 0 through 3", g_parse_state.key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.saw_army_orientation[orientation] = true;
    g_parse_state.current_army_orientation = orientation;
    return 1;
}

void finish_army()
{
    g_parse_state.current_army_orientation = -1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "layout", parse_root, nullptr, nullptr, nullptr },
    { "army", parse_army, finish_army, "layout", nullptr },
    { "position", parse_position, nullptr, "layout|army", nullptr }
};

int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    StagedLayout *out_definition)
{
    ErrorContextScope error_scope("formation_layout_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_buffer(
        filename,
        "FormationLayout",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || g_parse_state.key.empty()) {
        log_error("Unable to parse FormationLayout xml", filename, 0);
        return 0;
    }
    if (!g_parse_state.disabled &&
        static_cast<int>(g_parse_state.positions.size()) != AUTHORED_POSITION_COUNT) {
        log_error("FormationLayout requires exactly 16 authored positions", filename,
            static_cast<int>(g_parse_state.positions.size()));
        return 0;
    }
    if (!g_parse_state.disabled) {
        const bool uses_reference = !g_parse_state.army_offsets_reference.empty();
        bool has_army_content = uses_reference;
        for (bool saw_orientation : g_parse_state.saw_army_orientation) {
            has_army_content = has_army_content || saw_orientation;
        }
        for (int orientation = 0; orientation < ARMY_ORIENTATION_COUNT; ++orientation) {
            const int position_count = static_cast<int>(g_parse_state.army_offsets[orientation].size());
            if (has_army_content &&
                ((!uses_reference && (!g_parse_state.saw_army_orientation[orientation] ||
                    position_count != AUTHORED_ARMY_POSITION_COUNT)) ||
                (uses_reference && (g_parse_state.saw_army_orientation[orientation] || position_count != 0)))) {
                log_error(
                    "FormationLayout requires either army_from or four army orientations with seven positions each",
                    filename,
                    orientation);
                return 0;
            }
        }
    }

    if (out_definition) {
        out_definition->key = std::move(g_parse_state.key);
        out_definition->legacy_id = g_parse_state.legacy_id;
        out_definition->positions = std::move(g_parse_state.positions);
        out_definition->army_offsets = std::move(g_parse_state.army_offsets);
        out_definition->army_offsets_reference = std::move(g_parse_state.army_offsets_reference);
        out_definition->disabled = g_parse_state.disabled;
    }
    return 1;
}

int parse_definition_file(const mod_definition::DefinitionSource &source, StagedLayout *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "FormationLayout")) {
        return 0;
    }
    return parse_definition_buffer(source.full_path.c_str(), buffer, out_definition);
}

int stage_definition(StagedLayouts &staged, StagedLayout definition)
{
    if (!staged.overlays.apply(definition.key, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    staged.winners.insert_or_assign(definition.key, std::move(definition));
    return 1;
}

int resolve_and_validate_winners(StagedLayouts &staged, bool require_full_coverage)
{
    std::map<int, std::string> legacy_owners;
    for (auto &entry : staged.winners) {
        StagedLayout &winner = entry.second;
        if (winner.disabled) {
            continue;
        }
        const auto existing = legacy_owners.find(winner.legacy_id);
        if (existing != legacy_owners.end()) {
            staged.failure_reason = "FormationLayout legacy_id " + std::to_string(winner.legacy_id) +
                " is shared by '" + existing->second + "' and '" + winner.key + "'.";
            return 0;
        }
        legacy_owners.emplace(winner.legacy_id, winner.key);
        winner.definition = std::make_unique<FormationLayoutDef>(
            winner.key,
            winner.legacy_id,
            std::move(winner.positions),
            std::move(winner.army_offsets),
            std::move(winner.army_offsets_reference));
    }
    if (require_full_coverage && legacy_owners.size() != formation_layout_legacy::COUNT) {
        staged.failure_reason = "FormationLayout definitions must cover every supported legacy layout id.";
        return 0;
    }
    if (require_full_coverage) {
        for (const RequiredLayoutIdentity &required : REQUIRED_LAYOUTS) {
            const auto winner = staged.winners.find(required.key);
            if (winner == staged.winners.end() || winner->second.disabled ||
                !winner->second.definition || winner->second.definition->legacy_id() != required.legacy_id) {
                staged.failure_reason = "FormationLayout requires stable identity '" +
                    std::string(required.key) + "' with legacy_id " +
                    std::to_string(required.legacy_id) + ".";
                return 0;
            }
        }
    }
    for (auto &entry : staged.winners) {
        StagedLayout &winner = entry.second;
        if (winner.disabled || !winner.definition || winner.definition->has_authored_army_offsets()) {
            continue;
        }
        const std::string reference = winner.definition->army_offsets_reference();
        if (!require_full_coverage && reference.empty()) {
            continue;
        }
        const auto target = staged.winners.find(reference);
        if (reference.empty() || reference == winner.key || target == staged.winners.end() ||
            target->second.disabled || !target->second.definition ||
            !target->second.definition->has_authored_army_offsets()) {
            staged.failure_reason = "FormationLayout '" + winner.key +
                "' has invalid or indirect army_from reference '" + reference + "'.";
            return 0;
        }
        winner.definition->bind_army_offsets_reference(target->second.definition.get());
    }
    return 1;
}

} // namespace

const FormationLayoutDef *find_layout(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    for (const std::unique_ptr<FormationLayoutDef> &layout : g_layouts) {
        if (layout && layout->matches_key(key)) {
            return layout.get();
        }
    }
    return nullptr;
}

const FormationLayoutDef *find_layout_by_legacy_id(int legacy_id)
{
    for (const std::unique_ptr<FormationLayoutDef> &layout : g_layouts) {
        if (layout && layout->legacy_id() == legacy_id) {
            return layout.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<FormationLayoutDef>> &layouts()
{
    return g_layouts;
}

} // namespace formation_layout_registry_impl

FormationLayoutDef::FormationLayoutDef(
    std::string key,
    int legacy_id,
    std::vector<FormationLayoutPosition> positions,
    ArmyOffsets army_offsets,
    std::string army_offsets_reference)
    : key_(std::move(key)),
      legacy_id_(legacy_id),
      positions_(std::move(positions)),
      army_offsets_(std::move(army_offsets)),
      army_offsets_reference_(std::move(army_offsets_reference))
{
}

const char *FormationLayoutDef::key() const
{
    return key_.c_str();
}

bool FormationLayoutDef::matches_key(const char *key) const
{
    return xml_value::equals(key_.c_str(), key);
}

int FormationLayoutDef::legacy_id() const
{
    return legacy_id_;
}

int FormationLayoutDef::authored_position_count() const
{
    return static_cast<int>(positions_.size());
}

FormationLayoutPosition FormationLayoutDef::authored_position(int index) const
{
    if (index < 0 || index >= authored_position_count()) {
        return {0, 0};
    }
    return positions_[static_cast<size_t>(index)];
}

bool FormationLayoutDef::has_authored_army_offsets() const
{
    for (const std::vector<FormationLayoutPosition> &orientation : army_offsets_) {
        if (orientation.empty()) {
            return false;
        }
    }
    return true;
}

const char *FormationLayoutDef::army_offsets_reference() const
{
    return army_offsets_reference_.c_str();
}

void FormationLayoutDef::bind_army_offsets_reference(const FormationLayoutDef *layout)
{
    army_offsets_definition_ = layout;
}

FormationLayoutPosition FormationLayoutDef::army_offset(int orientation, int formation_index) const
{
    const FormationLayoutDef *source = army_offsets_definition_ ? army_offsets_definition_ : this;
    if (orientation < 0 || orientation >= static_cast<int>(source->army_offsets_.size()) ||
        formation_index < 0) {
        return {0, 0};
    }
    const std::vector<FormationLayoutPosition> &offsets =
        source->army_offsets_[static_cast<size_t>(orientation)];
    return formation_index < static_cast<int>(offsets.size()) ?
        offsets[static_cast<size_t>(formation_index)] : FormationLayoutPosition{0, 0};
}

const char *formation_layout_registry_get_failure_reason(void)
{
    return formation_layout_registry_impl::g_failure_reason.c_str();
}

const char *formation_layout_definition_source_path(const char *key)
{
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_layout_registry_impl::g_overlays.find(key ? key : "");
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int formation_layout_definition_is_suppressed(const char *key)
{
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_layout_registry_impl::g_overlays.find(key ? key : "");
    return entry && entry->disabled;
}

void formation_layout_registry_reset(void)
{
    formation_layout_registry_impl::g_layouts.clear();
    formation_layout_registry_impl::g_overlays.clear();
    formation_layout_registry_impl::g_failure_reason.clear();
}

const FormationLayoutDef *formation_layout_from_legacy_id(int legacy_id)
{
    const FormationLayoutDef *layout =
        formation_layout_registry_impl::find_layout_by_legacy_id(legacy_id);
    return layout ? layout : formation_layout_registry_impl::find_layout("column");
}

int formation_layout_to_legacy_id(const FormationLayoutDef *layout)
{
    return layout ? layout->legacy_id() : formation_layout_legacy::COLUMN;
}

int formation_layout_registry_load(void)
{
    using namespace formation_layout_registry_impl;

    StagedLayouts staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"FormationLayout"},
            "FormationLayout",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedLayout definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse FormationLayout xml: " + source.full_path;
                    return false;
                }
                return stage_definition(staged, std::move(definition)) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        g_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        return 0;
    }
    if (!resolve_and_validate_winners(staged, true)) {
        g_failure_reason = staged.failure_reason;
        return 0;
    }

    std::vector<std::unique_ptr<FormationLayoutDef>> published;
    published.reserve(staged.overlays.active_count());
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            published.push_back(std::move(entry.second.definition));
        }
    }
    g_layouts = std::move(published);
    g_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int formation_layout_layered_definition_buffers_are_valid_for_test(
    const formation_layout_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_layout_layer_test_result *result)
{
    using namespace formation_layout_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedLayouts staged;
    for (int index = 0; index < input_count; ++index) {
        const formation_layout_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedLayout definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "FormationLayoutLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "FormationLayout";
        if (!parse_definition_buffer(definition.source.full_path.c_str(), buffer, &definition) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!resolve_and_validate_winners(staged, false)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_legacy_id = -1;
        result->queried_source_layer = -1;
        const std::string key = query_key ? query_key : "";
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(key);
        const auto winner = staged.winners.find(key);
        if (overlay && winner != staged.winners.end()) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            if (!winner->second.disabled && winner->second.definition) {
                result->queried_legacy_id = winner->second.definition->legacy_id();
                result->queried_position_count = winner->second.definition->authored_position_count();
            }
        }
    }
    return 1;
}
#endif
