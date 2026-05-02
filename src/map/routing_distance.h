#ifndef MAP_ROUTING_DISTANCE_H
#define MAP_ROUTING_DISTANCE_H

extern "C" {
#include "map/point.h"
}

typedef struct building building;

namespace routing_distance {

struct BuildingRoadResult {
    int reachable = 0;
    int distance = 0;
    map_point road = { 0, 0 };
};

bool prepare_from_road(const map_point &road);
BuildingRoadResult find_access_road_to_building(
    const building *target,
    int radius,
    int max_distance = 0,
    int require_same_network = 0);

} // namespace routing_distance

#endif // MAP_ROUTING_DISTANCE_H
