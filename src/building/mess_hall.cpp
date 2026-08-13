#include "building/mess_hall.h"

#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/distribution.h"
#include "city/buildings.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "game/time.h"

#define MAX_DISTANCE 40

building *MessHall::legacy_record() const
{
    return const_cast<building *>(Building::record());
}

Building *MessHall::storage_destination()
{
    const building_type_registry_impl::Distribution *distribution =
        type ? type->distribution() : nullptr;
    if (!distribution) {
        return nullptr;
    }

    resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };

    if (!distribution->needed_resources_for(*this, info) ||
        !distribution->find_sources_for_building(info, *this, MAX_DISTANCE)) {
        return nullptr;
    }
    auto destination_for_resource = [&](resource_type resource) -> Building * {
        Building *destination = info[resource].source;
        if (destination) {
            set_fetch_inventory_id(resource);
        }
        return destination;
    };
    // Prefer whichever food we don't have.
    resource_type fetch_inventory = distribution->fetch_resource(*this, info, 0, 0, 1);
    if (fetch_inventory != RESOURCE_NONE) {
        return destination_for_resource(fetch_inventory);
    }
    // Then prefer smallest stock below baseline stock.
    fetch_inventory = distribution->fetch_resource(*this, info, BASELINE_STOCK, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        return destination_for_resource(fetch_inventory);
    }
    // All items well stocked: use the XML stock target.
    fetch_inventory = distribution->fetch_resource(*this, info, 0, 0, 0);
    if (fetch_inventory != RESOURCE_NONE) {
        return destination_for_resource(fetch_inventory);
    }
    return nullptr;
}

int MessHall::has_fort_supply_inventory() const
{
    int total_food_in_mess_hall = 0;
    for (resource_type resource = static_cast<resource_type>(RESOURCE_NONE + 1);
         resource < RESOURCE_SLOT_COUNT;
         resource = static_cast<resource_type>(resource + 1)) {
        total_food_in_mess_hall += resource_amount(resource);
    }
    return total_food_in_mess_hall > 0;
}

int MessHall::fort_can_receive_supplier(const Building &fort) const
{
    const building *fort_record = fort.record();
    return fort_record &&
        !fort_record->figure_id2 &&
        fort.distance_from_entry() &&
        legacy_record() &&
        legacy_record()->state == BUILDING_STATE_IN_USE &&
        has_fort_supply_inventory();
}

int MessHall::fort_supplier_delay_elapsed(building &fort_record) const
{
    const int spawn_delay = game_time_scale_legacy_day_ticks(20);
    fort_record.figure_spawn_delay++;
    if (fort_record.figure_spawn_delay <= spawn_delay) {
        return 0;
    }

    fort_record.figure_spawn_delay = 0;
    return 1;
}

int MessHall::road_access_point(map_point *road) const
{
    return has_road_access(road);
}

Figure *MessHall::create_fort_supplier(const map_point &road, Building &fort) const
{
    Figure *supplier = Figure::create(FIGURE_MESS_HALL_FORT_SUPPLIER, road.x, road.y, DIR_4_BOTTOM);
    if (!supplier) {
        return nullptr;
    }

    supplier->action_state = FIGURE_ACTION_236_SUPPLY_POST_GOING_TO_FORT;
    supplier->destination_x = static_cast<unsigned char>(fort.road_access_x());
    supplier->destination_y = static_cast<unsigned char>(fort.road_access_y());
    supplier->source_x = static_cast<unsigned char>(road.x);
    supplier->source_y = static_cast<unsigned char>(road.y);
    supplier->set_destination_building(&fort);
    attach_figure(supplier);
    return supplier;
}

void MessHall::attach_figure(Figure *figure) const
{
    if (figure) {
        building_runtime *runtime = runtime_instance();
        figure->set_home_building(runtime ? &runtime->building : nullptr);
    }
}

int MessHall::spawn_fort_supplier_to(Building &fort)
{
    building *fort_record = const_cast<building *>(fort.record());
    if (!fort_record) {
        return 0;
    }
    if (!fort_can_receive_supplier(fort) || !fort_supplier_delay_elapsed(*fort_record)) {
        return 0;
    }

    map_point road;
    if (!road_access_point(&road)) {
        return 0;
    }

    Figure *supplier = create_fort_supplier(road, fort);
    if (!supplier) {
        return 0;
    }

    fort_record->figure_id2 = supplier->id();
    return 1;
}

void MessHall::spawn_supplier_for_fort(Building &fort)
{
    const int mess_hall_id = city_buildings_get_mess_hall();
    Building *mess_hall = mess_hall_id > 0 ? Building::get(static_cast<unsigned int>(mess_hall_id)) : nullptr;
    if (mess_hall) {
        MessHall(*mess_hall).spawn_fort_supplier_to(fort);
    }
}

Building *building_mess_hall_get_storage_destination(Building mess_hall)
{
    return MessHall(mess_hall).storage_destination();
}
