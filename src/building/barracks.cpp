#include "barracks.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
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

static Building *first_building_with_attr(const char *attr)
{
    building_type type = building_type_registry_impl::type_from_attr(attr);
    return type == BUILDING_NONE ? nullptr : Building::first_of_type(type);
}

static int is_valid_destination(const Building &b, int road_network_id)
{
    if (!b.is_in_use()) {
        return 0;
    }
    if (!b.has_road_access(nullptr)) {
        return 0;
    }
    if (b.distance_from_entry() <= 0) {
        return 0;
    }
    if (b.road_network_id() != road_network_id) {
        return 0;
    }
    if (b.resource_amount(resource_weapons()) >= MAX_WEAPONS_BARRACKS) {
        return 0;
    }
    return b.accepts_good(resource_weapons());
}

Building *Barracks::for_weapon(int x, int y, resource_type resource, int road_network_id, map_point *dst)
{
    if (resource != resource_weapons()) {
        return nullptr;
    }
    if (city_resource_is_stockpiled(resource)) {
        return nullptr;
    }
    int min_dist = INFINITE;
    Building *min_building = nullptr;
    for (Building *b = first_building_with_attr("barracks"); b; b = b->next_of_type()) {
        if (!is_valid_destination(*b, road_network_id)) {
            continue;
        }
        int dist = b->max_distance_to(x, y);
        dist += 8 * b->resource_amount(resource_weapons());
        if (dist < min_dist) {
            min_dist = dist;
            min_building = b;
        }
    }
    if (Building *monument = grand_temple_for_god(GOD_MARS, false)) {
        if (monument->monument_phase() == MONUMENT_FINISHED &&
            is_valid_destination(*monument, road_network_id)) {
            int dist = monument->max_distance_to(x, y);
            dist += 8 * monument->resource_amount(resource_weapons());
            if (dist < min_dist) {
                min_dist = dist;
                min_building = monument;
            }
        }
    }
    if (min_building && min_dist < INFINITE) {
        if (dst) {
            map_point_store_result(min_building->road_access_x(), min_building->road_access_y(), dst);
        }
        return min_building;
    }
    return nullptr;
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

int Barracks::can_recruit_soldier_for(const formation &legion) const
{
    if (!legion.in_use || !legion.is_legion) {
        return 0;
    }
    if (legion.in_distant_battle || legion.legion_recruit_type == LEGION_RECRUIT_NONE) {
        return 0;
    }
    if (legion.num_figures >= legion.barracks_recruit_capacity()) {
        return 0;
    }
    if (legion.formation_type() && legion.formation_type()->primary_unit_requires_weapon() &&
        resource_amount(resource_weapons()) <= 0) {
        return 0;
    }
    return 1;
}

int Barracks::closest_legion_needing_soldiers() const
{
    int recruit_type = LEGION_RECRUIT_NONE;
    int min_formation_id = 0;
    int min_distance = INFINITE;
    int required_recruitment = recruit_type;

    switch (priority()) {
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
        if (!can_recruit_soldier_for(*m)) {
            continue;
        }
        int dist = max_distance_to(m->x, m->y);

        // find closest one by priority
        if (has_recruitment_priority(recruit_type, m->legion_recruit_type, required_recruitment, dist, min_distance)) {
            recruit_type = m->legion_recruit_type;
            min_distance = dist;
            min_formation_id = m->id;
        }
    }

    return min_formation_id;
}

static Building *get_closest_military_academy(int x, int y)
{
    Building *min_building = nullptr;
    int min_distance = INFINITE;
    for (Building *b = first_building_with_attr("military_academy"); b; b = b->next_of_type()) {
        if (b->is_in_use() && b->has_required_workers()) {
            int dist = b->max_distance_to(x, y);
            if (dist < min_distance) {
                min_distance = dist;
                min_building = b;
            }
        }
    }
    return min_building;
}

int Barracks::priority() const
{
    const building *record = this->record();
    return record ? record->subtype.barracks_priority : 0;
}

void Barracks::set_priority(int priority)
{
    building *record = const_cast<building *>(this->record());
    if (record) {
        record->subtype.barracks_priority = static_cast<short>(priority);
    }
}

int Barracks::create_soldier(int x, int y)
{
    formation_calculate_figures();
    int formation_id = closest_legion_needing_soldiers();
    if (formation_id > 0) {
        formation *m = formation_get(formation_id);
        const int fills_last_open_slot = m->num_figures + 1 >= m->barracks_recruit_capacity();
        Figure *f = Figure::create(static_cast<figure_type>(m->figure_type), x, y, DIR_0_TOP);
        f->formation_id = static_cast<short>(formation_id);
        f->formation_at_rest = 1;
        if (m->formation_type() && m->formation_type()->primary_unit_requires_weapon()) {
            if (resource_amount(resource_weapons()) > 0) {
                add_resource(resource_weapons(), -1);
            }
        }
        Building *academy = get_closest_military_academy(m->x, m->y);
        if (academy) {
            map_point road;
            if (academy->has_road_access(&road)) {
                f->action_state = FIGURE_ACTION_85_SOLDIER_GOING_TO_MILITARY_ACADEMY;
                f->destination_x = static_cast<unsigned char>(road.x);
                f->destination_y = static_cast<unsigned char>(road.y);
                f->destination_grid_offset = static_cast<short>(map_grid_offset(f->destination_x, f->destination_y));
            } else {
                f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
            }
        } else {
            f->action_state = FIGURE_ACTION_81_SOLDIER_GOING_TO_FORT;
        }
        if (fills_last_open_slot) {
            m->legion_recruit_type = LEGION_RECRUIT_NONE;
        }
    }
    formation_calculate_figures();
    return formation_id ? 1 : 0;
}

static Building *get_unmanned_tower_of_type(const char *attr, const Building &barracks, map_point *road)
{
    for (Building *b = first_building_with_attr(attr); b; b = b->next_of_type()) {
        if (b->is_in_use() && b->worker_count() &&
            !b->has_primary_figure() && !b->has_quaternary_figure() &&
            (b->road_network_id() == barracks.road_network_id() || config_get(CONFIG_GP_CH_TOWER_SENTRIES_GO_OFFROAD))) {
            if (b->has_road_access(road)) {
                return b;
            }
        }
    }
    return nullptr;
}

Building *Barracks::unmanned_tower(map_point *road) const
{
    const char *first_priority = "tower";
    const char *second_priority = "watchtower";

    // invert priority
    if (priority() == PRIORITY_WATCHTOWER) {
        first_priority = "watchtower";
        second_priority = "tower";
    }

    Building *tower = get_unmanned_tower_of_type(first_priority, *this, road);
    if (tower) {
        return tower;
    }
    return get_unmanned_tower_of_type(second_priority, *this, road);
}

int Barracks::create_tower_sentry(int x, int y)
{
    map_point road;
    Building *tower = unmanned_tower(&road);
    if (!tower) {
        return 0;
    }
    Figure *f = Figure::create(FIGURE_TOWER_SENTRY, x, y, DIR_0_TOP);
    f->action_state = FIGURE_ACTION_174_TOWER_SENTRY_GOING_TO_TOWER;
    if (tower->has_road_access(&road)) {
        f->destination_x = static_cast<unsigned char>(road.x);
        f->destination_y = static_cast<unsigned char>(road.y);
    } else {
        f->remove();
        return 0;
    }
    tower->set_primary_figure_id(f->id());
    f->building = tower;
    return 1;
}
