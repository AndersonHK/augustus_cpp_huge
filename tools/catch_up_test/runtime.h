#pragma once
#include "scenario_overrides.h"
#include "archive_origin.h"
#include "building/BuildingCityService.h"
#include "building/BuildingFoundation.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/figure_runtime_native.h"
#include <memory>
#include <array>
#include "building/BuildingComposition.h"
#include "building/warehouse.h"
#include "city/data_private.h"
#include "city/resource.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/monument_gifts.h"
#include "city/trade_ledger.h"
#include "core/buffer.h"
#include "game/time.h"
#include "graphics/window.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "window/advisors.h"
#include "window/city.h"
#include "window/empire.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
#include "SDL.h"
#include <filesystem>
#include "window/epithets.h"
#include "window/trade_ledger.h"
#include <cstdio>
#include <stdexcept>
#include <vector>
#include "assets/image_group_payload.h"

inline bool run_catch_up_runtime_test()
{
    using namespace building_type_registry_impl;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    std::vector<uint8_t> accounting_backup(64 * 1024 * 1024), gift_backup(65536), time_backup(32);
    buffer accounting, gifts, time;
    buffer_init(&accounting, accounting_backup.data(), accounting_backup.size()); city_trade_ledger_save(&accounting);
    buffer_init(&gifts, gift_backup.data(), gift_backup.size()); city_monument_gifts_save(&gifts);
    buffer_init(&time, time_backup.data(), time_backup.size()); game_time_save_state(&time);
    bool success = true;
    std::vector<std::pair<int, uint32_t>> terrain;
    try {
        validate_scenario_model_overrides();
        validate_archive_origins();
        require(!accounting.overflow && !gifts.overflow, "Could not preserve fixture accounting");
        const auto dog_type = figure_type_from_xml_name("dog");
        if (dog_type != FIGURE_NONE) {
            int road = -1;
            for (int y = 2; y < map_grid_height() - 2 && road < 0; ++y) for (int x = 2; x < map_grid_width() - 2; ++x) {
                const int offset = map_grid_offset(x, y);
                if (map_grid_is_inside(x, y, 2) && map_terrain_is(offset, TERRAIN_ROAD) && map_terrain_is(map_grid_offset(x + 1, y), TERRAIN_ROAD)) { road = offset; break; }
            }
            require(road >= 0, "Dog roaming test requires connected roads");
            auto cleanup = [](Figure *figure) { if (figure && figure->id()) figure->remove(); };
            std::unique_ptr<Figure, decltype(cleanup)> dog(Figure::create(dog_type, map_grid_offset_to_x(road), map_grid_offset_to_y(road), DIR_0_TOP), cleanup);
            require(dog && dog->id(), "Could not create dog fixture");
            Building *owner = nullptr;
            Building::for_each(BuildingRuntimeList::Housing, [&](Building *building) { if (!owner && building->is_in_use()) owner = building; });
            require(owner != nullptr, "Dog fixture requires a residential owner");
            dog->set_home_building(owner);
            for (int direction = 0; direction < 8; ++direction) for (int frame = 0; frame < 8; ++frame) {
                dog->direction = static_cast<signed char>(direction); dog->image_offset = static_cast<unsigned char>(frame);
                FigureGraphicDrawRequest request;
                require(figure_graphics_resolve_draw_request(*dog, request) && request.has_base_slice(), "Dog walk frame does not resolve");
                require(request.sprite_offset_x == 19 && request.sprite_offset_y == 29, "Dog frame does not use its ground-contact anchor");
            }
            dog->direction = DIR_0_TOP; dog->image_offset = 0;
            int moved = 0, previous = dog->grid_offset;
            for (int tick = 0; tick < 420 && dog->state == FIGURE_STATE_ALIVE; ++tick) {
                require(figure_runtime_execute(dog.get()) != 0, "Dog did not use its native data profile");
                require(!dog->use_cross_country && map_terrain_is(dog->grid_offset, TERRAIN_ROAD | TERRAIN_HIGHWAY), "Dog left its road network");
                if (dog->grid_offset != previous) { ++moved; previous = dog->grid_offset; }
            }
            require(moved > 0, "Dog road-roaming fixture did not move");
            std::fprintf(stdout, "Dog contracts passed: 64 anchored frames, %d road tile transitions.\n", moved);
            const auto citizen_type = figure_type_from_xml_name("wandering_citizen");
            if (citizen_type != FIGURE_NONE) {
                std::unique_ptr<Figure, decltype(cleanup)> citizen(Figure::create(citizen_type, map_grid_offset_to_x(road), map_grid_offset_to_y(road), DIR_0_TOP), cleanup);
                require(citizen && citizen->id(), "Could not create wandering citizen fixture");
                citizen->set_home_building(owner);
                for (int direction = 0; direction < 8; ++direction) for (int frame = 0; frame < 12; ++frame) {
                    citizen->direction = static_cast<signed char>(direction); citizen->image_offset = static_cast<unsigned char>(frame);
                    FigureGraphicDrawRequest request;
                    require(figure_graphics_resolve_draw_request(*citizen, request) && request.has_base_slice(), "Wandering citizen frame does not resolve");
                }
                citizen->direction = DIR_0_TOP; citizen->image_offset = 0;
                int citizen_moves = 0, previous_offset = citizen->grid_offset;
                for (int tick = 0; tick < 420 && citizen->state == FIGURE_STATE_ALIVE; ++tick) {
                    require(figure_runtime_execute(citizen.get()) != 0, "Citizen did not use its native data profile");
                    require(!citizen->use_cross_country && map_terrain_is(citizen->grid_offset, TERRAIN_ROAD | TERRAIN_HIGHWAY), "Citizen left its road network");
                    if (citizen->grid_offset != previous_offset) { ++citizen_moves; previous_offset = citizen->grid_offset; }
                }
                require(citizen_moves > 0, "Wandering citizen did not move");
                for (const char *path : {"Walkers\\Vespasian_Dog", "Walkers\\Wandering_Citizen"}) {
                    const auto *payload = image_group_payload_get(path);
                    if (!payload) continue; // Augustus retains its original logical scale.
                    require(payload->entry_count() > 0, "Scaled ambient walker has no entries");
                    for (int index = 0; index < payload->entry_count(); ++index) {
                        const auto *entry = payload->entry_at_index(index);
                        const auto *slice = entry->footprint();
                        if (slice && slice->is_valid()) require(slice->fixed_logical_size.width == slice->width * 96 && slice->fixed_logical_size.height == slice->height * 96, "Ambient walker did not use Vespasian logical scaling");
                    }
                }
                std::fprintf(stdout, "Citizen contracts passed: 96 walk frames, %d road tile transitions, Vespasian ambient walker scaling.\n", citizen_moves);
            }
        }
        city_trade_ledger_reset(); game_time_init(-20);
        const auto resource = resource_wheat();
        const std::string identity = resource_text_id(resource);
        city_trade_ledger_produced(resource, 173);
        city_trade_ledger_consumed(resource, 41);
        city_trade_ledger_exchange(resource, 2, 11, true, 7);
        city_trade_ledger_exchange(resource, 2, 11, true, 7);
        city_trade_ledger_exchange(resource, 1, 12, true, 7);
        city_trade_ledger_exchange(resource, 3, 17, false, 7);
        const auto &period = city_trade_ledger_periods().front();
        const auto &totals = period.resources.at(identity);
        require(totals.produced == 173 && totals.consumed == 41 && totals.imported == 500 && totals.exported == 300 && totals.income == 51 && totals.expense == 56, "Accounting amounts or direction are incorrect");
        require(period.transactions.size() == 3 && period.transactions.front().units == 400, "Transactions do not aggregate by visit, direction, storage and price");
        game_time_advance_year(); city_trade_ledger_year_change();
        require(city_trade_ledger_periods()[1].resources.at(identity).produced == 173 && city_trade_ledger_periods().front().resources.at(identity).produced == 0, "Year rollover lost or duplicated production");
        for (int i = 0; i < 10; ++i) { game_time_advance_year(); city_trade_ledger_year_change(); }
        require(city_trade_ledger_periods().size() == 8 && city_trade_ledger_periods()[7].year == game_time_year() - 7, "History retention or empty-year rollover is incorrect");
        city_trade_ledger_exchange(resource, 7, 13, true, 42);
        city_trade_ledger_consumed(resource, 37);
        std::vector<uint8_t> encoded(1024 * 1024), encoded_again(encoded.size());
        buffer first, second;
        buffer_init(&first, encoded.data(), encoded.size()); city_trade_ledger_save(&first);
        const auto length = first.index; buffer_reset(&first); city_trade_ledger_load(&first);
        buffer_init(&second, encoded_again.data(), encoded_again.size()); city_trade_ledger_save(&second);
        require(second.index == length && std::equal(encoded.begin(), encoded.begin() + length, encoded_again.begin()), "Accounting save roundtrip changed history");
        const auto *station = definition_for_type(type_from_attr("highway_station"));
        if (station && station->city_service().enabled()) {
            for (const auto *entry : {"Highway_Station_OFF", "Highway_Station_ON", "Highway_Station_Sand", "Highway_Station_Stone"}) {
                const std::string group = std::string("Admin_Logistics\\") + (std::string(entry) == "Highway_Station_ON" ? "Highway_Station_OFF" : entry);
                const auto image = ImageGroupEntryRef::from_group(group, entry);
                require(image.width() > 0 && image.height() > 0, "Highway Station graphics are missing from the installed pack");
                image.draw(0, 0);
            }
            std::vector<int> cells;
            for (int y = 0; y < map_grid_height(); ++y) for (int x = 0; x < map_grid_width(); ++x) {
                if (!map_grid_is_inside(x, y, 1)) continue;
                const int offset = map_grid_offset(x, y);
                terrain.emplace_back(offset, map_terrain_get(offset));
                map_terrain_remove(offset, TERRAIN_HIGHWAY);
                cells.push_back(offset);
            }
            require(cells.size() > 204, "Fixture map too small for infrastructure test");
            require(map_terrain_count(TERRAIN_HIGHWAY) == 0, "Terrain count cache failed removals");
            for (int i = 0; i < 204; ++i) map_terrain_add(cells[i], TERRAIN_HIGHWAY);
            require(map_terrain_count(TERRAIN_HIGHWAY) == 204, "Terrain count cache failed additions");
            building record{}; record.id = 65000; record.type = station->type(); record.state = BUILDING_STATE_IN_USE; record.num_workers = 1;
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{65000, 65000, &record, station, {}}});
            require(scope.valid(), "Could not create service runtime fixture");
            auto &service_definition = const_cast<BuildingType *>(station)->city_service();
            const auto original_service = service_definition;
            auto restore_service = std::shared_ptr<void>(nullptr, [&](void *) { service_definition = original_service; });
            require(original_service.input_source == ResourceConsumptionSource::GlobalStockpile, "Station must use the global stockpile");
            service_definition.input_source = ResourceConsumptionSource::Building;
            service_definition.stock_periods = 6;
            BuildingCityService service(scope.runtime_for_record(&record)->building);
            require(service.infrastructure_units() == 51 && service.demand(resource_stone()) == 200 && service.stock_target(resource_sand()) == 1200, "Station demand does not round up per 50 highway blocks");
            record.resources[resource_stone()] = 200; record.resources[resource_sand()] = 100;
            require(!service.operational(), "Station operates without a full monthly input");
            service.consume_monthly(); require(record.resources[resource_stone()] == 200, "Station consumed a partial month");
            record.resources[resource_sand()] = 200;
            require(service.operational(), "Supplied staffed station is not operational");
            service.consume_monthly(); require(record.resources[resource_stone()] == 0 && record.resources[resource_sand()] == 0, "Station did not consume exactly one month");
            require(service.receive_load(resource_sand()) && record.resources[resource_sand()] == 100 && !service.receive_load(resource_wheat()), "Station delivery accepted the wrong resource or amount");
            struct StockSnapshot { Building *building; resource_type assigned; std::array<short, RESOURCE_SLOT_COUNT> amounts; };
            std::vector<StockSnapshot> stocks;
            std::vector<Building *> warehouses;
            std::vector<Building *> usable_spaces;
            const auto resource_cache = city_data.resource;
            auto restore_stocks = std::shared_ptr<void>(nullptr, [&](void *) {
                for (const auto &stock : stocks) {
                    stock.building->set_warehouse_resource_id(stock.assigned);
                    for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) stock.building->set_resource_amount(static_cast<resource_type>(r), stock.amounts[r]);
                }
                for (auto *warehouse : warehouses) building_warehouse_recount_resources(*warehouse);
                city_data.resource = resource_cache;
            });
            Building::for_each(BuildingRuntimeList::Warehouses, [&](Building *warehouse) {
                if (!warehouse->is_in_use() || !warehouse->Composition) return;
                warehouses.push_back(warehouse);
                for (auto *part : warehouse->Composition->children()) {
                    Building *space = part->building();
                    if (!space || !space->type->attr_is("warehouse_space")) continue;
                    StockSnapshot snapshot{space, space->warehouse_resource_id()};
                    for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) snapshot.amounts[r] = static_cast<short>(space->resource_amount(static_cast<resource_type>(r)));
                    stocks.push_back(snapshot);
                    if (!warehouse->has_plague()) usable_spaces.push_back(space);
                }
            });
            require(usable_spaces.size() >= 2, "Stockpile fixture requires two warehouse bays");
            for (const auto &stock : stocks) {
                for (int r = 0; r < RESOURCE_SLOT_COUNT; ++r) stock.building->set_resource_amount(static_cast<resource_type>(r), 0);
                stock.building->set_warehouse_resource_id(RESOURCE_NONE);
            }
            auto stock = [&](int index, resource_type resource, int amount) {
                usable_spaces[index]->set_warehouse_resource_id(resource);
                usable_spaces[index]->set_resource_amount(resource, amount);
                building_warehouse_recount_resources(*usable_spaces[index]->Composition->owner());
            };
            service_definition = original_service;
            for (auto &amount : record.resources) amount = 0;
            stock(0, resource_stone(), 200); stock(1, resource_sand(), 100);
            require(!service.operational() && service.stock_target(resource_sand()) == 0 && service.delivery_loads_needed(resource_stone()) == 0, "Global service queued a delivery or operated without all inputs");
            service.consume_monthly();
            require(resource_stockpile_amount(resource_stone()) == 200 && resource_stockpile_amount(resource_sand()) == 100, "Global stockpile consumed a partial month");
            stock(1, resource_sand(), 200);
            require(service.operational(), "Stockpile-backed service is not operational");
            service.consume_monthly();
            require(resource_stockpile_amount(resource_stone()) == 0 && resource_stockpile_amount(resource_sand()) == 0, "Global service did not consume exactly one month");
            stock(0, resource_stone(), 200);
            require(!resource_stockpile_consume({{resource_stone(), 150}, {resource_stone(), 150}}) && resource_stockpile_amount(resource_stone()) == 200, "Duplicate stockpile inputs bypassed atomic preflight");
            record.resources[resource_stone()] = 100; record.resources[resource_sand()] = 200;
            require(service.operational(), "Legacy station stock was stranded by global sourcing");
            service.consume_monthly();
            require(record.resources[resource_stone()] == 0 && record.resources[resource_sand()] == 0 && resource_stockpile_amount(resource_stone()) == 100, "Legacy buffers and global stockpile were not accounted together");

        }
        for (auto [offset, original] : terrain) map_terrain_set(offset, original);
        terrain.clear();
        const auto *arch = definition_for_type(type_from_attr("triumphal_arch"));
        if (arch && arch->has_phased_construction()) {
            building record{}; record.id = 65001; record.type = arch->type(); record.state = BUILDING_STATE_IN_USE; record.x = 10; record.y = 10; record.grid_offset = static_cast<short>(map_grid_offset(10, 10));
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{65001, 65001, &record, arch, {}}});
            require(scope.valid(), "Could not create single-building monument fixture");
            building_monument_set_phase(&record, 1);
            require(record.resources[resource_stone()] == 12 && record.resources[resource_timber()] == 8, "Arch first phase requirements differ from Augustus");
            require(building_monument_deliver_resource(&record, resource_stone()) && record.resources[resource_stone()] == 11, "Single-building monument cannot receive materials");
            auto &building = scope.runtime_for_record(&record)->building;
            building.Foundation->state().begin_publication(10, 10, 0);
            city_monument_gift_supply(building, true, true, 4);
            require(record.figure_id != 0, "External monument material supplier did not spawn");
            const Figure *supplier = Figure::get(record.figure_id);
            require(supplier->building == &building && supplier->destination_building == &building && supplier->collecting_item_id != RESOURCE_NONE, "Gift supplier did not retain monument ownership");
            require(building_monument_resource_in_delivery(&record, supplier->collecting_item_id) == 4, "Gift supplier material reservation differs from its convoy");
            auto clear_convoy = [&]() {
                auto figures = Figure::figures_referencing_building(building);
                for (auto *figure : figures) figure->remove();
                record.figure_id = 0;
            };
            clear_convoy();
            require(!building_monument_has_delivery_for_building(record.id), "Gift convoy left stale reservations after removal");
            for (auto &amount : record.resources) amount = 0;
            record.resources[RESOURCE_NONE] = 1;
            city_monument_gift_supply(building, false, true, 4);
            require(record.figure_id && Figure::get(record.figure_id)->type == FIGURE_WORK_CAMP_ARCHITECT && Figure::figures_referencing_building(building).size() == 1, "External architect spawned an invalid material convoy");
            clear_convoy();
            building.Foundation->state().clear();
            for (auto &amount : record.resources) amount = 0;
            require(building_monument_progress(&record) && record.monument.phase == 2 && record.resources[resource_marble()] == 32, "Single-building monument cannot advance phases");
            for (auto &amount : record.resources) amount = 0;
            require(building_monument_progress(&record) && record.monument.phase == MONUMENT_FINISHED, "Single-building monument cannot finish");
        }
        city_monument_gifts_reset();
        const auto gift_type = type_from_attr("triumphal_arch");
        require(!city_monument_gift_available(gift_type), "Unearned monument gift is available");
        city_monument_gifts_award("distant_battle_victory", 100);
        require(city_monument_gift_available(gift_type), "Parameterized victory gift was not awarded");
        buffer_reset(&time); game_time_load_state(&time);
        buffer_reset(&gifts); city_monument_gifts_load(&gifts);
        buffer_reset(&accounting); city_trade_ledger_load(&accounting);
        auto render = [&](const char *name, window_id expected) {
            require(window_is(expected), "XML feature window did not open");
            window_draw(1); window_draw(1);
            const int width = screen_pixel_width(), height = screen_pixel_height();
            std::vector<color_t> pixels(static_cast<size_t>(width) * height);
            require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width) != 0, "Could not read feature window render");
            require(std::any_of(pixels.begin(), pixels.end(), [&](color_t pixel) { return pixel != pixels.front(); }), "Feature window rendered a blank surface");
            std::filesystem::create_directories("out/catch-up-ui");
            SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
            require(surface != nullptr, "Could not create screenshot surface");
            const int result = SDL_SaveBMP(surface, (std::string("out/catch-up-ui/") + name + ".bmp").c_str());
            SDL_FreeSurface(surface);
            require(result == 0, "Could not save feature window review image");
        };
        window_city_show(); window_advisors_show_advisor(ADVISOR_FINANCIAL); render("financial", WINDOW_ADVISORS);
        window_city_show(); window_advisors_show_advisor(ADVISOR_RELIGION); render("religion", WINDOW_ADVISORS);
        if (window_epithets_available()) { window_epithets_show(); render("epithets", WINDOW_EPITHETS); }
        window_city_show(); window_trade_ledger_show(); render("ledger", WINDOW_TRADE_LEDGER);
        window_city_show(); window_trade_ledger_show(0); render("transactions", WINDOW_TRADE_LEDGER);
        window_city_show(); window_empire_show(); render("empire", WINDOW_EMPIRE);
        window_city_show();
        std::fprintf(stdout, "Catch-up contracts passed: accounting, history, roundtrip, service demand/consumption, single-building phases, gifts.\n");
    } catch (const std::exception &error) { std::fprintf(stderr, "Catch-up contract failed: %s\n", error.what()); success = false; }
    for (auto [offset, original] : terrain) map_terrain_set(offset, original);
    buffer_reset(&time); game_time_load_state(&time);
    buffer_reset(&gifts); city_monument_gifts_load(&gifts);
    buffer_reset(&accounting); city_trade_ledger_load(&accounting);
    return success;
}
