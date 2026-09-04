#include "buildings.h"

#include "building/building.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/dock.h"
#include "city/data_private.h"
#include "core/calc.h"

static Building *get_first_working_building(const char *text_id)
{
    const building_type type = building_type_registry_impl::type_from_attr(text_id);
    for (Building &building : Building::of_type(type)) {
        if (building.is_in_use() || building.is_created() || building.is_mothballed()) {
            return &building;
        }
    }
    return nullptr;
}

int city_buildings_has_senate(void)
{
    return get_first_working_building("senate") != nullptr;
}

int city_buildings_has_governor_house(void)
{
    return get_first_working_building("governors_house") ||
        get_first_working_building("governors_villa") ||
        get_first_working_building("governors_palace");
}

int city_buildings_has_barracks(void)
{
    return city_buildings_get_barracks() != 0;
}

int city_buildings_get_barracks(void)
{
    Building *building = get_first_working_building("barracks");
    return building ? building->id : 0;
}

int city_buildings_has_mess_hall(void)
{
    return city_buildings_get_mess_hall() != 0;
}

int city_buildings_has_city_mint(void)
{
    return get_first_working_building("city_mint") != nullptr;
}

int city_buildings_get_mess_hall(void)
{
    Building *building = get_first_working_building("mess_hall");
    return building ? building->id : 0;
}

int city_buildings_has_hippodrome(void)
{
    return get_first_working_building("hippodrome") != nullptr;
}

int city_buildings_has_lighthouse(void)
{
    return get_first_working_building("lighthouse") != nullptr;
}

int city_buildings_has_caravanserai(void)
{
    return city_buildings_get_caravanserai() != 0;
}

int city_buildings_get_caravanserai(void)
{
    Building *building = get_first_working_building("caravanserai");
    return building ? building->id : 0;
}

int city_buildings_triumphal_arch_available(void)
{
    return city_data.building.triumphal_arches_available > city_data.building.triumphal_arches_placed;
}

void city_buildings_build_triumphal_arch(void)
{
    city_data.building.triumphal_arches_placed++;
}

void city_buildings_remove_triumphal_arch(void)
{
    if (city_data.building.triumphal_arches_placed > 0) {
        city_data.building.triumphal_arches_placed--;
    }
}

void city_buildings_earn_triumphal_arch(void)
{
    city_data.building.triumphal_arches_available++;
}

int city_buildings_has_working_dock(void)
{
    int has_working_dock = 0;
    Building::for_each([&](Building *dock) {
        if (!has_working_dock && dock->matches("dock") && building_dock_is_working(*dock)) {
            has_working_dock = 1;
        }
    });
    return has_working_dock;
}

void city_buildings_main_native_meeting_center(int *x, int *y)
{
    Building *native_meeting = get_first_working_building("native_meeting");
    *x = native_meeting ? native_meeting->x() : 0;
    *y = native_meeting ? native_meeting->y() : 0;
}

Building *city_buildings_get_closest_plague(int x, int y, int *distance)
{
    Building *closest_free = nullptr;
    Building *closest_occupied = nullptr;
    int min_occupied_dist = *distance = 10000;

    Building::for_each(BuildingRuntimeList::PlagueTargets, [&](Building *candidate) {
        if (!candidate || !candidate->has_plague() || !candidate->distance_from_entry() ||
            !candidate->is_in_use()) {
            return;
        }
        const int dist = candidate->max_distance_to(x, y);
        if (candidate->has_quaternary_figure()) {
            if (dist < min_occupied_dist) {
                min_occupied_dist = dist;
                closest_occupied = candidate;
            }
        } else if (dist < *distance) {
            *distance = dist;
            closest_free = candidate;
        }
    });

    if (!closest_free && min_occupied_dist <= 2) {
        *distance = 2;
        return closest_occupied;
    }
    return closest_free;
}

void city_buildings_update_plague(void)
{
    Building::for_each(BuildingRuntimeList::PlagueTargets, [](Building *building_object) {
        building_object->advance_plague_day();
    });
}
