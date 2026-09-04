#include "mod_definition_loader_test.h"

#include "assets/xml_path_resolution.h"
#include "core/file.h"
#include "game/mod_definition_loader.h"
#include "graphics/declarative_window.h"

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
            ("vespasian_mod_definition_loader_" + std::to_string(suffix));
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

bool write_fixture(const std::filesystem::path &path, const char *contents = "<fixture/>\n")
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

mod_definition::DefinitionSource source(
    std::size_t layer,
    const char *mod,
    const char *category,
    const char *file)
{
    mod_definition::DefinitionSource result;
    result.layer_index = layer;
    result.mod_name = mod ? mod : "";
    result.mod_root = "Mods/" + result.mod_name + "/";
    result.category = category ? category : "";
    result.file_name = file ? file : "";
    result.full_path = result.mod_root + result.category + "/" + result.file_name;
    result.normalized_definition_path = result.file_name;
    const std::size_t extension = result.normalized_definition_path.rfind(".xml");
    if (extension != std::string::npos) {
        result.normalized_definition_path.resize(extension);
    }
    result.registry_relative_path = result.category + "\\" + result.normalized_definition_path;
    return result;
}

bool validate_enumeration(std::ostream &errors)
{
    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create layered-definition temporary directory.\n";
        return false;
    }

    const std::filesystem::path julius = temporary.path() / "Julius";
    const std::filesystem::path augustus = temporary.path() / "Augustus";
    const std::filesystem::path vespasian = temporary.path() / "Vespasian";
    const std::filesystem::path sparse_patch = temporary.path() / "SparsePatch";
    if (!write_fixture(julius / "BuildingType" / "zeta.xml") ||
        !write_fixture(julius / "BuildingType" / "alpha.xml") ||
        !write_fixture(julius / "Tiles" / "road.xml") ||
        !write_fixture(sparse_patch / "BuildingType" / "alpha.xml") ||
        !write_fixture(sparse_patch / "BuildingType" / "new_building.xml")) {
        errors << "Unable to create layered-definition fixtures.\n";
        return false;
    }

    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Julius", julius.string()},
        {"Augustus", augustus.string()},
        {"Vespasian", vespasian.string()},
        {"SparsePatch", sparse_patch.string()}
    };
    std::vector<mod_definition::DefinitionSource> visited;
    mod_definition::DefinitionEnumerationSummary summary;
    std::string failure;
    if (!mod_definition::for_each_definition_file(
            layers,
            {"BuildingType", "Tiles/"},
            "LayerTest",
            true,
            [&](const mod_definition::DefinitionSource &definition) {
                visited.push_back(definition);
                return true;
            },
            &summary,
            &failure)) {
        errors << "Layered definition enumeration failed: " << failure << '\n';
        return false;
    }

    if (visited.size() != 5 || summary.layers != 4 || summary.directories != 3 || summary.files != 5 ||
        visited[0].layer_index != 0 || visited[0].mod_name != "Julius" ||
        visited[0].registry_relative_path != "BuildingType\\alpha" ||
        visited[1].registry_relative_path != "BuildingType\\zeta" ||
        visited[2].registry_relative_path != "Tiles\\road" ||
        visited[3].layer_index != 3 || visited[3].mod_name != "SparsePatch" ||
        visited[3].normalized_definition_path != "alpha" ||
        visited[4].normalized_definition_path != "new_building") {
        errors << "Layered definition enumeration did not preserve lower-to-upper, category, or file order.\n";
        return false;
    }

    mod_definition::DefinitionEnumerationSummary empty_summary;
    if (!mod_definition::for_each_definition_file(
            layers, {"Missing"}, "LayerTest", false, {}, &empty_summary, &failure) ||
        empty_summary.files != 0 || empty_summary.directories != 0) {
        errors << "Missing layered definition directories were not treated as optional.\n";
        return false;
    }
    if (mod_definition::for_each_definition_file(
            layers, {"Missing"}, "LayerTest", true, {}, nullptr, &failure) ||
        failure.find("No LayerTest xml files") == std::string::npos) {
        errors << "Required layered definition enumeration did not reject an empty whole stack.\n";
        return false;
    }

    failure.clear();
    if (mod_definition::for_each_definition_file(
            layers,
            {"BuildingType"},
            "LayerTest",
            true,
            [&](const mod_definition::DefinitionSource &) {
                failure = "specific registry diagnostic";
                return false;
            },
            nullptr,
            &failure) ||
        failure != "specific registry diagnostic") {
        errors << "Layered enumeration replaced an actionable registry diagnostic.\n";
        return false;
    }
    return true;
}

bool validate_complete_file_fallback(std::ostream &errors)
{
    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create layered-file temporary directory.\n";
        return false;
    }

    const std::filesystem::path julius = temporary.path() / "Julius";
    const std::filesystem::path augustus = temporary.path() / "Augustus";
    const std::filesystem::path vespasian = temporary.path() / "Vespasian";
    const std::filesystem::path relative = std::filesystem::path("UI") / "windows" / "mission_briefing.xml";
    if (!write_fixture(julius / relative) || !write_fixture(augustus / relative)) {
        errors << "Unable to create layered-file fallback fixtures.\n";
        return false;
    }

    const std::vector<mod_definition::DefinitionLayer> layers = {
        {"Julius", julius.string()},
        {"Augustus", augustus.string()},
        {"Vespasian", vespasian.string()}
    };
    mod_definition::LayeredFileSource selected;
    std::string failure;
    if (!mod_definition::find_nearest_file(
            layers, "UI/windows/mission_briefing.xml", &selected, &failure) ||
        selected.layer_index != 1 || selected.mod_name != "Augustus" ||
        selected.relative_path != "UI/windows/mission_briefing.xml" ||
        std::filesystem::path(selected.full_path) != augustus / relative) {
        errors << "Whole-file fallback did not select the nearest existing lower-layer file: " << failure << '\n';
        return false;
    }

    if (!write_fixture(vespasian / relative) ||
        !mod_definition::find_nearest_file(
            layers, "UI\\windows\\mission_briefing.xml", &selected, &failure) ||
        selected.layer_index != 2 || selected.mod_name != "Vespasian" ||
        std::filesystem::path(selected.full_path) != vespasian / relative) {
        errors << "Whole-file fallback did not prefer the topmost complete file: " << failure << '\n';
        return false;
    }

    if (mod_definition::find_nearest_file(layers, "UI/windows/missing.xml", &selected, &failure) ||
        failure.find("UI/windows/missing.xml") == std::string::npos || !selected.full_path.empty()) {
        errors << "Missing layered file did not produce a specific failure.\n";
        return false;
    }

    if (mod_definition::find_nearest_file(layers, "../outside.xml", &selected, &failure) ||
        failure.find("Invalid layered file path") == std::string::npos || !selected.full_path.empty()) {
        errors << "Layered-file fallback accepted a path outside the mod root.\n";
        return false;
    }

    const std::filesystem::path base_game = temporary.path() / "BaseGame";
    const std::filesystem::path total_conversion = temporary.path() / "TotalConversion";
    const std::filesystem::path tiny_patch = temporary.path() / "TinyPatch";
    const std::filesystem::path font_pack = std::filesystem::path("Fonts") / "fonts.xml";
    if (!write_fixture(base_game / font_pack) || !write_fixture(total_conversion / font_pack)) {
        errors << "Unable to create arbitrary-stack vector-font fixtures.\n";
        return false;
    }
    const std::vector<mod_definition::DefinitionLayer> arbitrary_layers = {
        {"BaseGame", base_game.string()},
        {"TotalConversion", total_conversion.string()},
        {"TinyPatch", tiny_patch.string()}
    };
    if (!mod_definition::find_nearest_file(arbitrary_layers, "Fonts/fonts.xml", &selected, &failure) ||
        selected.mod_name != "TotalConversion" ||
        std::filesystem::path(selected.full_path) != total_conversion / font_pack) {
        errors << "Vector-font whole-pack fallback still depends on named built-in mods: " << failure << '\n';
        return false;
    }
    return true;
}

bool validate_sparse_identity_composition(std::ostream &errors)
{
    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create sparse-composition temporary directory.\n";
        return false;
    }

    const char *names[] = {"A", "B", "C", "D", "BlankTop"};
    std::vector<mod_definition::DefinitionLayer> layers;
    for (const char *name : names) {
        const std::filesystem::path root = temporary.path() / name;
        std::error_code error;
        std::filesystem::create_directories(root, error);
        if (error) {
            errors << "Unable to create sparse-composition layer " << name << ".\n";
            return false;
        }
        layers.push_back({name, root.string()});
    }
    if (!write_fixture(temporary.path() / "A" / "BuildingType" / "hut.xml") ||
        !write_fixture(temporary.path() / "B" / "BuildingType" / "house.xml") ||
        !write_fixture(temporary.path() / "C" / "BuildingType" / "hut.xml") ||
        !write_fixture(temporary.path() / "D" / "BuildingType" / "hut.xml") ||
        !write_fixture(temporary.path() / "BlankTop" / "mod.xml",
            "<mod><name value=\"BlankTop\"/><description value=\"Blank overlay\"/>"
            "<version value=\"1\"/><dependencies><mod name=\"D\"/></dependencies></mod>\n")) {
        errors << "Unable to create sparse-composition definitions.\n";
        return false;
    }

    mod_definition::DefinitionOverlayTracker winners;
    mod_definition::DefinitionEnumerationSummary summary;
    std::vector<std::string> visit_order;
    std::string failure;
    if (!mod_definition::for_each_definition_file(
            layers, {"BuildingType"}, "SparseComposition", true,
            [&](const mod_definition::DefinitionSource &definition) {
                visit_order.push_back(definition.mod_name + "/" + definition.normalized_definition_path);
                if (!winners.apply(definition.normalized_definition_path, false, definition)) {
                    failure = winners.failure_reason();
                    return false;
                }
                return true;
            }, &summary, &failure)) {
        errors << "Sparse identity composition failed: " << failure << '\n';
        return false;
    }

    const std::vector<std::string> expected = {"A/hut", "B/house", "C/hut", "D/hut"};
    const mod_definition::DefinitionOverlayEntry *hut = winners.find("hut");
    const mod_definition::DefinitionOverlayEntry *house = winners.find("house");
    if (visit_order != expected || summary.layers != 5 || summary.directories != 4 || summary.files != 4 ||
        winners.active_count() != 2 || !hut || hut->source.mod_name != "D" ||
        !house || house->source.mod_name != "B") {
        errors << "Sparse composition did not retain the highest declaration per identity through a blank top mod.\n";
        return false;
    }
    return true;
}

bool validate_arbitrary_graphics_layer_composition(std::ostream &errors)
{
    TemporaryDirectory temporary;
    if (!temporary.valid()) {
        errors << "Unable to create graphics-layer temporary directory.\n";
        return false;
    }

    std::vector<GraphicsLayerSource> layers;
    for (std::size_t index = 0; index < 11; ++index) {
        const std::string name = "Custom" + std::to_string(index);
        const std::filesystem::path graphics = temporary.path() / name / "Graphics";
        std::error_code error;
        std::filesystem::create_directories(graphics, error);
        if (error) {
            errors << "Unable to create graphics layer " << name << ".\n";
            return false;
        }
        layers.push_back({index, name, graphics.string()});
        if (index < 10 && !write_fixture(graphics / "Shared" / "Group.xml")) {
            errors << "Unable to create graphics assetlist in layer " << name << ".\n";
            return false;
        }
    }

    const std::vector<GraphicsLayerSource> sources =
        xml_collect_assetlist_sources(layers, "Shared/Group");
    if (sources.size() != 10) {
        errors << "Graphics composition truncated, reordered, or skipped custom layers.\n";
        return false;
    }
    for (std::size_t index = 0; index < sources.size(); ++index) {
        const std::size_t expected_layer = 9 - index;
        if (sources[index].layer_index != expected_layer ||
            sources[index].mod_name != "Custom" + std::to_string(expected_layer)) {
            errors << "Graphics composition did not preserve top-to-bottom mod-list order.\n";
            return false;
        }
    }

    char assetlist_path[FILE_NAME_MAX] = {0};
    const std::filesystem::path expected_assetlist =
        std::filesystem::path(layers[5].graphics_root) / "Shared" / "Group.xml";
    if (!xml_resolve_assetlist_path(assetlist_path, "Shared/Group", layers[5]) ||
        std::filesystem::path(assetlist_path) != expected_assetlist) {
        errors << "An intermediate custom graphics layer lost its exact XML provenance.\n";
        return false;
    }

    const std::filesystem::path upper_png =
        std::filesystem::path(layers[9].graphics_root) / "Shared" / "Group" / "sprite.png";
    const std::filesystem::path inherited_png =
        std::filesystem::path(layers[3].graphics_root) / "Shared" / "Group" / "sprite.png";
    if (!write_fixture(upper_png, "upper") || !write_fixture(inherited_png, "lower")) {
        errors << "Unable to create graphics image fallback fixtures.\n";
        return false;
    }

    char resolved[FILE_NAME_MAX] = {0};
    if (!xml_resolve_image_path(
            layers, resolved, "Shared/Group", "sprite", layers[4]) ||
        std::filesystem::path(resolved) != inherited_png) {
        errors << "An intermediate custom graphics layer resolved an image from above itself.\n";
        return false;
    }
    return true;
}

bool validate_overlay(std::ostream &errors)
{
    using mod_definition::DefinitionOverlayAction;
    using mod_definition::DefinitionOverlayChange;
    using mod_definition::DefinitionOverlayTracker;

    DefinitionOverlayTracker overlay;
    DefinitionOverlayChange change;
    const auto julius = source(0, "Julius", "Foundations", "dock.xml");
    const auto augustus = source(1, "Augustus", "Foundations", "dock.xml");
    const auto vespasian = source(2, "Vespasian", "Foundations", "dock.xml");
    const auto pharaoh = source(3, "Pharaoh", "Foundations", "dock.xml");

    if (!overlay.apply("dock", false, julius, &change) ||
        change.action != DefinitionOverlayAction::Added || change.previous ||
        !overlay.apply("dock", false, augustus, &change) ||
        change.action != DefinitionOverlayAction::Replaced || !change.previous ||
        change.previous->source.mod_name != "Julius" ||
        !overlay.apply("dock", true, vespasian, &change) ||
        change.action != DefinitionOverlayAction::Suppressed ||
        !overlay.find("dock") || !overlay.find("dock")->disabled ||
        overlay.active_count() != 0 || overlay.suppressed_count() != 1 ||
        !overlay.apply("dock", false, pharaoh, &change) ||
        change.action != DefinitionOverlayAction::Restored ||
        !change.previous || !change.previous->disabled ||
        overlay.active_count() != 1 || overlay.suppressed_count() != 0 ||
        overlay.find("dock")->source.mod_name != "Pharaoh") {
        errors << "Definition overlay replacement, suppression, restoration, or provenance failed.\n";
        return false;
    }

    DefinitionOverlayTracker duplicate;
    const auto first = source(1, "Pharaoh", "BuildingType", "dock.xml");
    const auto second = source(1, "Pharaoh", "Tiles", "dock.xml");
    if (!duplicate.apply("dock", false, first) || duplicate.apply("dock", true, second) ||
        duplicate.failure_reason().find(first.full_path) == std::string::npos ||
        duplicate.failure_reason().find(second.full_path) == std::string::npos) {
        errors << "Same-layer duplicate detection did not cite both definition sources.\n";
        return false;
    }

    DefinitionOverlayTracker tombstone;
    if (!tombstone.apply("unused", true, source(0, "Julius", "BuildingType", "unused.xml"), &change) ||
        change.action != DefinitionOverlayAction::Suppressed ||
        !tombstone.find("unused") || !tombstone.find("unused")->disabled) {
        errors << "A first-layer tombstone was not retained for provenance.\n";
        return false;
    }

    DefinitionOverlayTracker out_of_order;
    if (!out_of_order.apply("road", false, source(2, "Vespasian", "Tiles", "road.xml")) ||
        out_of_order.apply("road", false, source(0, "Julius", "Tiles", "road.xml")) ||
        out_of_order.failure_reason().find("lower layer") == std::string::npos) {
        errors << "Out-of-order overlay application was not rejected.\n";
        return false;
    }

    DefinitionOverlayTracker invalid;
    if (invalid.apply("", false, julius) || invalid.failure_reason().empty()) {
        errors << "Empty definition identity was accepted.\n";
        return false;
    }
    return true;
}

bool validate_declarative_window_identity_overlay(std::ostream &errors)
{
    constexpr const char *LOWER_SHARED =
        "<window id=\"shared\" base_width=\"320\" base_height=\"240\"></window>";
    constexpr const char *LOWER_RETAINED =
        "<window id=\"retained\" base_width=\"400\" base_height=\"300\"></window>";
    constexpr const char *UPPER_SHARED =
        "<window id=\"shared\" base_width=\"640\" base_height=\"480\"></window>";

    const declarative_window_layer_test_input sparse_replacement[] = {
        {LOWER_SHARED, 0, "Lower", "Lower/UI/windows/legacy_name.xml"},
        {LOWER_RETAINED, 0, "Lower", "Lower/UI/windows/retained.xml"},
        {UPPER_SHARED, 1, "SparsePatch", "SparsePatch/UI/windows/renamed.xml"},
    };
    declarative_window_layer_test_result result;
    if (!declarative_window_layered_definition_buffers_are_valid_for_test(
            sparse_replacement, 3, "shared", &result) ||
        result.active_count != 2 || result.queried_source_layer != 1 || result.queried_base_width != 640) {
        errors << "Declarative windows were not overlaid by XML window id across differently named files.\n";
        return false;
    }
    if (!declarative_window_layered_definition_buffers_are_valid_for_test(
            sparse_replacement, 3, "retained", &result) ||
        result.active_count != 2 || result.queried_source_layer != 0 || result.queried_base_width != 400) {
        errors << "A sparse declarative-window layer did not preserve an unrelated lower-layer winner.\n";
        return false;
    }

    const declarative_window_layer_test_input duplicate[] = {
        {LOWER_SHARED, 0, "Lower", "Lower/UI/windows/first.xml"},
        {UPPER_SHARED, 0, "Lower", "Lower/UI/windows/second.xml"},
    };
    if (declarative_window_layered_definition_buffers_are_valid_for_test(
            duplicate, 2, "shared", &result)) {
        errors << "Declarative windows accepted an ambiguous same-layer duplicate XML id.\n";
        return false;
    }
    const std::string duplicate_failure = declarative_window_registry_get_failure_reason();
    if (duplicate_failure.find("first.xml") == std::string::npos ||
        duplicate_failure.find("second.xml") == std::string::npos) {
        errors << "Declarative-window duplicate diagnostics did not identify both sources.\n";
        return false;
    }
    return true;
}

} // namespace

bool validate_mod_definition_layering_contract(std::ostream &errors)
{
    if (!validate_enumeration(errors) || !validate_complete_file_fallback(errors) ||
        !validate_sparse_identity_composition(errors) ||
        !validate_arbitrary_graphics_layer_composition(errors) || !validate_overlay(errors) ||
        !validate_declarative_window_identity_overlay(errors)) {
        return false;
    }
    return true;
}
