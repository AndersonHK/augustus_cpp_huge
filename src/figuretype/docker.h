#pragma once

#include "figure/figure.h"
#include "game/resource.h"
#include "map/point.h"

namespace figuretype {

class Docker : public Figure {
public:
    void docker_action();

private:
    static Figure *valid_trade_ship_for_dock(Building &dock);
    static int try_import_resource(Building &storage, resource_type resource, int city_id, int quantity);
    static int try_export_resource(Building &storage, resource_type resource, int city_id);
    static int store_destination_map_point(const Building *destination, map_point *dst);
    static int is_invalid_destination(const Building *destination, const Building &dock);
    static Building *closest_import_storage_for_resource(
        int x, int y, const Building &dock, resource_type resource, map_point *dst);
    static Building *closest_building_for_import(
        int x, int y, int city_id, const Building &dock, map_point *dst, resource_type *import_resource);
    static Building *closest_export_storage_for_resource(
        int x, int y, const Building &dock, resource_type resource, map_point *dst);
    static Building *closest_building_for_export(
        int x, int y, int city_id, const Building &dock, map_point *dst, resource_type *export_resource);

    int deliver_import_resource(Building &dock);
    int fetch_export_resource(Building &dock, int add_to_bought);
    void publish_cart_contents();
    void set_as_idle();
};

} // namespace figuretype
