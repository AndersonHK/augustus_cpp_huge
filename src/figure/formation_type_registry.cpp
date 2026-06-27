#include "figure/formation_type.h"

#include "core/crash_context.h"
#include "core/log.h"
#include "core/xml_definition.h"
#include "core/xml_parser.h"
#include "core/xml_value.h"
#include "game/mod_manager.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace formation_type_registry_impl {

std::string g_failure_reason;
std::vector<std::unique_ptr<FormationType>> g_formation_types;

namespace {

struct ParseState {
    std::unique_ptr<FormationType> definition;
    bool saw_grid = false;
    bool saw_slot = false;
    bool error = false;
};

ParseState g_parse_state;

void set_failure_reason(const char *message, const char *detail = nullptr)
{
    g_failure_reason = xml_definition::format_failure_reason(message, detail);
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
    std::string key;
    if (!xml_definition::parse_required_nonempty_string_attribute("key", &key)) {
        log_error("FormationType xml requires a non-empty key", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    g_parse_state.definition = std::make_unique<FormationType>(key);
    return 1;
}

int parse_grid()
{
    if (!g_parse_state.definition) {
        g_parse_state.error = true;
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
    g_parse_state.definition->set_grid(width, height);
    g_parse_state.saw_grid = true;
    return 1;
}

int parse_slot()
{
    if (!g_parse_state.definition || !g_parse_state.definition->has_grid()) {
        log_error("FormationType slot requires a preceding grid", 0, 0);
        g_parse_state.error = true;
        return 0;
    }
    if (!xml_parser_has_attribute("unit")) {
        log_error("FormationType slot is missing required unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

    const char *unit_key = xml_parser_get_attribute_string("unit");
    if (xml_parser_has_attribute("fill") &&
        xml_value::equals(xml_parser_get_attribute_string("fill"), "all")) {
        if (!g_parse_state.definition->fill_slots(unit_key)) {
            g_parse_state.error = true;
            return 0;
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
            !g_parse_state.definition->contains_row_range(first_row, last_row)) {
            log_error("FormationType slot row range is invalid", row_text, 0);
            g_parse_state.error = true;
            return 0;
        }
        if (!g_parse_state.definition->add_slots_in_row_range(first_row, last_row, unit_key)) {
            g_parse_state.error = true;
            return 0;
        }
        g_parse_state.saw_slot = true;
        return 1;
    }

    int x = 0;
    int y = 0;
    if (!xml_definition::parse_required_nonnegative_int_attribute("x", &x) ||
        !xml_definition::parse_required_nonnegative_int_attribute("y", &y) ||
        !g_parse_state.definition->add_slot(x, y, unit_key)) {
        log_error("FormationType explicit slot requires valid x, y, and unit", 0, 0);
        g_parse_state.error = true;
        return 0;
    }

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

int parse_definition_file(const char *filename)
{
    ErrorContextScope error_scope("formation_type_registry.parse_definition", filename);

    g_parse_state = {};
    const int parsed = xml_definition::parse_file(
        filename,
        "FormationType",
        XML_ELEMENTS,
        static_cast<int>(sizeof(XML_ELEMENTS) / sizeof(XML_ELEMENTS[0])));
    if (!parsed) {
        log_error("Unable to parse FormationType xml", filename, 0);
        set_failure_reason("Unable to parse FormationType xml.", filename);
        return 0;
    }
    if (g_parse_state.error || !g_parse_state.definition ||
        !g_parse_state.saw_grid || !g_parse_state.saw_slot) {
        log_error("Unable to parse FormationType xml", filename, 0);
        set_failure_reason("Unable to parse FormationType xml.", filename);
        return 0;
    }
    if (!validate_definition(*g_parse_state.definition, filename)) {
        set_failure_reason("Invalid FormationType definition.", filename);
        return 0;
    }
    if (find_formation_type(g_parse_state.definition->key())) {
        log_error("Duplicate FormationType key", g_parse_state.definition->key(), 0);
        set_failure_reason("Duplicate FormationType key.", g_parse_state.definition->key());
        return 0;
    }

    g_formation_types.push_back(std::move(g_parse_state.definition));
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

int FormationType::capacity() const
{
    return width_ * height_;
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

void formation_type_registry_reset(void)
{
    formation_type_registry_impl::g_formation_types.clear();
    formation_type_registry_impl::g_failure_reason.clear();
}

int formation_type_registry_load(void)
{
    using namespace formation_type_registry_impl;

    formation_type_registry_reset();
    const std::string path = mod_manager::mod_path() + "FormationType/";
    bool found_any = false;
    if (!xml_definition::for_each_definition_file(
        path,
        "FormationType",
        true,
        [](const xml_definition::DefinitionFile &file, const std::string &) {
            if (parse_definition_file(file.full_path.c_str())) {
                return true;
            }
            log_error("Unable to parse FormationType xml", file.full_path.c_str(), 0);
            return false;
        },
        &found_any)) {
        if (!found_any) {
            set_failure_reason("No FormationType xml files found in", path.c_str());
        }
        return 0;
    }
    return 1;
}
