#pragma once

#include "city_overlay.h"
#ifdef __cplusplus
extern "C" {
#endif


const city_overlay *city_overlay_for_health(void);

const city_overlay *city_overlay_for_bathhouse(void);

const city_overlay *city_overlay_for_barber(void);

const city_overlay *city_overlay_for_clinic(void);

const city_overlay *city_overlay_for_hospital(void);

const city_overlay *city_overlay_for_sickness(void);

#ifdef __cplusplus
}
#endif
