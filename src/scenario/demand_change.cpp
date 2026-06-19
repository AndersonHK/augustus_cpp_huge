#include "demand_change.h"

#include "city/message.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "game/resource_id_bridge.h"
#include "game/time.h"
#include "game/save_version.h"
#include "scenario/property.h"

#include <vector>

#define DEMAND_CHANGES_STRUCT_SIZE_CURRENT (1 * sizeof(int32_t) + 1 * sizeof(int16_t) + 4 * sizeof(uint8_t))

static std::vector<demand_change_t> demand_changes;

static demand_change_t new_demand_change(unsigned int index)
{
    demand_change_t demand_change = {};
    demand_change.id = index;
    return demand_change;
}

static int demand_change_in_use(const demand_change_t &demand_change)
{
    return demand_change.year != 0;
}

static demand_change_t *demand_change_slot(unsigned int id)
{
    return id < demand_changes.size() ? &demand_changes[id] : nullptr;
}

static demand_change_t *append_demand_change()
{
    demand_changes.push_back(new_demand_change(static_cast<unsigned int>(demand_changes.size())));
    return &demand_changes.back();
}

static void resize_demand_changes(size_t size)
{
    demand_changes.clear();
    demand_changes.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_demand_change();
    }
}

static demand_change_t *create_demand_change()
{
    for (demand_change_t &demand_change : demand_changes) {
        if (!demand_change_in_use(demand_change)) {
            demand_change = new_demand_change(demand_change.id);
            return &demand_change;
        }
    }
    return append_demand_change();
}

static void trim_demand_changes()
{
    while (demand_changes.size() > 1 && !demand_change_in_use(demand_changes.back())) {
        demand_changes.pop_back();
    }
}

void scenario_demand_change_clear_all(void)
{
    demand_changes.clear();
}

void scenario_demand_change_init(void)
{
    for (demand_change_t &demand_change : demand_changes) {
        random_generate_next();
        if (demand_change.year) {
            demand_change.month = (random_byte() & 7) + 2;
        }
    }
}

int scenario_demand_change_new(void)
{
    demand_change_t *demand_change = create_demand_change();
    return demand_change ? demand_change->id : -1;
}

static void process_demand_change(demand_change_t *demand_change)
{
    if (!demand_change->year) {
        return;
    }
    if (game_time_year() != demand_change->year + scenario_property_start_year() ||
        game_time_month() != demand_change->month) {
        return;
    }
    int buys = demand_change->buys;
    int route = demand_change->route_id;
    int resource = demand_change->resource;
    int city_id = empire_city_get_for_trade_route(route);
    if (city_id < 0) {
        city_id = 0;
    }

    int last_amount = trade_route_limit(route, resource, buys);
    int amount = demand_change->amount;
    if (amount == DEMAND_CHANGE_LEGACY_IS_RISE) {
        amount = trade_route_legacy_increase_limit(route, resource, buys);
    } else if (amount == DEMAND_CHANGE_LEGACY_IS_FALL) {
        amount = trade_route_legacy_decrease_limit(route, resource, buys);
    } else {
        trade_route_set_limit(route, resource, amount, buys);
    }
    if (empire_city_is_trade_route_open(route)) {
        int change = amount - last_amount;
        if (amount > 0 && change > 0) {
            city_message_post(1, MESSAGE_INCREASED_TRADING, city_id, resource);
        } else if (amount > 0 && change < 0) {
            city_message_post(1, MESSAGE_DECREASED_TRADING, city_id, resource);
        } else if (amount <= 0) {
            city_message_post(1, MESSAGE_TRADE_STOPPED, city_id, resource);
        }
    }
}

void scenario_demand_change_process(void)
{
    for (demand_change_t &demand_change : demand_changes) {
        process_demand_change(&demand_change);
    }
}

const demand_change_t *scenario_demand_change_get(int id)
{
    return id < 0 ? nullptr : demand_change_slot(static_cast<unsigned int>(id));
}

void scenario_demand_change_update(const demand_change_t *demand_change)
{
    demand_change_t *base_demand_change = demand_change_slot(demand_change->id);
    if (base_demand_change) {
        *base_demand_change = *demand_change;
        trim_demand_changes();
    }
}

void scenario_demand_change_delete(int id)
{
    demand_change_t *demand_change = id < 0 ? nullptr : demand_change_slot(static_cast<unsigned int>(id));
    if (demand_change) {
        *demand_change = new_demand_change(demand_change->id);
        trim_demand_changes();
    }
}

void scenario_demand_change_remap_resource(void)
{
    for (demand_change_t &demand_change : demand_changes) {
        demand_change.resource = static_cast<int>(resource_remap(demand_change.resource));
    }
}

unsigned int scenario_demand_change_count_total(void)
{
    return static_cast<unsigned int>(demand_changes.size());
}

int scenario_demand_change_count_active(void)
{
    int count = 0;
    for (const demand_change_t &demand_change : demand_changes) {
        if (demand_change_in_use(demand_change)) {
            count++;
        }
    }
    return count;
}

void scenario_demand_change_save_state(buffer *buf)
{
    buffer_init_dynamic_array(buf, demand_changes.size(), DEMAND_CHANGES_STRUCT_SIZE_CURRENT);

    for (const demand_change_t &demand_change : demand_changes) {
        buffer_write_i16(buf, demand_change.year);
        buffer_write_u8(buf, demand_change.month);
        buffer_write_u8(buf, demand_change.resource);
        buffer_write_u8(buf, demand_change.route_id);
        buffer_write_i32(buf, demand_change.amount);
        buffer_write_u8(buf, demand_change.buys);
    }
}

void scenario_demand_change_load_state(buffer *buf, scenario_version_t version)
{
    size_t size = buffer_load_dynamic_array(buf);

    resize_demand_changes(size);

    for (size_t i = 0; i < size; i++) {
        demand_change_t *demand_change = demand_change_slot(static_cast<unsigned int>(i));
        demand_change->year = buffer_read_i16(buf);
        demand_change->month = buffer_read_u8(buf);
        demand_change->resource = buffer_read_u8(buf);
        demand_change->route_id = buffer_read_u8(buf);
        demand_change->amount = buffer_read_i32(buf);
        if (version > SCENARIO_LAST_NO_EMPIRE_EDITOR) {
            demand_change->buys = buffer_read_u8(buf);
        } else {
            // Migration not guaranteed to be right (wasn't before as well though)
            int city_id = empire_city_get_for_trade_route(demand_change->route_id);
            if (city_id < 0) {
                demand_change->buys = 1;
                continue;
            }
            demand_change->buys = empire_city_get(city_id)->buys_resource[demand_change->resource];
        }
    }

    trim_demand_changes();
}

void scenario_demand_change_load_state_old_version(buffer *buf, int is_legacy_change)
{
    resize_demand_changes(MAX_ORIGINAL_DEMAND_CHANGES);
    for (demand_change_t &demand_change : demand_changes) {
        demand_change.year = buffer_read_i16(buf);
    }
    for (demand_change_t &demand_change : demand_changes) {
        demand_change.month = buffer_read_u8(buf);
    }
    for (demand_change_t &demand_change : demand_changes) {
        demand_change.resource = buffer_read_u8(buf);
    }
    for (demand_change_t &demand_change : demand_changes) {
        demand_change.route_id = buffer_read_u8(buf);
    }
    if (is_legacy_change) {
        for (demand_change_t &demand_change : demand_changes) {
            int is_rise = buffer_read_u8(buf);
            int amount = is_rise ? DEMAND_CHANGE_LEGACY_IS_RISE : DEMAND_CHANGE_LEGACY_IS_FALL;
            demand_change.amount = amount;
        }
    } else {
        for (demand_change_t &demand_change : demand_changes) {
            demand_change.amount = buffer_read_i32(buf);
        }
    }
    // Migration
    for (demand_change_t &demand_change : demand_changes) {
        int city_id = empire_city_get_for_trade_route(demand_change.route_id);
        if (city_id < 0) {
            demand_change.buys = 1;
            continue;
        }
        demand_change.buys = empire_city_get(city_id)->buys_resource[demand_change.resource];
    }
    
    trim_demand_changes();
}
