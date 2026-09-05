#include "distribution_layering_test.h"

#include "building/distribution.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *LOWER_XML =
    "<distribution><takes resource=\"wheat\" priority=\"1\"/></distribution>";
constexpr const char *UPPER_XML =
    "<distribution><takes resource=\"wheat\" priority=\"9\"/>"
    "<takes resource=\"clay\" priority=\"2\"/></distribution>";
constexpr const char *TOMBSTONE_XML =
    "<distribution disabled=\"true\"></distribution>";

distribution_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *path,
    const char *source)
{
    return {xml, layer, mod, path, source};
}

bool valid(
    const distribution_layer_test_input *inputs,
    int count,
    const char *query,
    distribution_layer_test_result *result = nullptr)
{
    return distribution_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_distribution_layers_" + std::to_string(suffix));
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

bool validate_distribution_layering_contract(std::ostream &errors)
{
    const distribution_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "market", "Julius/Distribution/market.xml"),
        input(UPPER_XML, 1, "Vespasian", "market", "Vespasian/Distribution/market.xml"),
    };
    distribution_layer_test_result result;
    if (!valid(replacement, 2, "market", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_resource_count != 2 ||
        result.queried_wheat_priority != 9) {
        errors << "Distribution upper-layer winner was not an atomic whole-definition replacement.\n";
        return false;
    }

    const distribution_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "market", "Julius/Distribution/market.xml"),
        input(TOMBSTONE_XML, 1, "Vespasian", "market", "Vespasian/Distribution/market.xml"),
    };
    if (!valid(suppression, 2, "market", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_resource_count != 0) {
        errors << "Distribution tombstone did not suppress its inherited definition with provenance.\n";
        return false;
    }

    const distribution_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "market", "Julius/Distribution/market.xml"),
        input(UPPER_XML, 0, "Julius", "market", "Julius/Distribution/market-copy.xml"),
    };
    if (valid(duplicate, 2, "market")) {
        errors << "Distribution same-layer stable-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_TOMBSTONE_XML =
        "<distribution disabled=\"true\"><takes resource=\"wheat\"/></distribution>";
    const distribution_layer_test_input invalid_tombstone[] = {
        input(INVALID_TOMBSTONE_XML, 0, "Vespasian", "market", "Vespasian/Distribution/market.xml"),
    };
    if (valid(invalid_tombstone, 1, "market")) {
        errors << "Distribution tombstone accepted authored definition content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_REFERENCE_XML =
        "<distribution><takes storage=\"not_a_declared_storage_type\"/></distribution>";
    const distribution_layer_test_input deferred_reference[] = {
        input(INVALID_LOWER_REFERENCE_XML, 0, "Julius", "market", "Julius/Distribution/market.xml"),
        input(UPPER_XML, 1, "Vespasian", "market", "Vespasian/Distribution/market.xml"),
    };
    if (!valid(deferred_reference, 2, "market", &result) || result.queried_wheat_priority != 9) {
        errors << "Distribution references were resolved before overlay winners existed.\n";
        return false;
    }

    const distribution_layer_test_input unresolved_winner[] = {
        input(INVALID_LOWER_REFERENCE_XML, 0, "Vespasian", "market", "Vespasian/Distribution/market.xml"),
    };
    if (valid(unresolved_winner, 1, "market")) {
        errors << "Distribution final winner accepted an unresolved storage reference.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create Distribution missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "Distribution" / "market.xml", LOWER_XML)) {
        errors << "Unable to write Distribution missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!distribution_layered_definition_files_are_valid_for_test(
            layers, "market", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_wheat_priority != 1) {
        errors << "Distribution missing upper directory was not an empty overlay: " << failure << '\n';
        return false;
    }

    return true;
}
