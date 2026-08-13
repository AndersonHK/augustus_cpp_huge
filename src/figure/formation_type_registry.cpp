#include "figure/formation_type.h"

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

namespace formation_type_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<FormationType>> g_formation_types;
mod_definition::DefinitionOverlayTracker g_formation_type_overlays;

namespace {

struct SlotSpec {
    int x = 0;
    int y = 0;
    std::string unit_key;
};

struct ParseState {
    std::string key;
    int width = 0;
    int height = 0;
    int recruit_capacity = 0;
    std::vector<SlotSpec> slots;
    bool disabled = false;
    bool saw_root = false;
    bool saw_grid = false;
    bool saw_slot = false;
    bool error = false;
};

struct StagedFormationType {
    std::string key;
    int width = 0;
    int height = 0;
    int recruit_capacity = 0;
    std::vector<SlotSpec> slots;
    std::unique_ptr<FormationType> definition;
    mod_definition::DefinitionSource source;
    bool disabled = false;
};

struct StagedFormationTypes {
    std::map<std::string, StagedFormationType> winners;
    mod_definition::DefinitionOverlayTracker overlays;
    std::string failure_reason;
};

ParseState g_parse_state;

int parse_enabled_content(const char *element)
{
    if (!g_parse_state.saw_root || g_parse_state.disabled) {
        log_error("Disabled FormationType definition must contain only its root identity", element, 0);
        g_parse_state.error = true;
        return 0;
    }
    return 1;
}

bool parse_row_range(const char *text, int *first_row, int *last_row)
{
    if (!text || !first_row || !last_row) {
        return false;
    }

    const std::string value = xml_value::trim_copy(text);
    const size_t separator = value.find("..");
    if (separator == std::string::npos) {
        int row = 0;
        if (!xml_value::parse_int_strict(value, &row) || row < 0) {
            return false;
        }
        *first_row = row;
        *last_row = row;
        return true;
    }

    int begin = 0;
    int end = 0;
    if (!xml_value::parse_int_strict(value.substr(0, separator), &begin) ||
        !xml_value::parse_int_strict(value.substr(separator + 2), &end) ||
        begin < 0 || end < begin) {
        return false;
    }
    *first_row = begin;
    *last_row = end;
    return true;
}

int parse_root()
{
    if (g_parse_state.saw_root) {
        log_error("FormationType contains duplicate root nodes", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("FormationType xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    int disabled = 0;
    if (xml_parser_has_attribute("disabled") &&
        !xml_value::parse_bool(xml_parser_get_attribute_string("disabled"), &disabled)) {
        log_error("FormationType has invalid Boolean attribute 'disabled'", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    int recruit_capacity = 0;
    if (xml_parser_has_attribute("recruit_capacity") &&
        !xml_definition::parse_required_positive_int_attribute("recruit_capacity", &recruit_capacity)) {
        log_error("FormationType has invalid positive recruit_capacity", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    if (disabled && recruit_capacity) {
        log_error("Disabled FormationType cannot declare recruit_capacity", key.c_str(), 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.key = std::move(key);
    g_parse_state.recruit_capacity = recruit_capacity;
    g_parse_state.disabled = disabled != 0;
    g_parse_state.saw_root = true;
    return 1;
}

int parse_grid()
{
    if (!parse_enabled_content("grid")) {
        return 0;
    }

    int width = 0;
    int height = 0;
    if (!xml_definition::parse_required_positive_int_attribute("width", &width) ||
        !xml_definition::parse_required_positive_int_attribute("height", &height)) {
        log_error("FormationType grid requires positive width and height", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    g_parse_state.width = width;
    g_parse_state.height = height;
    g_parse_state.saw_grid = true;
    return 1;
}

int parse_slot()
{
    if (!parse_enabled_content("slot") || !g_parse_state.saw_grid) {
        log_error("FormationType slot requires a preceding grid", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (!xml_parser_has_attribute("unit")) {
        log_error("FormationType slot is missing required unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const std::string unit_key = xml_value::trim_copy(xml_parser_get_attribute_string("unit"));
    if (unit_key.empty()) {
        log_error("FormationType slot unit is empty", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (xml_parser_has_attribute("fill") &&
        xml_value::equals(xml_parser_get_attribute_string("fill"), "all")) {
        for (int y = 0; y < g_parse_state.height; ++y) {
            for (int x = 0; x < g_parse_state.width; ++x) {
                g_parse_state.slots.push_back({x, y, unit_key});
            }
        }
        g_parse_state.saw_slot = true;
        return 1;
    }

    if (xml_parser_has_attribute("row") || xml_parser_has_attribute("rows")) {
        int first_row = 0;
        int last_row = 0;
        const char *row_text = xml_parser_has_attribute("rows") ?
            xml_parser_get_attribute_string("rows") :
            xml_parser_get_attribute_string("row");
        if (!parse_row_range(row_text, &first_row, &last_row) ||
            first_row < 0 || last_row < first_row || last_row >= g_parse_state.height) {
            log_error("FormationType slot row range is invalid", row_text, 0);
            g_parse_state.error = true;
            return 0;
        }
        for (int y = first_row; y <= last_row; ++y) {
            for (int x = 0; x < g_parse_state.width; ++x) {
                g_parse_state.slots.push_back({x, y, unit_key});
            }
        }
        g_parse_state.saw_slot = true;
        return 1;
    }

    int x = 0;
    int y = 0;
    if (!xml_definition::parse_required_nonnegative_int_attribute("x", &x) ||
        !xml_definition::parse_required_nonnegative_int_attribute("y", &y) ||
        x >= g_parse_state.width || y >= g_parse_state.height) {
        log_error("FormationType explicit slot requires valid x, y, and unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.slots.push_back({x, y, unit_key});
    g_parse_state.saw_slot = true;
    return 1;
}

const xml_parser_element XML_ELEMENTS[] = {
    { "formation", parse_root, nullptr, nullptr, nullptr },
    { "grid", parse_grid, nullptr, "formation", nullptr },
    { "slot", parse_slot, nullptr, "formation", nullptr }
};

bool validate_definition(const FormationType &definition, const char *filename)
{
    if (!definition.has_grid() || definition.capacity() <= 0) {
        log_error("FormationType is missing a valid grid", definition.key(), 0);
        error_context_report_error("FormationType is missing a valid grid.", filename);
        return false;
    }
    if (definition.recruit_capacity() <= 0 ||
        definition.recruit_capacity() > definition.capacity()) {
        log_error("FormationType recruit_capacity exceeds its formation capacity", definition.key(), 0);
        error_context_report_error("FormationType has invalid recruit_capacity.", filename);
        return false;
    }
    if (!definition.has_slots()) {
        log_error("FormationType requires at least one slot", definition.key(), 0);
        error_context_report_error("FormationType requires at least one slot.", filename);
        return false;
    }

    if (!definition.has_valid_slots()) {
        log_error("FormationType contains invalid slot", definition.key(), 0);
        error_context_report_error("FormationType contains invalid slot.", filename);
        return false;
    }
    if (definition.has_duplicate_slots()) {
        log_error("FormationType contains duplicate slot coordinates", definition.key(), 0);
        error_context_report_error("FormationType contains duplicate slot coordinates.", filename);
        return false;
    }
    return true;
}

int parse_definition_buffer(
    const char *filename,
    const std::vector<char> &buffer,
    StagedFormationType *out_definition)
{
    ErrorContextScope error_scope("formation_type_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_buffer(
        filename,
        "FormationType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])),
        buffer);
    if (!parsed || g_parse_state.error || !g_parse_state.saw_root || g_parse_state.key.empty() ||
        (!g_parse_state.disabled && (!g_parse_state.saw_grid || !g_parse_state.saw_slot))) {
        log_error("Unable to parse FormationType xml", filename, 0);
        return 0;
    }

    if (out_definition) {
        out_definition->key = std::move(g_parse_state.key);
        out_definition->width = g_parse_state.width;
        out_definition->height = g_parse_state.height;
        out_definition->recruit_capacity = g_parse_state.recruit_capacity;
        out_definition->slots = std::move(g_parse_state.slots);
        out_definition->disabled = g_parse_state.disabled;
    }
    return 1;
}

int parse_definition_file(const mod_definition::DefinitionSource &source, StagedFormationType *out_definition)
{
    std::vector<char> buffer;
    if (!xml_definition::load_file_to_buffer(source.full_path.c_str(), buffer, "FormationType")) {
        return 0;
    }
    return parse_definition_buffer(source.full_path.c_str(), buffer, out_definition);
}

int stage_definition(StagedFormationTypes &staged, StagedFormationType definition)
{
    if (definition.key.empty()) {
        staged.failure_reason = "FormationType key must not be empty.";
        return 0;
    }
    if (!staged.overlays.apply(definition.key, definition.disabled, definition.source)) {
        staged.failure_reason = staged.overlays.failure_reason();
        return 0;
    }
    staged.winners.insert_or_assign(definition.key, std::move(definition));
    return 1;
}

int resolve_and_validate_winners(StagedFormationTypes &staged)
{
    for (auto &entry : staged.winners) {
        StagedFormationType &winner = entry.second;
        if (winner.disabled) {
            continue;
        }

        winner.definition = std::make_unique<FormationType>(winner.key);
        winner.definition->set_grid(winner.width, winner.height);
        winner.definition->set_recruit_capacity(winner.recruit_capacity);
        for (const SlotSpec &slot : winner.slots) {
            if (!winner.definition->add_slot(slot.x, slot.y, slot.unit_key.c_str())) {
                staged.failure_reason = "FormationType '" + entry.first + "' from " + winner.source.full_path +
                    " references unsupported UnitType '" + slot.unit_key + "'.";
                return 0;
            }
        }
        if (!validate_definition(*winner.definition, winner.source.full_path.c_str())) {
            staged.failure_reason = "Invalid FormationType definition: " + winner.source.full_path;
            return 0;
        }
    }
    return 1;
}

} // namespace

const FormationType *find_formation_type(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    for (const std::unique_ptr<FormationType> &definition : g_formation_types) {
        if (definition && definition->matches_key(key)) {
            return definition.get();
        }
    }
    return nullptr;
}

const std::vector<std::unique_ptr<FormationType>> &formation_types()
{
    return g_formation_types;
}

} // namespace formation_type_registry_impl

FormationType::FormationType(std::string key)
    : key_(std::move(key))
{
}

const char *FormationType::key() const
{
    return key_.c_str();
}

bool FormationType::matches_key(const char *key) const
{
    return xml_value::equals(key_.c_str(), key);
}

void FormationType::set_grid(int width, int height)
{
    width_ = width;
    height_ = height;
}

void FormationType::set_recruit_capacity(int capacity)
{
    recruit_capacity_ = capacity;
}

int FormationType::capacity() const
{
    return width_ * height_;
}

int FormationType::recruit_capacity() const
{
    return recruit_capacity_ > 0 ? recruit_capacity_ : capacity();
}

FormationLayoutFootprint FormationType::layout_footprint() const
{
    return {width_, height_};
}

bool FormationType::has_grid() const
{
    return width_ > 0 && height_ > 0;
}

bool FormationType::has_slots() const
{
    return !slots_.empty();
}

bool FormationType::has_valid_slots() const
{
    for (const FormationTypeSlot &slot : slots_) {
        if (!contains(slot.x, slot.y) || !slot.unit) {
            return false;
        }
    }
    return true;
}

bool FormationType::has_duplicate_slots() const
{
    std::vector<bool> occupied(static_cast<size_t>(capacity()), false);
    for (const FormationTypeSlot &slot : slots_) {
        if (!contains(slot.x, slot.y)) {
            return false;
        }
        const size_t index = static_cast<size_t>(slot_index(slot.x, slot.y));
        if (occupied[index]) {
            return true;
        }
        occupied[index] = true;
    }
    return false;
}

bool FormationType::contains_row_range(int first_row, int last_row) const
{
    return first_row >= 0 && last_row >= first_row && last_row < height_;
}

bool FormationType::contains(int x, int y) const
{
    return x >= 0 && y >= 0 && x < width_ && y < height_;
}

int FormationType::slot_index(int x, int y) const
{
    return y * width_ + x;
}

bool FormationType::add_slot(int x, int y, const char *unit_key)
{
    if (!unit_key || !*unit_key || !contains(x, y)) {
        return false;
    }

    std::string normalized_unit_key = xml_value::trim_copy(unit_key);
    const UnitType *unit = unit_type_registry_impl::find_unit_type(normalized_unit_key.c_str());
    if (!unit) {
        log_error("FormationType references unknown UnitType", normalized_unit_key.c_str(), 0);
        return false;
    }

    slots_.push_back({ x, y, std::move(normalized_unit_key), unit });
    return true;
}

bool FormationType::fill_slots(const char *unit_key)
{
    if (!has_grid()) {
        return false;
    }
    for (int y = 0; y < height_; y++) {
        for (int x = 0; x < width_; x++) {
            if (!add_slot(x, y, unit_key)) {
                return false;
            }
        }
    }
    return true;
}

bool FormationType::add_slots_in_row_range(int first_row, int last_row, const char *unit_key)
{
    if (!contains_row_range(first_row, last_row)) {
        return false;
    }
    for (int y = first_row; y <= last_row; y++) {
        for (int x = 0; x < width_; x++) {
            if (!add_slot(x, y, unit_key)) {
                return false;
            }
        }
    }
    return true;
}

const UnitType *FormationType::primary_unit() const
{
    return slots_.empty() ? nullptr : slots_.front().unit;
}

figure_type FormationType::primary_figure_type() const
{
    const UnitType *unit = primary_unit();
    return unit ? unit->figure_type_id() : FIGURE_NONE;
}

int FormationType::primary_recruit_type() const
{
    const UnitType *unit = primary_unit();
    return unit ? unit->recruit_type() : LEGION_RECRUIT_NONE;
}

bool FormationType::primary_unit_requires_weapon() const
{
    const UnitType *unit = primary_unit();
    return unit && unit->requires_weapon();
}

const char *formation_type_registry_get_failure_reason(void)
{
    return formation_type_registry_impl::g_failure_reason.c_str();
}

const char *formation_type_definition_source_path(const char *key)
{
    if (!key || !*key) {
        return nullptr;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_type_registry_impl::g_formation_type_overlays.find(key);
    return entry ? entry->source.full_path.c_str() : nullptr;
}

int formation_type_definition_is_suppressed(const char *key)
{
    if (!key || !*key) {
        return 0;
    }
    const mod_definition::DefinitionOverlayEntry *entry =
        formation_type_registry_impl::g_formation_type_overlays.find(key);
    return entry && entry->disabled;
}

void formation_type_registry_reset(void)
{
    formation_type_registry_impl::g_formation_types.clear();
    formation_type_registry_impl::g_formation_type_overlays.clear();
    formation_type_registry_impl::g_failure_reason.clear();
}

int formation_type_registry_load(void)
{
    using namespace formation_type_registry_impl;

    StagedFormationTypes staged;
    std::string enumeration_failure;
    if (!mod_definition::for_each_configured_definition_file(
            {"FormationType"},
            "FormationType",
            true,
            [&](const mod_definition::DefinitionSource &source) {
                StagedFormationType definition;
                definition.source = source;
                if (!parse_definition_file(source, &definition)) {
                    staged.failure_reason = "Unable to parse FormationType xml: " + source.full_path;
                    return false;
                }
                return stage_definition(staged, std::move(definition)) != 0;
            },
            nullptr,
            &enumeration_failure)) {
        g_failure_reason = staged.failure_reason.empty() ? enumeration_failure : staged.failure_reason;
        return 0;
    }
    if (!resolve_and_validate_winners(staged)) {
        g_failure_reason = staged.failure_reason;
        return 0;
    }

    std::vector<std::unique_ptr<FormationType>> published;
    published.reserve(staged.overlays.active_count());
    for (auto &entry : staged.winners) {
        if (!entry.second.disabled) {
            published.push_back(std::move(entry.second.definition));
        }
    }
    g_formation_types = std::move(published);
    g_formation_type_overlays = std::move(staged.overlays);
    g_failure_reason.clear();
    return 1;
}

#ifdef STARTUP_PARSER_TEST
int formation_type_layered_definition_buffers_are_valid_for_test(
    const formation_type_layer_test_input *inputs,
    int input_count,
    const char *query_key,
    formation_type_layer_test_result *result)
{
    using namespace formation_type_registry_impl;
    if (!inputs || input_count < 0) {
        return 0;
    }

    StagedFormationTypes staged;
    for (int index = 0; index < input_count; ++index) {
        const formation_type_layer_test_input &input = inputs[index];
        const char *xml = input.xml ? input.xml : "";
        std::vector<char> buffer(xml, xml + std::strlen(xml));

        StagedFormationType definition;
        definition.source.layer_index = input.layer_index < 0 ? 0 : static_cast<std::size_t>(input.layer_index);
        definition.source.mod_name = input.mod_name ? input.mod_name : "";
        definition.source.full_path = input.source_path ? input.source_path : "FormationTypeLayerTest.xml";
        definition.source.file_name = definition.source.full_path;
        definition.source.category = "FormationType";
        if (!parse_definition_buffer(definition.source.full_path.c_str(), buffer, &definition) ||
            !stage_definition(staged, std::move(definition))) {
            return 0;
        }
    }
    if (!resolve_and_validate_winners(staged)) {
        return 0;
    }

    if (result) {
        *result = {};
        result->active_count = static_cast<int>(staged.overlays.active_count());
        result->suppressed_count = static_cast<int>(staged.overlays.suppressed_count());
        result->queried_source_layer = -1;
        const std::string key = query_key ? query_key : "";
        const mod_definition::DefinitionOverlayEntry *overlay = staged.overlays.find(key);
        const auto winner = staged.winners.find(key);
        if (overlay && winner != staged.winners.end()) {
            result->queried_disabled = overlay->disabled ? 1 : 0;
            result->queried_source_layer = static_cast<int>(overlay->source.layer_index);
            if (!winner->second.disabled && winner->second.definition) {
                result->queried_capacity = winner->second.definition->capacity();
                result->queried_recruit_capacity = winner->second.definition->recruit_capacity();
            }
        }
    }
    return 1;
}
#endif
