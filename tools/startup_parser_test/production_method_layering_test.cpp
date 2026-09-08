#include "production_method_layering_test.h"

#include "building/production_method_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *LOWER_XML =
    "<production_method>"
    "<kind value=\"workshop\"/>"
    "<output resource=\"wheat\" production_per_month=\"20\"/>"
    "<batch_size value=\"1\"/>"
    "</production_method>";
constexpr const char *UPPER_XML =
    "<production_method>"
    "<kind value=\"workshop\"/>"
    "<output resource=\"wheat\" production_per_month=\"90\"/>"
    "<batch_size value=\"2\"/>"
    "<input resource=\"clay\" amount=\"5\"/>"
    "</production_method>";
constexpr const char *TOMBSTONE_XML =
    "<production_method disabled=\"true\"></production_method>";

production_method_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *source,
    const char *definition_path)
{
    return {xml, layer, mod, source, definition_path};
}

bool valid(
    const production_method_layer_test_input *inputs,
    int count,
    const char *query,
    production_method_layer_test_result *result = nullptr)
{
    return production_method_layered_definition_buffers_are_valid_for_test(
               inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_production_method_layers_" + std::to_string(suffix));
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

bool validate_production_method_layering_contract(std::ostream &errors)
{
    for (const char *percent : {"0", "50", "100", "200", "2147483647"}) {
        const std::string xml = std::string("<production_method><kind value=\"delay_factor\"/><output resource=\"wheat\" delay_percent=\"") + percent + "\"/></production_method>";
        const auto fixture = input(xml.c_str(), 0, "Julius", "Julius/ProductionMethod/delay.xml", "delay");
        if (!valid(&fixture, 1, "delay")) { errors << "Valid delay percentage rejected: " << percent << '\n'; return false; }
    }
    for (const char *output : {"delay_percent=\"-1\"", "delay_percent=\"invalid\"", "delay_percent=\"2147483648\"", "production_per_month=\"100\"", "rate_from=\"farm\"", "delay_percent=\"100\" production_per_month=\"100\""}) {
        const std::string xml = std::string("<production_method><kind value=\"delay_factor\"/><output resource=\"wheat\" ") + output + "/></production_method>";
        const auto fixture = input(xml.c_str(), 0, "Julius", "Julius/ProductionMethod/delay.xml", "delay");
        if (valid(&fixture, 1, "delay")) { errors << "Invalid delay rate accepted: " << output << '\n'; return false; }
    }
    constexpr const char *LINK_XML = "<production_method><kind value=\"workshop\"/><output resource=\"clay\" rate_from=\"farm\"/></production_method>";
    const production_method_layer_test_input linked[] = {
        input(LOWER_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(LINK_XML, 0, "Julius", "Julius/ProductionMethod/reverse.xml", "reverse"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/ProductionMethod/farm.xml", "farm"),
    };
    production_method_layer_test_result linked_result;
    if (!valid(linked, 3, "reverse", &linked_result) || linked_result.queried_production_per_month != 90) {
        errors << "ProductionMethod rate reference did not bind to the winning source definition.\n";
        return false;
    }
    if (valid(linked + 1, 1, "reverse")) {
        errors << "ProductionMethod accepted a missing rate source.\n";
        return false;
    }
    const production_method_layer_test_input self_cycle[] = {
        input(LINK_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
    };
    constexpr const char *CYCLE_XML = "<production_method><kind value=\"workshop\"/><output resource=\"wheat\" rate_from=\"reverse\"/></production_method>";
    const production_method_layer_test_input cycle[] = {
        input(CYCLE_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(LINK_XML, 0, "Julius", "Julius/ProductionMethod/reverse.xml", "reverse"),
    };
    if (valid(self_cycle, 1, "farm") || valid(cycle, 2, "farm")) {
        errors << "ProductionMethod accepted a cyclic rate dependency.\n";
        return false;
    }
    constexpr const char *AMBIGUOUS_RATE_XML = "<production_method><kind value=\"workshop\"/><output resource=\"clay\" production_per_month=\"20\" rate_from=\"farm\"/></production_method>";
    const production_method_layer_test_input ambiguous[] = {
        input(LOWER_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(AMBIGUOUS_RATE_XML, 0, "Julius", "Julius/ProductionMethod/reverse.xml", "reverse"),
    };
    if (valid(ambiguous, 2, "reverse")) {
        errors << "ProductionMethod accepted simultaneous literal and referenced rates.\n";
        return false;
    }
    const production_method_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/ProductionMethod/farm.xml", "farm"),
    };
    production_method_layer_test_result result;
    if (!valid(replacement, 2, "farm", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_production_per_month != 90 ||
        result.queried_input_count != 1) {
        errors << "ProductionMethod upper-layer winner was not an atomic whole-definition replacement.\n";
        return false;
    }

    const production_method_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(TOMBSTONE_XML, 1, "Vespasian", "Vespasian/ProductionMethod/farm.xml", "farm"),
    };
    if (!valid(suppression, 2, "farm", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_production_per_month != 0) {
        errors << "ProductionMethod tombstone did not suppress the inherited definition with provenance.\n";
        return false;
    }

    const production_method_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
        input(UPPER_XML, 0, "Julius", "Julius/ProductionMethod/farm-copy.xml", "farm"),
    };
    if (valid(duplicate, 2, "farm")) {
        errors << "ProductionMethod same-layer stable-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *INVALID_TOMBSTONE_XML =
        "<production_method disabled=\"true\"><kind value=\"workshop\"/></production_method>";
    const production_method_layer_test_input invalid_tombstone[] = {
        input(INVALID_TOMBSTONE_XML, 0, "Julius", "Julius/ProductionMethod/farm.xml", "farm"),
    };
    if (valid(invalid_tombstone, 1, "farm")) {
        errors << "ProductionMethod tombstone accepted authored definition content.\n";
        return false;
    }

    constexpr const char *INVALID_LOWER_REFERENCE_XML =
        "<production_method>"
        "<kind value=\"workshop\"/>"
        "<output resource=\"not_a_declared_resource\" production_per_month=\"20\"/>"
        "</production_method>";
    const production_method_layer_test_input deferred_reference[] = {
        input(
            INVALID_LOWER_REFERENCE_XML,
            0,
            "Julius",
            "Julius/ProductionMethod/farm.xml",
            "farm"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/ProductionMethod/farm.xml", "farm"),
    };
    if (!valid(deferred_reference, 2, "farm", &result) ||
        result.queried_production_per_month != 90) {
        errors << "ProductionMethod resource references were resolved before overlay winners existed.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create ProductionMethod missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "ProductionMethod" / "farm.xml", LOWER_XML)) {
        errors << "Unable to write ProductionMethod missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!production_method_layered_definition_files_are_valid_for_test(
            layers, "farm", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_production_per_month != 20) {
        errors << "ProductionMethod missing upper directory was not optional: " << failure << '\n';
        return false;
    }

    return true;
}
