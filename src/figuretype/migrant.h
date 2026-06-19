#pragma once

#include "figure/figure.h"


	void migrant_create_immigrant(Building &house, int num_people);

	void migrant_create_emigrant(Building &house, int num_people);

	Figure *migrant_create_homeless(Building &house, int num_people);

	void figure_immigrant_action(Figure *f);

	void figure_emigrant_action(Figure *f);

	void figure_homeless_action(Figure *f);

