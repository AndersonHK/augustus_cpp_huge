#include "startup/startup_parser.h"

#include "building/building_type_registry.h"
#include "building/building_type_startup_bridge.h"
#include "building/properties.h"
#include "core/config.h"
#include "figure/figure_type_registry.h"
#include "figure/formation_type.h"
#include "figure/unit_type.h"
#include "game/defines.h"
#include "game/mod_manager.h"
#include "game/resource.h"
#include "translation/translation.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <string>
#include <system_error>
#include <vector>

namespace startup_parser {

void install_graphics_validation_renderer();

namespace {

bool has_case_insensitive_extension(const std::filesystem::path &path, const char *extension)
{
    std::string actual = path.extension().string();
    std::string expected = extension ? extension : "";
    std::transform(actual.begin(), actual.end(), actual.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    std::transform(expected.begin(), expected.end(), expected.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return actual == expected;
}

int count_xml_files(const std::filesystem::path &directory, std::string &failure)
{
    std::error_code error;
    if (!std::filesystem::is_directory(directory, error)) {
        failure = "Missing XML directory: " + directory.string();
        return -1;
    }

    int count = 0;
    for (const std::filesystem::directory_entry &entry : std::filesystem::directory_iterator(directory, error)) {
        if (error) {
            failure = "Unable to enumerate XML directory: " + directory.string();
            return -1;
        }
        if (entry.is_regular_file(error) && has_case_insensitive_extension(entry.path(), ".xml")) {
            ++count;
        }
    }
    return count;
}

void append_step(StartupParseResult &result, const char *label, int succeeded, const std::string &detail = std::string())
{
    StartupParseStep step;
    step.label = label ? label : "";
    step.succeeded = succeeded;
    step.detail = detail;
    result.steps.push_back(step);
}

int fail_step(StartupParseResult &result, const char *label, const std::string &message)
{
    append_step(result, label, 0, message);
    result.failure_step = label ? label : "";
    result.failure_message = message;
    return 0;
}

int run_step(StartupParseResult &result, const char *label, int (*step)(), const char *failure_reason = nullptr)
{
    if (step()) {
        append_step(result, label, 1);
        return 1;
    }

    const std::string detail = failure_reason && *failure_reason ? failure_reason : "";
    return fail_step(result, label, detail);
}

int load_resources(StartupParseResult &result)
{
    const std::filesystem::path resource_path = std::filesystem::path(mod_manager::mod_path()) / "Resources";
    std::string failure;
    const int expected_resources = count_xml_files(resource_path, failure);
    if (expected_resources <= 0) {
        if (failure.empty()) {
            failure = "No resource XML files found in: " + resource_path.string();
        }
        return fail_step(result, "Resources", failure);
    }

    resource_init();
    const int loaded_resources = resource_loaded_count();
    if (loaded_resources != expected_resources) {
        std::ostringstream detail;
        detail << "Loaded " << loaded_resources << " resources, expected "
            << expected_resources << " XML files from " << resource_path.string() << ".";
        return fail_step(result, "Resources", detail.str());
    }

    result.definitions.resource_definitions = loaded_resources;
    std::ostringstream detail;
    detail << loaded_resources << " definitions";
    append_step(result, "Resources", 1, detail.str());
    return 1;
}

int prepare_graphics_validation(StartupParseResult &result)
{
    install_graphics_validation_renderer();
    result.definitions.graphics_validation_prepared = 1;
    append_step(
        result,
        "graphics validation prerequisites",
        1,
        "headless renderer installed; generated graphics must already exist");
    return 1;
}

} // namespace

StartupEnvironment inspect_startup_environment()
{
    StartupEnvironment environment;
    environment.game_root = std::filesystem::current_path().string();
    environment.mod_stack = mod_manager::mod_names();
    environment.mod_path = mod_manager::mod_path();
    return environment;
}

StartupParseResult parse_startup_definitions(const StartupParseRequest &request)
{
    StartupParseResult result;

    config_load();

    if (request.validate_mod_layout && !building_type_startup_bridge_validate_mod()) {
        fail_step(
            result,
            "mod data",
            std::string("Selected mod data is missing. Expected BuildingType folder: ") +
                building_type_startup_bridge_get_building_type_path());
        return result;
    }

    if (request.load_localization && !run_step(result, "localization", []() { return lang_load(0); })) {
        return result;
    }

    model_reset();
    if (!load_resources(result)) {
        return result;
    }
    if (!run_step(result, "game defines", game_defines_load, game_defines_get_failure_reason())) {
        return result;
    }
    if (request.prepare_graphics_validation && !prepare_graphics_validation(result)) {
        return result;
    }

    building_properties_init();
    figure_type_registry_reset();
    unit_type_registry_reset();
    formation_type_registry_reset();

    if (!run_step(result, "FigureType definitions", figure_type_registry_load, figure_type_registry_get_failure_reason())) {
        return result;
    }
    if (!run_step(result, "UnitType definitions", unit_type_registry_load, unit_type_registry_get_failure_reason())) {
        return result;
    }
    if (!run_step(result, "FormationType definitions", formation_type_registry_load, formation_type_registry_get_failure_reason())) {
        return result;
    }
    if (!run_step(result, "BuildingType definitions", building_type_registry_load)) {
        return result;
    }
    if (!run_step(result, "FigureType building references", figure_type_registry_resolve_building_references,
        figure_type_registry_get_failure_reason())) {
        return result;
    }

    result.succeeded = 1;
    return result;
}

} // namespace startup_parser
