#include "culture_module_layering_test.h"

#include "building/culture_module_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *THEATER_XML =
    "<culture_module><type value=\"theater\"/></culture_module>";
constexpr const char *SCHOOL_XML =
    "<culture_module><type value=\"school\"/></culture_module>";
constexpr const char *TOMBSTONE_XML =
    "<culture_module disabled=\"true\"></culture_module>";

culture_module_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *source,
    const char *definition_path)
{
    return {xml, layer, mod, source, definition_path};
}

bool valid(
    const culture_module_layer_test_input *inputs,
    int count,
    const char *query,
    culture_module_layer_test_result *result = nullptr)
{
    return culture_module_layered_definition_buffers_are_valid_for_test(
               inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_culture_module_layers_" + std::to_string(suffix));
        std::error_code error;
        std::filesystem::create_directories(path_, error);
        valid_ = !error;
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path &path() const { return path_; }
    bool valid() const { return valid_; }

private:
    std::filesystem::path path_;
    bool valid_ = false;
};

bool write_file(const std::filesystem::path &path, const char *contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    output << (contents ? contents : "");
    return output.good();
}

} // namespace

bool validate_culture_module_layering_contract(std::ostream &errors)
{
    using building_type_registry_impl::CultureModuleType;

    const culture_module_layer_test_input replacement[] = {
        input(THEATER_XML, 0, "Julius", "Julius/CultureModule/service.xml", "service"),
        input(SCHOOL_XML, 1, "Vespasian", "Vespasian/CultureModule/service.xml", "service"),
    };
    culture_module_layer_test_result result;
    if (!valid(replacement, 2, "service", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 ||
        result.queried_type != static_cast<int>(CultureModuleType::School)) {
        errors << "CultureModule upper-layer winner was not a whole-definition replacement.\n";
        return false;
    }

    const culture_module_layer_test_input suppression[] = {
        input(THEATER_XML, 0, "Julius", "Julius/CultureModule/service.xml", "service"),
        input(TOMBSTONE_XML, 1, "Vespasian", "Vespasian/CultureModule/service.xml", "service"),
    };
    if (!valid(suppression, 2, "service", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 ||
        result.queried_type != static_cast<int>(CultureModuleType::None)) {
        errors << "CultureModule path-identity tombstone did not suppress its inherited definition.\n";
        return false;
    }

    const culture_module_layer_test_input duplicate[] = {
        input(THEATER_XML, 0, "Julius", "Julius/CultureModule/service.xml", "service"),
        input(SCHOOL_XML, 0, "Julius", "Julius/CultureModule/service-copy.xml", "service"),
    };
    if (valid(duplicate, 2, "service")) {
        errors << "CultureModule same-layer normalized-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *MALFORMED_TOMBSTONE_XML =
        "<culture_module disabled=\"true\"><type value=\"school\"/></culture_module>";
    const culture_module_layer_test_input malformed_tombstone[] = {
        input(
            MALFORMED_TOMBSTONE_XML,
            0,
            "Julius",
            "Julius/CultureModule/service.xml",
            "service"),
    };
    if (valid(malformed_tombstone, 1, "service")) {
        errors << "CultureModule tombstone accepted semantic definition content.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create CultureModule missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "CultureModule" / "service.xml", THEATER_XML)) {
        errors << "Unable to write CultureModule missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!culture_module_layered_definition_files_are_valid_for_test(
            layers, "service", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_type != static_cast<int>(CultureModuleType::Theater)) {
        errors << "CultureModule missing upper directory did not preserve inheritance: " << failure << '\n';
        return false;
    }

    return true;
}
