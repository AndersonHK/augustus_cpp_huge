#include "buildings.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"
#include "building/dock.h"
#include "building/house.h"
#include "city/data_private.h"
#include "core/calc.h"

static const building DUMMY_BUILDING = { 0 };

static building *first_of_type(const char *text_id)
{
    building_type type = building_type_registry_impl::type_from_attr(text_id);
    return type == BUILDING_NONE ? nullptr : building_first_of_type(type);
}

static const building *get_first_working_building(const char *text_id)
{
    for (building *b = first_of_type(text_id); b; b = b->next_of_type) {
        if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED ||
            b->state == BUILDING_STATE_MOTHBALLED) {
            return b;
        }
    }
    return &DUMMY_BUILDING;
}

int city_buildings_has_senate(void)
{
    return get_first_working_building("senate")->id != 0;
}

int city_buildings_has_governor_house(void)
{
    return get_first_working_building("governors_house")->id != 0 ||
        get_first_working_building("governors_villa")->id != 0 ||
        get_first_working_building("governors_palace")->id != 0;
}

int city_buildings_has_barracks(void)
{
    return city_buildings_get_barracks() != 0;
}

int city_buildings_get_barracks(void)
{
    return get_first_working_building("barracks")->id;
}

int city_buildings_has_mess_hall(void)
{
    return city_buildings_get_mess_hall() != 0;
}

int city_buildings_has_city_mint(void)
{
    return get_first_working_building("city_mint")->id != 0;
}

int city_buildings_get_mess_hall(void)
{
    return get_first_working_building("mess_hall")->id;
}

int city_buildings_has_hippodrome(void)
{
    return get_first_working_building("hippodrome")->id != 0;
}

int city_buildings_has_lighthouse(void)
{
    return get_first_working_building("lighthouse")->id != 0;
}

int city_buildings_has_caravanserai(void)
{
    return city_buildings_get_caravanserai() != 0;
}

int city_buildings_get_caravanserai(void)
{
    return get_first_working_building("caravanserai")->id;
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
    for (building *dock = first_of_type("dock"); dock; dock = dock->next_of_type) {
        if (!dock || dock->state == BUILDING_STATE_UNUSED) {
            continue;
        }
        Building building_object(dock);
        if (building_dock_is_working(building_object)) {
            return 1;
        }
    }
    return 0;
}

void city_buildings_main_native_meeting_center(int *x, int *y)
{
    const building *native_meeting = get_first_working_building("native_meeting");
    *x = native_meeting->x;
    *y = native_meeting->y;
}

static int is_plague_service_building(const Building &building)
{
    const auto *definition = building.type;
    return definition &&
        (building.matches("dock") ||
            definition->is_warehouse() ||
            definition->is_granary());
}

int city_buildings_get_closest_plague(int x, int y, int *distance)
{
    int min_free_building_id = 0;
    int min_occupied_building_id = 0;
    int min_occupied_dist = *distance = 10000;

    auto record_plague_candidate = [&](building *b) {
        int dist = calc_maximum_distance(x, y, b->x, b->y);
        if (b->figure_id4) {
            if (dist < min_occupied_dist) {
                min_occupied_dist = dist;
                min_occupied_building_id = b->id;
            }
        } else if (dist < *distance) {
            *distance = dist;
            min_free_building_id = b->id;
        }
    };

    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (!b || !b->has_plague || !b->distance_from_entry) {
            continue;
        }
        Building building_object(b);
        if (building_house_is_active(building_object) ||
            (b->state == BUILDING_STATE_IN_USE && is_plague_service_building(building_object))) {
            record_plague_candidate(b);
        }
    }

    if (!min_free_building_id && min_occupied_dist <= 2) {
        min_free_building_id = min_occupied_building_id;
        *distance = 2;
    }
    return min_free_building_id;
}

static void update_sickness_duration(int building_id)
{
    building *b = building_get(building_id);

    if (b->state != BUILDING_STATE_IN_USE || !b->has_plague) {
        return;
    }

    // Stop plague after time or if doctor heals it
    if (b->sickness_duration == 99) {
        b->sickness_duration = 0;
        b->has_plague = 0;
        b->sickness_level = 0;
        b->sickness_doctor_cure = 0;
        b->figure_id4 = 0;
        b->fumigation_frame = 0;
        b->fumigation_direction = 0;
    } else {
        b->sickness_duration += 1;
    }
}

void city_buildings_update_plague(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (!b) {
            continue;
        }
        Building building_object(b);
        if (b->house_size || is_plague_service_building(building_object)) {
            update_sickness_duration(b->id);
        }
    }
}
