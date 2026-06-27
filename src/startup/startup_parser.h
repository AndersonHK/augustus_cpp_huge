#pragma once

#include <string>
#include <vector>

namespace startup_parser {

struct StartupParseRequest {
    int load_localization = 1;
    int validate_mod_layout = 1;
    int prepare_building_type_graphics = 1;
};

struct StartupDefinitions {
    int resource_definitions = 0;
    int building_type_graphics_prepared = 0;
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

StartupParseResult parse_startup_definitions(const StartupParseRequest &request = StartupParseRequest());

} // namespace startup_parser
