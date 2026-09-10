#pragma once
#include "building/construction_building.h"
#include "building/building_record.h"
#include "building/construction.h"
#include "building/rotation.h"
#include "building/BuildingComposition.h"
#include "building/BuildingGraphics.h"
#include "game/file.h"
#include "game/file_io.h"
#include "map/figure.h"
#include "map/building.h"
#include "widget/city_overlay_other.h"
#include "widget/city_overlay_risks.h"
#include <filesystem>
#include <chrono>
#include <stdexcept>

inline void validate_fort_orientation()
{
    using namespace building_type_registry_impl;
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    const auto *fort = definition_for_type(type_from_attr("fort_legionaries"));
    require(fort && fort->has_composition(), "Fort rotation fixture requires a composed fort");
    const auto prefix = std::filesystem::temp_directory_path() / ("vespasian-fort-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto original = prefix.string() + "-original.svv", rotated = prefix.string() + "-rotated.svv";
    require(game_file_io_write_saved_game(original.c_str()) != 0, "Could not checkpoint fort rotation fixture");
    auto restore = [&]() { return game_file_load_saved_game(original.c_str()) == FILE_LOAD_SUCCESS; };
    auto clean = [&]() { std::error_code ignored; std::filesystem::remove(original, ignored); std::filesystem::remove(rotated, ignored); };
    try {
        for (int rotation = 0; rotation < 4; ++rotation) {
            building_construction_set_type(fort, 0);
            building_rotation_setup_rotation(rotation);
            int x = -1, y = -1;
            for (int yy = 6; yy < map_grid_height() - 7 && x < 0; ++yy) for (int xx = 6; xx < map_grid_width() - 7; ++xx) {
                building_construction::ConstructionPlacementPlan plan(*fort, xx, yy, 1, 0, rotation, nullptr, rotation, false, true);
                if (!plan.can_place()) continue;
                bool occupied = false;
                for (const auto &part : plan.parts()) for (const auto &tile : part.tiles) if (map_figure_at(tile.grid_offset)) occupied = true;
                if (!occupied) { x = xx; y = yy; break; }
            }
            require(x >= 0, "Fort rotation fixture needs a clear composed footprint");
            // Exercise the shared publication kernel; this fixture does not
            // require the player's city to have a mess hall or a spare legion.
            const building_construction::ConstructionPlacementPlan placement(*fort, x, y, 1, 0, rotation, nullptr, rotation, false, true);
            require(building_construction_publish_placement_batch({placement}) != 0, "Rotated fort placement failed");
            const unsigned int id = map_building_at(map_grid_offset(x, y)).id;
            auto inspect = [&]() {
                require(Building::get(id) != nullptr, "Rotated fort disappeared");
                Building &building = *Building::get(id);
                require(building.type == fort && building.orientation() == rotation && building.Graphics().rotation() == rotation, "Fort placement/clone orientation aliases soldier type");
                require(building.fort_figure_type() == fort->military().primary_figure_type(), "Fort rotation changed soldier type");
                require(building.Composition && building.Composition->complete(), "Fort rotation lost its composed grounds");
                auto *record = const_cast<::building *>(building.record());
                const auto state = record->state;
                record->state = BUILDING_STATE_MOTHBALLED;
                for (const auto *child : building.Composition->children()) {
                    require(city_overlay_for_mothball()->show_building(child->building()->record()), "Composition child ignored its mothballed owner");
                    require(city_overlay_for_employment()->get_column_height(child->building()->record()) == NO_COLUMN, "Composition child displayed a duplicate employment column");
                    tooltip_context tooltip{};
                    require(city_overlay_for_employment()->get_tooltip_for_building(&tooltip, child->building()->record()) == 0, "Composition child displayed a duplicate employment tooltip");
                }
                record->state = state;
            };
            inspect();
            require(game_file_io_write_saved_game(rotated.c_str()) != 0 && game_file_load_saved_game(rotated.c_str()) == FILE_LOAD_SUCCESS, "Rotated fort save roundtrip failed");
            inspect();
            require(restore(), "Could not restore the city after fort rotation validation");
        }
    } catch (...) { restore(); clean(); throw; }
    clean();
    Figure standard; standard.type = FIGURE_FORT_STANDARD;
    require(city_overlay_for_enemy()->show_figure(&standard), "Enemy overlay hid the data-declared formation standard");
    std::fprintf(stdout, "Fort rotation contracts passed: all four placement/clone orientations, definition-owned soldiers and native save roundtrips.\n");
}
