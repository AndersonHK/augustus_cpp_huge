#include "building/building.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/caravanserai.h"
#include "building/lighthouse.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/resource.h"
#include "city/trade_policy.h"
#include "core/calc.h"
#include "game/resource_id_bridge.h"
#include "trade_prices.h"

#define MIN_PRICE 1

#include <string_view>

struct trade_price {
    int32_t buy;
    int32_t sell;
};

static struct trade_price prices[RESOURCE_SLOT_COUNT];

static int building_type_attr_is(const Building &building, std::string_view attr)
{
    const building_type_registry_impl::BuildingType *type = building.type_definition();
    return type && std::string_view(type->attr()) == attr;
}

static Building first_building(const char *text_id)
{
    for (int id = 1; id < Building::count(); id++) {
        Building building = Building::from_id(id);
        if (building_type_attr_is(building, text_id)) {
            return building;
        }
    }
    return Building(nullptr);
}

static int has_building(const char *text_id)
{
    return first_building(text_id).id() != 0;
}

static int trade_percentage_from_laborers(int percent, Building building)
{
    if (!building.id()) {
        return 0;
    }
    int percent_laborers = 0;
    // get workers percentage
    int pct_workers = calc_percentage(building.worker_count(), building.type().required_workers());
    if (pct_workers >= 100) { // full laborers
        percent_laborers = percent;
    } else if (pct_workers > 0) {
        percent_laborers = percent / 2;
    }
    return percent_laborers;
}

static int trade_get_caravanserai_factor(int percent)
{
    int caravanserai_percent = 0;
    if (building_caravanserai_is_fully_functional()) {
        caravanserai_percent = trade_percentage_from_laborers(percent, first_building("caravanserai"));
    }
    return caravanserai_percent;
}

static int trade_get_lighthouse_factor(int percent)
{
    int lighthouse_percent = 0;

    if (building_lighthouse_is_fully_functional()) {
        lighthouse_percent = trade_percentage_from_laborers(percent, first_building("lighthouse"));
    }
    return lighthouse_percent;
}

static int trade_factor_sell(int land_trader)
{
    int percent = 0;
    if (land_trader && has_building("caravanserai")) {
        trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);

        if (policy == TRADE_POLICY_1) {
            percent = trade_get_caravanserai_factor(POLICY_1_BONUS_PERCENT); // trader buy 20% more
        } else if (policy == TRADE_POLICY_2) {
            percent -= trade_get_caravanserai_factor(POLICY_2_MALUS_PERCENT); // trader buy 0% less
        }
    } else if (!land_trader && has_building("lighthouse")) {
        trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);

        if (policy == TRADE_POLICY_1) {
            percent = trade_get_lighthouse_factor(POLICY_1_BONUS_PERCENT); // trader buy 20% more
        } else if (policy == TRADE_POLICY_2) {
            percent -= trade_get_lighthouse_factor(POLICY_2_MALUS_PERCENT); // trader buy 0% less
        }
    }
    return percent;
}

static int trade_factor_buy(int land_trader)
{
    int percent = 0;
    if (land_trader && city_buildings_has_caravanserai()) {
        trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);

        if (policy == TRADE_POLICY_1) {
            percent = trade_get_caravanserai_factor(POLICY_1_MALUS_PERCENT); // player buy 10% more
        } else if (policy == TRADE_POLICY_2) {
            percent -= trade_get_caravanserai_factor(POLICY_2_BONUS_PERCENT); // player buy 20% less
        }
    } else if (!land_trader && has_building("lighthouse")) {
        trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);

        if (policy == TRADE_POLICY_1) {
            percent = trade_get_lighthouse_factor(POLICY_1_MALUS_PERCENT); // player buy 10% more
        } else if (policy == TRADE_POLICY_2) {
            percent -= trade_get_lighthouse_factor(POLICY_2_BONUS_PERCENT); // player buy 20% less
        }
    }
    return percent;
}

extern "C" void trade_prices_reset(void)
{
    for (resource_type resource = RESOURCE_NONE; resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        prices[resource].buy = 0;
        prices[resource].sell = 0;
    }
    for (int i = 0; i < resource_loaded_count(); i++) {
        resource_type resource = resource_get_loaded(i);
        resource_data *data = resource_get_data(resource);
        if (!data) {
            continue;
        }
        prices[resource].buy = data->default_trade_price.buy;
        prices[resource].sell = data->default_trade_price.sell;
    }
}

extern "C" int trade_price_base_buy(resource_type resource)
{
    return prices[resource].buy;
}

extern "C" int trade_price_buy(resource_type resource, int land_trader)
{
    return calc_adjust_with_percentage(prices[resource].buy, 100 + trade_factor_buy(land_trader));
}

extern "C" int trade_price_base_sell(resource_type resource)
{
    return prices[resource].sell;
}

extern "C" int trade_price_sell(resource_type resource, int land_trader)
{
    return calc_adjust_with_percentage(prices[resource].sell, 100 + trade_factor_sell(land_trader));
}

extern "C" int trade_factor_sign(int land_trader, int is_sell) { //return sign of price compared to base
    int factor = is_sell ? trade_factor_sell(land_trader) : trade_factor_buy(land_trader);

    if (factor > 0) {
        return 1;
    } else if (factor < 0) {
        return -1;
    } else {
        return 0;
    }
}


extern "C" int trade_price_change(resource_type resource, int amount)
{
    if (amount < 0 && prices[resource].sell <= 0) {
        // cannot lower the price to negative
        return 0;
    }
    if (amount < 0 && prices[resource].sell <= -amount) {
        prices[resource].buy = 2;
        prices[resource].sell = 0;
    } else {
        prices[resource].buy += amount;
        prices[resource].sell += amount;
    }
    return 1;
}

extern "C" int trade_price_set_buy(resource_type resource, int new_price)
{
    if (new_price < MIN_PRICE) {
        prices[resource].buy = MIN_PRICE;
    } else {
        prices[resource].buy = new_price;
    }

    return 1;
}

extern "C" int trade_price_set_sell(resource_type resource, int new_price)
{
    if (new_price < MIN_PRICE) {
        prices[resource].sell = MIN_PRICE;
    } else {
        prices[resource].sell = new_price;
    }
    
    return 1;
}

extern "C" void trade_prices_save_state(buffer *buf)
{
    for (int i = 0; i < resource_total_mapped(); i++) {
        resource_type resource = resource_remap(i);
        if (resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT || !resource_is_declared(resource)) {
            buffer_write_i32(buf, 0);
            buffer_write_i32(buf, 0);
            continue;
        }
        buffer_write_i32(buf, prices[resource].buy);
        buffer_write_i32(buf, prices[resource].sell);
    }
}

extern "C" void trade_prices_load_state(buffer *buf)
{
    trade_prices_reset();
    for (int i = 0; i < resource_total_mapped(); i++) {
        int buy = buffer_read_i32(buf);
        int sell = buffer_read_i32(buf);
        resource_type resource = resource_remap(i);
        if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT || !resource_is_declared(resource)) {
            continue;
        }
        prices[resource].buy = buy;
        prices[resource].sell = sell;
    }
}
