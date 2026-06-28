#include "building/count.h"
#include "building/industry.h"
#include "building/storage.h"

#include "warehouse.h"

#include "building/barracks.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"


#include "building/monument.h"
#include "building/properties.h"
#include "city/finance.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/config.h"
#include "empire/trade_prices.h"
#include "figure/figure.h"
#include "game/tutorial.h"
#include "map/grid.h"
#include "scenario/property.h"


#include "building/granary.h"

#include <algorithm>
#include <optional>

#define INFINITE 10000
#define MAX_CARTLOADS_PER_SPACE 4

static void refresh_warehouse_space_graphic(Building &space);

static int warehouse_resource_is_valid(resource_type resource)
{
    return resource > RESOURCE_NONE && resource < RESOURCE_SLOT_COUNT;
}

static int warehouse_space_assigned_loads(const Building &space)
{
    const resource_type resource = space.warehouse_resource_id();
    return warehouse_resource_is_valid(resource) ? space.resource_amount(resource) : 0;
}

static int warehouse_space_is_empty_or_stale(const Building &space)
{
    const resource_type resource = space.warehouse_resource_id();
    return !warehouse_resource_is_valid(resource) || warehouse_space_assigned_loads(space) <= 0;
}

static void clear_empty_or_invalid_warehouse_space_assignment(Building &space)
{
    const resource_type resource = space.warehouse_resource_id();
    if (!warehouse_resource_is_valid(resource)) {
        if (resource != RESOURCE_NONE) {
            space.set_warehouse_resource_id(RESOURCE_NONE);
        }
        return;
    }
    if (space.resource_amount(resource) <= 0) {
        space.set_resource_amount(resource, 0);
        space.set_warehouse_resource_id(RESOURCE_NONE);
    }
}

static building_type warehouse_type_id()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        for (const auto &definition : building_type_registry_impl::g_building_types) {
            if (definition && definition->is_warehouse()) {
                type = definition->type();
                break;
            }
        }
    }
    return type;
}

Building building_warehouse_first()
{
    return Building::first_of_type(warehouse_type_id());
}

static std::optional<Building> warehouse_first()
{
    for (Building building : Building::of_type(warehouse_type_id())) {
        return building;
    }
    return std::nullopt;
}

static std::optional<Building> warehouse_main_for_storage(const Building &building)
{
    if (building.type && building.type->is_warehouse()) {
        return building;
    }
    if (!building.id) {
        return std::nullopt;
    }
    Building main = building.main();
    if (main.type && main.type->is_warehouse()) {
        return main;
    }
    return std::nullopt;
}

int building_warehouse_get_space_info(const Building &warehouse)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return 0;
    }
    int total_loads = 0;
    int empty_spaces = 0;
    Building space = *main;
    for (int i = 0; i < 8; i++) {
        space = space.next();
        if (!space.id) {
            return 0;
        }
        const int stored_loads = warehouse_space_assigned_loads(space);
        if (warehouse_space_is_empty_or_stale(space)) {
            empty_spaces++;
        } else {
            total_loads += stored_loads;
        }
    }
    if (empty_spaces > 0) {
        return WAREHOUSE_ROOM;
    } else if (total_loads < FULL_WAREHOUSE) {
        return WAREHOUSE_SOME_ROOM;
    } else {
        return WAREHOUSE_FULL;
    }
}

int building_warehouse_get_main_grid_offset(const Building &warehouse)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return warehouse.grid_offset();
    }
    int lowest_x = main->x();
    int lowest_y = main->y();
    Building next = *main;
    for (int i = 0; i < 9; i++) {
        next = next.next();
        if (!next.id) {
            break;
        }
        if (next.x() < lowest_x) {
            lowest_x = next.x();
        }
        if (next.y() < lowest_y) {
            lowest_y = next.y();
        }

    }
    return map_grid_offset(lowest_x, lowest_y);
}

int building_warehouse_get_tower_grid_offset(const Building &warehouse)
{
    std::optional<Building> tower = warehouse_main_for_storage(warehouse);
    return tower ? tower->grid_offset() : warehouse.grid_offset();
}

int building_warehouse_get_amount(const Building &warehouse, resource_type resource)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return 0;
    }
    int loads = 0;
    Building space = *main;
    for (int i = 0; i < 8; i++) {
        space = space.next();
        if (!space.id) {
            return 0;
        }
        if (space.warehouse_resource_id() == resource) {
            loads += space.resource_amount(resource);
        }
    }
    return loads;
}

int building_warehouse_get_free_space_amount(Building &warehouse)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return 0;
    }
    building_warehouse_recount_resources(*main); //recount, since free space in warehouse is tied to individual spaces
    return main->resource_amount(RESOURCE_NONE);
}

int building_warehouse_get_available_amount(const Building &warehouse, resource_type resource)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main || !main->is_in_use() || main->has_plague()) {
        return 0;
    }

    if (building_storage_get_state(*main, resource, 1) == BUILDING_STORAGE_STATE_MAINTAINING) {
        return 0;
    }

    return building_warehouse_get_amount(*main, resource);
}

static std::optional<Building> building_warehouse_find_space(const Building &warehouse, resource_type resource, int adding)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return std::nullopt;
    }
    Building space = *main;

    if (adding) {
        // Step 1: Try partially filled bay
        for (int i = 0; i < 8; i++) {
            space = space.next();
            if (!space.id) {
                return std::nullopt;
            }
            const resource_type stored_resource = space.warehouse_resource_id();
            if (!warehouse_resource_is_valid(stored_resource) || stored_resource != resource) {
                continue;
            }
            if (space.resource_amount(resource) > 0 &&
                space.resource_amount(resource) < MAX_CARTLOADS_PER_SPACE) {
                return space;
            }
        }

        // Step 2: Try empty or assignable bay
        space = *main;
        for (int i = 0; i < 8; i++) {
            space = space.next();
            if (!space.id) {
                return std::nullopt;
            }
            if ((space.warehouse_resource_id() == resource || warehouse_space_is_empty_or_stale(space)) &&
                space.resource_amount(resource) < MAX_CARTLOADS_PER_SPACE) {
                return space;
            }
        }
    } else {
        // Removing: find first bay with at least one unit of the resource
        for (int i = 0; i < 8; i++) {
            space = space.next();
            if (!space.id) {
                return std::nullopt;
            }
            if (space.warehouse_resource_id() == resource &&
                space.resource_amount(resource) > 0) {
                return space;
            }
        }
    }
    return std::nullopt;
}

void building_warehouse_recount_resources(Building &main)
{
    std::optional<Building> resolved_main = warehouse_main_for_storage(main);
    if (!resolved_main) {
        return;
    }
    main = *resolved_main;
    //helper to reflect the resources in the main warehouse, like granary does
    if (!main.type || !main.type->is_warehouse()) {
        return;
    }

    // Reset all resource counters in the main warehouse
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        main.set_resource_amount(r, 0);
    }

    Building space = main;
    for (int i = 0; i < 8; i++) {
        space = space.next();
        if (!space.id) {
            continue;
        }

        resource_type resource = space.warehouse_resource_id();
        if (warehouse_resource_is_valid(resource) && space.resource_amount(resource) > 0) {
            main.add_resource(resource, space.resource_amount(resource));
        } else {
            clear_empty_or_invalid_warehouse_space_assignment(space);
        }
        refresh_warehouse_space_graphic(space);
    }
    // Total sum of all loads (regardless of type)
    int total_loads = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        total_loads += main.resource_amount(r);
    }
    main.set_resource_amount(RESOURCE_NONE, BUILDING_STORAGE_QUANTITY_MAX - total_loads);
}

int building_warehouse_try_add_resource(
    Building &b, resource_type resource, int quantity, int respect_settings, unsigned int ignore_figure_id)
{
    std::optional<Building> warehouse = warehouse_main_for_storage(b);
    if (!warehouse || quantity <= 0 || !resource) {
        return 0;
    }
    signed short max_acceptable = respect_settings ?
        building_warehouse_maximum_receptible_amount(*warehouse, resource, ignore_figure_id) :
        building_warehouse_get_free_space_amount(*warehouse);
    if (!max_acceptable) {
        return 0;
    }
    if (quantity > max_acceptable) { //if trying to add more than acceptable, limit it
        quantity = max_acceptable;
    }
    signed short added = 0;
    while (added < quantity) {
        std::optional<Building> space = building_warehouse_find_space(*warehouse, resource, 1);
        if (!space) {
            break;
        }
        signed short space_remaining = MAX_CARTLOADS_PER_SPACE - space->resource_amount(resource);
        //we cannot ignore the individual space limitations, since it will affect the image shown
        //we cannot mix multiple resources in one space either
        signed short to_add = ((quantity - added) < space_remaining) ? (quantity - added) : space_remaining;

        space->add_resource(resource, to_add);
        space->set_warehouse_resource_id(resource);
        added += to_add;

        city_resource_add_to_warehouse(static_cast<resource_type>(resource), to_add);
        refresh_warehouse_space_graphic(*space);
    }

    if (added) {
        building_warehouse_recount_resources(*warehouse);
        tutorial_on_add_to_warehouse();
    }
    return added;
}

int building_warehouses_add_resource(resource_type resource, int amount, int respect_settings)
{
    if (amount <= 0) {
        return 0;
    }

    for (Building b : Building::of_type(warehouse_type_id())) {
        if (amount <= 0) {
            break;
        }
        if (!b.is_in_use()) {
            continue;
        }

        int was_added = building_warehouse_try_add_resource(b, resource, amount, respect_settings);
        amount -= was_added;
    }

    return amount;
}

int building_warehouse_try_remove_resource(Building &warehouse, resource_type resource, int desired_amount)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (desired_amount <= 0 || !resource) {
        return 0;
    }
    if (!main || main->has_plague()) {
        return 0;
    }
    int remaining_desired = desired_amount;
    int removed_amount = 0;
    Building space = *main;
    for (int i = 0; i < 8; i++) {
        if (remaining_desired <= 0) {
            building_warehouse_recount_resources(*main);
            return removed_amount;
        }
        space = space.next();
        if (!space.id) {
            continue;
        }
        if (space.warehouse_resource_id() != resource || space.resource_amount(resource) <= 0) {
            continue;
        }
        if (space.resource_amount(resource) > remaining_desired) {
            removed_amount += remaining_desired;
            city_resource_remove_from_warehouse(static_cast<resource_type>(resource), remaining_desired);
            space.add_resource(resource, -remaining_desired);
            remaining_desired = 0;
        } else {
            const int stored = space.resource_amount(resource);
            removed_amount += stored;
            city_resource_remove_from_warehouse(static_cast<resource_type>(resource), stored);
            remaining_desired -= stored;
            space.set_resource_amount(resource, 0);
            space.set_warehouse_resource_id(RESOURCE_NONE);
        }
        refresh_warehouse_space_graphic(space);
    }
    if (removed_amount) {
        building_warehouse_recount_resources(*main);
    }
    return removed_amount;
}

void building_warehouse_remove_resource_curse(Building &warehouse, int amount)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main || !main->type || !main->type->is_warehouse()) {
        return;
    }

    Building space = *main;
    for (int i = 0; i < 8 && amount > 0; i++) {
        space = space.next();
        resource_type resource = space.warehouse_resource_id();
        if (!space.id) {
            continue;
        }
        if (!warehouse_resource_is_valid(resource) || space.resource_amount(resource) <= 0) {
            clear_empty_or_invalid_warehouse_space_assignment(space);
            refresh_warehouse_space_graphic(space);
            continue;
        }
        if (space.resource_amount(resource) > amount) {
            city_resource_remove_from_warehouse(static_cast<resource_type>(resource), amount);
            space.add_resource(resource, -amount);
            amount = 0;
        } else {
            const int stored = space.resource_amount(resource);
            city_resource_remove_from_warehouse(static_cast<resource_type>(resource), stored);
            amount -= stored;
            space.set_resource_amount(resource, 0);
            space.set_warehouse_resource_id(RESOURCE_NONE);
        }
        refresh_warehouse_space_graphic(space);
    }
    building_warehouse_recount_resources(*main);
}

static void refresh_warehouse_space_graphic(Building &space)
{
    space.refresh_graphic_if_native();
}

int building_warehouse_add_import(Building &warehouse, resource_type resource, int amount, int trader_type)
{
    std::optional<Building> target = warehouse_main_for_storage(warehouse);
    if (!target || resource == RESOURCE_NONE || amount <= 0) {
        return 0;
    }
    building_storage_permission_states permission;
    switch (trader_type) {
        default:
        case 0: // sea trader
            permission = BUILDING_STORAGE_PERMISSION_DOCK;
            break;
        case 1: // land trader
            permission = BUILDING_STORAGE_PERMISSION_TRADERS;
            break;
        case -1: //native trader
            permission = BUILDING_STORAGE_PERMISSION_NATIVES;
            trader_type = 1; // native trader is always land trader
            break;
    }
    if (!building_storage_get_permission(permission, *target)) {
        return 0; // cannot import to this warehouse
    }
    if (building_storage_get_state(*target, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // cannot accept this resource
    }
    int added_amount = building_warehouse_try_add_resource(*target, resource, 1, 1);
    if (added_amount <= 0) {
        return 0; // no space to add
    }
    int price = trade_price_buy(static_cast<resource_type>(resource), trader_type);
    city_finance_process_import(price * added_amount);
    return 1;
}

int building_warehouse_remove_export(Building &warehouse, resource_type resource, int amount, int trader_type)
{
    std::optional<Building> target = warehouse_main_for_storage(warehouse);
    if (!target || resource == RESOURCE_NONE || amount <= 0) {
        return 0; // invalid resource or amount
    }
    building_storage_permission_states permission;
    switch (trader_type) {
        default:
        case 0: // sea trader
            permission = BUILDING_STORAGE_PERMISSION_DOCK;
            break;
        case 1: // land trader
            permission = BUILDING_STORAGE_PERMISSION_TRADERS;
            break;
        case -1: //native trader
            permission = BUILDING_STORAGE_PERMISSION_NATIVES;
            trader_type = 1; // native trader is always land trader
            break;
    }

    if (!building_storage_get_permission(permission, *target)) {
        return 0; // cannot export from this warehouse
    }
    int removed_amount = building_warehouse_try_remove_resource(*target, resource, amount);
    int price = trade_price_sell(static_cast<resource_type>(resource), trader_type);
    city_finance_process_export(price * removed_amount);
    return removed_amount;
}

static std::optional<Building> get_next_warehouse(void)
{
    unsigned int building_id = city_resource_last_used_warehouse();
    std::optional<Building> first_in_use;
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!b.is_in_use()) {
            continue;
        }
        if (!first_in_use) {
            first_in_use = b;
        }
        if (b.id > building_id) {
            return b;
        }
    }
    return first_in_use;
}

static int warehouse_allows_getting(const Building &b, resource_type resource)
{
    const building_storage *s = building_storage_get(b.storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (b.has_plague() || (entry->state >= BUILDING_STORAGE_STATE_GETTING)) {
        return 0;
        //if the building has plague or gets or maintains resource - it doesnt allow getting
    }

    return 1;
}

static int get_acceptable_quantity(const Building &b, resource_type resource)
{
    const building_storage *s = building_storage_get(b.storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    const building_storage_state state = building_storage_get_state(b, resource, 1);
    if (state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // not accepting this resource
    } else {
        return entry->quantity;
    }
}

static int building_warehouse_max_space_for_resource(const Building &b, resource_type resource)
{
    std::optional<Building> main = warehouse_main_for_storage(b);
    if (!main) {
        return 0;
    }
    // internal function to check space with respect to tiled storage - keep static
    int max_storable = 0;
    Building space = *main;
    for (int i = 0; i < 8; i++) {
        space = space.next();
        if (!space.id) {
            return 0;
        }
        const resource_type stored_resource = space.warehouse_resource_id();
        const int stored_loads = warehouse_space_assigned_loads(space);
        if (stored_resource == resource && stored_loads > 0) {
            max_storable += MAX_CARTLOADS_PER_SPACE - stored_loads;
        } else if (warehouse_space_is_empty_or_stale(space)) {
            max_storable += MAX_CARTLOADS_PER_SPACE;
        }
    }
    return max_storable;
}

int building_warehouse_maximum_receptible_amount(
    Building &warehouse, resource_type resource, unsigned int ignore_figure_id)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main) {
        return 0;
    }
    building_warehouse_recount_resources(*main);
    if (main->has_plague() || building_storage_get_empty_all(main->id) ||
        !main->is_in_use() || main->resource_amount(RESOURCE_NONE) <= 0) {
        return 0;
    }
    if (building_storage_get_state(*main, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return 0; // early check for relative state
    }
    const int max_allowed = get_acceptable_quantity(*main, resource);
    const int current_amount = building_warehouse_get_amount(*main, resource);
    const int reserved_inbound = main->reserved_legacy_storage_loads(resource, ignore_figure_id);
    const int reserved_inbound_total = main->reserved_legacy_storage_loads(RESOURCE_NONE, ignore_figure_id);
    const int remaining_allowed = (max_allowed > current_amount + reserved_inbound) ?
        (max_allowed - current_amount - reserved_inbound) : 0;

    const int resource_space_limit = building_warehouse_max_space_for_resource(*main, resource); // max by tile layout
    const int free_space_overall = main->resource_amount(RESOURCE_NONE); // total free space
    const int free_space_after_reserved = std::max(0, free_space_overall - reserved_inbound_total);

    const int available_space = std::min(free_space_after_reserved, resource_space_limit); // tile storage and free space
    const int max_receptible = std::min(remaining_allowed, available_space);
    // Max the building is allowed to receive, considering all limits
    // allowed remaining is the amount that can be added to the warehouse considering set limit and current storage

    return max_receptible;
}

int building_warehouses_count_available_resource(resource_type resource, int respect_maintaining, int caesars_request)
{
    int total = 0;
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!b.is_in_use() || (caesars_request &&
            !building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b))) {
            continue;
        }
        if (!respect_maintaining ||
            building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_MAINTAINING ||
            building_storage_get_empty_all(b.id)) {
            total += building_warehouse_get_amount(b, resource);
        }
    }
    return total;
}

static void try_create_cart_to_rome(const Building &b, resource_type resource, int loads)
{
    map_point road;
    if (map_has_road_access_rotation(b.orientation(), b.x(), b.y(), 3, &road)) {
        Figure *f = Figure::create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
        f->action_state = FIGURE_ACTION_234_CARTPUSHER_GOING_TO_ROME_CREATED;
        f->resource_id = resource;
        f->loads_sold_or_carrying = loads;
        f->building = b;
    }
}

int building_warehouses_send_resources_to_rome(resource_type resource, int amount)
{
    // First go for emptying or non-getting, non-maintaining warehouses with Caesar permission.
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!amount) {
            break;
        }
        if (b.is_in_use()) {
            if ((building_storage_get_empty_all(b.id) ||
                 building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_GETTING) &&
                building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
                int taken_loads = building_warehouse_try_remove_resource(b, resource, amount);
                amount -= taken_loads;
                if (taken_loads) {
                    try_create_cart_to_rome(b, resource, taken_loads);
                }
            }
        }
    }
    if (amount <= 0) {
        return 0;
    }
    // if that doesn't work, take it anyway, but not from maintaining and no caesar permission warehouses
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!amount) {
            break;
        }
        if (b.is_in_use()) {
            if ((building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_MAINTAINING) &&
                building_storage_get_permission(BUILDING_STORAGE_PERMISSION_CAESAR, b)) {
                int taken_loads = building_warehouse_try_remove_resource(b, resource, amount);
                amount -= taken_loads;
                if (taken_loads) {
                    try_create_cart_to_rome(b, resource, taken_loads);
                }
            }
        }
    }
    return amount;
}

int building_warehouses_remove_resource(resource_type resource, int amount)
{
    std::optional<Building> next_warehouse = get_next_warehouse();
    if (!next_warehouse) {
        return amount;
    }
    Building b = *next_warehouse;
    const unsigned int initial_warehouse_id = b.id;

    // First go for non-getting, non-maintaining warehouses
    do {
        if (b.is_in_use()) {
            if (building_storage_get_state(b, resource, 1) < BUILDING_STORAGE_STATE_GETTING) {
                city_resource_set_last_used_warehouse(b.id);
                amount -= building_warehouse_try_remove_resource(b, resource, amount);
            }
        }
        Building next = b.next_of_type();
        if (next.id) {
            b = next;
        } else if (std::optional<Building> first = warehouse_first()) {
            b = *first;
        } else {
            break;
        }
    } while (b.id != initial_warehouse_id && amount > 0);

    if (amount <= 0) {
        return 0;
    }

    // If that doesn't work, take it anyway
    do {
        if (b.is_in_use()) {
            city_resource_set_last_used_warehouse(b.id);
            amount -= building_warehouse_try_remove_resource(b, resource, amount);
        }
        Building next = b.next_of_type();
        if (next.id) {
            b = next;
        } else if (std::optional<Building> first = warehouse_first()) {
            b = *first;
        } else {
            break;
        }
    } while (b.id != initial_warehouse_id && amount > 0);

    return amount;
}

int building_warehouse_accepts_storage(Building &warehouse, resource_type resource, int *understaffed)
{
    std::optional<Building> main = warehouse_main_for_storage(warehouse);
    if (!main || !main->is_in_use() || !main->type || !main->type->is_warehouse() ||
        !main->has_cached_road_access() || main->distance_from_entry() <= 0 || main->has_plague()) {
        return 0;
    }
    if (building_storage_get_state(*main, resource, 1) == BUILDING_STORAGE_STATE_NOT_ACCEPTING ||
        building_storage_get_empty_all(main->id)) {
        return 0;
    }
    int pct_workers = calc_percentage(main->worker_count(), main->type ? main->type->required_workers() : 0);
    if (pct_workers < 100) {
        if (understaffed) {
            *understaffed += 1;
        }
        return 0;
    }
    if (building_warehouse_max_space_for_resource(*main, resource)) {
        return 1;
    }
    return 0;
}

int building_warehouse_for_storing(int src_building_id, int x, int y, resource_type resource, int road_network_id,
    int *understaffed, map_point *dst)
{
    int min_dist = INFINITE;
    std::optional<Building> nearest_warehouse;
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (b.id == (unsigned int) src_building_id ||
            (road_network_id != -1 && b.road_network_id() != road_network_id) ||
            !building_warehouse_accepts_storage(b, resource, understaffed) ||
            (building_warehouse_maximum_receptible_amount(b, resource) <= 0)) {
            continue;
        }
        int dist = b.max_distance_to(x, y);
        if (dist < min_dist) {
            min_dist = dist;
            nearest_warehouse = b;
        }
    }
    if (!nearest_warehouse) {
        return 0;
    }
    if (nearest_warehouse->has_cached_road_access() == 1) {
        map_point_store_result(nearest_warehouse->x(), nearest_warehouse->y(), dst);
    } else if (!map_has_road_access_warehouse(nearest_warehouse->x(), nearest_warehouse->y(), dst)) {
        return 0;
    }
    return nearest_warehouse->id;
}

int building_warehouse_amount_can_get_from(const Building &destination, resource_type resource)
{
    std::optional<Building> main = warehouse_main_for_storage(destination);
    if (!main) {
        return 0;
    }
    int loads_stored = 0;
    Building space = *main;
    for (int t = 0; t < 8; t++) {
        space = space.next();
        if (space.id && space.warehouse_resource_id() == resource) {
            loads_stored += space.resource_amount(resource);
        }
    }
    return loads_stored;
}

int building_warehouse_for_getting(const Building &src, resource_type resource, map_point *dst)
{
    int min_dist = INFINITE;
    std::optional<Building> min_building;
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!b.is_in_use() || b.has_plague()) {
            continue;
        }
        if (b.id == src.id) {
            continue;
        }
        int loads_stored = building_warehouse_amount_can_get_from(b, resource);
        if (loads_stored > 0 && warehouse_allows_getting(b, resource)) {
            int dist = b.max_distance_to(src);
            dist -= 4 * loads_stored;
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        if (dst) {
            min_building->cached_road_access_point(dst);
        }
        return min_building->id;
    } else {
        return 0;
    }
}

int building_warehouse_with_resource(int x, int y, resource_type resource, int road_network_id,
     int *understaffed, map_point *dst, building_storage_permission_states p)
{
    int min_dist = INFINITE;
    std::optional<Building> min_building;
    for (Building b : Building::of_type(warehouse_type_id())) {
        if (!b.is_in_use() || b.has_plague()) {
            continue;
        }
        if (!b.has_cached_road_access() || b.distance_from_entry() <= 0 || b.road_network_id() != road_network_id) {
            continue;
        }
        if (!building_storage_get_permission(p, b)) {
            continue;
        }

        int pct_workers = calc_percentage(b.worker_count(), b.type ? b.type->required_workers() : 0);
        if (pct_workers < 100) {
            if (understaffed) {
                *understaffed += 1;
            }
            continue;
        }
        int loads_stored = building_warehouse_amount_can_get_from(b, resource);
        if (loads_stored > 0) {
            int dist = b.max_distance_to(x, y);
            dist -= 2 * loads_stored;
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building) {
        if (dst) {
            min_building->cached_road_access_point(dst);
        }
        return min_building->id;
    } else {
        return 0;
    }
}

int building_warehouse_determine_worker_task(Building &warehouse, int *resource)
{
    int pct_workers = calc_percentage(warehouse.worker_count(), warehouse.type ? warehouse.type->required_workers() : 0);
    if (pct_workers < 50) {
        return WAREHOUSE_TASK_NONE;
    }
    building_warehouse_recount_resources(warehouse);
    Building space(nullptr);
    //TASK 1: emptying takes priority
    if (building_storage_get_empty_all(warehouse.id)) {
        resource_type resource_to_empty = building_storage_get_highest_quantity_resource(warehouse);
        if (resource_to_empty) {
            space = building_warehouse_find_space(warehouse, resource_to_empty, 0);

            if (space.resource_amount(resource_to_empty)) {
                *resource = resource_to_empty;
                return WAREHOUSE_TASK_DELIVERING;
            }

        }
    }
    //TASK 2: getting resources
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        //determine if any of the resources need to be fetched becasuse of 'getting'
        if (building_storage_get_state(warehouse, r, 1) != BUILDING_STORAGE_STATE_GETTING || !resource_is_storable(r)) {
            continue;
        }
        if (!config_get(CONFIG_GP_CH_ENABLE_GETTING_WHILE_STOCKPILED) && city_resource_is_stockpiled(r)) {
            continue; // skip if stockpiled
        }
        unsigned char needed = building_warehouse_maximum_receptible_amount(warehouse, r);

        int fetch_amount = MAX_CARTLOADS_PER_SPACE;
        if (needed >= fetch_amount && fetch_amount > 0) {
            if (!building_warehouse_for_getting(warehouse, r, 0)) {
                continue;
            }
            *resource = r;
            return WAREHOUSE_TASK_GETTING;
        }

    }
    // TASK 3: delivering resources
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_WORKER, warehouse)) {
        return WAREHOUSE_TASK_NONE; //halt resource delivery to workshops and granaries
    }
    // deliver raw materials to workshops
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        const building_storage_state state = building_storage_get_state(warehouse, r, 1);
        if (warehouse.resource_amount(r) <= 0 || !resource_is_raw_material(r) || city_resource_is_stockpiled(r) ||
             state == BUILDING_STORAGE_STATE_GETTING || state == BUILDING_STORAGE_STATE_MAINTAINING ||
             !building_has_workshop_for_raw_material_with_room(r, warehouse.road_network_id())) {
            continue; // skip if no resource, not raw material, held by policy, stockpiled or no workshop
        }
        *resource = r;
        return WAREHOUSE_TASK_DELIVERING;
    }
    // deliver food to granaries
    unsigned char delivering_food = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        const building_storage_state state = building_storage_get_state(warehouse, r, 1);
        if (warehouse.resource_amount(r) <= 0 || !resource_is_food(r) || city_resource_is_stockpiled(r) ||
            state == BUILDING_STORAGE_STATE_GETTING || state == BUILDING_STORAGE_STATE_MAINTAINING) {
            continue; // skip if no resource, not food, held by policy, or stockpiled
        }
        if (building_granary_get_granary_needing_food(warehouse, r, 1).id) {
            *resource = r;
            delivering_food = 1;
            break; //found a getting granary in need of food
        }
        if (building_granary_get_granary_needing_food(warehouse, r, 0).id) {
            *resource = r; // keep checking in case there is a getting granary
            delivering_food = 1;
        }

    }
    if (delivering_food) {
        return WAREHOUSE_TASK_DELIVERING;
    }
    // Idle
    return WAREHOUSE_TASK_NONE;
}
