#pragma once

#include <string>
#include <string_view>

namespace xml_value {

bool equals(const char *value, std::string_view expected);
std::string trim_copy(std::string_view value);
int parse_bool(const char *value, int *out_value);
int parse_int_strict(std::string_view text, int *out_value);

} // namespace xml_value
