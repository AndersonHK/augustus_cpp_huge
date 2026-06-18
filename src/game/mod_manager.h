#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mod_manager {

void set_mod_name(std::string_view mod_name);
bool load_mod_list();

const std::string &failure_reason();
const std::string &mod_name();
const std::string &mod_path();
const std::string &graphics_path();
const std::string &augustus_graphics_path();
const std::string &julius_graphics_path();

const std::vector<std::string> &mod_names();
const std::vector<std::string> &mod_paths();
const std::vector<std::string> &graphics_paths();

bool validate_mod_path();
bool validate_graphics_path();

}

