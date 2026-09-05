#pragma once

#include "map/point.h"

typedef struct building building;
class Building;

struct road_access_area {
    map_point origin = { 0, 0 };
    int width = 1;
    int height = 1;
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

class RoadAccessQuery {
public:
    static RoadAccessQuery fromFootprint(int x, int y, int size);
    static RoadAccessQuery fromRectangle(int x, int y, int width, int height);
    static RoadAccessQuery fromRotatedFootprint(int rotation, int x, int y, int size);

    void visitCandidates(RoadAccessCandidateVisitor &visitor) const;
    int hasRoadAccess(map_point *road) const;

    const road_access_area *areas() const { return areas_; }
    int areaCount() const { return area_count_; }

private:
    void addArea(int x, int y, int width, int height);

    road_access_area areas_[8] = {};
    int area_count_ = 0;
};

void map_road_access_visit_candidates(
    const road_access_area *areas,
    int area_count,
    RoadAccessCandidateVisitor &visitor);
void map_road_access_visit_candidates(int x, int y, int size, RoadAccessCandidateVisitor &visitor);
void map_road_access_visit_building_candidates(
    const Building &building,
    RoadAccessCandidateVisitor &visitor);

int map_has_road_access(int x, int y, int size, map_point *road);

int map_has_road_access_rectangle(int x, int y, int width, int height, map_point *road);

int map_has_road_access_rotation(int rotation, int x, int y, int size, map_point *road);

// Uses the published foundations of the building and every fixed-composition
// member. Passage cells define entrances; buildings without authored passage
// use their complete foundation perimeter.
int map_has_road_access_building(int x, int y, map_point *road);

int map_road_get_internal_passage_tiles_count(building *b);
int map_building_has_internal_passage(const building *b);

void map_update_building_internal_roads(const building *b);

int map_closest_road_within_radius(int x, int y, int size, int radius, int *x_road, int *y_road);

int map_closest_road_within_radius_building(
    const Building &building,
    int radius,
    int *x_road,
    int *y_road);

int map_closest_road_within_radius_rectangle(
    int x,
    int y,
    int width,
    int height,
    int radius,
    int *x_road,
    int *y_road);
