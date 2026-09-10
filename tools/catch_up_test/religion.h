#pragma once
#include "building/religion_effects.h"
#include "building/god_registry.h"
#include "city/god.h"
#include "city/data_private.h"
#include "city/finance.h"
#include "core/buffer.h"
#include "game/file.h"
#include "game/file_io.h"
#include "game/time.h"
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include <stdexcept>

inline void city_gods_validate_effects()
{
    const auto original = city_data.religion;
    struct Restore { decltype(city_data.religion) state; ~Restore() { city_data.religion = state; } } restore{original};
    God thor("thor"); thor.set_legacy_type(GOD_CERES);
    thor.effects.push_back(religion::parse_effect(mod_content::parse("<effect><action callback='trade_bonus' months='7' percent='75'/><action callback='employment' months='30' months_per_point='10' base_bonus='2'/><action callback='military_protection' power='27'/></effect>"), religion::Trigger::Blessing));
    auto context = religion::context_for_god(thor.legacy_type());
    religion::apply_effect(thor.effects.front(), context, false);
    if (city_data.religion.neptune_trade_bonus_active != 7 || city_data.religion.trade_bonus_percent != 75 || city_god_venus_bonus_employment() != 5 || city_god_spirit_of_mars_power() != 27) throw std::runtime_error("Religion callbacks depend on deity identity or ignore their parameters");
    unsigned char bytes[12]{}; buffer archive; buffer_init(&archive, bytes, sizeof(bytes));
    religion::save_state(&archive);
    city_data.religion.trade_bonus_percent = 0; city_data.religion.employment_base_bonus = 0;
    buffer_reset(&archive);
    if (!religion::load_state(&archive) || city_data.religion.trade_bonus_percent != 75 || city_god_venus_bonus_employment() != 5) throw std::runtime_error("Active religion effect payload did not roundtrip");
    city_gods_update_blessings();
    if (city_data.religion.neptune_trade_bonus_active != 6 || city_data.religion.venus_blessing_months_left != 29) throw std::runtime_error("Religion effect duration did not advance");
    if (!religion::load_state(nullptr) || city_data.religion.trade_bonus_percent != 50 || city_data.religion.employment_months_per_point != 12) throw std::runtime_error("Legacy active religion magnitudes were not recovered");
    city_data.religion.neptune_trade_bonus_active = 1;
    religion::import_original_effects();
    if (city_data.religion.trade_bonus_percent != 100 || city_data.religion.neptune_trade_bonus_active != 12 - game_time_month()) throw std::runtime_error("Original year-end trade blessing flag was not recovered");
    std::fprintf(stdout, "Religion effect contracts passed: alternate deity, parameterized callbacks, active payload roundtrip, legacy defaults, duration countdown.\n");
}

inline void validate_religion_callbacks_in_city()
{
    city_gods_validate_effects();
    const auto snapshot = std::filesystem::temp_directory_path() / ("vespasian-religion-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svv");
    const auto path = snapshot.string();
    if (!game_file_io_write_saved_game(path.c_str())) throw std::runtime_error("Could not checkpoint religion test city");
    std::exception_ptr failure;
    try {
        for (int god = 0; god < GOD_ALL; ++god) {
            city_god_blessing(god);
            city_god_curse(god, 0);
            city_god_curse(god, 1);
        }
        // Check the real accounting consumer, not just the callback's stored number.
        city_data.religion.neptune_trade_bonus_active = 2;
        city_data.religion.trade_bonus_percent = 75;
        const auto before = city_data.finance.treasury;
        city_finance_process_export(100);
        if (city_data.finance.treasury != before + 175) throw std::runtime_error("Export consumer ignored the effect percentage");
    } catch (...) { failure = std::current_exception(); }
    if (game_file_load_saved_game(path.c_str()) != FILE_LOAD_SUCCESS) throw std::runtime_error("Religion test city recovery failed: " + path);
    std::filesystem::remove(snapshot);
    if (failure) std::rethrow_exception(failure);
    std::fprintf(stdout, "All active gods' blessings and both curse severities executed; city restored, export percentage verified.\n");
}

inline void validate_religion_payload_repair()
{
    std::vector<uint8_t> original;
    std::vector<SaveSnapshotPiece> pieces;
    if (!game_file_io_snapshot(original, &pieces)) throw std::runtime_error("Could not snapshot religion repair fixture");
    const auto piece = std::find_if(pieces.begin(), pieces.end(), [](const auto &entry) { return entry.name == "religion_effect_payload"; });
    if (piece == pieces.end() || piece->size != 12) throw std::runtime_error("Missing active religion payload");
    auto corrupted = original;
    for (size_t index = 0; index < 12; ++index) corrupted[piece->offset + index] = 0xff;
    const auto directory = std::filesystem::temp_directory_path() / ("vespasian-religion-repair-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto checkpoint = (directory / "checkpoint.svv").string();
    const auto damaged = (directory / "damaged.svv").string();
    const auto repaired = (directory / "repaired.svv").string();
    auto write = [](const std::string &path, const std::vector<uint8_t> &bytes) {
        std::ofstream output(path, std::ios::binary);
        output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
        if (!output) throw std::runtime_error("Could not write religion repair fixture");
    };
    write(checkpoint, original); write(damaged, corrupted);
    std::exception_ptr failure;
    try {
        if (game_file_load_saved_game(damaged.c_str()) != FILE_LOAD_SUCCESS) throw std::runtime_error("Recoverable religion payload stranded a usable save");
        auto repaired_values = [] { return city_data.religion.trade_bonus_percent == 50 && city_data.religion.employment_months_per_point == 12 && city_data.religion.employment_base_bonus == 1; };
        if (!repaired_values()) throw std::runtime_error("Religion payload did not repair its invalid fields");
        if (!game_file_io_write_saved_game(repaired.c_str()) || game_file_load_saved_game(repaired.c_str()) != FILE_LOAD_SUCCESS || !repaired_values()) throw std::runtime_error("Religion payload repair did not survive serialization");
    } catch (...) { failure = std::current_exception(); }
    if (game_file_load_saved_game(checkpoint.c_str()) != FILE_LOAD_SUCCESS) throw std::runtime_error("Religion repair test could not restore " + checkpoint);
    std::filesystem::remove(checkpoint); std::filesystem::remove(damaged); std::filesystem::remove(repaired); std::filesystem::remove(directory);
    if (failure) std::rethrow_exception(failure);
    std::fprintf(stdout, "Religion payload repair passed: three bad magnitudes repaired, clean re-save/reload, original city restored.\n");
}
