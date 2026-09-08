#pragma once
#include "imported_state.h"
#include "game/archive_origin.h"
#include "game/augustus_save_bridge.h"
#include "game/augustus_record_bridge.h"
#include "game/augustus_model_bridge.h"
#include "game/augustus_accounting_bridge.h"
#include "game/augustus_city_bridge.h"
#include "game/augustus_common_records.h"
#include "game/augustus_scenario_bridge.h"
#include "game/augustus_monument_bridge.h"
#include "game/augustus_empire_bridge.h"
#include "game/augustus_archive_layouts.generated.h"
#include "game/file_io.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include <cstdio>
#include "city/population.h"
#include "city/finance.h"
#include "city/houses.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
#include "game/file.h"
#include "game/state.h"
#include "game/time.h"
#include "game/settings.h"
#include "building/building.h"
#include "building/properties.h"
#include "map/TerrainMap.h"

inline void validate_archive_origins()
{
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    const int population = city_population(), treasury = city_finance_treasury();
    const int mapping = resource_mapping_get_version();
    for (int version : {175, 184, 189}) {
        AugustusArchive fixture; fixture.origin.family = ArchiveFamily::Augustus; fixture.origin.save_version = version;
        auto &bytes = fixture.pieces["custom_empire"];
        const bool extended = version >= 184;
        bytes.resize(4 + 87 + (extended ? 253 : 84));
        augustus_save::write_u32(bytes, 0, 1); bytes[4] = 1; bytes[5] = 1; bytes[26] = 7;
        if (extended) {
            augustus_save::write_u32(bytes, 32, 70000);
            augustus_save::write_u32(bytes, 32 + 168, 1234); bytes.back() = 1;
        } else augustus_save::write_u16(bytes, 32, 25000);
        augustus_save::EmpireRecords decoded; std::string diagnostic;
        require(augustus_save::decode_empire(fixture, decoded, diagnostic) && decoded.common.size() == 275, "Foreign empire object normalization failed");
        require(augustus_save::read_u32(decoded.common, 32) == (extended ? 70000u : 25000u), "Foreign empire quota was truncated");
        require(extended ? decoded.exceptions.size() == 1 && decoded.exceptions[0].route == 7 && decoded.exceptions[0].hidden && decoded.exceptions[0].costs[1] == 1234 : decoded.exceptions.empty(), "Foreign route overrides were lost");
        bytes.pop_back();
        require(!augustus_save::decode_empire(fixture, decoded, diagnostic) && decoded.common.size() == 275, "Truncated empire published partial state");
    }
    for (int version : {175, 183, 184, 188, 189}) {
        AugustusArchive fixture;
        fixture.origin.family = ArchiveFamily::Augustus; fixture.origin.save_version = version;
        auto make_array = [&](const char *name, size_t count, size_t stride) -> std::vector<uint8_t> & {
            auto &bytes = fixture.pieces[name]; bytes.resize(16 + count * stride);
            augustus_save::write_u32(bytes, 0, static_cast<uint32_t>(bytes.size()));
            augustus_save::write_u32(bytes, 8, static_cast<uint32_t>(count));
            augustus_save::write_u32(bytes, 12, static_cast<uint32_t>(stride));
            return bytes;
        };
        auto &actions = make_array("scenario_actions", 1, 28);
        augustus_save::write_u16(actions, 22, 44);
        auto &formulas = make_array("scenario_formulas", 2, 118);
        augustus_save::write_u32(formulas, 16, 1); formulas[20] = '7';
        std::fill(formulas.begin() + 16 + 118, formulas.end(), uint8_t{0xcc});
        if (version >= 184) {
            auto &texts = make_array("scenario_texts", 2, 132);
            augustus_save::write_u32(texts, 16, 1); texts[20] = 'X';
            std::fill(texts.begin() + 16 + 132, texts.end(), uint8_t{0xdd});
        }
        augustus_save::ScenarioRecords decoded;
        std::string diagnostic;
        const bool accepted = augustus_save::decode_scenario_records(fixture, augustus_save::ActionOrder::Unspecified, decoded, diagnostic);
        require(accepted, "Known producer order or approved version-189 compatibility policy was rejected");
        require(decoded.assumed_lock_actions.size() == (version == 189 ? 1 : 0), "Version-189 action-44 warning obligation was omitted or applied to an unambiguous producer");
        require(augustus_save::read_u16(decoded.actions, 22) == (version >= 184 && version < 189 ? 45 : 44), "Foreign house/lock actions were not translated by producer order");
        require(decoded.formulas.size() == 134 && augustus_save::read_u32(decoded.formulas, 8) == 1 && decoded.formulas[20] == '7', "Reserved formula allocation slot became a phantom formula");
        require(decoded.texts.size() == (version >= 184 ? 1 : 0) && (version < 184 || decoded.texts.at(1) == "X"), "Foreign text identity or unused allocation tail was mishandled");
        const auto preserved = decoded.formulas;
        formulas.pop_back();
        require(!augustus_save::decode_scenario_records(fixture, augustus_save::ActionOrder::LockThenHouse, decoded, diagnostic) && decoded.formulas == preserved, "Invalid scenario arrays published partial output");
    }
    for (int version : {184, 189}) {
        AugustusArchive fixture;
        fixture.origin.family = ArchiveFamily::Augustus; fixture.origin.save_version = version;
        auto &bytes = fixture.pieces["monument_stages"];
        constexpr size_t stride = 4 + 6 * 22 * 4;
        bytes.resize(std::size(augustus_monument_defaults::entries) * stride);
        for (size_t index = 0; index < std::size(augustus_monument_defaults::entries); ++index) {
            const auto &entry = augustus_monument_defaults::entries[index];
            augustus_save::write_u32(bytes, index * stride, entry.phases);
            for (int phase = 0; phase < 6; ++phase) for (int resource = 0; resource < 22; ++resource) augustus_save::write_u32(bytes, index * stride + 4 + 4 * (phase * 22 + resource), entry.resources[phase][resource]);
        }
        std::string diagnostic;
        std::vector<augustus_save::ConstructionException> exceptions;
        require(augustus_save::decode_construction_exceptions(fixture, exceptions, diagnostic) && exceptions.empty(), "Default monument definitions were imported as authored overrides");
        augustus_save::write_u32(bytes, 6 * stride + 4 + 4 * (22 + 11), 13);
        require(augustus_save::decode_construction_exceptions(fixture, exceptions, diagnostic) && exceptions.size() == 1 && exceptions[0].building == "oracle" && exceptions[0].phase == 2 && exceptions[0].resource == 11 && exceptions[0].amount == 13, "Authored construction exception lost its building, phase or resource identity");
        bytes.pop_back();
        require(!augustus_save::decode_construction_exceptions(fixture, exceptions, diagnostic) && exceptions.size() == 1, "Truncated construction snapshot published partial overrides");
    }
    for (int version : {175, 183, 184, 185, 186, 189}) {
        AugustusArchive fixture;
        fixture.origin.family = ArchiveFamily::Augustus; fixture.origin.save_version = version; fixture.origin.resource_version = 5;
        auto &bytes = fixture.pieces["city_data"];
        bytes.resize(version >= 186 ? augustus_city_layout::current_size : augustus_city_layout::current_size - 44, 0);
        bytes[0] = 7; bytes[bytes.size() - 1] = 11;
        if (version >= 184) { bytes[augustus_city_layout::migration_percentages] = 75; bytes[augustus_city_layout::migration_percentages + 4] = 150; }
        if (version >= 185) { bytes[augustus_city_layout::extra_religions] = 17; bytes[augustus_city_layout::extra_religions + 4] = 23; }
        augustus_save::CityRecord decoded;
        std::string diagnostic;
        require(augustus_save::decode_city(fixture, decoded, diagnostic), "Foreign city field boundaries were not recognized");
        require(decoded.common.size() == augustus_city_layout::common_size && decoded.common[0] == 7 && decoded.common[decoded.common.size() - decoded.missing_tail_bytes - 1] == 11, "Foreign city fields shifted during normalization");
        require(decoded.immigration_percent == (version >= 184 ? 75 : 100) && decoded.fifth_religion == (version >= 185 ? 23 : 0), "Foreign scenario or housing additions were lost");
        require(decoded.missing_tail_bytes == (version >= 186 ? 0 : version >= 185 ? 44 : version >= 184 ? 36 : 28), "Foreign city truncation repair did not match its producer");
        bytes.pop_back();
        require(!augustus_save::decode_city(fixture, decoded, diagnostic) && decoded.common[0] == 7, "Invalid foreign city published partial state");
    }
    for (int version : {181, 182, 183, 189}) {
        AugustusArchive fixture;
        fixture.origin.family = ArchiveFamily::Augustus; fixture.origin.save_version = version;
        auto write32 = [](std::vector<uint8_t> &bytes, size_t offset, uint32_t value) {
            for (int byte = 0; byte < 4; ++byte) bytes[offset + byte] = static_cast<uint8_t>(value >> (8 * byte));
        };
        const int route_stride = 22 * 4 * 4 + (version >= 182);
        auto &route_bytes = fixture.pieces["trade_routes"];
        route_bytes.resize(4 + route_stride);
        write32(route_bytes, 0, 1); write32(route_bytes, 4 + 8, 25); write32(route_bytes, 4 + 12, 7);
        if (version >= 182) {
            route_bytes.back() = 1;
            auto &history = fixture.pieces["trade_history"];
            history.resize(1 + 7 * 4 + route_stride);
            history[0] = 1;
            std::copy(route_bytes.begin(), route_bytes.end(), history.begin() + 1);
            auto &ledger = fixture.pieces["finance_ledger"];
            const size_t transactions = 4 + 2 + 8 * (8 + 22 * 6 * 4);
            ledger.resize(transactions + 4 + 12 + 4 + (version >= 183 ? 1 + 6 * 15 * 4 : 0));
            write32(ledger, 0, static_cast<uint32_t>(ledger.size()));
            ledger[4] = 1;
            write32(ledger, transactions, 1);
            write32(ledger, transactions + 4, 17);
            ledger[transactions + 4 + 4] = 1; ledger[transactions + 4 + 7] = 11; ledger[transactions + 4 + 8] = 1;
            ledger[transactions + 4 + 9] = 0x34; ledger[transactions + 4 + 10] = 0x12; ledger[transactions + 4 + 11] = uint8_t(-3);
            if (version >= 183) { ledger[transactions + 20] = 1; write32(ledger, transactions + 21 + 14 * 4, 12345); }
        }
        augustus_save::AccountingArchive accounting;
        augustus_save::TradeRouteArchive routes;
        std::string diagnostic;
        require(augustus_save::decode_accounting(fixture, accounting, diagnostic), "Foreign finance schema did not decode");
        require(augustus_save::decode_trade_routes(fixture, routes, diagnostic), "Foreign route schema did not decode");
        require(routes.current.size() == 1 && routes.current[0].values[0][1] == 25 && routes.current[0].values[1][1] == 7, "Foreign route limit and traded values were transposed");
        if (version >= 182) {
            require(routes.years == 1 && routes.history[0][0].open && routes.history[0][0].values[1][1] == 7, "Foreign route history was flattened into current routes");
            require(accounting.transactions[0].size() == 1 && accounting.transactions[0][0].trader == 0x1234 && accounting.transactions[0][0].quantity == -3, "Foreign transaction width or direction was lost");
            if (version >= 183) require(accounting.finance_years == 1 && accounting.finances[0][14] == 12345, "Foreign finance overview was lost");
            fixture.pieces["finance_ledger"].pop_back();
            require(!augustus_save::decode_accounting(fixture, accounting, diagnostic) && accounting.transactions[0][0].trader == 0x1234, "Failed accounting decode published partial state");
        }
        route_bytes.pop_back();
        require(!augustus_save::decode_trade_routes(fixture, routes, diagnostic) && routes.current[0].values[1][1] == 7, "Failed route decode published partial state");
    }
    for (const auto &baseline : augustus_model_defaults::candidates) {
        AugustusArchive models;
        models.origin.family = ArchiveFamily::Augustus;
        models.origin.save_version = baseline.version;
        const size_t prefix = baseline.version >= 187 ? 8 : 0;
        const size_t building_bytes = baseline.building_count * 24, house_bytes = baseline.version >= 184 ? 20 * 17 * 4 : 0;
        auto &bytes = models.pieces["model_data"];
        bytes.resize(prefix + building_bytes + house_bytes);
        auto write = [&](size_t offset, int value) {
            for (int byte = 0; byte < 4; ++byte) bytes[offset + byte] = static_cast<uint8_t>(static_cast<uint32_t>(value) >> (8 * byte));
        };
        if (prefix) { write(0, static_cast<int>(building_bytes)); write(4, static_cast<int>(house_bytes)); }
        for (int model = 0; model < baseline.building_count; ++model) for (int field = 0; field < 6; ++field) write(prefix + 24 * model + 4 * field, baseline.buildings[model][field]);
        if (house_bytes) for (int model = 0; model < 20; ++model) for (int field = 0; field < 17; ++field) write(prefix + building_bytes + 68 * model + 4 * field, baseline.houses[model][field]);
        std::vector<augustus_save::ModelException> exceptions;
        std::string diagnostic;
        require(augustus_save::decode_model_exceptions(models, exceptions, diagnostic) && exceptions.empty(), "Foreign class defaults became scenario overrides");
        write(prefix + 31 * 24 + 5 * 4, 11);
        require(augustus_save::decode_model_exceptions(models, exceptions, diagnostic) && exceptions.size() == 1 && exceptions[0].target == "theater" && exceptions[0].field == 5 && exceptions[0].value == 11, "Authored theater requirement was not recovered independently of source defaults");
        if (house_bytes) {
            write(prefix + building_bytes + 3 * 4, 3);
            require(augustus_save::decode_model_exceptions(models, exceptions, diagnostic) && exceptions.size() == 2 && exceptions[1].housing && exceptions[1].value == 2, "Source fountain requirement did not map to the native fountain value");
            write(prefix + building_bytes + 3 * 4, 2);
            require(augustus_save::decode_model_exceptions(models, exceptions, diagnostic) && exceptions.size() == 2 && exceptions[1].value == 3, "Source latrine-or-fountain requirement did not retain its distinct meaning");
        }
        bytes.pop_back();
        require(!augustus_save::decode_model_exceptions(models, exceptions, diagnostic) && !exceptions.empty() && exceptions[0].value == 11, "Truncated foreign model payload published partial exceptions");
    }
    {
        AugustusArchive fixture;
        fixture.origin.family = ArchiveFamily::Augustus;
        fixture.origin.save_version = 189;
        fixture.origin.resource_version = 5;
        auto &payload = fixture.pieces["buildings"];
        payload.assign(4 + 3 * 209, 0);
        payload[0] = 209;
        payload[4 + 201] = 1;
        payload[4 + 201 + 10] = 10;
        payload[4 + 201 + 99] = 0x34;
        payload[4 + 201 + 100] = 0x12;
        payload[4 + 201 + 202] = 7;
        payload[4 + 201 + 202 + 10] = 211;
        std::fill(payload.begin() + 4 + 201 + 202 + 201, payload.end(), uint8_t{0xcc});
        std::vector<augustus_save::BuildingRecord> records;
        std::string diagnostic;
        require(augustus_save::index_buildings(fixture, records, diagnostic), "Foreign sequential building fixture failed");
        require(records.size() == 3 && records[1].evolution_text == 0x1234 && records[2].type == 211 && records[2].state == 7, "Foreign building allocation stride shifted a live record");
        const auto preserved = records;
        payload.pop_back();
        require(!augustus_save::index_buildings(fixture, records, diagnostic) && records.size() == preserved.size() && records[2].offset == preserved[2].offset, "Invalid foreign record indexing published partial output");
        auto &figures = fixture.pieces["figures"];
        figures.assign(4 + 2 * 171, 0);
        figures[0] = 171;
        figures[4 + 157 + 14] = 98;
        figures[4 + 157 + 18] = 1;
        figures[4 + 157 + 84] = 123;
        figures[4 + 157 + 127] = 0x34;
        figures[4 + 157 + 128] = 0x12;
        std::fill(figures.begin() + 4 + 2 * 157, figures.end(), uint8_t{0xcc});
        std::vector<augustus_save::FigureRecord> figure_records;
        require(augustus_save::index_figures(fixture, figure_records, diagnostic), "Foreign sequential figure fixture failed");
        require(figure_records.size() == 2 && figure_records[1].type == 98 && figure_records[1].owner == 123 && figure_records[1].trader == 0x1234, "Foreign figure allocation stride or widened trader identity shifted a record");
        payload.push_back(0xcc);
        augustus_save::CommonRecords common;
        require(augustus_save::decode_common_records(fixture, [](const char *name) { return std::string(name) == "dog" ? 223 : 0; }, common, diagnostic), "Foreign common record translation failed");
        require(common.buildings.size() == 4 + 3 * 201 && common.buildings[4 + 201 + 99] == 0 && common.buildings[4 + 402] == 7 && common.buildings[4 + 402 + 10] == 211, "House evolution byte shifted the following building");
        require(common.figures[4 + 156 + 14] == 223 && common.figures[4 + 156 + 84] == 123 && common.figures[4 + 156 + 127] == 0 && common.source_figures[1].trader == 0x1234 && common.invalid_trader_references.empty(), "Foreign figure identity or unused trader payload was mistranslated");
        const auto previous_common = common.figures;
        require(!augustus_save::decode_common_records(fixture, [](const char *) { return 0; }, common, diagnostic) && common.figures == previous_common, "Missing figure binding published partial translated records");
        figures[4 + 157 + 14] = 19;
        require(augustus_save::decode_common_records(fixture, [](const char *) { return 0; }, common, diagnostic) && common.invalid_trader_references == std::vector<size_t>{1}, "Invalid live trader reference was truncated without retaining its repair obligation");
        figures[4 + 157 + 14] = 98;
        auto &terrain = fixture.pieces["terrain_grid"];
        terrain.assign(162 * 162 * 4, 0); terrain[2] = 0x20;
        std::set<augustus_save::RequiredDefinition> requirements;
        require(augustus_save::collect_required_definitions(fixture, requirements, diagnostic) && requirements.size() == 3, "Foreign preflight lost station, dog or shallow-water identity");
        require(!augustus_save::validate_required_definitions(requirements, [](const auto &) { return false; }, diagnostic) && diagnostic.find("highway_station") != std::string::npos && diagnostic.find("clear_trees") == std::string::npos && diagnostic.find("dog") != std::string::npos && diagnostic.find("shallow_water") != std::string::npos, "Missing owning-mod preflight reported native numeric identities");
        require(augustus_save::validate_required_definitions(requirements, [](const auto &) { return true; }, diagnostic), "Available foreign content failed preflight");
        payload[4 + 201 + 202 + 10] = 56; payload[4 + 201 + 202 + 118] = 2;
        require(augustus_save::collect_required_definitions(fixture, requirements, diagnostic) && requirements.contains({augustus_save::DefinitionKind::PhasedConstruction, "triumphal_arch"}), "Unfinished foreign arch did not require phased construction");
        const auto preserved_requirements = requirements;
        terrain[3] = 0x80;
        require(!augustus_save::collect_required_definitions(fixture, requirements, diagnostic) && requirements.size() == preserved_requirements.size(), "Invalid foreign terrain published a partial import plan");
    }
    auto check_foreign = [&](int version, int scenario_version, auto visitor) {
        std::vector<uint8_t> bytes;
        auto write = [&](uint32_t value) { for (int shift = 0; shift < 32; shift += 8) bytes.push_back(static_cast<uint8_t>(value >> shift)); };
        visitor([&](const char *name, int size, int compressed) {
            if (!size) { write(0); return; }
            if (compressed) write(0x80000000);
            if (std::string(name) == "file_version") write(version);
            else if (std::string(name) == "resource_version") write(5);
            else if (std::string(name) == "scenario_version") write(scenario_version);
            else bytes.resize(bytes.size() + size);
        });
        const auto result = game_file_io_identify_archive(bytes.data(), bytes.size());
        require(result.status == ArchiveIdentification::Identified && result.family == ArchiveFamily::Augustus, "Foreign archive was not distinguished from a colliding native version");
        require(result.save_version == version && result.resource_version == 5 && result.scenario_version == scenario_version, "Source schema metadata was lost");
        AugustusArchive decoded;
        std::string diagnostic;
        if (!game_file_io_decode_augustus_archive(bytes.data(), bytes.size(), decoded, diagnostic)) throw std::runtime_error("Foreign payload decoding failed: " + diagnostic);
        require(decoded.origin.save_version == version && decoded.pieces.count("figures") && decoded.pieces.count("buildings"), "Decoded foreign archive omitted named payloads");
        require(game_file_io_identify_archive(bytes.data(), bytes.size(), ArchiveFamily::Vespasian).status == ArchiveIdentification::Invalid, "Incorrect explicit origin was accepted");
        bytes.push_back(1);
        require(game_file_io_identify_archive(bytes.data(), bytes.size()).status == ArchiveIdentification::Invalid, "Trailing archive data was silently accepted");
    };
    check_foreign(0xb0, 22, [](auto emit) { augustus_archive_layouts::layout_0::visit(0xb0, 22, 6, emit); });
    check_foreign(0xb7, 22, [](auto emit) { augustus_archive_layouts::layout_1::visit(0xb7, 22, 6, emit); });
    check_foreign(0xb8, 23, [](auto emit) { augustus_archive_layouts::layout_2::visit(0xb8, 22, 6, emit); });
    check_foreign(0xba, 23, [](auto emit) { augustus_archive_layouts::layout_3::visit(0xba, 22, 6, emit); });
    check_foreign(0xbd, 26, [](auto emit) { augustus_archive_layouts::layout_4::visit(0xbd, 22, 6, emit); });
    struct Temporary {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("vespasian-origin-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svx");
        ~Temporary() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } temporary;
    require(game_file_io_write_saved_game(temporary.path.string().c_str()) != 0, "Could not write native archive with foreign extension");
    std::ifstream stream(temporary.path, std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
    const auto result = game_file_io_identify_archive(bytes.data(), bytes.size());
    require(result.status == ArchiveIdentification::Identified && result.family == ArchiveFamily::Vespasian, "Renamed native archive lost its family");
    saved_game_info info{};
    require(game_file_io_read_saved_game_info(temporary.path.string().c_str(), 0, &info) == SAVEGAME_STATUS_OK, "Native preview rejected the foreign extension");
    for (size_t cut : {size_t(0), size_t(7), size_t(11), bytes.size() / 2, bytes.size() - 1}) {
        require(game_file_io_identify_archive(bytes.data(), cut).status == ArchiveIdentification::Invalid, "Truncated native archive was identified as valid");
    }
    require(city_population() == population && city_finance_treasury() == treasury && resource_mapping_get_version() == mapping, "Archive identification mutated live city state");
    std::fprintf(stdout, "Archive origin contracts passed: five foreign layouts, collisions, renamed native preview, explicit mismatch, truncation, trailing data.\n");
}

inline bool validate_foreign_archive_file(const char *filename, bool compare_runtime = false)
{
    const int population = city_population(), treasury = city_finance_treasury(), mapping = resource_mapping_get_version();
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) { std::fprintf(stderr, "Cannot read foreign archive fixture: %s\n", filename); return false; }
    const std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), {});
    AugustusArchive archive;
    std::string diagnostic;
    if (!game_file_io_decode_augustus_archive(bytes.data(), bytes.size(), archive, diagnostic)) {
        std::fprintf(stderr, "Foreign archive decode failed: %s\n", diagnostic.c_str()); return false;
    }
    for (const char *name : {"figures", "buildings", "formations", "city_data", "scenario", "terrain_grid"}) {
        const auto found = archive.pieces.find(name);
        if (found == archive.pieces.end() || found->second.empty()) { std::fprintf(stderr, "Foreign city fixture lacks %s\n", name); return false; }
        std::fprintf(stdout, "Foreign payload %s: %zu bytes\n", name, found->second.size());
    }
    std::vector<augustus_save::BuildingRecord> records;
    if (!augustus_save::index_buildings(archive, records, diagnostic)) {
        std::fprintf(stderr, "Foreign building decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign sequential building records verified: %zu records.\n", records.size());
    std::vector<augustus_save::FigureRecord> figures;
    if (!augustus_save::index_figures(archive, figures, diagnostic)) {
        std::fprintf(stderr, "Foreign figure decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign sequential figure records verified: %zu records.\n", figures.size());
    std::vector<augustus_save::ModelException> model_exceptions;
    if (!augustus_save::decode_model_exceptions(archive, model_exceptions, diagnostic)) {
        std::fprintf(stderr, "Foreign model exception decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign authored model exceptions decoded: %zu; source defaults remain excluded.\n", model_exceptions.size());
    augustus_save::AccountingArchive accounting;
    augustus_save::TradeRouteArchive routes;
    if (!augustus_save::decode_accounting(archive, accounting, diagnostic) || !augustus_save::decode_trade_routes(archive, routes, diagnostic)) {
        std::fprintf(stderr, "Foreign accounting decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign accounting decoded: %zu current routes, %u history years, %zu current transactions.\n", routes.current.size(), routes.years, accounting.transactions[0].size());
    if (compare_runtime && archive.origin.save_version >= 182 && !validate_imported_state(model_exceptions, accounting)) return false;
    augustus_save::CityRecord city;
    if (!augustus_save::decode_city(archive, city, diagnostic)) {
        std::fprintf(stderr, "Foreign city decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign city fields decoded: %zu common bytes, %zu missing tail bytes.\n", city.common.size(), city.missing_tail_bytes);
    augustus_save::ScenarioRecords scenario_records;
    if (!augustus_save::decode_scenario_records(archive, augustus_save::ActionOrder::Unspecified, scenario_records, diagnostic)) {
        std::fprintf(stderr, "Foreign scenario record decode failed: %s\n", diagnostic.c_str()); return false;
    }
    std::fprintf(stdout, "Foreign scenario records decoded: %zu actions, %zu authored formulas, %zu texts.\n", (scenario_records.actions.size() - 16) / 28, (scenario_records.formulas.size() - 16) / 118, scenario_records.texts.size());
    AugustusArchive unchanged = archive;
    if (game_file_io_decode_augustus_archive(bytes.data(), bytes.size() - 1, archive, diagnostic) || archive.pieces != unchanged.pieces) {
        std::fprintf(stderr, "Failed foreign decode published partial output\n"); return false;
    }
    if (city_population() != population || city_finance_treasury() != treasury || resource_mapping_get_version() != mapping) {
        std::fprintf(stderr, "Foreign archive inspection mutated the loaded city\n"); return false;
    }
    std::fprintf(stdout, "Foreign archive structural decode passed: version=%d scenario=%d pieces=%zu; runtime comparison runs only for the matching loaded source.\n", archive.origin.save_version, archive.origin.scenario_version, archive.pieces.size());
    return true;
}

inline bool validate_load_transaction()
{
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    struct RestoreDemands { house_demands saved = *city_houses_demands(); ~RestoreDemands() { *city_houses_demands() = saved; } } restore_demands;
    city_houses_demands()->missing.fourth_religion = 17;
    city_houses_demands()->missing.fifth_religion = 9;
    std::vector<uint8_t> archive;
    std::vector<SaveSnapshotPiece> pieces;
    require(game_file_io_snapshot(archive, &pieces), "Could not create the load-transaction fixture");
    if (std::getenv("VESPASIAN_SNAPSHOT_LAYOUT")) {
        std::ofstream layout(std::getenv("VESPASIAN_SNAPSHOT_LAYOUT"));
        for (const auto &piece : pieces) layout << piece.offset << " " << piece.size << " " << piece.name << "\n";
    }
    // Corrupt the explicit model payload, keeping its framing intact. This must
    // reach a late semantic failure after figures, buildings and grids were read.
    const uint8_t signature[] = {'V', 'M', 'O', '3'};
    auto marker = std::search(archive.begin(), archive.end(), std::begin(signature), std::end(signature));
    require(marker != archive.end() && std::search(marker + 4, archive.end(), std::begin(signature), std::end(signature)) == archive.end(), "Model payload fixture is not uniquely identifiable");
    *marker = '!';
    require(game_file_io_identify_archive(archive.data(), archive.size()).status == ArchiveIdentification::Identified, "Semantic failure fixture damaged archive framing");
    struct Temporary {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("vespasian-load-transaction-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svv");
        ~Temporary() { std::error_code ignored; std::filesystem::remove(path, ignored); }
    } temporary;
    { std::ofstream stream(temporary.path, std::ios::binary); stream.write(reinterpret_cast<const char *>(archive.data()), archive.size()); require(bool(stream), "Could not write semantic failure fixture"); }
    const int population = city_population(), treasury = city_finance_treasury(), buildings = Building::count();
    const auto figures = Figure::count();
    const int year = game_time_year(), month = game_time_month(), day = game_time_day(), tick = game_time_tick();
    const int width = map_grid_width(), height = map_grid_height(), speed = setting_game_speed();
    const int paused = game_state_is_paused(), overlay = game_state_overlay();
    std::vector<TerrainSet> terrain;
    for (int tile = 0; tile < GRID_SIZE * GRID_SIZE; ++tile) terrain.push_back(terrain_map().at(tile));
    game_state_pause(); game_state_set_overlay(OVERLAY_FIRE);
    const int result = game_file_load_saved_game(temporary.path.string().c_str());
    require(result != FILE_LOAD_SUCCESS, "Late semantic failure was accepted");
    require(city_population() == population && city_finance_treasury() == treasury && Building::count() == buildings && Figure::count() == figures, "Rejected load changed the live city's population, treasury or object counts");
    require(game_time_year() == year && game_time_month() == month && game_time_day() == day && game_time_tick() == tick, "Rejected load changed simulation time");
    require(map_grid_width() == width && map_grid_height() == height && setting_game_speed() == speed && game_state_is_paused() && game_state_overlay() == OVERLAY_FIRE, "Rejected load changed the map or view settings");
    for (int tile = 0; tile < GRID_SIZE * GRID_SIZE; ++tile) require(terrain_map().at(tile) == terrain[tile], "Rejected load changed a terrain tile");
    game_state_set_overlay(overlay); if (paused) game_state_pause(); else game_state_unpause();
    *marker = 'V';
    std::vector<uint8_t> restored_archive;
    require(game_file_io_snapshot(restored_archive), "Could not inspect the restored city");
    if (archive != restored_archive) {
        const auto shared = std::min(archive.size(), restored_archive.size());
        std::size_t first = 0;
        while (first < shared && archive[first] == restored_archive[first]) ++first;
        std::fprintf(stderr, "Restored archive differs at byte %zu (before=%zu bytes after=%zu bytes)\n", first, archive.size(), restored_archive.size());
        for (const auto &piece : pieces) {
            if (piece.offset + piece.size > shared) continue;
            std::size_t differences = 0, first_difference = piece.size;
            for (std::size_t at = 0; at < piece.size; ++at) if (archive[piece.offset + at] != restored_archive[piece.offset + at]) {
                ++differences; first_difference = std::min(first_difference, at);
            }
            if (differences) std::fprintf(stderr, "Piece %s changed: offset=%zu size=%zu first=%zu count=%zu\n", piece.name.c_str(), piece.offset, piece.size, first_difference, differences);
        }
        for (const auto &entry : {std::make_pair(".before", &archive), std::make_pair(".after", &restored_archive)}) {
            const auto path = temporary.path.string() + entry.first;
            std::ofstream stream(path, std::ios::binary);
            stream.write(reinterpret_cast<const char *>(entry.second->data()), entry.second->size());
            std::fprintf(stderr, "Rollback inspection archive: %s\n", path.c_str());
        }
        throw std::runtime_error("Rejected load altered serialized city state");
    }
    std::fprintf(stdout, "Load transaction restored the previous city after a late model-payload failure; all terrain tiles, object counts, finances, time and view settings agree.\n");
    game_file_clear_scenario_data_for_save_load();
    const int empty_buildings = Building::count(), empty_population = city_population();
    const auto empty_figures = Figure::count();
    require(game_file_load_saved_game(temporary.path.string().c_str()) != FILE_LOAD_SUCCESS, "Cold semantic failure was accepted");
    require(Building::count() == empty_buildings && Figure::count() == empty_figures && city_population() == empty_population, "Cold failed load retained partial objects or population");
    for (int tile = 0; tile < GRID_SIZE * GRID_SIZE; ++tile) require(terrain_map().at(tile).empty(), "Cold failed load retained terrain");
    { std::ofstream stream(temporary.path, std::ios::binary); stream.write(reinterpret_cast<const char *>(archive.data()), archive.size()); require(bool(stream), "Could not write recovery fixture"); }
    require(game_file_load_saved_game(temporary.path.string().c_str()) == FILE_LOAD_SUCCESS, "Clean city could not load after a cold failure");
    require(city_population() == population && city_finance_treasury() == treasury && Building::count() == buildings && Figure::count() == figures, "Cold failure polluted the next loaded city");
    require(city_houses_demands()->missing.fourth_religion == 17 && city_houses_demands()->missing.fifth_religion == 9, "Native roundtrip lost fourth/fifth religion demands");
    setting_set_game_speed(speed);
    game_state_set_overlay(overlay); if (paused) game_state_pause(); else game_state_unpause();
    std::fprintf(stdout, "Cold failed load discarded all terrain and objects; the following clean load restored the original city.\n");
    return true;
}
