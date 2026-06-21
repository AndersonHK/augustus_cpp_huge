#pragma once

#include "building/building.h"
#include "core/buffer.h"

#include "map/point.h"

class Figure;

void building_local_workforce_clear(void);
void building_local_workforce_initialize_city(void);

void building_local_workforce_save_state(buffer *buf);
void building_local_workforce_load_state(buffer *buf, int has_saved_state);

int building_local_workforce_is_workforce_building(const Building &building);
void building_local_workforce_refresh_access_score(Building &building);
void building_local_workforce_refresh_access_scores(void);
int building_local_workforce_access_score(const Building &building);
int building_local_workforce_house_available_workers(Building &house);
int building_local_workforce_labor_seeker_is_workforce(const Figure *f);

void building_local_workforce_reconcile_house(Building &house);
void building_local_workforce_remove_building(Building &building);

int building_local_workforce_spawn_acquisition(Building &workplace, const map_point *road);
int building_local_workforce_spawn_validation(Building &workplace, const map_point *road);
int building_local_workforce_prepare_labor_seeker_target(Figure *f);
void building_local_workforce_labor_seeker_arrived(Figure *f);
void building_local_workforce_labor_seeker_failed(Figure *f);
void building_local_workforce_cancel_labor_seeker(Figure *f);


namespace building_local_workforce {

int is_workforce_building(const Building &building);
void refresh_access_score(Building &building);
int access_score(const Building &building);
int house_available_workers(Building &house);
void reconcile_house(Building &house);
void remove_building(Building &building);
void replace_house(const Building &from, const Building &to);
int spawn_acquisition(Building &workplace, const map_point *road);
int spawn_validation(Building &workplace, const map_point *road);

} // namespace building_local_workforce
