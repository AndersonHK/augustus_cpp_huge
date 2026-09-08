#pragma once
#include "building/construction_plan.h"
#include "building/construction_building.h"
#include "building/construction.h"
#include "building/construction_routed.h"
#include "building/BuildingComposition.h"
#include "building/BuildingGraphics.h"
#include "building/storage.h"
#include "building/granary.h"
#include "building/warehouse.h"
#include "building/building_record.h"
#include "game/file.h"
#include "game/file_io.h"
#include "game/undo.h"
#include "map/image.h"
#include "map/figure.h"
#include "figure/figure_runtime_api.h"
#include "map/aqueduct.h"
#include "map/tiles.h"
#include "widget/city_overlay_other.h"
#include "scenario/price_change.h"
#include "scenario/demand_change.h"
#include <chrono>
#include <filesystem>
#include <stdexcept>

inline void validate_april_placement_and_counters()
{
    using namespace building_type_registry_impl;
    using building_construction::ConstructionPlacementPlan;
    const auto require = [](bool result, const char *message) { if (!result) throw std::runtime_error(message); };
    const auto path = std::filesystem::temp_directory_path() / ("vespasian-april-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".svv");
    require(game_file_io_write_saved_game(path.string().c_str()) != 0, "April fixture could not checkpoint the city");
    auto restore = [&]() { return game_file_load_saved_game(path.string().c_str()) == FILE_LOAD_SUCCESS; };
    try {
        const int prices = scenario_price_change_count_active(), demands = scenario_demand_change_count_active();
        const int price_id = scenario_price_change_new(), demand_id = scenario_demand_change_new();
        price_change_t price = *scenario_price_change_get(price_id);
        demand_change_t demand = *scenario_demand_change_get(demand_id);
        require(scenario_price_change_count_active() == prices && scenario_demand_change_count_active() == demands, "Inactive editor changes affected active counters");
        price.year = demand.year = 73;
        scenario_price_change_update(&price); scenario_demand_change_update(&demand);
        require(scenario_price_change_count_active() == prices + 1 && scenario_demand_change_count_active() == demands + 1, "Editor active counters omitted a scheduled change");
        scenario_price_change_delete(price_id); scenario_demand_change_delete(demand_id);
        require(scenario_price_change_count_active() == prices && scenario_demand_change_count_active() == demands, "Editor active counters retained a deleted change");

        const auto *aqueduct = definition_for_type(type_from_attr("aqueduct"));
        const auto *reservoir = definition_for_type(type_from_attr("reservoir"));
        require(aqueduct && reservoir, "April fixture requires aqueduct/reservoir definitions");
        int x = -1, y = -1;
        for (int yy = 8; yy < map_grid_height() - 12 && x < 0; ++yy) for (int xx = 8; xx < map_grid_width() - 12; ++xx) {
            bool clear = true;
            for (int dy = 0; dy < 10 && clear; ++dy) for (int dx = 0; dx < 10; ++dx) {
                const int offset = map_grid_offset(xx + dx, yy + dy);
                if (map_building_exists_at(offset) || map_figure_at(offset) || !ConstructionPlacementPlan(*aqueduct, xx + dx, yy + dy, 1, 0).can_place()) { clear = false; break; }
            }
            if (clear) { x = xx; y = yy; break; }
        }
        require(x >= 0, "April fixture requires a clear placement area");
        for (bool warehouse : {false, true}) {
            Building::for_each(BuildingRuntimeList::Storage, [&](Building *b) {
                if (b->type && b->type->is_storage()) building_storage_set_permission(BUILDING_STORAGE_PERMISSION_CAESAR, *b, 1);
            });
            const auto *definition = definition_for_type(type_from_attr(warehouse ? "warehouse" : "granary"));
            ConstructionPlacementPlan plan(*definition, x, y, 1, 0);
            require(plan.can_place() && building_construction_publish_placement_batch({plan}), "Could not place storage request fixture");
            building_update_state();
            Building &storage = map_building_at(map_grid_offset(x, y));
            auto *record = const_cast<building *>(storage.record());
            record->num_workers = 12;
            record->has_road_access = 1;
            building_storage_accept_all(storage.storage_id);
            const auto wheat = resource_wheat();
            require((warehouse ? building_warehouse_try_add_resource(storage, wheat, 4, 0) : building_granary_try_add_resource(storage, wheat, 4, 0, 0)) == 4, "Storage request fixture did not accept four loads");
            auto settings = *building_storage_get(storage.storage_id);
            settings.resource_state[wheat].state = BUILDING_STORAGE_STATE_MAINTAINING;
            settings.resource_state[wheat].quantity = BUILDING_STORAGE_QUANTITY_MAX;
            building_storage_set_data(storage.storage_id, settings);
            // The legacy setter writes the deny bit, whereas the getter returns permission.
            building_storage_set_permission(BUILDING_STORAGE_PERMISSION_CAESAR, storage, 0);
            auto available = [&]() { return warehouse ? building_warehouses_count_available_resource(wheat, 1, 1) : building_granaries_count_available_resource(wheat, 1, 1); };
            require(available() == 0, "Maintained stock leaked into an ordinary request");
            building_storage_toggle_empty_all(storage.storage_id);
            if (available() != 4) std::fprintf(stderr, "Storage request fixture: warehouse=%d id=%u state=%d storage=%u empty=%d permission=%d amount=%d available=%d\n", warehouse, static_cast<unsigned int>(storage.id), record->state, static_cast<unsigned int>(storage.storage_id), building_storage_get_empty_all(storage), building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, storage), warehouse ? building_warehouse_get_amount(storage, wheat) : building_granary_get_amount(storage, wheat), available());
            require(available() == 4, "Empty-all stock remained excluded from a request");
            building_storage_set_permission(BUILDING_STORAGE_PERMISSION_CAESAR, storage, 1);
            require(available() == 0, "Empty-all bypassed the Caesar permission");
            building_storage_set_permission(BUILDING_STORAGE_PERMISSION_CAESAR, storage, 0);
            require((warehouse ? building_warehouses_send_resources_to_rome(wheat, 3) : building_granaries_send_resources_to_rome(wheat, 3)) == 0, "Request failed to dispatch emptying stock");
            require(available() == 1, "Request dispatch removed the wrong amount");
            require(restore(), "Storage request fixture could not restore the city");
        }
        for (const bool highway : {false, true}) {
            const auto *surface = definition_for_type(type_from_attr(highway ? "highway" : "road"));
            require(surface, "April crossing fixture requires road/highway data");
            const int cross_x = x + 4, cross_y = y + 4;
            int cost = 0;
            building_construction_set_type(aqueduct, 0);
            require(game_undo_start_build(aqueduct->type()) != 0, "Could not start aqueduct crossing fixture");
            require(building_construction_place_aqueduct(0, aqueduct->type(), cross_x, y + 1, cross_x, y + 8, &cost) > 0, "Could not place crossing aqueduct");
            game_undo_finish_build(cost); game_undo_disable();
            map_image_update_all();
            const int crossing = map_grid_offset(cross_x, cross_y);
            const auto old_terrain = terrain_map().at(crossing);
            const auto old_image = map_image_at(crossing);
            const auto old_owner = map_building_at(crossing).id;
            building_construction_set_type(surface, 0);
            require(game_undo_start_build(surface->type()) != 0, "Could not start crossing preview");
            auto place = [&](int preview) {
                return highway ? building_construction_place_highway(preview, x + 1, cross_y, x + 8, cross_y, surface->type()) : building_construction_place_road(preview, x + 1, cross_y, x + 8, cross_y, surface->type());
            };
            require(place(1) > 0, "Road/highway preview could not cross an aqueduct");
            game_undo_restore_map(1);
            require(terrain_map().at(crossing) == old_terrain && map_image_at(crossing) == old_image && map_building_at(crossing).id == old_owner, "Cancelling a crossing preview changed aqueduct terrain, image or owner");
            require(place(0) > 0, "Road/highway could not be published under an aqueduct");
            require(terrain_map().contains(crossing, terrain_types().aqueduct) && terrain_map().contains(crossing, highway ? terrain_types().highway : terrain_types().road), "Published crossing lost its aqueduct or transport surface");
            game_undo_finish_build(0); game_undo_perform();
            require(terrain_map().at(crossing) == old_terrain && map_image_at(crossing) == old_image && map_building_at(crossing).id == old_owner, "Undo changed the original aqueduct terrain, image or owner");
            require(restore(), "Crossing fixture could not restore the city");
        }
        {
            const auto *granary = definition_for_type(type_from_attr("granary"));
            const auto *depot_type = definition_for_type(type_from_attr("cart_depot"));
            std::array<Building *, 3> storages{};
            for (int index = 0; index < 3; ++index) {
                const int xx = x + (index == 1 ? 4 : 0), yy = y + (index == 2 ? 4 : 0);
                require(building_construction_publish_placement_batch({ConstructionPlacementPlan(*granary, xx, yy, 1, 0)}), "Could not place depot storage fixture");
                storages[index] = &map_building_at(map_grid_offset(xx, yy));
                building_storage_accept_all(storages[index]->storage_id);
            }
            require(depot_type && building_construction_publish_placement_batch({ConstructionPlacementPlan(*depot_type, x + 4, y + 4, 1, 0)}), "Could not place depot fixture");
            building_update_state();
            Building &depot = map_building_at(map_grid_offset(x + 4, y + 4));
            auto *record = const_cast<building *>(depot.record());
            auto &order = record->data.depot.current_order;
            order.src_storage_id = storages[0]->id; order.dst_storage_id = storages[1]->id; order.resource_type = resource_wheat();
            Figure *cart = Figure::create(figure_type_from_xml_name("depot_cart_pusher"), x + 1, y + 1, DIR_0_TOP);
            require(cart && cart->set_home_building(&depot), "Depot fixture could not bind its native cart");
            record->data.distribution.cartpusher_ids[0] = cart->id();
            cart->set_destination_building(storages[1]);
            cart->action_state = FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION;
            cart->resource_id = static_cast<unsigned char>(resource_wheat()); cart->loads_sold_or_carrying = 4; cart->wait_ticks = 0;
            order.dst_storage_id = storages[2]->id;
            require(figure_runtime_execute(cart) && cart->destination_building == storages[2] && cart->loads_sold_or_carrying == 4, "Changing a depot destination lost cargo or failed to reroute");
            order.resource_type = resource_fruit();
            require(figure_runtime_execute(cart) && cart->resource_id == resource_wheat() && cart->loads_sold_or_carrying == 4, "Changing a depot order resource replaced existing cargo");
            cart->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
            require(figure_runtime_execute(cart) && cart->action_state == FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE && cart->destination_building == storages[0] && cart->loads_sold_or_carrying == 4, "Recalling a depot cart dumped its cargo instead of returning it");
            cart->action_state = FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE; cart->wait_ticks = 30000;
            require(figure_runtime_execute(cart) && cart->loads_sold_or_carrying == 0 && building_granary_get_amount(*storages[0], resource_wheat()) == 4, "Recalled depot cargo was not returned exactly once");
            cart->remove();
            require(restore(), "Depot fixture could not restore the city");
        }
        for (int dy = 0; dy < 3; ++dy) for (int dx = 0; dx < 3; ++dx) {
            ConstructionPlacementPlan plan(*aqueduct, x + dx, y + dy, 1, 0);
            require(plan.can_place() && building_construction_publish_placement_batch({plan}), "Aqueduct fixture publication failed");
        }
        ConstructionPlacementPlan over_aqueduct(*reservoir, x, y, 1, 0);
        require(over_aqueduct.can_place() && building_construction_publish_placement_batch({over_aqueduct}), "Reservoir cannot replace arbitrary aqueduct cells");
        building_update_state(); map_image_update_all();
        const auto reservoir_id = map_building_at(map_grid_offset(x, y)).id;
        for (int dy = 0; dy < 3; ++dy) for (int dx = 0; dx < 3; ++dx) {
            const int offset = map_grid_offset(x + dx, y + dy);
            require(map_building_at(offset).id == reservoir_id && !terrain_map().contains(offset, terrain_types().aqueduct), "Reservoir replacement retained an aqueduct owner or terrain bit");
        }
        require(restore(), "April fixture could not restore the city");
        const auto *hippodrome = definition_for_type(type_from_attr("hippodrome"));
        require(hippodrome && hippodrome->has_composition(), "April overlay fixture requires a composed hippodrome");
        ConstructionPlacementPlan hippodrome_plan(*hippodrome, x, y, 1, 0);
        require(hippodrome_plan.can_place() && building_construction_publish_placement_batch({hippodrome_plan}), "Hippodrome overlay fixture could not be placed");
        Building &owner = map_building_at(map_grid_offset(x, y));
        const_cast<building *>(owner.record())->state = BUILDING_STATE_MOTHBALLED;
        for (const auto *child : owner.Composition->children()) {
            const auto *record = child->building()->record();
            require(city_overlay_for_mothball()->show_building(record), "Hippodrome child does not use its owner's mothball state");
            tooltip_context tooltip{};
            require(city_overlay_for_employment()->get_column_height(record) == NO_COLUMN && !city_overlay_for_employment()->get_tooltip_for_building(&tooltip, record), "Hippodrome child duplicated its owner's employment data");
        }
        require(restore(), "April fixture could not restore the original city");
    } catch (...) {
        restore(); std::error_code ignored; std::filesystem::remove(path, ignored); throw;
    }
    std::error_code ignored; std::filesystem::remove(path, ignored);
    std::fprintf(stdout, "April contracts passed: active editor counters, storage requests and empty-all permissions, depot cargo recall/order changes, road/highway aqueduct crossings with cancel/undo, reservoir over nine aqueducts, hippodrome owner overlays.\n");
}
