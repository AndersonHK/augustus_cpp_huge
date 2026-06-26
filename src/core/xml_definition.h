#pragma once

#include <string>
#include <vector>

namespace xml_definition {

std::string normalize_path(const char *value);
int load_file_to_buffer(const char *filename, std::vector<char> &buffer, const char *label);

} // namespace xml_definition
