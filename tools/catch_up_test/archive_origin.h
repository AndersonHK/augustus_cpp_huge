#pragma once
#include "game/archive_origin.h"
#include "game/augustus_archive_layouts.generated.h"
#include "game/file_io.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include <cstdio>
#include "city/population.h"
#include "city/finance.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>

inline void validate_archive_origins()
{
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    const int population = city_population(), treasury = city_finance_treasury();
    const int mapping = resource_mapping_get_version();
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
