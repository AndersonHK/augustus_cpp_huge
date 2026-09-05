#include "building_type_registry_layering_test.h"

#include "building/building_type_registry.h"
#include "building/building_type_registry_internal.h"

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
            ("vespasian_building_type_layers_" + std::to_string(suffix));
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

std::string model_xml(const char *type, int cost)
{
    return "<building type=\"" + std::string(type) + "\"><model cost=\"" +
        std::to_string(cost) + "\"/></building>\n";
}

mod_definition::DefinitionLayer layer(const char *name, const std::filesystem::path &path)
{
    return {name ? name : "", path.string()};
}

const building_type_registry_impl::BuildingType *live_road_definition()
{
    using namespace building_type_registry_impl;
    return definition_for_type(type_from_attr("road"));
}

} // namespace

bool validate_building_type_registry_layering_contract(std::ostream &errors)
{
    using namespace building_type_registry_impl;

    const BuildingType *road_before = live_road_definition();
    const mod_definition::DefinitionOverlayEntry *road_overlay =
        find_building_type_definition_overlay("road");
    if (!road_before || !road_overlay || road_overlay->disabled ||
        road_overlay->stable_id != "road" || road_overlay->source.full_path.empty()) {
        errors << "BuildingType layering fixture requires live road definition provenance.\n";
        return false;
    }

    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create BuildingType layering fixture directory.\n";
        return false;
    }

    const std::filesystem::path lower = temporary.path() / "Lower";
    const std::filesystem::path augustus = temporary.path() / "Augustus";
    const std::filesystem::path vespasian = temporary.path() / "Vespasian";
    const std::filesystem::path almost_empty_upper = temporary.path() / "AlmostEmptyUpper";
    if (!write_file(lower / "BuildingType" / "layered.xml", model_xml("layered", 1)) ||
        !write_file(lower / "BuildingType" / "inherited.xml", model_xml("inherited", 4)) ||
        !write_file(
            almost_empty_upper / "BuildingType" / "replacement.xml",
            model_xml("layered", 9))) {
        errors << "Unable to write sparse BuildingType replacement fixtures.\n";
        return false;
    }

    building_type_layer_test_result result;
    std::string failure;
    const std::vector<mod_definition::DefinitionLayer> sparse_layers = {
        layer("Lower", lower),
        layer("Augustus", augustus),
        layer("Vespasian", vespasian),
        layer("AlmostEmptyUpper", almost_empty_upper)
    };
    if (!building_type_registry_layers_are_valid_for_test(
            sparse_layers, "layered", &result, &failure) ||
        result.active_count != 2 || result.suppressed_count != 0 ||
        result.queried_disabled || result.queried_cost != 9 ||
        result.queried_runtime_type == BUILDING_NONE || result.queried_source_layer != 3 ||
        live_road_definition() != road_before) {
        errors << "BuildingType almost-empty upper layer did not replace atomically: " << failure << '\n';
        return false;
    }
    if (!building_type_registry_layers_are_valid_for_test(
            sparse_layers, "inherited", &result, &failure) ||
        result.queried_cost != 4 || result.queried_source_layer != 0 ||
        live_road_definition() != road_before) {
        errors << "BuildingType inherited lower winner did not survive sparse upper directories.\n";
        return false;
    }

    const std::filesystem::path reference_lower = temporary.path() / "ReferenceLower";
    const std::filesystem::path reference_upper = temporary.path() / "ReferenceUpper";
    if (!write_file(
            reference_lower / "BuildingType" / "deferred.xml",
            "<building type=\"deferred\"><storages>"
            "<storage path=\"definition_that_does_not_exist\"/>"
            "</storages></building>\n") ||
        !write_file(reference_lower / "BuildingType" / "removed.xml", model_xml("removed", 3)) ||
        !write_file(reference_upper / "BuildingType" / "deferred.xml", model_xml("deferred", 7)) ||
        !write_file(
            reference_upper / "BuildingType" / "removed.xml",
            "<building type=\"removed\" disabled=\"true\"></building>\n")) {
        errors << "Unable to write BuildingType deferred-reference fixtures.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> reference_layers = {
        layer("ReferenceLower", reference_lower),
        layer("ReferenceUpper", reference_upper)
    };
    failure.clear();
    if (!building_type_registry_layers_are_valid_for_test(
            reference_layers, "deferred", &result, &failure) ||
        result.active_count != 1 || result.suppressed_count != 1 ||
        result.queried_cost != 7 || result.queried_source_layer != 1 ||
        live_road_definition() != road_before) {
        errors << "BuildingType references were resolved before final overlay winners: " << failure << '\n';
        return false;
    }
    if (!building_type_registry_layers_are_valid_for_test(
            reference_layers, "removed", &result, &failure) ||
        !result.queried_disabled || result.queried_runtime_type != BUILDING_NONE ||
        result.queried_source_layer != 1) {
        errors << "BuildingType tombstone suppression or provenance failed.\n";
        return false;
    }

    const std::filesystem::path duplicate = temporary.path() / "Duplicate";
    if (!write_file(duplicate / "BuildingType" / "first.xml", model_xml("duplicate", 1)) ||
        !write_file(duplicate / "BuildingType" / "second.xml", model_xml("duplicate", 2))) {
        errors << "Unable to write BuildingType duplicate fixtures.\n";
        return false;
    }
    failure.clear();
    if (building_type_registry_layers_are_valid_for_test(
            {layer("Duplicate", duplicate)}, "duplicate", nullptr, &failure) ||
        failure.find("first") == std::string::npos ||
        failure.find("second") == std::string::npos ||
        live_road_definition() != road_before) {
        errors << "BuildingType same-layer duplicate rejection was not diagnostic or atomic.\n";
        return false;
    }

    const std::filesystem::path invalid_tombstone = temporary.path() / "InvalidTombstone";
    if (!write_file(
            invalid_tombstone / "BuildingType" / "invalid.xml",
            "<building type=\"invalid\" disabled=\"true\"><model cost=\"1\"/></building>\n")) {
        errors << "Unable to write invalid BuildingType tombstone fixture.\n";
        return false;
    }
    failure.clear();
    if (building_type_registry_layers_are_valid_for_test(
            {layer("InvalidTombstone", invalid_tombstone)}, "invalid", nullptr, &failure) ||
        live_road_definition() != road_before) {
        errors << "BuildingType identity-only tombstone accepted definition payload.\n";
        return false;
    }

    const std::filesystem::path invalid_tombstone_attribute =
        temporary.path() / "InvalidTombstoneAttribute";
    if (!write_file(
            invalid_tombstone_attribute / "BuildingType" / "invalid.xml",
            "<building type=\"invalid\" disabled=\"true\" cost=\"1\"></building>\n")) {
        errors << "Unable to write BuildingType tombstone attribute fixture.\n";
        return false;
    }
    failure.clear();
    if (building_type_registry_layers_are_valid_for_test(
            {layer("InvalidTombstoneAttribute", invalid_tombstone_attribute)},
            "invalid", nullptr, &failure) || live_road_definition() != road_before) {
        errors << "BuildingType identity-only tombstone accepted extra root attributes.\n";
        return false;
    }

    const std::filesystem::path invalid_replacement = temporary.path() / "InvalidReplacement";
    if (!write_file(
            invalid_replacement / "BuildingType" / "invalid.xml",
            "<building type=\"invalid\"><foundation path=\"land_1x1\" "
            "replaces=\"missing_replacement_type\"/></building>\n")) {
        errors << "Unable to write unresolved foundation replacement fixture.\n";
        return false;
    }
    failure.clear();
    if (building_type_registry_layers_are_valid_for_test(
            {layer("InvalidReplacement", invalid_replacement)},
            "invalid", nullptr, &failure) || live_road_definition() != road_before) {
        errors << "BuildingType foundation accepted an unresolved replacement type.\n";
        return false;
    }

    const std::filesystem::path invalid_spawn = temporary.path() / "InvalidSpawn";
    if (!write_file(
            invalid_spawn / "LegacyMode" / "BuildingType" / "legacy_mode.xml",
            "<building type=\"legacy_mode\"><spawn_group>"
            "<spawn mode=\"profiled_figure\" spawn_figure=\"doctor\" profile=\"service\"/>"
            "</spawn_group></building>\n") ||
        !write_file(
            invalid_spawn / "SpecialProfile" / "BuildingType" / "special_profile.xml",
            "<building type=\"special_profile\"><spawn_group>"
            "<spawn mode=\"temple_supplier\" spawn_figure=\"priest_supplier\" profile=\"legacy\"/>"
            "</spawn_group></building>\n") ||
        !write_file(
            invalid_spawn / "MissingProfile" / "BuildingType" / "missing_profile.xml",
            "<building type=\"missing_profile\"><spawn_group>"
            "<spawn spawn_figure=\"doctor\"/>"
            "</spawn_group></building>\n")) {
        errors << "Unable to write BuildingType spawn authority fixtures.\n";
        return false;
    }
    struct invalid_spawn_fixture {
        const char *directory;
        const char *identity;
    };
    const invalid_spawn_fixture invalid_spawn_fixtures[] = {
        {"LegacyMode", "legacy_mode"},
        {"SpecialProfile", "special_profile"},
        {"MissingProfile", "missing_profile"}
    };
    for (const invalid_spawn_fixture &fixture : invalid_spawn_fixtures) {
        failure.clear();
        if (building_type_registry_layers_are_valid_for_test(
                {layer("InvalidSpawn", invalid_spawn / fixture.directory)}, fixture.identity, nullptr, &failure) ||
            live_road_definition() != road_before) {
            errors << "BuildingType accepted an ambiguous spawn mode/profile definition: " << fixture.identity << ".\n";
            return false;
        }
    }

    struct PlagueTargetExpectation {
        const char *type;
        bool expected;
    };
    constexpr PlagueTargetExpectation plague_targets[] = {
        {"house_small_tent", true},
        {"dock", true},
        {"warehouse", true},
        {"granary", true},
        {"market", false},
        {"prefecture", false},
    };
    for (const PlagueTargetExpectation &expectation : plague_targets) {
        const BuildingType *definition = definition_for_type(type_from_attr(expectation.type));
        if (!definition ||
            static_cast<bool>(definition->is_plague_treatment_target()) != expectation.expected) {
            errors << "BuildingType plague-treatment classification changed for "
                << expectation.type << ".\n";
            return false;
        }
    }

    return true;
}
