#include "housing_profile_registry_layering_test.h"

#include "building/housing_profile_registry.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_housing_profile_layers_" + std::to_string(suffix));
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

bool write_file(const std::filesystem::path &path, const std::string &contents)
{
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) {
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    output << contents;
    return output.good();
}

std::string profile_xml(
    const char *type,
    int level,
    const char *resident_class,
    int prosperity,
    int tax_multiplier)
{
    return
        "<housing_profile type=\"" + std::string(type) + "\" level=\"" +
        std::to_string(level) + "\">\n"
        "  <residents class=\"" + std::string(resident_class) + "\"/>\n"
        "  <evolution devolve_desirability=\"-10\" evolve_desirability=\"20\"/>\n"
        "  <requirements entertainment=\"1\" water=\"well\" religion=\"2\" education=\"3\""
        " barber=\"4\" bathhouse=\"5\" health=\"6\" food_types=\"1\""
        " pottery=\"7\" oil=\"8\" furniture=\"9\" wine=\"10\"/>\n"
        "  <prosperity value=\"" + std::to_string(prosperity) + "\"/>\n"
        "  <tax multiplier=\"" + std::to_string(tax_multiplier) + "\"/>\n"
        "</housing_profile>\n";
}

mod_definition::DefinitionLayer layer(const char *name, const std::filesystem::path &path)
{
    return {name ? name : "", path.string()};
}

} // namespace

bool validate_housing_profile_registry_layering_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create HousingProfile layering fixture directory.\n";
        return false;
    }

    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path upper = temporary.path() / "Upper";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code directory_error;
    std::filesystem::create_directories(missing_upper, directory_error);
    if (directory_error ||
        !write_file(
            lower / "HousingProfile" / "layered_home.xml",
            profile_xml("layered_home", 40, "patrician", 11, 2)) ||
        !write_file(
            lower / "HousingProfile" / "removed_home.xml",
            profile_xml("removed_home", 41, "plebeian", 12, 3)) ||
        !write_file(
            upper / "HousingProfile" / "layered_home.xml",
            profile_xml("layered_home", 42, "plebeian", 99, 7)) ||
        !write_file(
            upper / "HousingProfile" / "removed_home.xml",
            "<housing_profile type=\"removed_home\" disabled=\"true\"></housing_profile>\n")) {
        errors << "Unable to write HousingProfile layering fixtures.\n";
        return false;
    }

    const std::vector<mod_definition::DefinitionLayer> layers = {
        layer("Lower", lower),
        layer("Upper", upper),
        layer("MissingUpper", missing_upper),
    };
    std::string failure;
    if (!housing_profile_registry_load_layers(layers, &failure)) {
        errors << "HousingProfile sparse layering failed: " << failure << '\n';
        return false;
    }

    const HousingProfileDef *replacement = find_housing_profile_definition("layered_home");
    if (!replacement || replacement->compatibility_level != 42 ||
        replacement->resident_class_value != HousingResidentClass::Plebeian ||
        replacement->prosperity != 99 || replacement->tax_multiplier != 7 ||
        find_housing_profile_definition_for_compatibility_level(40) ||
        find_housing_profile_definition("removed_home") ||
        find_housing_profile_definition_for_compatibility_level(41) ||
        housing_profile_compatibility_level_count() != 1 ||
        housing_profile_compatibility_level_at(0) != 42) {
        errors << "HousingProfile replacement, tombstone, or missing-directory layering was not authoritative.\n";
        return false;
    }

    const std::filesystem::path duplicates = temporary.path() / "Duplicates";
    if (!write_file(
            duplicates / "HousingProfile" / "duplicate_a.xml",
            profile_xml("duplicate_a", 50, "plebeian", 1, 1)) ||
        !write_file(
            duplicates / "HousingProfile" / "duplicate_b.xml",
            profile_xml("duplicate_b", 50, "patrician", 2, 2))) {
        errors << "Unable to write HousingProfile duplicate-level fixtures.\n";
        return false;
    }
    failure.clear();
    if (housing_profile_registry_load_layers({layer("Duplicates", duplicates)}, &failure) ||
        failure.find("duplicate_a") == std::string::npos ||
        failure.find("duplicate_b") == std::string::npos ||
        find_housing_profile_definition("layered_home") != replacement) {
        errors << "HousingProfile same-layer duplicate rejection was not diagnostic or transactional.\n";
        return false;
    }

    const std::filesystem::path invalid_tombstone = temporary.path() / "InvalidTombstone";
    if (!write_file(
            invalid_tombstone / "HousingProfile" / "invalid.xml",
            "<housing_profile type=\"invalid\" disabled=\"true\">"
            "<prosperity value=\"1\"/>"
            "</housing_profile>\n")) {
        errors << "Unable to write HousingProfile invalid tombstone fixture.\n";
        return false;
    }
    failure.clear();
    if (housing_profile_registry_load_layers(
            {layer("InvalidTombstone", invalid_tombstone)}, &failure) || failure.empty()) {
        errors << "HousingProfile tombstone content was accepted.\n";
        return false;
    }

    if (!write_file(
            invalid_tombstone / "HousingProfile" / "invalid.xml",
            "<housing_profile type=\"invalid\" level=\"1\" disabled=\"true\"></housing_profile>\n")) {
        errors << "Unable to write HousingProfile tombstone attribute fixture.\n";
        return false;
    }
    failure.clear();
    if (housing_profile_registry_load_layers(
            {layer("InvalidTombstone", invalid_tombstone)}, &failure) || failure.empty()) {
        errors << "HousingProfile tombstone accepted redundant profile attributes.\n";
        return false;
    }

    return true;
}
