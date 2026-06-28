#pragma once

#include "figure/figure.h"

class Building;

namespace figuretype {

class Trader : public Figure {
public:
    void draw(building_info_context *c);
};

} // namespace figuretype


    enum {
        TRADE_SHIP_NONE = 0,
        TRADE_SHIP_BUYING = 1,
        TRADE_SHIP_SELLING = 2,
    };

    int figure_create_trade_caravan(int x, int y, int city_id);

    int figure_create_trade_ship(int x, int y, int city_id);

    int figure_trade_caravan_can_buy(Figure *trader, const Building *storage, int city_id);

    int figure_trade_caravan_can_sell(Figure *trader, const Building *storage, int city_id);

    void figure_trade_caravan_action(Figure *f);

    void figure_trade_caravan_donkey_action(Figure *f);

    void figure_native_trader_action(Figure *f);

    int figure_trade_ship_is_trading(Figure *ship);

    void figure_trade_ship_action(Figure *f);

    int figure_trade_land_trade_units(void);

    int figure_trade_sea_trade_units(void);

    int figure_trader_ship_can_queue_for_import(Figure *ship);

    int figure_trader_ship_can_queue_for_export(Figure *ship);

    int figure_trader_ship_get_distance_to_dock(const Figure *ship, unsigned int dock_id);

    int figure_trader_ship_other_ship_closer_to_dock(unsigned int dock_id, int distance);


#define IMAGE_CAMEL 4922
