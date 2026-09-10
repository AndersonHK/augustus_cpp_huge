#include "production_method_layering_test.h"

#include "building/production_method_registry.h"
#include "building/building_type.h"
#include "core/calc.h"
#include "game/mod_content.h"

#include <chrono>
#include <climits>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

namespace {

bool validate_cycle_throughput(std::ostream &errors)
{
    using namespace building_type_registry_impl;
    ProductionMethod recruitment("recruitment");
    recruitment.set_kind(ProductionMethodKind::Workshop);
    recruitment.set_output_resource(static_cast<resource_type>(RESOURCE_NONE + 1));
    recruitment.set_base_monthly_production(200);
    BuildingType barracks(BUILDING_NONE, "test_barracks");
    barracks.add_production_method(&recruitment);
    if (!barracks.has_native_production() || barracks.output_resource() != recruitment.output_resource()) {
        errors << "Troop production must use the native production runtime.\n";
        return false;
    }
    ProductionMethod effect("shipyard");
    effect.set_kind(ProductionMethodKind::Workshop);
    effect.set_output_effect(ProductionOutputEffect::SpawnFishingBoat);
    BuildingType shipyard(BUILDING_NONE, "test_shipyard");
    shipyard.add_production_method(&effect);
    if (!shipyard.has_native_production() || shipyard.output_resource() != RESOURCE_NONE) {
        errors << "Effect-only production must keep its native runtime.\n";
        return false;
    }
    for (auto kind : {ProductionMethodKind::Farm, ProductionMethodKind::Workshop}) {
        const int rate = kind == ProductionMethodKind::Farm ? 32 : 40;
        const int fields = kind == ProductionMethodKind::Farm ? 5 : 1;
        for (auto ratio : {std::pair{1, 5}, std::pair{1, 1}, std::pair{2, 1}, std::pair{5, 1}}) {
            ProductionMethod method("throughput");
            method.set_kind(kind);
            method.set_output_resource(static_cast<resource_type>(RESOURCE_NONE + 1));
            method.set_cart_loads(ratio.first, ratio.second);
            method.add_input({static_cast<resource_type>(RESOURCE_NONE + 1), 100});
            const int work = method.scale_cycle_work(calc_percentage(16 * 2 * 10, rate));
            int progress = 0;
            int output = 0;
            // A complete period for every harvest size, including multi-load workshops.
            for (int update = 0; update < 125 * 12 * 16 * 2; ++update) {
                progress += 10;
                if (progress >= work) {
                    output += fields * 100 * ratio.first / ratio.second;
                    progress = 0;
                }
            }
            if (output != 125 * 12 * rate * fields || progress != 0 || method.inputs().front().amount != 100) {
                errors << "Output cadence changed monthly throughput or literal input amounts.\n";
                return false;
            }
            method.set_cart_capacity(7);
            if (method.scale_cycle_work(calc_percentage(16 * 2 * 10, rate)) != work) {
                errors << "Cart transport capacity changed production work.\n";
                return false;
            }
            for (int days : {16, 28, 30, 31}) {
                const int month_work = method.scale_cycle_work(calc_percentage(days * 2 * 10, rate));
                const double monthly = 1.0 * days * 2 * 10 / month_work * 100 * ratio.first / ratio.second;
                if (monthly < rate - 0.0001 || monthly > rate * 1.002) {
                    errors << "Month normalization changed production beyond integer-work rounding.\n";
                    return false;
                }
            }
        }
    }
    ProductionMethod default_output("default");
    default_output.set_output_resource(static_cast<resource_type>(RESOURCE_NONE + 1));
    if (default_output.cart_load_numerator() != 1 || default_output.cart_load_denominator() != 1 || default_output.scale_cycle_work(1000) != 1000) return false;
    default_output.set_cart_loads(1, 5);
    if (default_output.scale_cycle_work(1000) != 200 || default_output.scale_cycle_work(1) != 1 || default_output.scale_cycle_work(0) != 0) return false;
    default_output.set_cart_loads(5, 1);
    if (default_output.scale_cycle_work(INT_MAX) != INT_MAX) return false;
    ProductionMethod boat("boat");
    boat.set_output_effect(ProductionOutputEffect::SpawnFishingBoat);
    boat.set_cart_loads(5, 1);
    if (boat.scale_cycle_work(1000) != 1000) { errors << "Cartloads changed non-resource effect timing.\n"; return false; }
    return true;
}

bool validate_upstream_production_rates(std::ostream &errors)
{
    struct RestoreContent {
        mod_content::Session previous = mod_content::runtime();
        ~RestoreContent() { mod_content::runtime() = std::move(previous); }
    } restore;
    // Legacy defaults from upstream 95e120d80:src/game/resource.c, the merged upstream baseline.
    struct Rate { const char *method; int since_layer; int fields; int monthly; };
    const Rate rates[] = {
        {"wheat_farm_field_basic", 0, 5, 160}, {"vegetable_farm_field_basic", 0, 5, 80},
        {"fruit_farm_field_basic", 0, 5, 80}, {"pig_farm_field_basic", 0, 5, 80},
        {"olive_farm_field_basic", 0, 5, 80}, {"vines_farm_field_basic", 0, 5, 80},
        {"clay_pit_basic", 0, 1, 80}, {"timber_yard_basic", 0, 1, 80},
        {"iron_mine_basic", 0, 1, 80}, {"marble_quarry_basic", 0, 1, 40},
        {"pottery_workshop_basic", 0, 1, 40}, {"furniture_workshop_basic", 0, 1, 40},
        {"oil_workshop_basic", 0, 1, 40}, {"wine_workshop_basic", 0, 1, 40},
        {"weapons_workshop_basic", 0, 1, 40}, {"fish_wharf_basic", 0, 1, 100},
        {"gold_mine_basic", 1, 1, 20}, {"sand_pit_basic", 1, 1, 120},
        {"stone_quarry_basic", 1, 1, 80}, {"concrete_maker_basic", 1, 1, 120},
        {"brickworks_basic", 1, 1, 60}, {"city_mint_basic", 1, 1, 200},
        // Upstream industry.c uses RESOURCE_DENARII's rate for both city-mint outputs.
        {"city_mint_gold_basic", 1, 1, 200}
    };
    std::vector<mod_definition::DefinitionLayer> layers;
    std::vector<mod_content::Layer> content_layers;
    for (const char *mod : {"Julius", "Augustus", "Vespasian"}) {
        layers.push_back({mod, std::string("Mods/") + mod});
        content_layers.push_back({mod, std::filesystem::path("Mods") / mod});
        mod_content::runtime().load(content_layers, {});
        for (const auto &rate : rates) {
            if (rate.since_layer >= static_cast<int>(layers.size())) continue;
            production_method_layer_test_result result{};
            std::string failure;
            if (!production_method_layered_definition_files_are_valid_for_test(layers, rate.method, &result, &failure) || result.queried_production_per_month * rate.fields != rate.monthly) {
                errors << "Upstream production rate mismatch: " << mod << '/' << rate.method << ": " << failure << '\n';
                return false;
            }
            if (rate.fields == 5 && (result.queried_cart_numerator != 1 || result.queried_cart_denominator != (layers.size() == 3 ? 1 : 5))) {
                errors << "Shipped field harvest grouping changed: " << mod << '/' << rate.method << '\n';
                return false;
            }
        }
    }
    return true;
}

constexpr const char *LOWER_XML =
    "<production_method>"
    "<kind value=\"workshop\"/>"
    "<output resource=\"wheat\" production_per_month=\"20\"/>"
    "<cart_loads value=\"1\"/>"
    "</production_method>";
constexpr const char *UPPER_XML =
    "<production_method>"
    "<kind value=\"workshop\"/>"
    "<output resource=\"wheat\" production_per_month=\"90\"/>"
    "<cart_loads value=\"2\"/>"
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
    if (!validate_cycle_throughput(errors)) return false;
    const std::string training = "<production_method><kind value=\"workshop\"/><output resource=\"troops\" production_per_month=\"200\"/>";
    for (const char *attributes : {"source=\"unknown\" threshold=\"20\" percent_per_point=\"12.5\"", "source=\"military_food_stress\" threshold=\"-1\" percent_per_point=\"12.5\"", "source=\"military_food_stress\" threshold=\"20\" percent_per_point=\"NaN\"", "source=\"military_food_stress\" threshold=\"20\" percent_per_point=\"-1\""}) {
        const std::string xml = training + "<work_modifier " + attributes + "/></production_method>";
        const auto fixture = input(xml.c_str(), 0, "Julius", "Julius/ProductionMethod/training.xml", "training");
        if (valid(&fixture, 1, "training")) { errors << "Invalid work modifier accepted.\n"; return false; }
    }
    const std::string modifier = "<work_modifier source=\"military_food_stress\" threshold=\"20\" percent_per_point=\"12.5\"/>";
    for (int count : {1, 2}) {
        const std::string xml = training + modifier + (count == 2 ? modifier : "") + "</production_method>";
        const auto fixture = input(xml.c_str(), 0, "Julius", "Julius/ProductionMethod/training.xml", "training");
        if (valid(&fixture, 1, "training") != (count == 1)) { errors << "Work modifier validity/duplicate contract failed.\n"; return false; }
    }
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
        result.queried_input_count != 1 || result.queried_input_amount != 5 || result.queried_cart_numerator != 2) {
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

    return validate_upstream_production_rates(errors);
}
