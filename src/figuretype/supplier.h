#pragma once

#include "figure/figure.h"

namespace figuretype {

class Supplier : public Figure {
public:
    void draw(building_info_context *c);
};

} // namespace figuretype

#define MAX_FOOD_STOCKED_MARKET 800
#define MAX_FOOD_STOCKED_MESS_HALL 1600 
#define MAX_FOOD_STOCKED_CARAVANSERAI 1600


	int figure_supplier_max_stocked_mess_hall_adjusted(void);

	int figure_supplier_create_delivery_boy(int leader_id, int first_figure_id, int type);

	void figure_supplier_action(Figure *f);

	void figure_delivery_boy_action(Figure *f);

	void figure_fort_supplier_action(Figure *f);
