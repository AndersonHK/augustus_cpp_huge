#ifndef STARTUP_DEFINITION_LOADER_H
#define STARTUP_DEFINITION_LOADER_H

#include <string>
#include <vector>

namespace startup_definition_loader {

struct Request {
    bool load_config = true;
    bool load_localization = true;
    bool validate_mod_layout = true;
    bool prepare_graphics_validation = true;
};

struct Step {
    std::string label;
    bool succeeded = false;
    std::string detail;
};

struct Result {
    bool succeeded = false;
    int resource_definitions = 0;
    bool graphics_validation_prepared = false;
    std::vector<Step> steps;
    std::string failure_step;
    std::string failure_message;
};

struct Environment {
    std::string game_root;
    std::vector<std::string> mod_stack;
    std::string mod_path;
};

Result load(const Request &request);
Environment inspect_environment();

} // namespace startup_definition_loader

#endif // STARTUP_DEFINITION_LOADER_H
