#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace mod_manager {

struct ModMetadata {
    std::string name;
    std::string description;
    std::string version;
    std::vector<std::string> dependencies;
};

// An empty name uses the full saved mod list, with its last entry selected.
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
const std::vector<ModMetadata> &metadata();
const ModMetadata *selected_metadata();

bool validate_mod_path();
bool validate_graphics_path();

#ifdef STARTUP_PARSER_TEST
bool select_mod_stack_for_test(std::vector<std::string> &mods, std::string_view selected_mod);
bool parse_metadata_source_for_test(const char *source, ModMetadata &metadata_out);
bool metadata_stack_is_valid_for_test(
    const std::vector<std::string> &mod_names,
    const std::vector<ModMetadata> &metadata,
    std::string *failure_reason = nullptr);
#endif

}

