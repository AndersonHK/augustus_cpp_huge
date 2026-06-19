#include "price_change.h"

#include "city/message.h"
#include "core/log.h"
#include "core/random.h"
#include "game/resource_id_bridge.h"
#include "empire/trade_prices.h"
#include "game/time.h"
#include "scenario/property.h"

#include <vector>

#define PRICE_CHANGES_STRUCT_SIZE_CURRENT (1 * sizeof(int16_t) + 4 * sizeof(uint8_t))

static std::vector<price_change_t> price_changes;

static price_change_t new_price_change(unsigned int index)
{
    price_change_t price_change = {};
    price_change.id = index;
    return price_change;
}

static int price_change_in_use(const price_change_t &price_change)
{
    return price_change.year != 0;
}

static price_change_t *price_change_slot(unsigned int id)
{
    return id < price_changes.size() ? &price_changes[id] : nullptr;
}

static price_change_t *append_price_change()
{
    price_changes.push_back(new_price_change(static_cast<unsigned int>(price_changes.size())));
    return &price_changes.back();
}

static void resize_price_changes(size_t size)
{
    price_changes.clear();
    price_changes.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_price_change();
    }
}

static price_change_t *create_price_change()
{
    for (price_change_t &price_change : price_changes) {
        if (!price_change_in_use(price_change)) {
            price_change = new_price_change(price_change.id);
            return &price_change;
        }
    }
    return append_price_change();
}

static void trim_price_changes()
{
    while (price_changes.size() > 1 && !price_change_in_use(price_changes.back())) {
        price_changes.pop_back();
    }
}

void scenario_price_change_clear_all(void)
{
    price_changes.clear();
}

void scenario_price_change_init(void)
{
    for (price_change_t &price_change : price_changes) {
        random_generate_next();
        if (price_change.year) {
            price_change.month = (random_byte() & 7) + 2;
        }
    }
}

int scenario_price_change_new(void)
{
    price_change_t *price_change = create_price_change();
    return price_change ? price_change->id : -1;
}

static void process_price_change(price_change_t *price_change)
{
    if (!price_change->year) {
        return;
    }
    if (game_time_year() != price_change->year + scenario_property_start_year() ||
        game_time_month() != price_change->month) {
        return;
    }
    if (price_change->is_rise) {
        if (trade_price_change(price_change->resource, price_change->amount)) {
            city_message_post(1, MESSAGE_PRICE_INCREASED, price_change->amount, price_change->resource);
        }
    } else {
        if (trade_price_change(price_change->resource, -price_change->amount)) {
            city_message_post(1, MESSAGE_PRICE_DECREASED, price_change->amount, price_change->resource);
        }
    }
}

void scenario_price_change_process(void)
{
    for (price_change_t &price_change : price_changes) {
        process_price_change(&price_change);
    }
}

const price_change_t *scenario_price_change_get(int id)
{
    return id < 0 ? nullptr : price_change_slot(static_cast<unsigned int>(id));
}

void scenario_price_change_update(const price_change_t *price_change)
{
    price_change_t *base_price_change = price_change_slot(price_change->id);
    if (base_price_change) {
        *base_price_change = *price_change;
        trim_price_changes();
    }
}

void scenario_price_change_delete(int id)
{
    price_change_t *price_change = id < 0 ? nullptr : price_change_slot(static_cast<unsigned int>(id));
    if (price_change) {
        *price_change = new_price_change(price_change->id);
        trim_price_changes();
    }
}

void scenario_price_change_remap_resource(void)
{
    for (price_change_t &price_change : price_changes) {
        price_change.resource = static_cast<int>(resource_remap(price_change.resource));
    }
}

unsigned int scenario_price_change_count_total(void)
{
    return static_cast<unsigned int>(price_changes.size());
}

int scenario_price_change_count_active(void)
{
    int count = 0;
    for (const price_change_t &price_change : price_changes) {
        if (price_change_in_use(price_change)) {
            count++;
        }
    }
    return count;
}

void scenario_price_change_save_state(buffer *buf)
{
    buffer_init_dynamic_array(buf, price_changes.size(), PRICE_CHANGES_STRUCT_SIZE_CURRENT);

    for (const price_change_t &price_change : price_changes) {
        buffer_write_i16(buf, price_change.year);
        buffer_write_u8(buf, price_change.month);
        buffer_write_u8(buf, price_change.resource);
        buffer_write_u8(buf, price_change.amount);
        buffer_write_u8(buf, price_change.is_rise);
    }
}

void scenario_price_change_load_state(buffer *buf)
{
    size_t size = buffer_load_dynamic_array(buf);

    resize_price_changes(size);

    for (size_t i = 0; i < size; i++) {
        price_change_t *price_change = price_change_slot(static_cast<unsigned int>(i));
        price_change->year = buffer_read_i16(buf);
        price_change->month = buffer_read_u8(buf);
        price_change->resource = buffer_read_u8(buf);
        price_change->amount = buffer_read_u8(buf);
        price_change->is_rise = buffer_read_u8(buf);
    }

    trim_price_changes();
}

void scenario_price_change_load_state_old_version(buffer *buf)
{
    resize_price_changes(MAX_ORIGINAL_PRICE_CHANGES);
    for (price_change_t &price_change : price_changes) {
        price_change.year = buffer_read_i16(buf);
    }
    for (price_change_t &price_change : price_changes) {
        price_change.month = buffer_read_u8(buf);
    }
    for (price_change_t &price_change : price_changes) {
        price_change.resource = buffer_read_u8(buf);
    }
    for (price_change_t &price_change : price_changes) {
        price_change.amount = buffer_read_u8(buf);
    }
    for (price_change_t &price_change : price_changes) {
        price_change.is_rise = buffer_read_u8(buf);
    }
    trim_price_changes();
}
