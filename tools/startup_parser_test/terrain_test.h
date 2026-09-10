#pragma once
#include "map/TerrainRegistry.h"
#include "map/TerrainSaveBridge.h"
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ostream>
#include <sstream>

inline bool validate_terrain_contract(std::ostream &errors)
{
    struct Fixture {
        std::filesystem::path path = std::filesystem::temp_directory_path() / ("vespasian_terrain_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TerrainRegistry previous = std::move(terrain_registry());
        Fixture() { std::filesystem::create_directories(path / "Terrain-Types"); }
        ~Fixture() { terrain_save::reset(); terrain_registry() = std::move(previous); std::error_code error; std::filesystem::remove_all(path, error); }
    } fixture;
    const auto declaration = [](const std::string &name, const TerrainTraits &t, const std::string &extra = "") {
        std::ostringstream xml;
        xml << std::boolalpha << "<terrain text_id=\"" << name << "\"><traversal land=\"" << t.land << "\" sea=\"" << t.sea
            << "\" enemy=\"" << t.enemy << "\" herd=\"" << t.herd << "\" earthquake=\"" << t.earthquake << "\" water=\"" << t.water
            << "\"/><placement blocking=\"" << t.blocks_construction << "\" clearable=\"" << t.clearable << "\" editable=\"" << t.editable
            << "\" paintable=\"" << t.paintable << "\"/>" << extra << "</terrain>";
        return xml.str();
    };
    const auto write = [&](const std::string &name, const std::string &xml) { std::ofstream file(fixture.path / "Terrain-Types" / (name + ".xml")); file << xml; return file.good(); };
    for (const auto &[name, terrain] : fixture.previous.definitions()) {
        TerrainTraits t{terrain->allows_land(), terrain->allows_sea(), terrain->allows_enemy(), terrain->allows_herd(), terrain->allows_earthquake(), terrain->is_water(), terrain->blocks_construction(), terrain->clearable(), terrain->editable(), terrain->paintable()};
        if (!write(name, declaration(name, t))) return false;
    }
    for (int i = 0; i < 100; ++i) if (!write("custom_" + std::to_string(i), declaration("custom_" + std::to_string(i), {}))) return false;
    TerrainTraits build_only, walk_only;
    build_only.land = build_only.enemy = false;
    walk_only.blocks_construction = true;
    if (!write("custom_0", declaration("custom_0", build_only)) || !write("custom_1", declaration("custom_1", walk_only))) return false;
    if (!write("custom_99", declaration("custom_99", {}, "<includes terrain=\"custom_98\"/>"))) return false;
    std::string failure;
    const std::vector<mod_definition::DefinitionLayer> layers{{"Terrain fixture", fixture.path.string()}};
    if (!terrain_registry().load(layers, failure)) { errors << failure << '\n'; return false; }
    TerrainSet every;
    for (const auto &[name, terrain] : terrain_registry().definitions()) every.add(*terrain);
    const auto &last = terrain_registry().require("custom_99", "test");
    const auto &previous = terrain_registry().require("custom_98", "test");
    const auto &buildable = terrain_registry().require("custom_0", "test");
    const auto &walkable = terrain_registry().require("custom_1", "test");
    const auto &types = terrain_types();
    if (!types.impassable.contains(buildable) || !types.impassable_enemy.contains(buildable) || types.not_clear.contains(buildable) ||
        types.impassable.contains(walkable) || types.impassable_enemy.contains(walkable) || !types.not_clear.contains(walkable)) {
        errors << "Terrain traversal and construction policies must bind independently.\n";
        return false;
    }
    if (every.entries().size() < 100 || !every.contains(last) || !last.includes().contains(previous)) { errors << "Arbitrary terrain count or startup binding failed.\n"; return false; }
    terrain_save::prepare();
    const auto id = terrain_save::encode(every);
    buffer ledger{};
    terrain_save::write_ledger(&ledger);
    const bool loaded = terrain_save::load_ledger(&ledger, true);
    free(ledger.data);
    if (!loaded || terrain_save::decode(id) != every) { errors << "Terrain reference ledger lost membership above 64 definitions.\n"; return false; }
    const std::string permuted = "terrain-ledger\t1\nterrain\t901\tcustom_99\nterrain\t17\tcustom_98\nset\t0\nset\t700\t17\t901\nend-terrain-ledger\n";
    buffer_init_dynamic(&ledger, permuted.size());
    buffer_write_raw(&ledger, permuted.data(), permuted.size());
    const bool reordered = terrain_save::load_ledger(&ledger, true);
    free(ledger.data);
    if (!reordered || terrain_save::decode(700) != TerrainSet{&last, &previous}) { errors << "Terrain archive numbers leaked into runtime identity.\n"; return false; }
    if (terrain_save::decode_legacy(1u << 6) != terrain_types().road) { errors << "Legacy terrain bit translation changed.\n"; return false; }
    // Failed registry loads must keep existing bound object addresses intact.
    write("custom_99", declaration("custom_99", {}, "<includes terrain=\"missing\"/>"));
    if (terrain_registry().load(layers, failure) || terrain_registry().find("custom_99") != &last) { errors << "Missing terrain reference was accepted or invalidated live definitions.\n"; return false; }
    write("custom_99", declaration("custom_99", {}, "<includes terrain=\"custom_98\"/>"));
    write("custom_98", declaration("custom_98", {}, "<includes terrain=\"custom_99\"/>"));
    if (terrain_registry().load(layers, failure) || terrain_registry().find("custom_99") != &last) { errors << "Terrain includes cycle was accepted.\n"; return false; }
    return true;
}
