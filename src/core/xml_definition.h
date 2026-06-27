#pragma once

#include <functional>
#include <string>
#include <vector>

#include "core/xml_parser.h"

namespace xml_definition {

struct DefinitionFile {
    std::string name;
    std::string full_path;
};

using DefinitionFileVisitor = std::function<bool(const DefinitionFile &, const std::string &)>;

int starts_with_ignore_case_ascii(const std::string &value, const char *prefix);
int ends_with_ignore_case_ascii(const std::string &value, const char *suffix);
std::string normalize_path(const char *value);
std::string format_failure_reason(const char *message, const char *detail = nullptr);
bool directory_exists(const char *path);
std::vector<DefinitionFile> find_files_with_extension(const std::string &directory, const char *extension);
bool for_each_definition_file(
    const std::string &directory,
    const char *label,
    bool require_files,
    const DefinitionFileVisitor &visitor,
    bool *out_found_any = nullptr);
int load_file_to_buffer(const char *filename, std::vector<char> &buffer, const char *label);
bool parse_buffer(
    const char *filename,
    const char *label,
    const xml_parser_element *elements,
    int element_count,
    const std::vector<char> &buffer,
    bool stop_on_invalid_xml = true);
bool parse_file(
    const char *filename,
    const char *label,
    const xml_parser_element *elements,
    int element_count,
    bool stop_on_invalid_xml = true);
bool parse_required_nonnegative_int_attribute(const char *attribute, int *out_value);
bool parse_required_positive_int_attribute(const char *attribute, int *out_value);
bool parse_optional_nonnegative_int_attribute(const char *attribute, int *out_value);
bool parse_required_nonempty_string_attribute(const char *attribute, std::string *out_value);

} // namespace xml_definition
