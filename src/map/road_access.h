#pragma once

#include "map/point.h"

typedef struct building building;

struct road_access_area {
    map_point origin = { 0, 0 };
    int size = 1;
};

struct road_access_candidate {
    int grid_offset = 0;
    map_point road = { 0, 0 };
    int network_id = 0;
    int network_index = 0;
};

class RoadAccessCandidateVisitor {
public:
    virtual ~RoadAccessCandidateVisitor() = default;
    virtual void visit(const road_access_candidate &candidate) = 0;
};

void map_road_access_visit_candidates(
    const road_access_area *areas,
    int area_count,
    RoadAccessCandidateVisitor &visitor);
void map_road_access_visit_candidates(int x, int y, int size, RoadAccessCandidateVisitor &visitor);
void map_road_access_visit_hippodrome_candidates(
    int x,
    int y,
    int rotation,
    RoadAccessCandidateVisitor &visitor);
void map_road_access_visit_monument_construction_candidates(
    int x,
    int y,
    int size,
    RoadAccessCandidateVisitor &visitor);

int map_has_road_access(int x, int y, int size, map_point *road);

int map_has_road_access_rotation(int rotation, int x, int y, int size, map_point *road);

int map_has_road_access_hippodrome(int x, int y, map_point *road);

int map_has_road_access_hippodrome_rotation(int x, int y, map_point *road, int rotation);

int map_has_road_access_warehouse(int x, int y, map_point *road);

int map_road_get_granary_inner_road_tiles_count(building *b);

void map_update_granary_internal_roads(const building *b);

int map_has_road_access_granary(int x, int y, map_point *road);

int map_has_road_access_monument_construction(int x, int y, int size);

int map_closest_road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road);

int map_get_adjacent_road_tiles_for_roaming(int grid_offset, int *road_tiles, int p);
