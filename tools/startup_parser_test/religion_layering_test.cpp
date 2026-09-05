#include "religion_layering_test.h"

#include "building/religion.h"
#include "building/religion_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *LOWER_XML =
    "<religion>"
    "<god path=\"ceres\"/>"
    "<tier value=\"small\"/>"
    "<capacity amount=\"100\"/>"
    "</religion>";
constexpr const char *UPPER_XML =
    "<religion>"
    "<god path=\"mars\"/>"
    "<tier value=\"large\"/>"
    "<capacity amount=\"300\"/>"
    "</religion>";
constexpr const char *TOMBSTONE_XML =
    "<religion disabled=\"true\"></religion>";

religion_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *source,
    const char *definition_path)
{
    return {xml, layer, mod, source, definition_path};
}

bool valid(
    const religion_layer_test_input *inputs,
    int count,
    const char *query,
    religion_layer_test_result *result = nullptr)
{
    return religion_layered_definition_buffers_are_valid_for_test(
               inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_religion_layers_" + std::to_string(suffix));
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

bool validate_religion_layering_contract(std::ostream &errors)
{
    using building_type_registry_impl::ReligionTier;

    const religion_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Religions/temple.xml", "temple"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/Religions/temple.xml", "temple"),
    };
    religion_layer_test_result result;
    if (!valid(replacement, 2, "temple", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 ||
        result.queried_tier != static_cast<int>(ReligionTier::Large) ||
        result.queried_capacity != 300 || result.queried_god_count != 1 || result.queried_all_gods) {
        errors << "Religion upper-layer winner was not a complete authoritative replacement.\n";
        return false;
    }

    const religion_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Religions/temple.xml", "temple"),
        input(TOMBSTONE_XML, 1, "Vespasian", "Vespasian/Religions/temple.xml", "temple"),
    };
    if (!valid(suppression, 2, "temple", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 ||
        result.queried_tier != static_cast<int>(ReligionTier::None) || result.queried_god_count != 0) {
        errors << "Religion path-identity tombstone did not suppress its inherited definition.\n";
        return false;
    }

    const religion_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Religions/temple.xml", "temple"),
        input(UPPER_XML, 0, "Julius", "Julius/Religions/temple-copy.xml", "temple"),
    };
    if (valid(duplicate, 2, "temple")) {
        errors << "Religion same-layer normalized-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *MALFORMED_TOMBSTONE_XML =
        "<religion disabled=\"true\"><capacity amount=\"1\"/></religion>";
    const religion_layer_test_input malformed_tombstone[] = {
        input(
            MALFORMED_TOMBSTONE_XML,
            0,
            "Julius",
            "Julius/Religions/temple.xml",
            "temple"),
    };
    if (valid(malformed_tombstone, 1, "temple")) {
        errors << "Religion tombstone accepted semantic definition content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_REFERENCE_XML =
        "<religion>"
        "<god path=\"missing_god\"/>"
        "<tier value=\"small\"/>"
        "<capacity amount=\"100\"/>"
        "</religion>";
    const religion_layer_test_input deferred_reference[] = {
        input(
            INVALID_LOWER_REFERENCE_XML,
            0,
            "Julius",
            "Julius/Religions/temple.xml",
            "temple"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/Religions/temple.xml", "temple"),
    };
    if (!valid(deferred_reference, 2, "temple", &result) ||
        result.queried_tier != static_cast<int>(ReligionTier::Large) ||
        result.queried_capacity != 300 || result.queried_god_count != 1) {
        errors << "Religion God references were resolved before final overlay winners existed.\n";
        return false;
    }

    const religion_layer_test_input invalid_final_reference[] = {
        input(
            INVALID_LOWER_REFERENCE_XML,
            0,
            "Julius",
            "Julius/Religions/temple.xml",
            "temple"),
    };
    if (valid(invalid_final_reference, 1, "temple")) {
        errors << "Religion final winner retained an unresolved God reference.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create Religion missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "Religions" / "temple.xml", LOWER_XML)) {
        errors << "Unable to write Religion missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!religion_layered_definition_files_are_valid_for_test(
            layers, "temple", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_tier != static_cast<int>(ReligionTier::Small) ||
        result.queried_capacity != 100 || result.queried_god_count != 1) {
        errors << "Religion missing upper directory did not preserve inheritance: " << failure << '\n';
        return false;
    }

    return true;
}
