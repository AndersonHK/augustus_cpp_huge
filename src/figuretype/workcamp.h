#pragma once

#include "figure/figure.h"

namespace figuretype {

class WorkCampWorker : public Figure {
public:
    void draw(building_info_context *c);
};

} // namespace figuretype

#define CARTLOADS_PER_MONUMENT_DELIVERY 4


	void figure_workcamp_worker_action(Figure *f);

	void figure_workcamp_slave_action(Figure *f);

	void figure_workcamp_architect_action(Figure *f);
