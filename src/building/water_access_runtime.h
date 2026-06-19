#pragma once


#include "building/building_type.h"

typedef struct building building;

void water_access_runtime_reset(void);
void water_access_runtime_refresh(void);
void water_access_runtime_refresh_building(building *b);

int water_access_runtime_range_for_building(building_type type);
const char *water_access_runtime_primary_provider_access_text(building_type type);
int water_access_runtime_building_type_provides_access(building_type type);
int water_access_runtime_building_type_provides_access_text(building_type type, const char *text_id);
int water_access_runtime_building_type_requires_access_text(building_type type, const char *text_id);
int water_access_runtime_tile_has_access(int grid_offset, const char *text_id);
int water_access_runtime_building_area_has_access(const building *b, const char *text_id);
int water_access_runtime_building_has_required_access(const building *b);
int water_access_runtime_building_type_has_required_access_at(building_type type, int x, int y, int size);
int water_access_runtime_reservoir_has_network_access(int grid_offset);

void water_access_runtime_begin_preview(building_type type, int primary_grid_offset, int secondary_grid_offset);
void water_access_runtime_end_preview(void);
int water_access_runtime_tile_has_preview_highlight(int grid_offset);
int water_access_runtime_should_draw_overlay_at(int grid_offset);

