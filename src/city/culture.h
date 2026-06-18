#pragma once

#include "city/constants.h"
#include "core/buffer.h"

#ifdef __cplusplus
#include "building/culture_module.h"
#endif

typedef struct building building;

#ifdef __cplusplus
extern "C" {
#endif

void city_culture_update_coverage(void);

int city_culture_coverage_tavern(void);
int city_culture_coverage_theater(void);
int city_culture_coverage_amphitheater(void);
int city_culture_coverage_arena(void);
int city_culture_coverage_colosseum(void);
int city_culture_coverage_hippodrome(void);
int city_culture_coverage_average_entertainment(void);

int city_culture_coverage_religion(god_type god);

int city_culture_coverage_school(void);
int city_culture_coverage_library(void);
int city_culture_coverage_academy(void);

int city_culture_coverage_hospital(void);

int city_culture_average_education(void);
int city_culture_average_entertainment(void);
int city_culture_average_health(void);

void city_culture_calculate(void);

void city_culture_clear_module_capacity_cache(void);
void city_culture_rebuild_module_capacity_cache(void);
void city_culture_add_building_module_capacity(const building *b);
void city_culture_remove_building_module_capacity(const building *b);
void city_culture_refresh_building_module_capacity(const building *b);

void city_culture_save_state(buffer *buf);

void city_culture_load_state(buffer *buf);

#ifdef __cplusplus
}

int city_culture_module_capacity(building_type_registry_impl::CultureModuleType type);
#endif
