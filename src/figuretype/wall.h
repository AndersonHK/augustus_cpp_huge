#pragma once

#include "figure/figure.h"
#include "building/building_fwd.h"


	void figure_ballista_action(Figure *f);

	void figure_tower_sentry_set_image(Figure *f);

	void figure_tower_sentry_action(Figure *f);

	void figure_tower_sentry_reroute(void);

	void figure_kill_tower_sentries_at(int x, int y);

	void figure_kill_tower_sentries_in_building(Building &building);

	void figure_watchman_action(Figure *f);

	void figure_watchtower_archer_action(Figure *f);

