#pragma once

#include "figure/figure.h"

namespace figuretype {

class Boat : public Figure {
public:
    void draw(building_info_context *c);
};

} // namespace figuretype


	void figure_create_flotsam(void);

	void figure_flotsam_action(Figure *f);

	void figure_flotsam_update_graphics(Figure *f);

	void figure_shipwreck_action(Figure *f);

	void figure_sink_all_ships(void);
	void figure_sink_half_ships(void);

