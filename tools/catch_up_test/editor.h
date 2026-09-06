#pragma once
#include "editor/editor.h"
#include "editor/tool.h"
#include "map/building.h"
#include "game/game.h"
#include "game/file_io.h"
#include "game/file_editor.h"
#include "window/editor/map.h"
#include "window/editor/attributes.h"
#include "window/editor/model_data.h"
#include "window/editor/scenario_events.h"
#include "window/editor/custom_variables.h"
#include "window/editor/scenario_event_details.h"
#include "window/editor/scenario_action_edit.h"
#include "scenario/event/controller.h"
#include "scenario/event/action_types.h"
#include "empire/trade_route.h"
#include "scenario/event/event.h"
#include "scenario/event/parameter_data.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/export_xml.h"
#include "empire/import_xml.h"
#include "core/encoding.h"
#include "scenario/definition_overrides.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "graphics/window.h"
#include "SDL.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <stdexcept>
#include <vector>

inline bool run_editor_compatibility_test()
{
    try {
        if (!editor_is_present() || !game_init_editor()) throw std::runtime_error("Editor support or editor initialization failed");
        const std::filesystem::path output("out/editor-review");
        std::filesystem::create_directories(output);
        auto capture = [&](const char *name) {
            window_draw(1); window_draw(1);
            const int width = screen_pixel_width(), height = screen_pixel_height();
            std::vector<color_t> pixels(static_cast<size_t>(width) * height);
            if (!graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width)) throw std::runtime_error("Editor capture failed");
            if (std::all_of(pixels.begin(), pixels.end(), [&](color_t value) { return value == pixels.front(); })) throw std::runtime_error("Editor surface is blank");
            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            if (!surface) throw std::runtime_error("Editor screenshot allocation failed");
            const int status = SDL_SaveBMP(surface, (output / (std::string(name) + ".bmp")).string().c_str());
            SDL_FreeSurface(surface);
            if (status) throw std::runtime_error("Editor screenshot write failed");
        };
        capture("map");
        window_editor_attributes_show(); capture("attributes"); window_editor_map_show();
        window_model_data_show(); capture("models"); window_editor_map_show();
        window_editor_scenario_events_show(); capture("events"); window_editor_map_show();
        window_editor_validate_variable_colors();
        auto *event = scenario_event_create(0, 0, 0);
        scenario_event_condition_group_add(event);
        const int event_id = event->id;
        for (int type = ACTION_TYPE_LOCK_TRADE_ROUTE; type < ACTION_TYPE_MAX; ++type) {
            auto *action = scenario_event_action_create(event, type);
            const auto *metadata = scenario_events_parameter_data_get_actions_xml_attributes(action->type);
            const std::array<const xml_data_attribute_t *, 5> attributes = {&metadata->xml_parm1, &metadata->xml_parm2, &metadata->xml_parm3, &metadata->xml_parm4, &metadata->xml_parm5};
            const std::array<int *, 5> values = {&action->parameter1, &action->parameter2, &action->parameter3, &action->parameter4, &action->parameter5};
            for (size_t i = 0; i < attributes.size(); ++i) *values[i] = attributes[i]->type == PARAMETER_TYPE_FORMULA ? scenario_formula_add(reinterpret_cast<const uint8_t *>("1"), attributes[i]->min_limit, attributes[i]->max_limit) : scenario_events_parameter_data_get_default_value_for_parameter(const_cast<xml_data_attribute_t *>(attributes[i]));
        }
        window_editor_scenario_event_details_show(event_id); capture("event-details"); window_editor_map_show();
        for (auto &action : event->actions) {
            window_editor_scenario_action_edit_show(&action); capture(("action-" + std::to_string(action.type)).c_str()); window_editor_map_show();
        }
        int route_object_id = 0;
        for (int id = 1; id < empire_object_count(); ++id) {
            auto *object = empire_object_get_full(id);
            if (object && object->in_use && object->obj.type == EMPIRE_OBJECT_CITY && object->city_type == EMPIRE_CITY_TRADE) { route_object_id = id; break; }
        }
        if (!route_object_id) throw std::runtime_error("Empire XML fixture needs a trade city");
        auto *trade = empire_object_get_full(route_object_id);
        trade->obj.empire_city_icon = static_cast<empire_city_icon_type>(19); trade->trade_route_cost = 0;
        encoding_from_utf8("XML Caf\xc3\xa9", trade->city_custom_name, sizeof(trade->city_custom_name));
        scenario_definition_override_set({ScenarioOverrideKind::RouteResource, std::to_string(trade->obj.trade_route_id), 0, resource_text_id(resource_wheat()), 17});
        scenario_definition_override_set({ScenarioOverrideKind::HiddenRoute, std::to_string(trade->obj.trade_route_id), 0, {}, 1});
        auto *trade_city = empire_city_get(empire_city_get_for_object(route_object_id));
        scenario_action_t demand{}; demand.parameter1 = trade_city->route_id; demand.parameter2 = resource_wheat();
        demand.parameter3 = scenario_formula_add(reinterpret_cast<const uint8_t *>("29"), 0, 1000000);
        for (int buys : {0, 1}) {
            if (buys) empire_city_change_buying_of_resource(trade_city, resource_wheat(), 0);
            else empire_city_change_selling_of_resource(trade_city, resource_wheat(), 0);
            demand.parameter5 = buys;
            if (!scenario_action_type_trade_route_amount_execute(&demand) || trade_route_limit(trade_city->route_id, resource_wheat(), buys) != 29 || !(buys ? trade_city->buys_resource[resource_wheat()] : trade_city->sells_resource[resource_wheat()])) throw std::runtime_error("Demand increase from zero did not enable the resource");
        }
        const auto empire_path = (output / "roundtrip-empire.xml").string();
        if (!empire_export_xml(empire_path.c_str()) || !empire_xml_parse_file(empire_path.c_str(), 0)) throw std::runtime_error("Empire XML roundtrip failed");
        uint8_t name[64]; encoding_from_utf8("XML Caf\xc3\xa9", name, sizeof(name));
        const auto *restored_city = empire_city_get(empire_city_get_id_by_name(name));
        if (!restored_city || !restored_city->in_use || restored_city->cost_to_open != 0 || empire_city_trade_resource_cost(restored_city->route_id, resource_wheat()) != 17) throw std::runtime_error("Empire XML lost city name, zero denarii cost or resource cost");
        if (empire_object_get(restored_city->empire_object_id)->empire_city_icon != 19 || !scenario_definition_override_value(ScenarioOverrideKind::HiddenRoute, std::to_string(restored_city->route_id), 0, {}, 0)) throw std::runtime_error("Empire XML lost button icon or hidden route");
        const tool_type tools[] = {TOOL_NATIVE_HUT, TOOL_NATIVE_HUT_ALT, TOOL_NATIVE_CENTER, TOOL_NATIVE_FIELD, TOOL_NATIVE_DECORATION, TOOL_NATIVE_MONUMENT, TOOL_NATIVE_WATCHTOWER};
        const char *types[] = {"native_hut", "native_hut_alt", "native_meeting", "native_crops", "native_decor", "native_monument", "native_watchtower"};
        for (int index = 0; index < 7; ++index) {
            const int x = 12 + 8 * index, y = 30;
            map_tile tile{x, y, map_grid_offset(x, y)};
            editor_tool_set_type(tools[index]); editor_tool_start_use(&tile); editor_tool_end_use(&tile);
            if (!map_building_exists_at(tile.grid_offset) || !map_building_at(tile.grid_offset).matches(types[index])) throw std::runtime_error("Editor native building placement failed");
        }
        editor_tool_deactivate(); capture("native-buildings");
        const auto scenario_path = (output / "roundtrip.mapx").string();
        if (!game_file_editor_write_scenario(scenario_path.c_str()) || !game_file_editor_load_scenario(scenario_path.c_str())) throw std::runtime_error("Editor scenario roundtrip failed");
        for (int index = 0; index < 7; ++index) {
            const int offset = map_grid_offset(12 + 8 * index, 30);
            if (!map_building_exists_at(offset) || !map_building_at(offset).matches(types[index])) throw std::runtime_error("Scenario roundtrip lost native building identity");
        }
        const auto *restored_event = scenario_event_get(event_id);
        if (!restored_event || restored_event->actions.size() != ACTION_TYPE_MAX - ACTION_TYPE_LOCK_TRADE_ROUTE) throw std::runtime_error("Scenario roundtrip lost new event actions");
        capture("roundtrip");
        game_exit_editor();
        std::fprintf(stdout, "Editor compatibility smoke test passed: map, attributes, models, events, scenario roundtrip.\n");
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Editor compatibility smoke test failed: %s\n", error.what());
        return false;
    }
}
