#include "god_layering_test.h"

#include "building/god_registry.h"
#include "city/god.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

constexpr const char *LOWER_XML =
    "<god legacy=\"neptune\"><blessing type=\"neptune_trade_bonus\" months=\"4\"/></god>";
constexpr const char *UPPER_XML =
    "<god legacy=\"neptune\"><blessing type=\"neptune_trade_bonus\" months=\"12\"/></god>";
constexpr const char *TOMBSTONE_XML =
    "<god legacy=\"neptune\" disabled=\"true\"></god>";

god_layer_test_input input(
    const char *xml,
    int layer,
    const char *mod,
    const char *source,
    const char *definition_path)
{
    return {xml, layer, mod, source, definition_path};
}

bool valid(
    const god_layer_test_input *inputs,
    int count,
    const char *query,
    god_layer_test_result *result = nullptr)
{
    return god_layered_definition_buffers_are_valid_for_test(inputs, count, query, result) != 0;
}

class TemporaryDirectory {
public:
    TemporaryDirectory()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() /
            ("vespasian_god_layers_" + std::to_string(suffix));
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

bool validate_god_layering_contract(std::ostream &errors)
{
    const god_layer_test_input replacement[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
        input(UPPER_XML, 1, "Vespasian", "Vespasian/Gods/neptune.xml", "neptune"),
    };
    god_layer_test_result result;
    if (!valid(replacement, 2, "neptune", &result) ||
        result.active_count != 1 || result.suppressed_count != 0 || result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_legacy_type != GOD_NEPTUNE ||
        result.queried_runtime_id != 0 || result.queried_neptune_blessing_months != 12 ||
        result.runtime_count != 1) {
        errors << "God upper-layer winner was not a complete authoritative replacement.\n";
        return false;
    }

    const god_layer_test_input suppression[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
        input(TOMBSTONE_XML, 1, "Vespasian", "Vespasian/Gods/neptune.xml", "neptune"),
    };
    if (!valid(suppression, 2, "neptune", &result) ||
        result.active_count != 0 || result.suppressed_count != 1 || !result.queried_disabled ||
        result.queried_source_layer != 1 || result.queried_legacy_type != GOD_NEPTUNE ||
        result.queried_runtime_id != -1 || result.runtime_count != 0) {
        errors << "God identity-only tombstone did not suppress its inherited definition.\n";
        return false;
    }

    const god_layer_test_input duplicate[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
        input(UPPER_XML, 0, "Julius", "Julius/Gods/neptune-copy.xml", "neptune"),
    };
    if (valid(duplicate, 2, "neptune")) {
        errors << "God same-layer normalized-path duplicate was accepted.\n";
        return false;
    }

    constexpr const char *MALFORMED_TOMBSTONE_XML =
        "<god legacy=\"neptune\" disabled=\"true\">"
        "<blessing type=\"neptune_trade_bonus\" months=\"1\"/>"
        "</god>";
    const god_layer_test_input malformed_tombstone[] = {
        input(MALFORMED_TOMBSTONE_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
    };
    if (valid(malformed_tombstone, 1, "neptune")) {
        errors << "God tombstone accepted semantic definition content.\n";
        return false;
    }

    constexpr const char *MOVED_LEGACY_XML =
        "<god legacy=\"venus\"></god>";
    const god_layer_test_input moved_identity[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
        input(MOVED_LEGACY_XML, 1, "Vespasian", "Vespasian/Gods/neptune.xml", "neptune"),
    };
    if (valid(moved_identity, 2, "neptune")) {
        errors << "God replacement changed its stable legacy identity.\n";
        return false;
    }

    constexpr const char *ALIAS_XML =
        "<god legacy=\"neptune\"></god>";
    constexpr const char *ALIAS_TOMBSTONE_XML =
        "<god legacy=\"neptune\" disabled=\"true\"></god>";
    const god_layer_test_input final_winner_validation[] = {
        input(LOWER_XML, 0, "Julius", "Julius/Gods/neptune.xml", "neptune"),
        input(ALIAS_XML, 0, "Julius", "Julius/Gods/sea.xml", "sea"),
        input(ALIAS_TOMBSTONE_XML, 1, "Vespasian", "Vespasian/Gods/sea.xml", "sea"),
    };
    if (!valid(final_winner_validation, 3, "neptune", &result) ||
        result.active_count != 1 || result.suppressed_count != 1 || result.runtime_count != 1) {
        errors << "God legacy-identity validation ran before final winners existed.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create God missing-directory fixture.\n";
        return false;
    }
    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path missing_upper = temporary.path() / "MissingUpper";
    std::error_code error;
    std::filesystem::create_directories(missing_upper, error);
    if (error || !write_file(lower / "Gods" / "neptune.xml", LOWER_XML)) {
        errors << "Unable to write God missing-directory fixture.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Lower", lower.string()},
        {"MissingUpper", missing_upper.string()},
    };
    std::string failure;
    if (!god_layered_definition_files_are_valid_for_test(
            layers, "neptune", &result, &failure) ||
        result.active_count != 1 || result.queried_source_layer != 0 ||
        result.queried_neptune_blessing_months != 4 || result.runtime_count != 1) {
        errors << "God missing upper directory did not preserve inheritance: " << failure << '\n';
        return false;
    }

    return true;
}
