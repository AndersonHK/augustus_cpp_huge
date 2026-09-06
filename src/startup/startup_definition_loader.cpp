#include "startup/startup_definition_loader.h"

#include "building/building_type_registry.h"
#include "building/building_type_startup_bridge.h"
#include "building/properties.h"
#include "core/config.h"
#include "figure/figure_type_registry.h"
#include "figure/formation_layout.h"
#include "figure/formation_type.h"
#include "figure/unit_type.h"
#include "game/defines.h"
#include "game/mod_manager.h"
#include "game/mod_content.h"
#include "core/dir.h"
#include "game/resource.h"
#include "translation/translation.h"

#include <filesystem>
#include <sstream>

namespace startup_parser {
void install_graphics_validation_renderer();
}

namespace startup_definition_loader {
namespace {

void append_step(Result &result, const char *label, bool succeeded, const std::string &detail = {})
{
    result.steps.push_back({label ? label : "", succeeded, detail});
}

bool fail_step(Result &result, const char *label, const std::string &message)
{
    append_step(result, label, false, message);
    result.failure_step = label ? label : "";
    result.failure_message = message;
    return false;
}

bool run_step(Result &result, const char *label, int (*step)(), const char *(*failure_reason)() = nullptr)
{
    if (step()) {
        append_step(result, label, true);
        return true;
    }
    const char *reason = failure_reason ? failure_reason() : nullptr;
    return fail_step(result, label, reason && *reason ? reason : "");
}

bool load_resources(Result &result)
{
    if (!resource_init()) {
        return fail_step(result, "Resources", resource_get_failure_reason());
    }
    result.resource_definitions = resource_loaded_count();
    if (result.resource_definitions <= 0) {
        return fail_step(result, "Resources", "No enabled Resource definitions were loaded from the configured mod stack.");
    }
    std::ostringstream detail;
    detail << result.resource_definitions << " definitions";
    append_step(result, "Resources", true, detail.str());
    return true;
}

void prepare_graphics_validation(Result &result)
{
    startup_parser::install_graphics_validation_renderer();
    result.graphics_validation_prepared = true;
    append_step(
        result,
        "graphics validation prerequisites",
        true,
        "headless renderer installed; generated graphics must already exist");
}

} // namespace

Environment inspect_environment()
{
    return {
        std::filesystem::current_path().string(),
        mod_manager::mod_names(),
        mod_manager::mod_path()};
}

Result load(const Request &request)
{
    Result result;
    if (request.load_config) {
        config_load();
        try {
            std::vector<mod_content::Layer> layers;
            const auto &names = mod_manager::mod_names();
            const auto &paths = mod_manager::mod_paths();
            for (size_t i = 0; i < paths.size(); ++i) layers.push_back({names.at(i), mod_content::utf8_path(paths[i])});
            mod_content::Session compiled;
            compiled.load(layers, mod_content::utf8_path(dir_append_location("mod-settings.xml", PATH_LOCATION_CONFIG)));
            mod_content::runtime() = std::move(compiled);
        } catch (const std::exception &error) {
            fail_step(result, "mod settings and fields", error.what());
            return result;
        }
    }
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
    if (!load_resources(result) ||
        !run_step(result, "game defines", game_defines_load, game_defines_get_failure_reason)) {
        return result;
    }
    if (request.prepare_graphics_validation) {
        prepare_graphics_validation(result);
    }

    building_properties_init();
    figure_type_registry_reset();
    unit_type_registry_reset();
    formation_layout_registry_reset();
    formation_type_registry_reset();

    if (!run_step(result, "FigureType definitions", figure_type_registry_load, figure_type_registry_get_failure_reason) ||
        !run_step(result, "UnitType definitions", unit_type_registry_load, unit_type_registry_get_failure_reason) ||
        !run_step(result, "FormationLayout definitions", formation_layout_registry_load,
            formation_layout_registry_get_failure_reason) ||
        !run_step(result, "FormationType definitions", formation_type_registry_load,
            formation_type_registry_get_failure_reason) ||
        !run_step(result, "BuildingType definitions", building_type_registry_load,
            building_type_registry_get_failure_reason) ||
        !run_step(result, "FigureType building references", figure_type_registry_resolve_building_references,
            figure_type_registry_get_failure_reason)) {
        return result;
    }

    result.succeeded = true;
    return result;
}

} // namespace startup_definition_loader
