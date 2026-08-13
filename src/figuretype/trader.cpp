#include "building/storage.h"
#include "city/health.h"
#include "city/trade.h"
#include "empire/empire.h"

#include "trader.h"

#include <algorithm>
#include <math.h>
#include <stdio.h>

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/caravanserai.h"
#include "building/dock.h"
#include "building/lighthouse.h"

#include "building/building_record.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/finance.h"
#include "city/map.h"
#include "city/message.h"
#include "city/resource.h"
#include "city/trade_policy.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "game/ResourceGraphics.h"
#include "game/resource.h"
#include "map/water_navigation.h"
#include "game/time.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "map/grid.h"
#include "scenario/map.h"
#include "scenario/property.h"
#include "translation/translation.h"
#include "window/building/common.h"

#define INFINITE 10000
#define TRADER_INITIAL_WAIT GAME_TIME_TICKS_PER_DAY

#define SCORE_BASE 100 // base for scoring system, resource multipliers are in relation to this value
#define DISTANCE_BASELINE 80 // baseline for chess distance - 2* (2/3 of small province, 1/4 of enormous province)
#define PRICE_BASELINE 80
#define MULTIPLIER_PRICE_MIN 50 // minimum multiplier for resource basing on price
#define MULTIPLIER_PRICE_MAX 300 // maximum multiplier for resource basing on price
#define MULTIPLIER_DISTANCE_MIN 50
#define MULTIPLIER_DISTANCE_MAX 300

#define LOGARITHIMIC_SCALER_DISTANCE 200
#define LOGARITHMIC_SCALER_SELL 0  // from perspectvie of the trader - trader sells, player buys
#define LOGARITHMIC_SCALER_BUY 80
// adjustable scaling factors. Higher value = more influence.
// 0 =  no influence. Generally, values between 20-100 are most useful.

/* e.g. with buy scaler at 50 and sell scaler at 0, traders will not consider the price when selling goods to the player,
but will consider price a factor when buying goods from the player -> the more expensive the resource, more likely
the trader will go to the location to buy it. */

typedef struct {
    int value_multiplier[RESOURCE_SLOT_COUNT];
} sell_multipliers;

typedef struct {
    int value_multiplier[RESOURCE_SLOT_COUNT];
} buy_multipliers;

static struct {
    sell_multipliers sell_multiplier;
    buy_multipliers buy_multiplier;
} data;

static Figure &head_of_caravan(Figure &figure)
{
    Figure *current = &figure;
    while (current->type == FIGURE_TRADE_CARAVAN_DONKEY) {
        current = Figure::get(current->leading_figure_id);
    }
    return *current;
}

static Building *building_ref(unsigned int id)
{
    return Building::get(id);
}

static ::building *record_for(Building *runtime_building)
{
    return runtime_building ? const_cast<::building *>(runtime_building->record()) : nullptr;
}

static ::building *record_for_building_id(unsigned int id)
{
    return record_for(building_ref(id));
}

void figuretype::Trader::draw(building_info_context *c)
{
        Figure &trader = head_of_caravan(*this);

        Figure::draw_big_people_image(static_cast<figure_type>(trader.type), c->x_offset + 28, c->y_offset + 83);
        lang_text_draw(current_string_key(65, trader.name), c->x_offset + 90, c->y_offset + 79,
            FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));

        const empire_city *city = empire_city_get(trader.empire_city_id);
        int width = lang_text_draw(current_string_key(64, trader.type), c->x_offset + 90, c->y_offset + 110,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        const uint8_t *city_name = empire_city_get_name(city);

        if (trader.type != FIGURE_NATIVE_TRADER) {
            text_draw(city_name, c->x_offset + 90 + width, c->y_offset + 110,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        }
        width = lang_text_draw("main_strings.129.1", c->x_offset + 90, c->y_offset + 130,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        int units_capacity = 0;
        switch (trader.type) {
            case FIGURE_TRADE_CARAVAN:
                units_capacity = figure_trade_land_trade_units();
                break;
            case FIGURE_TRADE_SHIP:
                units_capacity = figure_trade_sea_trade_units();
                break;
            case FIGURE_NATIVE_TRADER:
                units_capacity = figure_trade_land_trade_units() / 3 + 1;
                break;
        }
        lang_text_draw_amount(current_string_amount_key(8, 10, units_capacity), units_capacity,
            c->x_offset + 90 + width, c->y_offset + 130,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));

        int panel_trader_id = trader.trader_id;
        if (trader.type == FIGURE_TRADE_SHIP) {
            int text_id;
            switch (trader.action_state) {
                case FIGURE_ACTION_114_TRADE_SHIP_ANCHORED: text_id = 6; break;
                case FIGURE_ACTION_112_TRADE_SHIP_MOORED: text_id = 7; break;
                case FIGURE_ACTION_115_TRADE_SHIP_LEAVING: text_id = 8; break;
                default: text_id = 9; break;
            }
            lang_text_draw(current_string_key(129, text_id), c->x_offset + 90, c->y_offset + 150,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        } else {
            int text_id;
            switch (trader.action_state) {
                case FIGURE_ACTION_101_TRADE_CARAVAN_ARRIVING:
                case FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE:
                    text_id = 12;
                    break;
                case FIGURE_ACTION_102_TRADE_CARAVAN_TRADING:
                case FIGURE_ACTION_163_NATIVE_TRADER_AT_STORAGE:
                    text_id = 10;
                    break;
                case FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING:
                    text_id = trader_has_traded(panel_trader_id) ? 11 : 13;
                    break;
                default:
                    text_id = 11;
                    break;
            }
            lang_text_draw(current_string_key(129, text_id), c->x_offset + 90, c->y_offset + 150,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }
        if (trader_has_traded(panel_trader_id)) {
            int total_bought = 0;
            int total_sold = 0;
            for (int r = RESOURCE_NONE + 1; r < RESOURCE_SLOT_COUNT; r++) {
                total_bought += trader_bought_resources(panel_trader_id, static_cast<resource_type>(r));
                total_sold += trader_sold_resources(panel_trader_id, static_cast<resource_type>(r));
            }

            int y_base = c->y_offset + 174;
            text_draw_number(total_bought, '(', ")", c->x_offset + 410, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);

            width = lang_text_draw("main_strings.129.4", c->x_offset + 40, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            for (int r = RESOURCE_NONE + 1; r < RESOURCE_SLOT_COUNT; r++) {
                resource_type resource = static_cast<resource_type>(r);
                if (trader_bought_resources(panel_trader_id, resource)) {
                    width += text_draw_number(trader_bought_resources(panel_trader_id, resource), '@', " ",
                        c->x_offset + 40 + width, y_base,
                        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                    const ImageGroupEntryRef &icon = resource_graphics(resource).panel_icon();
                    int base_height = (25 - icon.height()) / 2;
                    icon.draw(c->x_offset + 35 + width, y_base - 7 + base_height);
                    width += 25;
                }
            }

            y_base = c->y_offset + 202;
            text_draw_number(total_sold, '(', ")", c->x_offset + 410, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);

            width = lang_text_draw("main_strings.129.5", c->x_offset + 40, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            for (int r = RESOURCE_NONE + 1; r < RESOURCE_SLOT_COUNT; r++) {
                resource_type resource = static_cast<resource_type>(r);
                if (trader_sold_resources(panel_trader_id, resource)) {
                    width += text_draw_number(trader_sold_resources(panel_trader_id, resource), '@', " ",
                        c->x_offset + 40 + width, y_base,
                        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                    const ImageGroupEntryRef &icon = resource_graphics(resource).panel_icon();
                    int base_height = (25 - icon.height()) / 2;
                    icon.draw(c->x_offset + 35 + width, y_base - 7 + base_height);
                    width += 25;
                }
            }
        } else {
            int y_base = c->y_offset + 174;
            width = lang_text_draw("main_strings.129.2", c->x_offset + 40, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            for (int r = RESOURCE_NONE + 1; r < RESOURCE_SLOT_COUNT; r++) {
                resource_type resource = static_cast<resource_type>(r);
                if (city->buys_resource[r] && resource_is_storable(resource)) {
                    const ImageGroupEntryRef &icon = resource_graphics(resource).panel_icon();
                    int base_width = (25 - icon.width()) / 2;
                    int base_height = (25 - icon.height()) / 2;
                    icon.draw(c->x_offset + 40 + width + base_width, y_base - 7 + base_height);
                    width += 25;
                }
            }

            y_base = c->y_offset + 202;
            width = lang_text_draw("main_strings.129.3", c->x_offset + 40, y_base,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            for (int r = RESOURCE_NONE + 1; r < RESOURCE_SLOT_COUNT; r++) {
                resource_type resource = static_cast<resource_type>(r);
                if (city->sells_resource[r] && resource_is_storable(resource)) {
                    const ImageGroupEntryRef &icon = resource_graphics(resource).panel_icon();
                    int base_width = (25 - icon.width()) / 2;
                    int base_height = (25 - icon.height()) / 2;
                    icon.draw(c->x_offset + 40 + width + base_width, y_base - 7 + base_height);
                    width += 25;
                }
            }
        }

        if (building_monument_working(building_type_registry_impl::type_from_attr("caravanserai")) &&
            trader.type != FIGURE_TRADE_SHIP) {
            trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);
            if (policy) {
                int text_width = text_draw(translation_for_key("TR_BUILDING_CARAVANSERAI_POLICY_TITLE"),
                    c->x_offset + 40, c->y_offset + 222,
                    FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                switch (policy) {
                    case TRADE_POLICY_1:
                        text_draw(translation_for_key("TR_BUILDING_CARAVANSERAI_POLICY_1_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    case TRADE_POLICY_2:
                        text_draw(translation_for_key("TR_BUILDING_CARAVANSERAI_POLICY_2_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    case TRADE_POLICY_3:
                        text_draw(translation_for_key("TR_BUILDING_CARAVANSERAI_POLICY_3_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    default:
                        break;
                }
            } else {
                text_draw(translation_for_key("TR_BUILDING_CARAVANSERAI_NO_POLICY"),
                    c->x_offset + 40, c->y_offset + 222,
                    FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            }
        }

        if (building_monument_working(building_type_registry_impl::type_from_attr("lighthouse")) &&
            trader.type == FIGURE_TRADE_SHIP) {
            trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);
            if (policy) {
                int text_width = text_draw(translation_for_key("TR_BUILDING_LIGHTHOUSE_POLICY_TITLE"),
                    c->x_offset + 40, c->y_offset + 222,
                    FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                switch (policy) {
                    case TRADE_POLICY_1:
                        text_draw(translation_for_key("TR_BUILDING_LIGHTHOUSE_POLICY_1_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    case TRADE_POLICY_2:
                        text_draw(translation_for_key("TR_BUILDING_LIGHTHOUSE_POLICY_2_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    case TRADE_POLICY_3:
                        text_draw(translation_for_key("TR_BUILDING_LIGHTHOUSE_POLICY_3_TITLE"),
                            c->x_offset + 40 + text_width + 10, c->y_offset + 222,
                            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                        break;
                    default:
                        break;
                }
            } else {
                text_draw(translation_for_key("TR_BUILDING_LIGHTHOUSE_NO_POLICY"),
                    c->x_offset + 40, c->y_offset + 222,
                    FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            }
        }
    }

static resource_type get_least_filled_quota_resource(Building &building, int city_id, signed char trader_buying);

static int random_initial_wait_ticks(void)
{
    const int max_wait_ticks = TRADER_INITIAL_WAIT;
    if (max_wait_ticks <= 0) {
        return 0;
    }
    random_generate_next();
    return random_short() % (max_wait_ticks + 1);
}

static int calculate_log_score(int baseline, int multiplier_min, int multiplier_max,
     int logarithmic_scaler, int input_value)
{
    if (input_value <= 0) input_value = 1; // Avoid log(0) errors
    double ratio = (double) input_value / baseline;
    int score = (int) (SCORE_BASE + logarithmic_scaler * log10(ratio));
    score = std::min(std::max(score, multiplier_min), multiplier_max);
    return score;
}

static void resource_multiplier_init(void)
{
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        const resource_type resource = static_cast<resource_type>(r);
        // player buys, traders sell
        int price_sell_multiplier = calculate_log_score(PRICE_BASELINE, MULTIPLIER_PRICE_MIN, MULTIPLIER_PRICE_MAX,
        LOGARITHMIC_SCALER_SELL, trade_price_buy(resource, 1)); //trader sells, player buys 
        data.sell_multiplier.value_multiplier[r] = price_sell_multiplier;
        int price_buy_multiplier = calculate_log_score(PRICE_BASELINE, MULTIPLIER_PRICE_MIN, MULTIPLIER_PRICE_MAX,
        LOGARITHMIC_SCALER_BUY, trade_price_sell(resource, 1)); //trader buys, player sells 
        data.buy_multiplier.value_multiplier[r] = price_buy_multiplier;
        // add any other rules that increase priority of a resource here, e.g.: resource_is_food(r) ? 150 : 100;
    }
}

static void resource_multiplier_reset(void)
{
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        data.sell_multiplier.value_multiplier[r] = 0;
        data.buy_multiplier.value_multiplier[r] = 0;
    }
}

// Mercury Grand Temple base bonus to trader speed
static int trader_bonus_speed(void)
{
    if (grand_temple_for_god(GOD_MERCURY, true)) {
        return 25;
    } else {
        return 0;
    }
}

// Neptune Grand Temple base bonus to trader speed
static int sea_trader_bonus_speed(void)
{
    if (grand_temple_for_god(GOD_NEPTUNE, true)) {
        return 25;
    } else {
        return 0;
    }
}

int figure_create_trade_caravan(int x, int y, int city_id)
{
    Figure *caravan = Figure::create(FIGURE_TRADE_CARAVAN, x, y, DIR_0_TOP);
    caravan->empire_city_id = static_cast<unsigned char>(city_id);
    caravan->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    caravan->wait_ticks = static_cast<short>(random_initial_wait_ticks());
    // donkey 1
    Figure *donkey1 = Figure::create(FIGURE_TRADE_CARAVAN_DONKEY, x, y, DIR_0_TOP);
    donkey1->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    donkey1->leading_figure_id = static_cast<short>(caravan->id());
    // donkey 2
    Figure *donkey2 = Figure::create(FIGURE_TRADE_CARAVAN_DONKEY, x, y, DIR_0_TOP);
    donkey2->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
    donkey2->leading_figure_id = static_cast<short>(donkey1->id());
    return caravan->id();
}

int figure_create_trade_ship(int x, int y, int city_id)
{
    Figure *ship = Figure::create(FIGURE_TRADE_SHIP, x, y, DIR_0_TOP);
    ship->empire_city_id = static_cast<unsigned char>(city_id);
    ship->action_state = FIGURE_ACTION_110_TRADE_SHIP_CREATED;
    ship->wait_ticks = static_cast<short>(random_initial_wait_ticks());
    return ship->id();
}

int figure_trade_caravan_can_buy(Figure *trader, const Building *building, int)
{
    if (!building ||
        (!building->matches("warehouse") &&
            !(config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES) && building->matches("granary")))) {
        return 0;
    }
    if (building->has_plague()) {
        return 0;
    }
    if (trader->trader_amount_bought >= figure_trade_land_trade_units()) {
        return 0;
    }
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_TRADERS, *building)) {
        return 0;
    }
    return 1;
}

int figure_trade_caravan_can_sell(Figure *trader, const Building *building, int)
{
    if (!building || (!building->matches("warehouse") && !building->matches("granary"))) {
        return 0;
    }
    if (building->has_plague()) {
        return 0;
    }
    if (trader->loads_sold_or_carrying >= figure_trade_land_trade_units()) {
        return 0;
    }
    if (!building_storage_get_permission(BUILDING_STORAGE_PERMISSION_TRADERS, *building)) {
        return 0;
    }
    if (building_storage_get(building->storage_id)->empty_all) {
        return 0;
    }
    return 1;
}

resource_type get_native_trader_buy_resource(Building &storage)
{
    resource_type highest_resource = RESOURCE_NONE;
    if (storage.matches("warehouse")) {
        building_warehouse_recount_resources(storage);
    }
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        const resource_type resource = static_cast<resource_type>(r);
        if (storage.resource_amount(resource) > highest_resource &&
            (city_resource_trade_status(resource) & TRADE_STATUS_EXPORT)) {
            highest_resource = resource;
        }
    }
    return highest_resource;
}

static resource_type trader_get_buy_resource(Building &building, int city_id)
{
    //TODO: get all the logic of trade happening into this function, rather than decision making in the action function
    unsigned char land_trader = 1; // 1 = land trader, 0 = sea trader
    if (!building.matches("warehouse") && !building.matches("granary")) {
        return RESOURCE_NONE;
    }

    if (building.matches("granary") && !config_get(CONFIG_GP_CH_ALLOW_EXPORTING_FROM_GRANARIES)) {
        return RESOURCE_NONE;
    }
    resource_type resource = get_least_filled_quota_resource(building, city_id, 1); // 1 = trader buying
    if (resource == RESOURCE_NONE && city_id == 0) { //native trader
        resource = static_cast<resource_type>(building_storage_get_highest_quantity_resource(building));
    }
    if (resource == RESOURCE_NONE) {
        return RESOURCE_NONE;
    }
    int success = 0;
    if (building.matches("granary")) {
        success = building_granary_remove_export(building, resource, 1, land_trader);
    } else {
        success = building_warehouse_remove_export(building, resource, 1, land_trader);
    }

    return success ? resource : RESOURCE_NONE;
}

static resource_type trader_get_sell_resource(Building &building, int city_id)
{
    unsigned char land_trader = 1; // 1 = land trader, 0 = sea trader
    if (!building.matches("warehouse") && !building.matches("granary")) {
        return RESOURCE_NONE;
    }

    resource_type resource = get_least_filled_quota_resource(building, city_id, 0); // 0 = trader selling
    if (resource == RESOURCE_NONE) {
        return RESOURCE_NONE;
    }

    int success = 0;
    if (building.matches("granary")) {
        success = building_granary_add_import(building, resource, 1, land_trader);
    } else {
        success = building_warehouse_add_import(building, resource, 1, land_trader);
    }

    return success ? resource : RESOURCE_NONE;
}

static resource_type get_least_filled_quota_resource(Building &storage, int city_id, signed char trader_buying)
{
    const int is_granary = storage.matches("granary");
    int r_start = RESOURCE_NONE + 1;
    int r_end = RESOURCE_SLOT_COUNT;

    resource_type best_resource = RESOURCE_NONE;
    int lowest_percent = 101; // Higher than max possible fill (100%)

    int route_id = empire_city_get_route_id(city_id);
    int available = 0;
    for (int r = r_start; r < r_end; r++) {
        // Check if resource is available
        const resource_type resource = static_cast<resource_type>(r);
        if (trader_buying) {
            if (is_granary) {
                available = building_granary_count_available_resource(storage, resource, 1);
            } else {
                available = building_warehouse_get_available_amount(storage, resource);
            }

        } else {
            if (is_granary) {
                available = building_granary_maximum_receptible_amount(storage, resource);
            } else {
                available = building_warehouse_maximum_receptible_amount(storage, resource);
            }
        }
        if (available <= 0) {
            continue;
        }
        if (trader_buying) {
            if (!empire_can_export_resource_to_city(city_id, resource)) {
                continue;
            }
        } else {
            if (!empire_can_import_resource_from_city(city_id, resource)) {
                continue;
            }
        }

        int limit = trade_route_limit(route_id, resource, trader_buying);
        if (limit <= 0) continue;

        int traded = trade_route_traded(route_id, resource, trader_buying);
        int fill_percent = (traded * 100) / limit;

        if (fill_percent < lowest_percent) {
            lowest_percent = fill_percent;
            best_resource = resource;
        }
    }

    return best_resource;
}

static Building *get_closest_storage(const Figure *f, int, int, int city_id, map_point *dst)
{
    const int max_trade_units = (f->type != FIGURE_NATIVE_TRADER) ?
        figure_trade_land_trade_units() : figure_trade_land_trade_units() / 3 + 1;

    resource_multiplier_init();
    int sellable[RESOURCE_SLOT_COUNT] = { 0 };
    int buyable[RESOURCE_SLOT_COUNT] = { 0 };
    int route_id = empire_city_get_route_id(f->empire_city_id);
    // 1. Determine what resources and how many can this caravan sell and buy
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        const resource_type resource = static_cast<resource_type>(r);
        signed char resource_sell = empire_can_import_resource_from_city(city_id, resource) ? 1 : 0;
        signed char resource_buy = empire_can_export_resource_to_city(city_id, resource) ? 1 : 0;
        if (resource_sell || resource_buy) {
            int remaining_sell = trade_route_limit(route_id, resource, 0) - trade_route_traded(route_id, resource, 0);
            int remaining_buy = trade_route_limit(route_id, resource, 1) - trade_route_traded(route_id, resource, 1);
            if (route_id == 0 && f->type == FIGURE_NATIVE_TRADER) { // no limits for native traders
                remaining_buy = figure_trade_land_trade_units();
            }
            if (resource_sell && remaining_sell > 0) {
                sellable[r] = remaining_sell;
            }
            if (resource_buy && remaining_buy > 0) {
                buyable[r] = remaining_buy;
            }
        }
    }
    building_storage_permission_states permissions =
        f->type == FIGURE_NATIVE_TRADER ? BUILDING_STORAGE_PERMISSION_NATIVES : BUILDING_STORAGE_PERMISSION_TRADERS;
    int sell_capacity = max_trade_units - f->loads_sold_or_carrying;
    int buy_capacity = max_trade_units - f->trader_amount_bought;
    int best_score = -1;
    Building *best_building = nullptr;
    const unsigned int destination_id = f->destination_building ? f->destination_building->id : 0;
    auto consider_storage = [&](Building *building, int is_granary) {
        // Skip buildings
        if (!building->is_in_use() || building->has_plague() || !building->has_cached_road_access()
            || (figure_visited_building_in_list(f->last_visited_index, building->id)) ||
            building->id == destination_id ||
            !building_storage_get_permission(permissions, *building)) {
            return; // Not active, infected, unreachable by road, recently visited, currently at, not accepted
        }

        int sell_score = 0; // Score for how many units the trader can sell to this building
        int buy_score = 0;  // Score for how many units the trader can buy from this building
        // Loop through all resource types
        for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
            const resource_type resource = static_cast<resource_type>(r);
            if (is_granary && !resource_is_food(resource)) {
                continue;
            }
            // === SELL SCORING: Trader -> Building ===
            if (sellable[r] > 0 && sell_capacity > 0) {
                // Get how much of this resource the building can accept
                int receptable = is_granary
                    ? building_granary_maximum_receptible_amount(*building, resource) :
                    building_warehouse_maximum_receptible_amount(*building, resource);
                // Limit to how much can actually be sold here
                int can_add = std::min(std::min(sellable[r], receptable), sell_capacity);
                can_add = can_add * data.sell_multiplier.value_multiplier[r]; // Apply sell multiplier
                sell_score += can_add; // Add this to the total sell score
            }

            // === BUY SCORING: Building -> Trader ===
            if (buyable[r] > 0 && buy_capacity > 0) {
                // Get how much of this resource the building currently holds
                int available = is_granary
                    ? building_granary_get_amount(*building, resource) : building_warehouse_get_available_amount(*building, resource);
                // Limit to how much the trader can actually buy from here
                int can_take = std::min(std::min(buyable[r], available), buy_capacity);
                can_take = can_take * data.buy_multiplier.value_multiplier[r]; // Apply buy multiplier
                buy_score += can_take; // Add this to the total buy score
            }
        }
        const map_tile *exit = city_map_exit_point();
        int raw_distance = map_grid_chess_distance(f->grid_offset, building->grid_offset());
        if (route_id == 0 && f->type == FIGURE_NATIVE_TRADER) {
            raw_distance += raw_distance; //native traders always return home after 1 trade, so double the distance
        } else {
            raw_distance += map_grid_chess_distance(building->grid_offset(), exit->grid_offset);
        }
        int distance_score = calculate_log_score(raw_distance, MULTIPLIER_DISTANCE_MIN, MULTIPLIER_DISTANCE_MAX,
            LOGARITHIMIC_SCALER_DISTANCE, DISTANCE_BASELINE);
        //swapping the input and baseline gives inverted score: higher score for shorter distances
        int total_score = (sell_score + buy_score) * distance_score / 100; // Normalize by 100
        // If this building is the best candidate so far, store it
        if (total_score > best_score && total_score > 0) {
            best_score = total_score;
            best_building = building;
        }
    };
    for (Building *building = building_granary_first(); building; building = building->next_of_type()) {
        consider_storage(building, 1);
    }
    for (Building *building = building_warehouse_first(); building; building = building->next_of_type()) {
        consider_storage(building, 0);
    }
    // 5. Return result 
    if (best_building) {
        Building *best = best_building;
        if (best->matches("granary") && best->has_cached_road_access()) {
            // go to center of granary
            map_point_store_result(best->x() + 1, best->y() + 1, dst);
        } else if (best->matches("warehouse") && best->has_cached_road_access()) {
            map_point_store_result(best->x(), best->y(), dst);
        } else if (!map_has_road_access_building(best->x(), best->y(), dst)) {
            resource_multiplier_reset();
            return nullptr; // No road access found
        } else {
            map_point_store_result(best->x(), best->y(), dst); //fallback
        }
        resource_multiplier_reset();
        return best;
    }
    resource_multiplier_reset();
    return nullptr;
}

static void go_to_next_storage(Figure *f)
{
    map_point dst;
    Building *destination = get_closest_storage(f, f->x, f->y, f->empire_city_id, &dst);
    if (destination) {
    f->set_destination_building(destination);
        f->action_state = FIGURE_ACTION_101_TRADE_CARAVAN_ARRIVING;
        f->destination_x = static_cast<unsigned char>(dst.x);
        f->destination_y = static_cast<unsigned char>(dst.y);
    } else {
        const map_tile *exit = city_map_exit_point();
        f->action_state = FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING;
        f->destination_x = static_cast<unsigned char>(exit->x);
        f->destination_y = static_cast<unsigned char>(exit->y);
    }
}

static int trader_image_id(void)
{
    if (scenario_property_climate() == CLIMATE_DESERT) {
        return IMAGE_CAMEL;
    } else {
        return image_group(GROUP_FIGURE_TRADE_CARAVAN);
    }
}

void figure_trade_caravan_action(Figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;

    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }

    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_100_TRADE_CARAVAN_CREATED:
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > TRADER_INITIAL_WAIT) {
                f->wait_ticks = 0;
                go_to_next_storage(f);
            }
            f->image_offset = 0;
            break;
        case FIGURE_ACTION_101_TRADE_CARAVAN_ARRIVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            switch (f->direction) {
                case DIR_FIGURE_AT_DESTINATION:
                    f->action_state = FIGURE_ACTION_102_TRADE_CARAVAN_TRADING;
                    break;
                case DIR_FIGURE_REROUTE:
                    Route::remove(f);
                    break;
                case DIR_FIGURE_LOST:
                    f->state = FIGURE_STATE_DEAD;
                    f->is_ghost = 1;
                    break;
            }
            break;
        case FIGURE_ACTION_102_TRADE_CARAVAN_TRADING:
        {
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                f->wait_ticks = 0;
                int move_on = 0;
                Building *destination = f->destination_building;
                if (figure_trade_caravan_can_buy(f, destination, f->empire_city_id)) {
                    resource_type resource = trader_get_buy_resource(*destination, f->empire_city_id);
                    if (resource) {
                        trade_route_increase_traded(empire_city_get_route_id(f->empire_city_id), resource, 1);
                        trader_record_bought_resource(f->trader_id, resource);
                        city_health_update_sickness_level_in_building(f->destination_building);
                        f->trader_amount_bought++;
                    } else {
                        move_on++;
                    }
                } else {
                    move_on++;
                }
                if (figure_trade_caravan_can_sell(f, destination, f->empire_city_id)) {
                    resource_type resource = trader_get_sell_resource(*destination, f->empire_city_id);
                    if (resource) {
                        trade_route_increase_traded(empire_city_get_route_id(f->empire_city_id), resource, 0);
                        trader_record_sold_resource(f->trader_id, resource);
                        city_health_update_sickness_level_in_building(f->destination_building);
                        f->loads_sold_or_carrying++;
                    } else {
                        move_on++;
                    }
                } else {
                    move_on++;
                }
                if (move_on == 2) {
                    f->last_visited_index = static_cast<short>(figure_visited_buildings_add(f->last_visited_index,
                         destination ? destination->id : 0));
                    go_to_next_storage(f);
                }
            }
            f->image_offset = 0;
            break;
        }
        case FIGURE_ACTION_103_TRADE_CARAVAN_LEAVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            switch (f->direction) {
                case DIR_FIGURE_AT_DESTINATION:
                    f->action_state = FIGURE_ACTION_100_TRADE_CARAVAN_CREATED;
                    f->state = FIGURE_STATE_DEAD;
                    break;
                case DIR_FIGURE_REROUTE:
                    Route::remove(f);
                    break;
                case DIR_FIGURE_LOST:
                    f->state = FIGURE_STATE_DEAD;
                    break;
            }
            break;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);


    f->select_legacy_directional_frame_image(trader_image_id(), dir, f->image_offset);
}

void figure_trade_caravan_donkey_action(Figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;

    if (config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD)) {
        f->terrain_usage = TERRAIN_USAGE_ANY;
    } else {
        f->terrain_usage = TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;
    }

    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();

    Figure *leader = Figure::get(f->leading_figure_id);
    if (f->leading_figure_id <= 0) {
        f->state = FIGURE_STATE_DEAD;
    } else {
        if (leader->action_state == FIGURE_ACTION_149_CORPSE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->state != FIGURE_STATE_ALIVE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->type != FIGURE_TRADE_CARAVAN && leader->type != FIGURE_TRADE_CARAVAN_DONKEY) {
            f->state = FIGURE_STATE_DEAD;
        } else {
            figure_movement_follow_ticks_with_percentage(f, 1, move_speed);
        }
    }

    if (leader->is_ghost && !leader->height_adjusted_ticks) {
        f->is_ghost = 1;
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    f->select_legacy_directional_frame_image(trader_image_id(), dir, f->image_offset);
}

void figure_native_trader_action(Figure *f)
{
    int move_speed = trader_bonus_speed();

    f->is_ghost = 0;
    f->terrain_usage = TERRAIN_USAGE_ANY;
    figure_image_increase_offset(f, 12);
    f->clear_legacy_cart_overlay_image();
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_163_NATIVE_TRADER_AT_STORAGE;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
                f->is_ghost = 1;
            }
            if (!f->destination_building || !f->destination_building->is_in_use()) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
        case FIGURE_ACTION_161_NATIVE_TRADER_RETURNING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            }
            break;
        case FIGURE_ACTION_162_NATIVE_TRADER_CREATED:
        {
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                f->wait_ticks = 0;
                map_point tile;
                Building *building = get_closest_storage(f, f->x, f->y, 0, &tile);
                if (building) {
                    f->action_state = FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE;
    f->set_destination_building(building);
                    f->destination_x = static_cast<unsigned char>(tile.x);
                    f->destination_y = static_cast<unsigned char>(tile.y);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            f->image_offset = 0;
            break;
        }
        case FIGURE_ACTION_163_NATIVE_TRADER_AT_STORAGE:
        {
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(10)) {
                f->wait_ticks = 0;
                Building *building = f->destination_building;
                resource_type resource = building ? get_native_trader_buy_resource(*building) : RESOURCE_NONE; // preemptive check of resource to avoid standing idle
                if (building &&
                    building_storage_get_permission(BUILDING_STORAGE_PERMISSION_NATIVES, *building) &&
                    f->trader_amount_bought < figure_trade_land_trade_units() && resource != RESOURCE_NONE) {
                    int removed = 0;
                    if (building->matches("granary")) {
                        removed = building_granary_try_remove_resource(*building, resource, 1);
                    } else if (building->matches("warehouse")) {
                        removed = building_warehouse_try_remove_resource(*building, resource, 1);
                    }
                    if (removed) {
                        trader_record_bought_resource(f->trader_id, resource);
                        int price = trade_price_sell(resource, 1);
                        city_finance_process_export(price * removed);
                        city_health_update_sickness_level_in_building(f->destination_building);
                        f->trader_amount_bought += 3; //native traders 3 times less efficient
                    }

                } else {
                    map_point tile;
                    Building *next_storage = get_closest_storage(f, f->x, f->y, 0, &tile);
                    if (next_storage) {
                        f->action_state = FIGURE_ACTION_160_NATIVE_TRADER_GOING_TO_STORAGE;
    f->set_destination_building(next_storage);
                        f->destination_x = static_cast<unsigned char>(tile.x);
                        f->destination_y = static_cast<unsigned char>(tile.y);
                    } else {
                        f->action_state = FIGURE_ACTION_161_NATIVE_TRADER_RETURNING;
                        f->destination_x = f->source_x;
                        f->destination_y = f->source_y;
                    }
                }
            }
            f->image_offset = 0;
            break;
        }
    }
    int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

    if (f->action_state == FIGURE_ACTION_149_CORPSE) {
        f->select_legacy_corpse_image(image_group(GROUP_FIGURE_CARTPUSHER) + 96);
    } else {
        f->select_legacy_directional_frame_image(image_group(GROUP_FIGURE_CARTPUSHER), dir, f->image_offset);
    }
}

int figure_trade_ship_is_trading(Figure *ship)
{
    Building *dock = ship->destination_building;
    building *b = record_for(dock);
    if (!b || !dock->is_in_use() || !dock->matches("dock")) {
        return TRADE_SHIP_BUYING;
    }
    for (int i = 0; i < 3; i++) {
        Figure *f = Figure::get(b->data.distribution.cartpusher_ids[i]);
        if (!b->data.distribution.cartpusher_ids[i] || f->state != FIGURE_STATE_ALIVE) {
            continue;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_133_DOCKER_IMPORT_QUEUE:
            case FIGURE_ACTION_135_DOCKER_IMPORT_GOING_TO_STORAGE:
            case FIGURE_ACTION_138_DOCKER_IMPORT_RETURNING:
            case FIGURE_ACTION_139_DOCKER_IMPORT_AT_STORAGE:
                return TRADE_SHIP_BUYING;
            case FIGURE_ACTION_134_DOCKER_EXPORT_QUEUE:
            case FIGURE_ACTION_136_DOCKER_EXPORT_GOING_TO_STORAGE:
            case FIGURE_ACTION_137_DOCKER_EXPORT_RETURNING:
            case FIGURE_ACTION_140_DOCKER_EXPORT_AT_STORAGE:
                return TRADE_SHIP_SELLING;
        }
    }
    return TRADE_SHIP_NONE;
}

static int trade_dock_ignoring_ship(Figure *f)
{
    Building *dock = f->destination_building;
    building *b = record_for(dock);
    if (b && building_dock_is_working(*dock) &&
        (unsigned int) b->data.dock.trade_ship_id == f->id()) {
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                Figure *docker = Figure::get(b->data.distribution.cartpusher_ids[i]);
                if (docker->state == FIGURE_STATE_ALIVE && docker->action_state != FIGURE_ACTION_132_DOCKER_IDLING) {
                    return 0;
                }
            }
        }
        f->trade_ship_failed_dock_attempts++;
        if (f->trade_ship_failed_dock_attempts >= 10) {
            f->trade_ship_failed_dock_attempts = 11;
            return 1;
        }
        return 0;
    }
    return 1;
}

static int record_dock(Figure *ship, const Building &dock)
{
    building *record = const_cast<building *>(dock.record());
    if (!record || (record->data.dock.trade_ship_id != 0 && (unsigned int) record->data.dock.trade_ship_id != ship->id())) {
        return 0;
    }
    ship->last_visited_index = static_cast<short>(figure_visited_buildings_add(ship->last_visited_index, dock.id));
    return 1;
}

void figure_trade_ship_action(Figure *f)
{
    int move_speed = sea_trader_bonus_speed();
    f->is_ghost = 0;
    f->is_boat = 1;
    figure_image_increase_offset(f, 12);
    switch (f->action_state) {
        case FIGURE_ACTION_150_ATTACK:
            figure_combat_handle_attack(f);
            break;
        case FIGURE_ACTION_149_CORPSE:
            figure_combat_handle_corpse(f);
            break;
        case FIGURE_ACTION_110_TRADE_SHIP_CREATED:
        {
            f->loads_sold_or_carrying = static_cast<unsigned char>(figure_trade_sea_trade_units());
            f->trader_amount_bought = 0;
            f->is_ghost = 1;
            f->wait_ticks++;
            if (f->wait_ticks > TRADER_INITIAL_WAIT) {
                f->wait_ticks = 0;
                map_point queue_tile;
                Building *destination_dock = building_dock_get_destination(*f, nullptr, &queue_tile);
                if (destination_dock) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
                    f->destination_x = static_cast<unsigned char>(queue_tile.x);
                    f->destination_y = static_cast<unsigned char>(queue_tile.y);
    f->set_destination_building(destination_dock);
                    f->wait_ticks = FIGURE_REROUTE_DESTINATION_TICKS;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
            f->image_offset = 0;
            break;
        }
        case FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE:
        {
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            f->height_adjusted_ticks = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->wait_ticks = static_cast<short>(TRADER_INITIAL_WAIT);
                f->action_state = FIGURE_ACTION_114_TRADE_SHIP_ANCHORED;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                f->wait_ticks = 0;
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->wait_ticks = 0;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->wait_ticks++ >= FIGURE_REROUTE_DESTINATION_TICKS) {
                f->wait_ticks = 0;
                map_point tile;
                Building *destination_dock = building_dock_get_destination(*f, nullptr, &tile);
                if (!f->destination_building && destination_dock) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                    f->destination_x = static_cast<unsigned char>(tile.x);
                    f->destination_y = static_cast<unsigned char>(tile.y);
                    Route::remove(f);
                }
                destination_dock = building_dock_get_closer_free_destination(*f, SHIP_DOCK_REQUEST_2_FIRST_QUEUE, &tile);
                if (destination_dock) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                    f->destination_x = static_cast<unsigned char>(tile.x);
                    f->destination_y = static_cast<unsigned char>(tile.y);
                    Route::remove(f);
                } else if (!f->destination_building ||
                    !building_dock_is_working(*f->destination_building) ||
                    !building_dock_accepts_ship(*f, *f->destination_building)) {
                    destination_dock = building_dock_get_destination(*f, nullptr, &tile);
                    if (destination_dock) {
                        f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                        f->destination_x = static_cast<unsigned char>(tile.x);
                        f->destination_y = static_cast<unsigned char>(tile.y);
                        Route::remove(f);
                    }
                }
            }
            break;
        }
        case FIGURE_ACTION_114_TRADE_SHIP_ANCHORED:
        {
            f->wait_ticks++;
            if (f->wait_ticks > game_time_scale_legacy_day_ticks(40)) {
                f->wait_ticks = 0;
                map_point tile;
                if (!f->destination_building ||
                    !building_dock_is_working(*f->destination_building) ||
                    !building_dock_accepts_ship(*f, *f->destination_building)) {
                    Building *destination_dock = building_dock_get_destination(*f, f->destination_building, &tile);
                    if (destination_dock) {
                        f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                        f->destination_x = static_cast<unsigned char>(tile.x);
                        f->destination_y = static_cast<unsigned char>(tile.y);
                    } else {
                        f->action_state = FIGURE_ACTION_115_TRADE_SHIP_LEAVING;
                        f->wait_ticks = 0;
                        map_point river_exit = scenario_map_river_exit();
                        f->destination_x = static_cast<unsigned char>(river_exit.x);
                        f->destination_y = static_cast<unsigned char>(river_exit.y);
                        switch (f->destination_building ? f->destination_building->dock_orientation() : 0) {
                            case 0: f->direction = DIR_2_RIGHT; break;
                            case 1: f->direction = DIR_4_BOTTOM; break;
                            case 2: f->direction = DIR_6_LEFT; break;
                            default:f->direction = DIR_0_TOP; break;
                        }
                        f->image_offset = 0;
                        city_message_reset_category_count(MESSAGE_CAT_BLOCKED_DOCK);
                    }
                } else if (building_dock_request_docking(*f, *f->destination_building, &tile)) {
                    f->action_state = FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK;
                    f->destination_x = static_cast<unsigned char>(tile.x);
                    f->destination_y = static_cast<unsigned char>(tile.y);
                    building *record = record_for(f->destination_building);
                    if (record) {
                        record->data.dock.trade_ship_id = static_cast<short>(f->id());
                    }
                } else {
                    Building *destination_dock =
                        building_dock_get_closer_free_destination(*f, SHIP_DOCK_REQUEST_1_DOCKING, &tile);
                    if (destination_dock && building_dock_request_docking(*f, *destination_dock, &tile)) {
                        f->action_state = FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK;
    f->set_destination_building(destination_dock);
                        f->destination_x = static_cast<unsigned char>(tile.x);
                        f->destination_y = static_cast<unsigned char>(tile.y);
                        building *record = record_for(f->destination_building);
                        if (record) {
                            record->data.dock.trade_ship_id = static_cast<short>(f->id());
                        }
                    } else {
                        destination_dock = building_dock_reposition_anchored_ship(*f, &tile);
                        if (destination_dock) {
                            f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                            f->destination_x = static_cast<unsigned char>(tile.x);
                            f->destination_y = static_cast<unsigned char>(tile.y);
                        }
                    }
                }
            }
            f->image_offset = 0;
            break;
        }
        case FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
            f->height_adjusted_ticks = 0;
            f->trade_ship_failed_dock_attempts = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                if (f->destination_building && record_dock(f, *f->destination_building)) {
                    f->action_state = FIGURE_ACTION_112_TRADE_SHIP_MOORED;
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
                if (!city_message_get_category_count(MESSAGE_CAT_BLOCKED_DOCK)) {
                    city_message_post(1, MESSAGE_NAVIGATION_IMPOSSIBLE, 0, 0);
                    city_message_increase_category_count(MESSAGE_CAT_BLOCKED_DOCK);
                }
            }
            break;
        case FIGURE_ACTION_112_TRADE_SHIP_MOORED:
        {
            if (!f->destination_building ||
                !building_dock_is_working(*f->destination_building) ||
                !building_dock_accepts_ship(*f, *f->destination_building) ||
                trade_dock_ignoring_ship(f)) {
                building *record = record_for(f->destination_building);
                if (record && (unsigned int) record->data.dock.trade_ship_id == f->id()) {
                    record->data.dock.trade_ship_id = 0;
                }
                map_point tile;
                Building *destination_dock = building_dock_get_destination(*f, f->destination_building, &tile);
                if (destination_dock) {
                    f->action_state = FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE;
    f->set_destination_building(destination_dock);
                    f->destination_x = static_cast<unsigned char>(tile.x);
                    f->destination_y = static_cast<unsigned char>(tile.y);
                } else {
                    building *dst = record_for(f->destination_building);
    f->set_destination_building(nullptr);
                    f->trade_ship_failed_dock_attempts = 0;
                    f->action_state = FIGURE_ACTION_115_TRADE_SHIP_LEAVING;
                    f->wait_ticks = 0;
                    map_point river_entry = scenario_map_river_entry();
                    map_point river_spot;
                    if (scenario_map_has_river_exit()) {
                        map_point river_exit = scenario_map_river_exit();
                        int exit_grid_offset = map_grid_offset(river_exit.x, river_exit.y);
                        int exit_distance = map_grid_chess_distance(f->grid_offset, exit_grid_offset);
                        int entrance_grid_offset = map_grid_offset(river_entry.x, river_entry.y);
                        int entrance_distance = map_grid_chess_distance(f->grid_offset, entrance_grid_offset);
                        river_spot = exit_distance < entrance_distance ? river_exit : river_entry;
                    } else {
                        river_spot = river_entry;
                    }
                    f->destination_x = static_cast<unsigned char>(river_spot.x);
                    f->destination_y = static_cast<unsigned char>(river_spot.y);
                    if (dst) {
                        dst->data.dock.queued_docker_id = 0;
                        dst->data.dock.num_ships = 0;
                    }
                }
            } else {
                switch (f->destination_building->dock_orientation()) {
                    case 0: f->direction = DIR_2_RIGHT; break;
                    case 1: f->direction = DIR_4_BOTTOM; break;
                    case 2: f->direction = DIR_6_LEFT; break;
                    default:f->direction = DIR_0_TOP; break;
                }
            }
            f->image_offset = 0;
            city_message_reset_category_count(MESSAGE_CAT_BLOCKED_DOCK);
            break;
        }
        case FIGURE_ACTION_115_TRADE_SHIP_LEAVING:
            figure_movement_move_ticks_with_percentage(f, 1, move_speed);
    f->set_destination_building(nullptr);
            f->height_adjusted_ticks = 0;
            if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                f->action_state = FIGURE_ACTION_110_TRADE_SHIP_CREATED;
                f->state = FIGURE_STATE_DEAD;
            } else if (f->direction == DIR_FIGURE_REROUTE) {
                Route::remove(f);
            } else if (f->direction == DIR_FIGURE_LOST) {
                f->state = FIGURE_STATE_DEAD;
            }
            break;
    }
}

int figure_trade_land_trade_units(void)
{
    int unit = 8;

    if (Building *mercury_gt = grand_temple_for_god(GOD_MERCURY, true)) {
        int add_unit = 0;
        int pct_workers = calc_percentage(mercury_gt->worker_count(), mercury_gt->employment_required_workers());
        if (pct_workers >= 100) { // full laborers
            add_unit = 4;
        } else if (pct_workers > 0) {
            add_unit = 2;
        }
        unit += add_unit;
    }

    if (building_caravanserai_is_fully_functional()) {
        building *b = record_for_building_id(city_buildings_get_caravanserai());

        trade_policy policy = city_trade_policy_get(LAND_TRADE_POLICY);

        int add_unit = 0;
        if (policy == TRADE_POLICY_3) {
            int pct_workers = calc_percentage(b->num_workers, model_get_building(b->type)->laborers);
            if (pct_workers >= 100) { // full laborers
                add_unit = POLICY_3_BONUS;
            } else if (pct_workers > 0) {
                add_unit = POLICY_3_BONUS / 2;
            }
        }
        unit += add_unit;
    }
    return unit;
}

int figure_trade_sea_trade_units(void)
{
    int unit = 12;
    if (Building *mercury_gt = grand_temple_for_god(GOD_MERCURY, true)) {
        int add_unit = 0;
        int pct_workers = calc_percentage(mercury_gt->worker_count(), mercury_gt->employment_required_workers());
        if (pct_workers >= 100) { // full laborers
            add_unit = 6;
        } else if (pct_workers > 0) {
            add_unit = 3;
        }
        unit += add_unit;
    }

    if (building_lighthouse_is_fully_functional()) {
        trade_policy policy = city_trade_policy_get(SEA_TRADE_POLICY);

        int add_unit = 0;
        if (policy == TRADE_POLICY_3) {
            Building lighthouse = building_lighthouse_first();

            if (lighthouse.id) {
                int pct_workers = calc_percentage(lighthouse.worker_count(),
                    lighthouse.type ? lighthouse.type->required_workers() : 0);
                if (pct_workers >= 100) { // full laborers
                    add_unit = POLICY_3_BONUS;
                } else if (pct_workers > 0) {
                    add_unit = POLICY_3_BONUS / 2;
                }
            }
        }
        unit += add_unit;
    }

    return unit;
}

// if ship is moored, do not forward to another dock unless it has more than one third of capacity available.
// otherwise better leave, free space for new ships with full load of imports
int figure_trader_ship_can_queue_for_import(Figure *ship)
{
    if (ship->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        return ship->loads_sold_or_carrying >= (figure_trade_sea_trade_units() / 3);
    }
    return 1;
}

int figure_trader_ship_can_queue_for_export(Figure *ship)
{
    if (ship->action_state == FIGURE_ACTION_112_TRADE_SHIP_MOORED) {
        int available_space = figure_trade_sea_trade_units() - ship->trader_amount_bought;
        return available_space >= (figure_trade_sea_trade_units() / 3);
    }
    return 1;
}

int figure_trader_ship_get_distance_to_dock(const Figure *ship, unsigned int dock_id)
{
    if (ship->destination_building && ship->destination_building->id == dock_id) {
        return ship->routing_path_length - ship->routing_path_current_tile;
    }
    Building *dock = building_ref(dock_id);
    if (!dock) {
        return INFINITE;
    }
    map_point tile;
    building_dock_get_ship_request_tile(*dock, SHIP_DOCK_REQUEST_1_DOCKING, &tile);
    return water_navigation::path_length(
        { ship->x, ship->y }, tile, WaterNavigationProfile::Boat);
}

int figure_trader_ship_other_ship_closer_to_dock(unsigned int dock_id, int distance)
{
    for (int route_id = 0; route_id < 20; route_id++) {
        if (empire_object_is_sea_trade_route(route_id) && empire_city_is_trade_route_open(route_id)) {
            int city_id = empire_city_get_for_trade_route(route_id);
            if (city_id != -1) {
                empire_city *city = empire_city_get(city_id);
                for (int i = 0; i < 3; i++) {
                    Figure *other_ship = Figure::get(city->trader_figure_ids[i]);
                    if (other_ship->destination_building &&
                        other_ship->destination_building->id == dock_id && other_ship->routing_path_length) {
                        int other_ship_distance_to_dock = figure_trader_ship_get_distance_to_dock(other_ship, dock_id);
                        if (other_ship_distance_to_dock < distance) {
                            return other_ship->id();
                        }
                    }
                }
            }
        }
    }
    return 0;
}
