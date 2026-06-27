#include "core/xml_definition.h"

#include "core/crash_context.h"
#include "core/xml_value.h"

#include "core/file.h"
#include "core/dir.h"
#include "core/log.h"
#include "core/xml_parser.h"
#include "platform/file_manager.h"

#include <cstdio>
#include <cstring>
#include <limits>
#include <utility>

namespace xml_definition {

static int equals_ignore_case_ascii(char left, char right)
{
    if (left >= 'A' && left <= 'Z') {
        left = static_cast<char>(left - 'A' + 'a');
    }
    if (right >= 'A' && right <= 'Z') {
        right = static_cast<char>(right - 'A' + 'a');
    }
    return left == right;
}

int starts_with_ignore_case_ascii(const std::string &value, const char *prefix)
{
    if (!prefix) {
        return 0;
    }

    const size_t prefix_length = std::strlen(prefix);
    if (value.size() < prefix_length) {
        return 0;
    }

    for (size_t i = 0; i < prefix_length; i++) {
        if (!equals_ignore_case_ascii(value[i], prefix[i])) {
            return 0;
        }
    }
    return 1;
}

int ends_with_ignore_case_ascii(const std::string &value, const char *suffix)
{
    if (!suffix) {
        return 0;
    }

    const size_t suffix_length = std::strlen(suffix);
    if (value.size() < suffix_length) {
        return 0;
    }

    const size_t start = value.size() - suffix_length;
    for (size_t i = 0; i < suffix_length; i++) {
        if (!equals_ignore_case_ascii(value[start + i], suffix[i])) {
            return 0;
        }
    }
    return 1;
}

std::string normalize_path(const char *value)
{
    std::string normalized = xml_value::trim_copy(value ? value : "");
    if (normalized.empty()) {
        return std::string();
    }

    for (char &ch : normalized) {
        if (ch == '/') {
            ch = '\\';
        }
    }

    std::string collapsed;
    collapsed.reserve(normalized.size());
    char previous = '\0';
    for (char ch : normalized) {
        if (ch == '\\' && previous == '\\') {
            continue;
        }
        collapsed.push_back(ch);
        previous = ch;
    }

    if (!collapsed.empty() && (collapsed.front() == '\\' || collapsed.back() == '\\')) {
        return std::string();
    }
    if (ends_with_ignore_case_ascii(collapsed, ".xml")) {
        collapsed.resize(collapsed.size() - 4);
    }
    return collapsed;
}

std::string format_failure_reason(const char *message, const char *detail)
{
    if (detail && *detail) {
        return std::string(message ? message : "") + "\n\n" + detail;
    }
    return message ? message : "";
}

static int stop_on_first_directory_entry([[maybe_unused]] const char *name, [[maybe_unused]] long unused)
{
    return LIST_MATCH;
}

bool directory_exists(const char *path)
{
    return path && platform_file_manager_list_directory_contents(
        path, TYPE_DIR | TYPE_FILE, 0, stop_on_first_directory_entry) != LIST_ERROR;
}

std::vector<DefinitionFile> find_files_with_extension(const std::string &directory, const char *extension)
{
    std::vector<DefinitionFile> result;
    const dir_listing *files = dir_find_files_with_extension(directory.c_str(), extension);
    if (!files || files->num_files <= 0) {
        return result;
    }

    result.reserve(static_cast<size_t>(files->num_files));
    for (int i = 0; i < files->num_files; i++) {
        DefinitionFile file;
        file.name = files->files[i].name;
        file.full_path = directory + file.name;
        result.push_back(std::move(file));
    }
    return result;
}

bool for_each_definition_file(
    const std::string &directory,
    const char *label,
    bool require_files,
    const DefinitionFileVisitor &visitor,
    bool *out_found_any)
{
    const std::vector<DefinitionFile> files = find_files_with_extension(directory, "xml");
    if (out_found_any) {
        *out_found_any = !files.empty();
    }
    if (files.empty()) {
        if (require_files) {
            const std::string message = std::string("No ") + (label ? label : "definition") + " xml files found in";
            log_error(message.c_str(), directory.c_str(), 0);
        }
        return !require_files;
    }

    for (const DefinitionFile &file : files) {
        const std::string normalized_path = normalize_path(file.name.c_str());
        if (normalized_path.empty()) {
            const std::string message = std::string("Unsupported ") + (label ? label : "definition") + " file name";
            log_error(message.c_str(), file.name.c_str(), 0);
            return false;
        }
        if (visitor && !visitor(file, normalized_path)) {
            return false;
        }
    }
    return true;
}

int load_file_to_buffer(const char *filename, std::vector<char> &buffer, const char *label)
{
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        log_error("Unable to open xml definition", label ? label : filename, 0);
        return 0;
    }

    if (std::fseek(fp, 0, SEEK_END) != 0) {
        file_close(fp);
        log_error("Unable to seek xml definition", label ? label : filename, 0);
        return 0;
    }

    long size = std::ftell(fp);
    if (size < 0) {
        file_close(fp);
        log_error("Unable to size xml definition", label ? label : filename, 0);
        return 0;
    }
    std::rewind(fp);

    buffer.resize(static_cast<size_t>(size));
    const size_t read = std::fread(buffer.data(), 1, buffer.size(), fp);
    file_close(fp);
    if (read != buffer.size()) {
        log_error("Unable to read xml definition", label ? label : filename, 0);
        return 0;
    }
    return 1;
}

bool parse_file(
    const char *filename,
    const char *label,
    const xml_parser_element *elements,
    int element_count,
    bool stop_on_invalid_xml)
{
    std::vector<char> buffer;
    if (!load_file_to_buffer(filename, buffer, label)) {
        const std::string message = std::string("Failed to load ") + (label ? label : "xml") + " definition.";
        error_context_report_error(message.c_str(), filename);
        return false;
    }

    return parse_buffer(filename, label, elements, element_count, buffer, stop_on_invalid_xml);
}

bool parse_buffer(
    const char *filename,
    const char *label,
    const xml_parser_element *elements,
    int element_count,
    const std::vector<char> &buffer,
    bool stop_on_invalid_xml)
{
    if (buffer.size() > std::numeric_limits<unsigned int>::max()) {
        const std::string message = std::string(label ? label : "xml") + " definition is too large to parse.";
        log_error(message.c_str(), filename, 0);
        error_context_report_error(message.c_str(), filename);
        return false;
    }

    if (!xml_parser_init(elements, element_count, stop_on_invalid_xml ? 1 : 0)) {
        const std::string message = std::string("Unable to initialize ") + (label ? label : "xml") + " xml parser.";
        log_error(message.c_str(), filename, 0);
        error_context_report_error(message.c_str(), filename);
        return false;
    }

    const bool parsed = xml_parser_parse(buffer.data(), static_cast<unsigned int>(buffer.size()), 1) != 0;
    xml_parser_free();
    return parsed;
}

static bool parse_required_int_attribute_at_least(const char *attribute, int minimum, int *out_value)
{
    if (!attribute || !out_value || !xml_parser_has_attribute(attribute)) {
        return false;
    }

    int value = 0;
    if (!xml_value::parse_int_strict(xml_parser_get_attribute_string(attribute), &value) || value < minimum) {
        return false;
    }
    *out_value = value;
    return true;
}

bool parse_required_nonnegative_int_attribute(const char *attribute, int *out_value)
{
    return parse_required_int_attribute_at_least(attribute, 0, out_value);
}

bool parse_required_positive_int_attribute(const char *attribute, int *out_value)
{
    return parse_required_int_attribute_at_least(attribute, 1, out_value);
}

bool parse_optional_nonnegative_int_attribute(const char *attribute, int *out_value)
{
    if (!attribute || !xml_parser_has_attribute(attribute)) {
        return true;
    }
    return parse_required_nonnegative_int_attribute(attribute, out_value);
}

bool parse_required_nonempty_string_attribute(const char *attribute, std::string *out_value)
{
    if (!attribute || !out_value || !xml_parser_has_attribute(attribute)) {
        return false;
    }

    *out_value = xml_value::trim_copy(xml_parser_get_attribute_string(attribute));
    return !out_value->empty();
}

} // namespace xml_definition
