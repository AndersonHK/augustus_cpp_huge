#pragma once

#include "building/building.h"
#include "map/point.h"

class Figure;

class MessHall : public Building {
public:
    using Building::Building;
    explicit MessHall(Building building) : Building(building) {}

    Building *storage_destination();
    int spawn_fort_supplier_to(Building &fort);

private:
    building *legacy_record() const;
    int has_fort_supply_inventory() const;
    int fort_can_receive_supplier(const Building &fort) const;
    int fort_supplier_delay_elapsed(building &fort_record) const;
    int road_access_point(map_point *road) const;
    Figure *create_fort_supplier(const map_point &road, Building &fort) const;
    void attach_figure(Figure *figure) const;
};

Building *building_mess_hall_get_storage_destination(Building mess_hall);
