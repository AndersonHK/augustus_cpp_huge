#pragma once

#include "core/buffer.h"
#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

#include "map/point.h"

typedef struct building building;

void building_local_workforce_clear(void);
void building_local_workforce_initialize_city(void);

void building_local_workforce_save_state(buffer *buf);
void building_local_workforce_load_state(buffer *buf, int has_saved_state);

int building_local_workforce_is_workforce_building(const building *b);
int building_local_workforce_access_score(const building *b);
int building_local_workforce_house_available_workers(building *house);
int building_local_workforce_labor_seeker_is_workforce(const figure *f);

void building_local_workforce_reconcile_house(building *house);
void building_local_workforce_remove_building(building *b);

int building_local_workforce_spawn_acquisition(building *workplace, const map_point *road);
int building_local_workforce_spawn_validation(building *workplace, const map_point *road);
int building_local_workforce_prepare_labor_seeker_target(figure *f);
void building_local_workforce_labor_seeker_arrived(figure *f);
void building_local_workforce_labor_seeker_failed(figure *f);
void building_local_workforce_cancel_labor_seeker(figure *f);

#ifdef __cplusplus
}
#endif
