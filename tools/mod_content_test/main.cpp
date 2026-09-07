#include "game/mod_content.h"
#include "figure/type.h"
#include <windows.h>
#include <iostream>
#include <stdexcept>

using namespace mod_content;
void check(bool condition, const char *message) { if (!condition) throw std::runtime_error(message); }
template<class F> void rejects(F action) { bool failed = false; try { action(); } catch (const std::exception &) { failed = true; } check(failed, "Invalid input was accepted"); }

int main(int argc, char **argv)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    try {
        const auto initial_dog = figure_type_register("test_dog", "sheep");
        check(initial_dog >= FIGURE_BUILTIN_TYPE_MAX, "Dynamic figure registration failed");
        const std::string saved_identity = figure_type_identity(initial_dog);
        figure_type_identity_reset();
        figure_type_register("earlier_mod_figure", "patrician");
        const auto reordered_dog = figure_type_register("test_dog", "sheep");
        check(reordered_dog != initial_dog && figure_type_from_xml_name(saved_identity.c_str()) == reordered_dog && figure_type_base(reordered_dog) == FIGURE_SHEEP, "Saved figure identity must survive reordered mod types");
        check(figure_type_register("test_dog", "patrician") == FIGURE_NONE, "Conflicting dynamic base types must be rejected");
        figure_type_identity_reset();
        auto n = parse("<?xml version=\"1.0\"?><building type=\"barracks\"><construction max_count=\"1\"/><desirability value=\"-2\"/></building>");
        auto merged = overlay(n, parse("<building type='barracks'><construction/></building>"));
        check(merged.child("construction")->attributes.empty(), "Empty field must replace subtree");
        check(merged.child("desirability")->attribute("value") == "-2", "Omitted field must inherit");
        check(parse(serialize(merged)).children.size() == 2, "XML roundtrip");
        rejects([] { parse("<a><b></a>"); });
        rejects([] { parse("<a/><b/>"); });
        rejects([] { parse("<a x='1' x='2'/>"); });
        auto root = std::filesystem::temp_directory_path() / ("vespasian-settings-test-" + std::to_string(GetCurrentProcessId()));
        std::filesystem::create_directories(root);
        struct Cleanup { std::filesystem::path path; ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); } } cleanup{root};
        write_atomic(root / "A/mod.xml", "<mod><name value='A'/><description value='Test'/><version value='1'/><dependencies/><settings><setting id='MB' name='Multiple barracks' type='bool' default='false'/><setting id='RA' name='Retirement age' type='int(40,90)' default='50'/></settings></mod>");
        write_atomic(root / "A/BuildingType/b.xml", "<building type='barracks'><construction $!MB{max_count=\"1\"}/><labor age='$RA'/></building>");
        write_atomic(root / "B/mod.xml", "<mod><name value='B'/><description value='Test'/><version value='1'/><dependencies><mod name='A'/></dependencies></mod>");
        Session s; s.load({{"A", root / "A"}}, root / "values.xml");
        check(s.settings()[0].effective && s.settings()[1].effective, "Settings must be effective");
        s.set("A:MB", 1);
        check(parse(*s.file(root / "A/BuildingType/b.xml")).child("construction")->attributes.empty(), "Conditional must disappear");
        s.set("A:RA", 60); s.save();
        rejects([&] { s.set("A:RA", 91); });
        write_atomic(root / "B/BuildingType/renamed.xml", "<building type='barracks'><construction/></building>");
        s.load({{"A", root / "A"}, {"B", root / "B"}}, root / "values.xml");
        check(!s.settings()[0].effective && s.settings()[1].effective && s.settings()[1].value == 60, "Provenance and persistence");
        rejects([&] { s.set("A:MB", 0); });
        check(parse(*s.file(root / "B/BuildingType/renamed.xml")).child("labor")->attribute("age") == "60", "Identity across filenames");
        write_atomic(root / "B/BuildingType/renamed.xml", "<building type='barracks'><construction $![A:MB]{max_count=\"1\"}/><labor age='$[A:RA]'/></building>");
        s.load({{"A", root / "A"}, {"B", root / "B"}}, root / "values.xml");
        check(s.settings()[0].effective && s.settings()[1].effective, "Qualified overrides must retain setting ownership and effectiveness");
        s.set("A:MB", 0);
        check(parse(*s.file(root / "B/BuildingType/renamed.xml")).child("construction")->attribute("max_count") == "1", "Qualified negated setting must update the overriding mod");
        rejects([&] { s.expand("$[Missing:RA]", "B"); });
        rejects([&] { s.expand("$[A:RA", "B"); });
        if (argc > 1) {
            Session real; auto repo = std::filesystem::u8path(argv[1]);
            real.load({{"Julius", repo / "Mods/Julius"}, {"Augustus", repo / "Mods/Augustus"}, {"Vespasian", repo / "Mods/Vespasian"}}, {});
            std::cout << "Compiled " << real.files().size() << " repository definitions\n";
            for (const auto &pair : {std::pair<const char *, const char *>{"WILDLIFE_BLOCKED_BY_DEFENSES", "wolf"}, {"RIOTERS_ATTACK_DEFENSES", "rioter"}}) {
                const auto path = repo / "Mods/Augustus/FigureType" / (std::string(pair.second) + ".xml");
                const char *attribute = std::string(pair.second) == "wolf" ? "recheck_animal_terrain" : "attack_fireproof_defenses";
                real.set(std::string("Augustus:") + pair.first, 0);
                check(parse(*real.file(path)).child("behavior")->attribute(attribute) == "false", "D09 setting must restore original behavior");
                real.set(std::string("Augustus:") + pair.first, 1);
                check(parse(*real.file(path)).child("behavior")->attribute(attribute) == "true", "D09 setting must enable adopted behavior");
            }
            for (const char *figure : {"trade_caravan", "trade_caravan_donkey"}) {
                auto resolved = parse(*real.file(repo / "Mods/Vespasian/FigureType" / (std::string(figure) + ".xml")));
                check(resolved.attribute("graphics_only") != "true" && resolved.child("profiles") && resolved.child("graphics"), "Vespasian caravans must inherit complete native profiles");
                const auto *profile = resolved.child("profiles")->child("profile");
                check(profile && profile->child("movement") && profile->child("pathing") && profile->child("native"), "Caravan profile lost required behavior data");
                check(profile->child("movement")->attribute("speed_bonus_percent") == "25", "Caravan must inherit the Augustus bonus data");
                check(std::filesystem::exists(repo / "Mods/Julius/UnitType" / (std::string(figure) + ".xml")), "Caravan must declare combat stats");
            }
            for (const char *foundation : {"roadblock_1x1", "highway_2x2", "distant_water_2x2", "distant_water_3x3", "land_7x7", "road_or_land_1x1"}) {
                check(!std::filesystem::exists(repo / "Mods/Julius/Foundations" / (std::string(foundation) + ".xml")) && std::filesystem::exists(repo / "Mods/Augustus/Foundations" / (std::string(foundation) + ".xml")), "Augustus foundation must not ship under Julius");
            }
            for (const char *resource : {"Brick", "Concrete", "Gold", "Sand", "Stone"}) {
                auto source = read(repo / "Mods/Vespasian/Graphics/Industry" / (std::string(resource) + "_Carts.xml"));
                const auto doctype = source.find("<!DOCTYPE assetlist>");
                if (doctype != std::string::npos) source.erase(doctype, std::string("<!DOCTYPE assetlist>").size());
                auto group = parse(source);
                check(group.children.size() == 16, "Cart group must contain all eight directions and both load variants");
            }
            real.set("Augustus:WANDERING_CITIZENS", 1);
            auto house = parse(*real.file(repo / "Mods/Vespasian/BuildingType/house_small_tent.xml"));
            bool citizen = false, beggar = false;
            for (const auto &group : house.children) if (group.name == "spawn_group") for (const auto &spawn : group.children) {
                if (spawn.attribute("spawn_figure") == "wandering_citizen") {
                    citizen = true;
                    check(spawn.attribute("requires_config").empty() && spawn.attribute("chance_per_million_bands") != "0:0", "Vespasian citizens must spawn without global labour when enabled");
                }
                beggar |= spawn.attribute("spawn_figure") == "beggar";
            }
            check(citizen && beggar, "Vespasian must retain independent citizen and beggar spawn groups");
            real.set("Augustus:WANDERING_CITIZENS", 0);
            house = parse(*real.file(repo / "Mods/Vespasian/BuildingType/house_small_tent.xml"));
            for (const auto &group : house.children) if (group.attribute("id") == "ambient_citizens") check(group.children.front().attribute("chance_per_million_bands") == "0:0", "Qualified citizen setting must disable Vespasian spawning immediately");
            auto layers = real.layers();
            layers.push_back({"No Monuments", repo / "Mods/No Monuments"});
            Session instant; instant.load(layers, {});
            int monuments = 0;
            for (const auto &entry : std::filesystem::directory_iterator(repo / "Mods/No Monuments/BuildingType")) {
                Node declaration = parse(read(entry.path()));
                if (!declaration.child("construction")) continue;
                auto original_path = repo / "Mods/Augustus/BuildingType" / entry.path().filename();
                auto vespasian_path = repo / "Mods/Vespasian/BuildingType" / entry.path().filename();
                if (real.file(vespasian_path)) original_path = vespasian_path;
                if (!real.file(original_path)) original_path = repo / "Mods/Augustus/Tiles" / entry.path().filename();
                check(real.file(original_path) != nullptr, "Missing monument baseline");
                Node original = parse(*real.file(original_path));
                Node resolved = parse(*instant.file(entry.path()));
                std::map<std::string, int> expected, actual;
                const auto collect = [](const auto &self, const Node &node, std::map<std::string, int> &costs) -> void {
                    if (node.name == "requirement" && node.attribute("type") != "architects" && node.attribute("type") != "concrete") costs[node.attribute("type")] += std::stoi(node.attribute("amount"));
                    for (const auto &child : node.children) self(self, child, costs);
                };
                check(original.child("construction") != nullptr, "Missing monument construction baseline");
                const Node *gift = original.child("construction")->child("gift");
                const bool externally_funded = gift && gift->attribute("materials") == "true";
                if (!externally_funded) collect(collect, *original.child("construction"), expected);
                const Node &construction = *resolved.child("construction");
                check(construction.attribute("mode", "instant") == "instant", "No Monuments must build instantly");
                if (gift) check(construction.child("gift") && construction.child("gift")->attribute("event") == gift->attribute("event"), "Instant monuments must preserve event reward eligibility");
                for (const auto &child : construction.children) {
                    if (child.name == "gift") continue;
                    check(child.name == "requirement" && child.attribute("type") != "concrete" && child.attribute("type") != "architects", "No Monuments has non-instant work or concrete costs");
                    actual[child.attribute("type")] += std::stoi(child.attribute("amount"));
                }
                check(actual == expected, "No Monuments changed a non-concrete material cost");
                check((original.child("model") ? serialize(*original.child("model")) : "") == (resolved.child("model") ? serialize(*resolved.child("model")) : ""), "No Monuments changed the monetary cost");
                ++monuments;
            }
            Node producer = parse(*instant.file(repo / "Mods/No Monuments/BuildingType/concrete_maker.xml"));
            check(producer.child("button")->attributes.empty() && producer.child("storages")->children.empty() && producer.child("production_methods")->children.empty(), "No Monuments must disable concrete production and storage");
            check(monuments > 10, "No Monuments coverage is incomplete");
            std::cout << "Verified instant costs for " << monuments << " monument definitions; concrete production disabled\n";
        }
        std::cout << "Mod content contracts passed\n";
        return 0;
    } catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
}
