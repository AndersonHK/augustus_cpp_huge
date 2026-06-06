#include "barracks.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/count.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/storage.h"
#include "core/config.h"
#include "city/buildings.h"
#include "city/military.h"
#include "city/resource.h"
#include "core/calc.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "map/grid.h"
#include "map/road_access.h"

#define INFINITE 10000

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int is_valid_destination(const Building &b, int road_network_id)
{
    return b.is_in_use() && b.has_road_access(nullptr) &&
        b.distance_from_entry() > 0 && b.road_network_id() == road_network_id &&
        b.resource_amount(resource_weapons()) < MAX_WEAPONS_BARRACKS &&
        b.accepts_good(resource_weapons());
}

Building Barracks::for_weapon(int x, int y, resource_type resource, int road_network_id, map_point *dst)
{
    if (resource != resource_weapons()) {
        return Building(nullptr);
    }
    if (city_resource_is_stockpiled(resource)) {
        return Building(nullptr);
    }
    int min_dist = INFINITE;
    Building min_building(nullptr);
    for (Building b = Building::first_of_type(runtime_type("barracks")); b.id(); b = b.next_of_type()) {
        if (!is_valid_destination(b, road_network_id)) {
            continue;
        }
        if (b.resource_amount(resource_weapons()) >= MAX_WEAPONS_BARRACKS) {
            continue;
        }
        if (!b.accepts_good(resource_weapons())) {
            continue;
        }
        int dist = b.max_distance_to(x, y);
        dist += 8 * b.resource_amount(resource_weapons());
        if (dist < min_dist) {
            min_dist = dist;
            min_building = b;
        }
    }
    Building monument = Building::from_id(building_monument_get_grand_temple_for_god(GOD_MARS));
    if (monument.id() && monument.monument_phase() == MONUMENT_FINISHED &&
        is_valid_destination(monument, road_network_id) &&
        monument.resource_amount(resource_weapons()) < MAX_WEAPONS_BARRACKS &&
        monument.accepts_good(resource_weapons())) {
        int dist = monument.max_distance_to(x, y);
        dist += 8 * monument.resource_amount(resource_weapons());
        if (dist < min_dist) {
            min_dist = dist;
            min_building = monument;
        }
    }
    if (min_building.id() && min_dist < INFINITE) {
        if (dst) {
            map_point_store_result(min_building.road_access_x(), min_building.road_access_y(), dst);
        }
        return min_building;
    }
    return Building(nullptr);
}

static int has_recruitment_priority(int current_type, int legion_type, int priority_type, int dist, int min_distance)
{
    if (legion_type == priority_type) {
        if (current_type != priority_type) {
            return 1;
        }
    } else if (legion_type == LEGION_RECRUIT_LEGIONARY) {
        if (current_type != LEGION_RECRUIT_LEGIONARY && current_type != priority_type) {
            return 1;
        }
    }
    if (priority_type != LEGION_RECRUIT_NONE && current_type == priority_type && legion_type != priority_type) {
        return 0;
    }

    return dist < min_distance;
}

static int get_closest_legion_needing_soldiers(const Barracks &barracks)
{
    int recruit_type = LEGION_RECRUIT_NONE;
    int min_formation_id = 0;
    int min_distance = INFINITE;
    int required_recruitment = recruit_type;

    switch (barracks.priority()) {
        case PRIORITY_FORT: 
            required_recruitment = LEGION_RECRUIT_LEGIONARY;
            break;
        case PRIORITY_FORT_JAVELIN:
            required_recruitment = LEGION_RECRUIT_JAVELIN;
            break;
        case PRIORITY_FORT_MOUNTED:
            required_recruitment = LEGION_RECRUIT_MOUNTED;
            break;
        case PRIORITY_FORT_AUXILIA_INFANTRY:
            required_recruitment = LEGION_RECRUIT_INFANTRY;
            break;
        case PRIORITY_FORT_AUXILIA_ARCHERY:
            required_recruitment = LEGION_RECRUIT_ARCHER;
            break;
        default:
            break;
    }

    // find by recruitment priority
    for (int i = 1; i < formation_count(); i++) {
        formation *m = formation_get(i);
        if (!m->in_use || !m->is_legion) {
            continue;
        }
        if (m->in_distant_battle || m->legion_recruit_type == LEGION_RECRUIT_NONE) {
            continue;
        }
        if (m->legion_recruit_type == LEGION_RECRUIT_LEGIONARY && barracks.resource_amount(resource_weapons()) <= 0) {
            continue;
        }
        Building fort = Building::from_id(m->building_id);
        int dist = barracks.max_distance_to(fort);

        // find closest one by priority
        if (has_recruitment_priority(recruit_type, m->legion_recruit_type, required_recruitment, dist, min_distance)) {
            recruit_type = m->legion_recruit_type;
            min_distance = dist;
            min_formation_id = m->id;
        }
    }

    return min_formation_id;
}

static int get_closest_military_academy(const Building &fort)
{
    int min_building_id = 0;
    int min_distance = INFINITE;
    for (Building b = Building::first_of_type(runtime_type("military_academy")); b.id(); b = b.next_of_type()) {
        if (b.is_in_use() && b.has_required_workers()) {
            int dist = fort.max_distance_to(b);
            if (dist < min_distance) {
                min_distance = dist;
                min_building_id = b.id();
            }
        }
    }
    return min_building_id;
}

int Barracks::priority() const
{
    return legacy_record() ? legacy_record()->subtype.barracks_priority : 0;
}

void Barracks::set_priority(int priority)
{
    if (legacy_record()) {
        legacy_record()->subtype.barracks_priority = priority;
    }
}

int Barracks::create_soldier(int x, int y)
{
    int formation_id = get_closest_legion_needing_soldiers(*this);
    if (formation_id > 0) {
        formation *m = formation_get(formation_id);
        figure *f = figure_create(static_cast<figure_type>(m->figure_type), x, y, DIR_0_TOP);
        f->formation_id = formation_id;
        f->formation_at_rest = 1;
        if (m->figure_type == FIGURE_FORT_LEGIONARY) {
            if (resource_amount(resource_weapons()) > 0) {
                add_resource(resource_weapons(), -1);
            }
        }
        int academy_id = get_closest_military_academy(Building::from_id(m->building_id));
        if (academy_id) {
            map_point road;
            Building academy = Building::from_id(academy_id);
            if (academy.has_road_access(&road)) {
                f->action_state = FIGURE_ACTION_85_SOLDIER_GOING_TO_MILITARY_ACADEMY;
                f->destination_x = road.x;
                f->destination_y = road.y;
                f->destination_grid_offset = map_grid_offset(f->destination_x, f->destination_y);
            } else {
                f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            }
        } else {
            f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
        }
        if (m->num_figures == MAX_FORMATION_FIGURES - 1) {
            m->legion_recruit_type = LEGION_RECRUIT_NONE;
        }
    }
    formation_calculate_figures();
    return formation_id ? 1 : 0;
}

static Building get_unmanned_tower_of_type(building_type type, const Building &barracks, map_point *road)
{
    for (Building b = Building::first_of_type(type); b.id(); b = b.next_of_type()) {
        if (b.is_in_use() && b.has_workers() &&
            !b.has_primary_figure() && !b.has_quaternary_figure() &&
            (b.road_network_id() == barracks.road_network_id() || config_get(CONFIG_GP_CH_TOWER_SENTRIES_GO_OFFROAD))) {
            if (b.has_road_access(road)) {
                return b;
            }
        }
    }
    return Building(nullptr);
}

Building Barracks::unmanned_tower(map_point *road) const
{
    building_type first_priority = runtime_type("tower");
    building_type second_priority = runtime_type("watchtower");

    // invert priority
    if (priority() == PRIORITY_WATCHTOWER) {
        first_priority = runtime_type("watchtower");
        second_priority = runtime_type("tower");
    }

    Building tower = get_unmanned_tower_of_type(first_priority, *this, road);
    if (tower.id()) {
        return tower;
    }
    return get_unmanned_tower_of_type(second_priority, *this, road);
}

int Barracks::create_tower_sentry(int x, int y)
{
    map_point road;
    Building tower = unmanned_tower(&road);
    if (!tower.id()) {
        return 0;
    }
    figure *f = figure_create(FIGURE_TOWER_SENTRY, x, y, DIR_0_TOP);
    f->action_state = FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER;
    if (tower.has_road_access(&road)) {
        f->destination_x = road.x;
        f->destination_y = road.y;
    } else {
        f->state = FIGURE_STATE_DEAD;
    }
    tower.set_primary_figure_id(f->id);
    f->building_id = tower.id();
    return 1;
}
