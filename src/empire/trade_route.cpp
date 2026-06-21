#include "trade_route.h"

#include "core/log.h"
#include "empire/city.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"

#include <string.h>
#include <vector>

struct route_resource {
    int limit[RESOURCE_SLOT_COUNT];
    int traded[RESOURCE_SLOT_COUNT];
};

struct trade_route {
    route_resource buys;
    route_resource sells;
};

static std::vector<trade_route> routes;

static trade_route *route_at(int route_id)
{
    if (route_id < 0 || route_id >= static_cast<int>(routes.size())) {
        return nullptr;
    }
    return &routes[route_id];
}

static int resource_is_valid(resource_type resource)
{
    return resource >= RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT;
}

int trade_route_init(void)
{
    routes.clear();
    routes.resize(1); // Discard route 0
    return 1;
}

int trade_route_new(void)
{
    routes.emplace_back();
    return static_cast<int>(routes.size() - 1);
}

int trade_route_count(void)
{
    return static_cast<int>(routes.size());
}

int trade_route_is_valid(int route_id)
{
    return route_id >= 0 && route_id < static_cast<int>(routes.size());
}

void trade_route_set(int route_id, resource_type resource, int limit, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route || !resource_is_valid(resource)) {
        return;
    }
    if (buying) {
        route->buys.limit[resource] = limit;
        route->buys.traded[resource] = 0;
    } else {
        route->sells.limit[resource] = limit;
        route->sells.traded[resource] = 0;
    }
}

int trade_route_limit(int route_id, resource_type resource, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route || !resource_is_valid(resource)) {
        return 0;
    }
    return buying ? route->buys.limit[resource] : route->sells.limit[resource];
}

int trade_route_traded(int route_id, resource_type resource, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route || !resource_is_valid(resource)) {
        return 0;
    }
    return buying ? route->buys.traded[resource] : route->sells.traded[resource];
}

void trade_route_set_limit(int route_id, resource_type resource, int amount, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route || !resource_is_valid(resource)) {
        return;
    }
    if (buying) {
        route->buys.limit[resource] = amount;
    } else {
        route->sells.limit[resource] = amount;
    }
}

static route_resource *get_route_resource(int route_id, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route) {
        return nullptr;
    }
    if (buying) {
        return &route->buys;
    } else {
        return &route->sells;
    }
}

int trade_route_legacy_increase_limit(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    if (!route || !resource_is_valid(resource)) {
        return 0;
    }
    switch (route->limit[resource]) {
        case 0: route->limit[resource] = 15; break;
        case 15: route->limit[resource] = 25; break;
        case 25: route->limit[resource] = 40; break;
    }
    return route->limit[resource];
}

int trade_route_legacy_decrease_limit(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    if (!route || !resource_is_valid(resource)) {
        return 0;
    }
    switch (route->limit[resource]) {
        case 40: route->limit[resource] = 25; break;
        case 25: route->limit[resource] = 15; break;
        case 15: route->limit[resource] = 0; break;
    }
    return route->limit[resource];
}

void trade_route_increase_traded(int route_id, resource_type resource, int buying)
{
    trade_route *route = route_at(route_id);
    if (!route || !resource_is_valid(resource)) {
        return;
    }
    if (buying) {
        route->buys.traded[resource]++;
    } else {
        route->sells.traded[resource]++;
    }
}

void trade_route_reset_traded(int route_id)
{
    trade_route *route = route_at(route_id);
    if (!route) {
        return;
    }
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        route->buys.traded[r] = route->sells.traded[r] = 0;
    }
}

int trade_route_limit_reached(int route_id, resource_type resource, int buying)
{
    route_resource *route = get_route_resource(route_id, buying);
    if (!route || !resource_is_valid(resource)) {
        return 1;
    }
    return route->traded[resource] >= route->limit[resource];
}

void trade_routes_save_state(buffer *trade_routes)
{
    int route_count = static_cast<int>(routes.size());
    int buf_size = sizeof(int32_t) * RESOURCE_SLOT_COUNT * 2 * route_count * 2;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size + sizeof(int32_t)));
    buffer_init(trade_routes, buf_data, buf_size + sizeof(int32_t));
    buffer_write_i32(trade_routes, route_count);

    for (trade_route &route : routes) {
        for (int i = 0; i < 2; i++) {
            for (resource_type r = 0; r < RESOURCE_SLOT_COUNT; r++) {
                buffer_write_i32(trade_routes, i ? route.buys.limit[r] : route.sells.limit[r]);
                buffer_write_i32(trade_routes, i ? route.buys.traded[r] : route.sells.traded[r]);
            }
        }
    }
}

void trade_routes_load_state(buffer *trade_routes)
{
    int routes_to_load = buffer_read_i32(trade_routes);
    routes.clear();
    routes.resize(routes_to_load);
    for (int i = 0; i < routes_to_load; i++) {
        trade_route *route = route_at(i);
        if (!route) {
            continue;
        }
        for (int i = 0; i < 2; i++) {
            for (int r = 0; r < resource_total_mapped(); r++) {
                resource_type remapped = resource_remap(r);
                if (i) {
                    route->buys.limit[remapped] = buffer_read_i32(trade_routes);
                    route->buys.traded[remapped] = buffer_read_i32(trade_routes);
                } else {
                    route->sells.limit[remapped] = buffer_read_i32(trade_routes);
                    route->sells.traded[remapped] = buffer_read_i32(trade_routes);
                }
            }
        }
    }
}

void trade_routes_migrate_to_buys_sells(buffer *limit, buffer *traded, int version)
{
    int routes_to_load = version <= SAVE_GAME_LAST_STATIC_SCENARIO_OBJECTS ? LEGACY_MAX_ROUTES : buffer_read_i32(limit);
    routes.clear();
    routes.resize(routes_to_load);
    for (int i = 0; i < routes_to_load; i++) {
        trade_route *route = route_at(i);
        if (!route) {
            continue;
        }
        int city_id = empire_city_get_for_trade_route(i);
        if (city_id < 0) {
            continue;
        }
        for (int r = 0; r < resource_total_mapped(); r++) {
            resource_type remapped = resource_remap(r);
            int limit_amount = buffer_read_i32(limit);
            int traded_amount = buffer_read_i32(traded);
            if (empire_city_buys_resource(city_id, remapped)) {
                route->buys.limit[remapped] = limit_amount;
                route->buys.traded[remapped] = traded_amount;
                route->sells.limit[remapped] = route->sells.traded[remapped] = 0;
            } else if (empire_city_sells_resource(city_id, remapped)) {
                route->sells.limit[remapped] = limit_amount;
                route->sells.traded[remapped] = traded_amount;
                route->buys.limit[remapped] = route->buys.traded[remapped] = 0;
            } else {
                route->sells.limit[remapped] = route->sells.traded[remapped] =
                    route->buys.limit[remapped] = route->buys.traded[remapped] = 0;
            }
        }
    }
}
