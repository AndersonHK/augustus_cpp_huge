#include "housing_profile_registry_layering_test.h"

#include "building/housing_profile_registry.h"
#include "game/mod_content.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>
#include <cmath>
#include "building/HousingStateBridge.h"

static bool validate_annual_goods_consumption(std::ostream &errors)
{
    using building_type_registry_impl::HousingGoodsRate;
    struct Example { const char *rate; int population; int events; double annual; };
    for (const Example &example : { Example{"12/19", 19, 1, 12}, {"24/76", 76, 2, 24},
            {"24/84", 84, 2, 24}, {"24/200", 200, 2, 24}, {"48/200", 200, 2, 48},
            {"1", 19, 1, 19}, {"1", 76, 2, 76}, {"2", 200, 2, 400},
            {"0.3", 19, 1, 5.7}, {"0.3", 76, 2, 22.8}, {"0.6", 106, 2, 63.6}, {"0.6", 200, 2, 120} }) {
        HousingGoodsRate rate;
        double remainder = 0;
        int consumed = 0;
        if (!HousingGoodsRate::parse(example.rate, rate)) return false;
        for (int event = 0; event < example.events * 12; ++event) consumed += rate.consume(example.population, example.events, remainder);
        if (consumed != static_cast<int>(std::floor(example.annual + 1e-9)) || std::abs(consumed + remainder - example.annual) > 1e-8) {
            errors << "Annual housing consumption mismatch for " << example.rate << ": " << consumed << '\n';
            return false;
        }
    }
    HousingGoodsRate rate;
    for (const char *invalid : {"", "-1", "1/0", "1/-2", "nan", "inf", "1/2/3", "1.2.3", "10001", "0.0000001"}) {
        if (HousingGoodsRate::parse(invalid, rate)) { errors << "Invalid housing rate accepted: " << invalid << '\n'; return false; }
    }
    if (!HousingGoodsRate::parse("0.63", rate) || rate.numerator != 63 || rate.denominator != 100) return false;
    HousingGoodsRate::parse("12/19", rate);
    HousingState state;
    int consumed = 0;
    for (int month = 0; month < 12; ++month) {
        consumed += rate.consume(5, 1, state.goods_consumption_remainder[0]);
        building_save_bridge::LegacyBuildingSaveDto saved;
        housing_state_to_legacy_save(state, saved);
        state = housing_state_from_legacy_save(saved);
    }
    if (consumed != 3 || std::abs(state.goods_consumption_remainder[0] - 3.0 / 19) > 1e-8 || rate.stock_target(19, 1, 8) != 8) return false;
    HousingGoodsRate::parse("2", rate);
    if (rate.stock_target(200, 2, 8) != 134) return false;
    HousingGoodsRate::parse("0.6", rate);
    if (rate.stock_target(200, 2, 8) != 40) return false;
    HousingGoodsRate::parse("0.3", rate);
    double decade_remainder = 0;
    int decade_consumed = 0;
    for (int event = 0; event < 120; ++event) decade_consumed += rate.consume(19, 1, decade_remainder);
    if (decade_consumed != 57 || std::abs(decade_remainder) > 1e-8) return false;
    const double remainder = state.goods_consumption_remainder[0];
    if (rate.consume(0, 2, state.goods_consumption_remainder[0]) != 0 || state.goods_consumption_remainder[0] != remainder) return false;
    return true;
}

static bool validate_shipped_annual_goods_rates(std::ostream &errors)
{
    using namespace building_type_registry_impl;
    struct RestoreContent {
        mod_content::Session previous = mod_content::runtime();
        ~RestoreContent() { mod_content::runtime() = std::move(previous); }
    } restore_content;
    struct Example { const char *path; int capacity; int events; int annual[4]; int wine_sources; };
    const Example examples[] = {
        {"house_grand_insula", 84, 2, {24, 24, 24, 0}, 0},
        {"house_grand_villa", 100, 2, {24, 24, 24, 24}, 1},
        {"house_large_casa_2x2", 76, 2, {24, 0, 0, 0}, 0},
        {"house_large_casa", 19, 1, {12, 0, 0, 0}, 0},
        {"house_large_hovel", 15, 1, {0, 0, 0, 0}, 0},
        {"house_large_insula", 84, 2, {24, 24, 24, 0}, 0},
        {"house_large_palace", 190, 2, {24, 24, 24, 48}, 2},
        {"house_large_shack", 11, 1, {0, 0, 0, 0}, 0},
        {"house_large_tent", 7, 1, {0, 0, 0, 0}, 0},
        {"house_large_villa", 90, 2, {24, 24, 24, 24}, 1},
        {"house_luxury_palace", 200, 2, {24, 24, 24, 48}, 2},
        {"house_medium_insula_2x2", 80, 2, {24, 0, 24, 0}, 0},
        {"house_medium_insula", 20, 1, {12, 0, 12, 0}, 0},
        {"house_medium_palace", 112, 2, {24, 24, 24, 48}, 2},
        {"house_medium_villa", 42, 2, {24, 24, 24, 24}, 1},
        {"house_small_casa", 17, 1, {0, 0, 0, 0}, 0},
        {"house_small_hovel", 13, 1, {0, 0, 0, 0}, 0},
        {"house_small_insula_2x2", 76, 2, {24, 0, 0, 0}, 0},
        {"house_small_insula", 19, 1, {12, 0, 0, 0}, 0},
        {"house_small_palace", 106, 2, {24, 24, 24, 48}, 2},
        {"house_small_shack", 9, 1, {0, 0, 0, 0}, 0},
        {"house_small_tent", 5, 1, {0, 0, 0, 0}, 0},
        {"house_small_villa", 40, 2, {24, 24, 24, 24}, 1},
    };
    std::vector<mod_definition::DefinitionLayer> layers;
    std::vector<mod_content::Layer> content_layers;
    for (const char *mod : {"Julius", "Augustus", "Vespasian"}) {
        layers.push_back({mod, std::string("Mods/") + mod});
        content_layers.push_back({mod, std::filesystem::path("Mods") / mod});
        mod_content::runtime().load(content_layers, {});
        std::string failure;
        if (!housing_profile_registry_load_layers(layers, &failure)) { errors << failure << '\n'; return false; }
        if (housing_profile_compatibility_level_count() != 20) return false;
        for (const Example &example : examples) {
            const auto *profile = find_housing_profile_definition(example.path);
            if (!profile || profile->requirements.wine_sources != example.wine_sources) return false;
            const auto &r = profile->requirements;
            const HousingGoodsRate rates[] = {r.pottery, r.oil, r.furniture, r.wine};
            for (int good = 0; good < 4; ++good) {
                double expected = example.annual[good];
                if (std::string(mod) == "Vespasian" && expected) {
                    double per_person = profile->resident_class_value == HousingResidentClass::Patrician ? 0.6 : 0.3;
                    if (good == 3 && profile->compatibility_level <= 15) per_person = 0.3;
                    expected = per_person * example.capacity;
                }
                double remainder = 0;
                int consumed = 0;
                for (int event = 0; event < example.events * 12; ++event) consumed += rates[good].consume(example.capacity, example.events, remainder);
                if (consumed != static_cast<int>(std::floor(expected + 1e-9)) || std::abs(consumed + remainder - expected) > 1e-8) {
                    errors << mod << ' ' << example.path << " annual good " << good << ": expected " << expected << ", got " << consumed << '\n';
                    return false;
                }
            }
        }
    }
    return true;
}
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
    if (!validate_annual_goods_consumption(errors)) { errors << "Annual housing goods consumption contract failed.\n"; return false; }
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

    const auto variants = temporary.path() / "Variants";
    std::string variant_xml = profile_xml("variant", 3, "plebeian", 1, 1);
    variant_xml.insert(variant_xml.find('>'), " variant_of=\"canonical\"");
    if (!write_file(variants / "HousingProfile/canonical.xml", profile_xml("canonical", 3, "plebeian", 1, 1)) ||
        !write_file(variants / "HousingProfile/variant.xml", variant_xml) ||
        !housing_profile_registry_load_layers({layer("Variants", variants)}, &failure) ||
        housing_profile_compatibility_level_count() != 1 || !find_housing_profile_definition("variant") ||
        find_housing_profile_definition_for_compatibility_level(3)->path_id != "canonical") {
        errors << "HousingProfile variant failed shared-level resolution.\n";
        return false;
    }
    variant_xml.replace(variant_xml.find("variant_of=\"canonical\""), std::string("variant_of=\"canonical\"").size(), "variant_of=\"missing\"");
    if (!write_file(variants / "HousingProfile/variant.xml", variant_xml) ||
        housing_profile_registry_load_layers({layer("Variants", variants)}, &failure) ||
        failure.find("variant") == std::string::npos || !find_housing_profile_definition("canonical")) {
        errors << "HousingProfile missing variant base was not rejected transactionally.\n";
        return false;
    }
    return validate_shipped_annual_goods_rates(errors);
}
