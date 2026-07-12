#include "trade.h"

#include "building/caravanserai.h"
#include "building/count.h"
#include "building/industry.h"
#include "building/lighthouse.h"
#include "building/monument.h"
#include "city/constants.h"
#include "core/config.h"
#include "city/data_private.h"
#include "empire/city.h"
#include "figure/figure.h"
#include "game/resource.h"

void city_trade_update(void)
{
    city_data.trade.num_sea_routes = 0;
    city_data.trade.num_land_routes = 0;
    // Wine types
    city_data.resource.wine_types_available = building_count_active(building_producer_for_resource(resource_wine())) > 0 ? 1 : 0;
    if (building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        city_data.resource.wine_types_available += 1;
    }

    city_data.resource.wine_types_available += empire_city_count_wine_sources();

    // Update trade problems
    if (city_data.trade.land_trade_problem_duration > 0) {
        city_data.trade.land_trade_problem_duration--;
        if (building_caravanserai_is_fully_functional()) {
            city_data.trade.land_trade_problem_duration--;
        }
    } else {
        city_data.trade.land_trade_problem_duration = 0;
    }
    if (city_data.trade.sea_trade_problem_duration > 0) {
        city_data.trade.sea_trade_problem_duration--;
        if (building_lighthouse_is_fully_functional()) {
            city_data.trade.sea_trade_problem_duration--;
        }
    }
    if (city_data.trade.sea_trade_problem_duration <= 0) {
        city_data.trade.sea_trade_problem_duration = 0;
    }
    if (city_data.trade.land_trade_problem_duration <= 0) {
        city_data.trade.land_trade_problem_duration = 0;
    }


    empire_city_generate_trader();
}

void city_trade_add_land_trade_route(void)
{
    city_data.trade.num_land_routes++;
}

void city_trade_add_sea_trade_route(void)
{
    city_data.trade.num_sea_routes++;
}

int city_trade_has_land_trade_route(void)
{
    return city_data.trade.num_land_routes > 0;
}

int city_trade_has_sea_trade_route(void)
{
    return city_data.trade.num_sea_routes > 0;
}

void city_trade_start_land_trade_problems(int duration)
{
    city_data.trade.land_trade_problem_duration = static_cast<int16_t>(duration);
}

void city_trade_start_sea_trade_problems(int duration)
{
    city_data.trade.sea_trade_problem_duration = static_cast<int16_t>(duration);
}

int city_trade_has_land_trade_problems(void)
{
    return city_data.trade.land_trade_problem_duration > 0;
}

int city_trade_has_sea_trade_problems(void)
{
    return city_data.trade.sea_trade_problem_duration > 0;
}

static int next_docker_resource(int *cursor)
{
    const int count = resource_loaded_count();
    if (!cursor || count <= 0) {
        return RESOURCE_NONE;
    }

    int cursor_index = -1;
    for (int i = 0; i < count; i++) {
        if (resource_get_loaded(i) == *cursor) {
            cursor_index = i;
            break;
        }
    }

    for (int offset = 1; offset <= count; offset++) {
        const int index = cursor_index >= 0 ? (cursor_index + offset) % count : offset - 1;
        resource_type resource = resource_get_loaded(index);
        if (resource_is_tradeable(resource) && resource_is_storable(resource)) {
            *cursor = resource;
            return resource;
        }
    }

    *cursor = RESOURCE_NONE;
    return RESOURCE_NONE;
}

int city_trade_next_docker_import_resource(void)
{
    return next_docker_resource(&city_data.trade.docker_import_resource);
}

int city_trade_next_docker_export_resource(void)
{
    return next_docker_resource(&city_data.trade.docker_export_resource);
}

int trade_caravan_count(void)
{
    int count = 0;
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (f->type == FIGURE_TRADE_CARAVAN || f->type == FIGURE_TRADE_CARAVAN_DONKEY || f->type == FIGURE_NATIVE_TRADER) {
            count++;
        }
    }
    return count;
}
