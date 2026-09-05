#include "font_vector_runtime_layering_test.h"

#include "graphics/font_vector_runtime.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

class TemporaryFontLayers {
public:
    TemporaryFontLayers()
    {
        const auto suffix = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() / ("vespasian_font_layers_" + std::to_string(suffix));
        std::error_code error;
        std::filesystem::create_directories(root, error);
        valid = !error;
    }

    ~TemporaryFontLayers()
    {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }

    bool add_layer(const char *name)
    {
        const std::filesystem::path path = root / name;
        std::error_code error;
        std::filesystem::create_directories(path / "Fonts", error);
        if (error) return false;
        layers.push_back({name, path.string()});
        return true;
    }

    bool write(const char *layer, const char *relative_path, const char *contents) const
    {
        const std::filesystem::path path = root / layer / relative_path;
        std::error_code error;
        std::filesystem::create_directories(path.parent_path(), error);
        if (error) return false;
        std::ofstream output(path, std::ios::binary);
        output << (contents ? contents : "");
        return output.good();
    }

    std::filesystem::path root;
    std::vector<mod_definition::DefinitionLayer> layers;
    bool valid = false;
};

constexpr const char *BASE_FONT_PACK =
    "<fonts>"
    "<family id=\"body\"><regular src=\"base.ttf\"/><bold src=\"base.ttf\"/>"
    "<italic src=\"base.ttf\"/><bold_italic src=\"base.ttf\"/></family>"
    "<family id=\"title\"><regular src=\"base.ttf\"/></family>"
    "<surface id=\"FONT_NORMAL_PLAIN\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"1\" baseline_offset=\"0\" semantic=\"body\"/>"
    "<surface id=\"FONT_NORMAL_BLACK\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"body\"/>"
    "<surface id=\"FONT_NORMAL_WHITE\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"body\"/>"
    "<surface id=\"FONT_NORMAL_RED\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"body\"/>"
    "<surface id=\"FONT_LARGE_PLAIN\" family=\"title\" logical_size=\"26\" line_height=\"26\" letter_spacing=\"1\" baseline_offset=\"0\" semantic=\"title\"/>"
    "<surface id=\"FONT_LARGE_BLACK\" family=\"title\" logical_size=\"26\" line_height=\"26\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"header\"/>"
    "<surface id=\"FONT_LARGE_BROWN\" family=\"title\" logical_size=\"27\" line_height=\"27\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"header\"/>"
    "<surface id=\"FONT_SMALL_PLAIN\" family=\"body\" logical_size=\"11\" line_height=\"11\" letter_spacing=\"1\" baseline_offset=\"0\" semantic=\"body\"/>"
    "<surface id=\"FONT_NORMAL_GREEN\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"link\"/>"
    "<surface id=\"FONT_NORMAL_BROWN\" family=\"body\" logical_size=\"13\" line_height=\"13\" letter_spacing=\"0\" baseline_offset=\"0\" semantic=\"body\"/>"
    "</fonts>";

constexpr const char *PARTIAL_FONT_PACK =
    "<fonts>"
    "<family id=\"extra\"><regular src=\"partial.ttf\"/></family>"
    "<surface id=\"FONT_NORMAL_PLAIN\" family=\"body\" logical_size=\"19\" line_height=\"19\" letter_spacing=\"2\" baseline_offset=\"1\" semantic=\"body\"/>"
    "</fonts>";

constexpr const char *OVERRIDE_FONT_PACK =
    "<fonts><family id=\"body\"><regular src=\"override.ttf\"/><bold src=\"override.ttf\"/>"
    "<italic src=\"override.ttf\"/><bold_italic src=\"override.ttf\"/></family></fonts>";

bool same_path(const std::string &actual, const std::filesystem::path &expected)
{
    return std::filesystem::path(actual).lexically_normal() == expected.lexically_normal();
}

bool rejects_ambiguous_pack(const char *pack, const char *expected_failure, std::ostream &errors)
{
    TemporaryFontLayers fixture;
    if (!fixture.valid || !fixture.add_layer("Ambiguous") ||
        !fixture.write("Ambiguous", "Fonts/fonts.xml", pack)) {
        errors << "Unable to create ambiguous vector-font fixture.\n";
        return false;
    }

    std::string failure;
    if (font_vector_runtime_layers_are_valid_for_test(
            fixture.layers, "same", FONT_NORMAL_PLAIN, nullptr, &failure) ||
        failure.find(expected_failure) == std::string::npos) {
        errors << "Font composition accepted an ambiguous same-layer definition; expected: "
            << expected_failure << "\n";
        return false;
    }
    return true;
}

} // namespace

bool validate_font_vector_runtime_layering_contract(std::ostream &errors)
{
    TemporaryFontLayers fixture;
    if (!fixture.valid || !fixture.add_layer("Base") || !fixture.add_layer("Partial") ||
        !fixture.add_layer("Override") || !fixture.add_layer("BlankTop") ||
        !fixture.write("Base", "Fonts/fonts.xml", BASE_FONT_PACK) ||
        !fixture.write("Base", "Fonts/base.ttf", "fixture") ||
        !fixture.write("Partial", "Fonts/fonts.xml", PARTIAL_FONT_PACK) ||
        !fixture.write("Partial", "Fonts/partial.ttf", "fixture") ||
        !fixture.write("Override", "Fonts/fonts.xml", OVERRIDE_FONT_PACK) ||
        !fixture.write("Override", "Fonts/override.ttf", "fixture")) {
        errors << "Unable to create layered vector-font fixtures.\n";
        return false;
    }

    font_vector_layer_test_result result;
    std::string failure;
    if (!font_vector_runtime_layers_are_valid_for_test(
            fixture.layers, "body", FONT_NORMAL_PLAIN, &result, &failure) ||
        !result.found_pack || result.family_count != 3 || result.surface_count != FONT_TYPES_MAX ||
        !result.queried_family_defined ||
        !same_path(result.queried_regular_face, fixture.root / "Override" / "Fonts" / "override.ttf") ||
        !result.queried_surface_defined || result.queried_surface_family_id != "body" ||
        result.queried_surface_logical_size != 19) {
        errors << "Partial font layers did not preserve inherited definitions or highest-identity winners: "
            << failure << '\n';
        return false;
    }

    if (!font_vector_runtime_layers_are_valid_for_test(
            fixture.layers, "title", FONT_LARGE_BLACK, &result, &failure) ||
        !result.queried_family_defined ||
        !same_path(result.queried_regular_face, fixture.root / "Base" / "Fonts" / "base.ttf") ||
        !result.queried_surface_defined || result.queried_surface_family_id != "title" ||
        result.queried_surface_logical_size != 26) {
        errors << "Blank and partial upper font layers did not retain lower definitions: " << failure << '\n';
        return false;
    }

    if (!rejects_ambiguous_pack(
            "<fonts><family id=\"same\"><regular src=\"one.ttf\"/></family>"
            "<family id=\"same\"><regular src=\"two.ttf\"/></family></fonts>",
            "duplicate family id", errors) ||
        !rejects_ambiguous_pack(
            "<fonts><family id=\"same\"><regular src=\"one.ttf\"/>"
            "<regular src=\"two.ttf\"/></family></fonts>",
            "duplicate face variant", errors) ||
        !rejects_ambiguous_pack(
            "<fonts><surface id=\"FONT_NORMAL_PLAIN\" family=\"same\" logical_size=\"13\" line_height=\"13\"/>"
            "<surface id=\"font_normal_plain\" family=\"same\" logical_size=\"14\" line_height=\"14\"/></fonts>",
            "duplicate surface id", errors) ||
        !rejects_ambiguous_pack(
            "<fonts><family id=\"same\"><regular src=\"one.ttf\"/>"
            "<fallback family=\"other\" locale=\"en\" script=\"latin\"/>"
            "<fallback family=\"other\" locale=\"EN\" script=\"LATIN\"/>"
            "</family></fonts>",
            "duplicate fallback rule", errors)) {
        return false;
    }
    return true;
}
