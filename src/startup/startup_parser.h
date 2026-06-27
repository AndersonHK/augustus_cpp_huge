#pragma once

#include <string>
#include <vector>

namespace startup_parser {

struct StartupParseRequest {
    int load_localization = 1;
    int validate_mod_layout = 1;
    int prepare_graphics_validation = 1;
};

struct StartupDefinitions {
    int resource_definitions = 0;
    int graphics_validation_prepared = 0;
};

struct StartupEnvironment {
    std::string game_root;
    std::vector<std::string> mod_stack;
    std::string mod_path;
};

struct StartupParseStep {
    std::string label;
    int succeeded = 0;
    std::string detail;
};

struct StartupParseResult {
    int succeeded = 0;
    StartupDefinitions definitions;
    std::vector<StartupParseStep> steps;
    std::string failure_step;
    std::string failure_message;
};

StartupEnvironment inspect_startup_environment();
StartupParseResult parse_startup_definitions(const StartupParseRequest &request = StartupParseRequest());

} // namespace startup_parser
