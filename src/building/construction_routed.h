#pragma once

#include "building/building_type.h"
#include "map/grid.h"

int building_construction_place_road(
    int measure_only, int x_start, int y_start, int x_end, int y_end,
    building_type road_type = BUILDING_NONE);

int building_construction_place_highway(
    int measure_only, int x_start, int y_start, int x_end, int y_end,
    building_type highway_type = BUILDING_NONE);

int building_construction_can_place_aqueduct_endpoint(building_type aqueduct_type, int grid_offset);

int building_construction_place_aqueduct(
    int measure_only, building_type aqueduct_type, int x_start, int y_start, int x_end, int y_end, int *cost);

int building_construction_preview_aqueduct_route(
    building_type aqueduct_type, int x_start, int y_start, int x_end, int y_end, grid_slice *route, int *cost);

int building_construction_place_aqueduct_for_reservoir(
    int measure_only,
    building_type aqueduct_type,
    int x_start,
    int y_start,
    int x_end,
    int y_end,
    int *items,
    grid_slice *route = nullptr);
