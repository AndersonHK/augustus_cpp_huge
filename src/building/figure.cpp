#include "building/figure.h"
#include "building/image.h"
#include "building/industry.h"
#include "building/mess_hall.h"
#include "building/tavern.h"
#include "city/entertainment.h"
#include "city/games.h"
#include "map/building_tiles.h"
#include "map/image.h"
#include "map/road_access.h"
#include "map/water.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/barracks.h"
#include "building/distribution.h"
#include "building/market.h"
#include "building/temple.h"
#include "figure/formation_legion.h"

#include "building/local_workforce.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"

#include <cstring>
#include <initializer_list>
#include <string_view>

#include "scenario/scenario.h"

#include "assets/assets.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/message.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/movement.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/desirability.h"
#include "map/random.h"
#include "map/terrain.h"

static int type_attr_is(building_type type, std::string_view attr)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && std::string_view(definition->attr()) == attr;
}

static int type_attr_is_any(building_type type, std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (type_attr_is(type, text_id)) {
            return 1;
        }
    }
    return 0;
}

static int building_matches(const building *b, const char *text_id)
{
    return b && type_attr_is(b->type, text_id);
}

static int building_matches_any(const building *b, std::initializer_list<const char *> text_ids)
{
    return b && type_attr_is_any(b->type, text_ids);
}

static Building working_pantheon()
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (definition && definition->is_pantheon()) {
            return Building(building_get(building_monument_working(type)));
        }
    }
    return Building(nullptr);
}

static int type_is_basic_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition &&
        (definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Small) ||
            definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Large));
}

static int type_uses_native_spawn(building_type type)
{
    return type_attr_is_any(type, {
        "engineers_post",
        "prefecture",
        "actor_colony",
        "gladiator_school",
        "bathhouse",
        "library",
        "academy",
        "barber",
        "doctor",
        "hospital",
        "workcamp",
        "school",
        "lion_house",
        "chariot_maker",
        "amphitheater",
        "arena",
        "architect_guild",
        "grand_temple_ceres",
        "grand_temple_neptune",
        "grand_temple_mercury",
        "grand_temple_mars",
        "grand_temple_venus",
        "pantheon",
        "lighthouse",
        "watchtower",
        "caravanserai",
        "armoury"
    });
}

static int type_is_fort(building_type type)
{
    return type_attr_is_any(type, {
        "fort_legionaries",
        "fort_javelin",
        "fort_archers",
        "fort_swords",
        "fort_mounted"
    });
}

static int figure_belongs_to_building(const Figure *f, const building *b)
{
    return f && b && f->building.id() == b->id;
}

static void attach_figure_to_building(Figure *f, building *b)
{
    if (f) {
        f->building = Building(b);
    }
}

static int worker_percentage(const Building &building)
{
    return calc_percentage(building.worker_count(), building.type->required_workers());
}

static void check_labor_problem(building *b)
{
    Building building_object(b);
    if (building_local_workforce_is_workforce_building(building_object)) {
        if (building_local_workforce_access_score(building_object) <= 0) {
            b->show_on_problem_overlay = 2;
        }
        return;
    }
    if (building_local_workforce_access_score(building_object) <= 0) {
        b->show_on_problem_overlay = 2;
    }
}

static void generate_labor_seeker(building *b, int x, int y)
{
    if (city_population() <= 0) {
        return;
    }
    Building building_object(b);
    if (building_local_workforce_is_workforce_building(building_object)) {
        map_point road = { x, y };
        const int trigger_workers = building_object.type->required_workers();
        const int workforce_access = building_local_workforce_access_score(building_object);
        if (workforce_access < trigger_workers) {
            if (!building_local_workforce_spawn_acquisition(building_object, &road) && workforce_access > 0) {
                building_local_workforce_spawn_validation(building_object, &road);
            }
        } else {
            building_local_workforce_spawn_validation(building_object, &road);
        }
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        // If it can access Rome
        if (b->distance_from_entry) {
            b->houses_covered = 100;
        } else {
            b->houses_covered = 0;
        }
        return;
    }
    if (b->figure_id2) {
        Figure *f = Figure::get(b->figure_id2);
        if (!f->state || f->type != FIGURE_LABOR_SEEKER || !figure_belongs_to_building(f, b)) {
            b->figure_id2 = 0;
        }
    } else {
        Figure *f = Figure::create(FIGURE_LABOR_SEEKER, x, y, DIR_0_TOP);
        f->action_state = FIGURE_ACTION_125_ROAMING;
        attach_figure_to_building(f, b);
        b->figure_id2 = f->id();
        figure_movement_init_roaming(f);
    }
}

static void spawn_labor_seeker(building *b, int x, int y, int min_houses)
{
    Building building_object(b);
    if (building_local_workforce_is_workforce_building(building_object)) {
        generate_labor_seeker(b, x, y);
        return;
    }
    if (config_get(CONFIG_GP_CH_GLOBAL_LABOUR)) {
        // If it can access Rome
        if (b->distance_from_entry) {
            b->houses_covered = 2 * min_houses;
        } else {
            b->houses_covered = 0;
        }
    } else if (building_local_workforce_access_score(building_object) <= min_houses) {
        generate_labor_seeker(b, x, y);
    }
}

static int has_figure_of_types(building *b, figure_type type1, figure_type type2)
{
    if (b->figure_id <= 0) {
        return 0;
    }
    Figure *f = Figure::get(b->figure_id);
    if (f->state && figure_belongs_to_building(f, b) && (f->type == type1 || f->type == type2)) {
        return 1;
    } else {
        b->figure_id = 0;
        return 0;
    }
}

static int has_figure_of_type(building *b, figure_type type)
{
    return has_figure_of_types(b, type, static_cast<figure_type>(0));
}

static int default_spawn_delay(const Building &building)
{
    int pct_workers = worker_percentage(building);
    if (pct_workers >= 100) {
        return game_time_scale_legacy_day_ticks(3);
    } else if (pct_workers >= 75) {
        return game_time_scale_legacy_day_ticks(7);
    } else if (pct_workers >= 50) {
        return game_time_scale_legacy_day_ticks(15);
    } else if (pct_workers >= 25) {
        return game_time_scale_legacy_day_ticks(29);
    } else if (pct_workers >= 1) {
        return game_time_scale_legacy_day_ticks(44);
    } else {
        return 0;
    }
}

static void create_roaming_figure(building *b, int x, int y, figure_type type)
{
    Figure *f = Figure::create(type, x, y, DIR_0_TOP);
    f->action_state = FIGURE_ACTION_125_ROAMING;
    attach_figure_to_building(f, b);
    b->figure_id = f->id();
    figure_movement_init_roaming(f);
}

static void spawn_figure_warehouse(building *b)
{
    check_labor_problem(b);
    building *space = b;
    for (int i = 0; i < 8; i++) {
        space = building_next(space);
        if (space->id) {
            space->show_on_problem_overlay = b->show_on_problem_overlay;
        }
    }
    map_point road;
    if (map_has_road_access_warehouse(b->x, b->y, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int resource;
        Building warehouse(b);
        int task = building_warehouse_determine_worker_task(warehouse, &resource);
        if (task != WAREHOUSE_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            f->loads_sold_or_carrying = 0; // spawn with 0, load is decided by the action.
            //this way we transfer resources from buildings to figures directly, without having to keep track
            if (task == WAREHOUSE_TASK_GETTING) {
                f->resource_id = RESOURCE_NONE;
                f->collecting_item_id = resource;
            } else {
                f->resource_id = resource;
            }
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

static void spawn_figure_granary(building *b)
{
    check_labor_problem(b);
    map_update_granary_internal_roads(b); //internal roads updated basing on surrounding terrain
    map_point road;
    if (map_has_road_access_granary(b->x, b->y, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
        if (has_figure_of_type(b, FIGURE_WAREHOUSEMAN)) {
            return;
        }
        int task = building_granary_determine_worker_task(Building(b));
        if (task != GRANARY_TASK_NONE) {
            Figure *f = Figure::create(FIGURE_WAREHOUSEMAN, b->x + 1, b->y + 1, DIR_4_BOTTOM);
            //spawn in the center of the granary
            f->action_state = FIGURE_ACTION_50_WAREHOUSEMAN_CREATED;
            //f->loads_sold_or_carrying = 1; //set loads at action, not spawn
            f->resource_id = task;
            b->figure_id = f->id();
            attach_figure_to_building(f, b);
        }
    }
}

static void spawn_figure_tower(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        if (b->num_workers <= 0) {
            return;
        }
        if (!b->figure_id4 && b->figure_id) { // has sentry but no ballista -> create
            Figure *f = Figure::create(FIGURE_BALLISTA, b->x, b->y, DIR_0_TOP);
            b->figure_id4 = f->id();
            attach_figure_to_building(f, b);
            f->action_state = FIGURE_ACTION_180_BALLISTA_CREATED;
        }
    }
}

static void spawn_figure_chariot(building *b, map_point road, int use_figure_2)
{
    Building owner(b);
    Figure *f = figure_runtime_create_profiled(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP, owner, "venue_seeker");
    if (!f) {
        f = Figure::create(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP);
        if (!f) {
            return;
        }
        f->action_state = FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED;
        attach_figure_to_building(f, b);
    }
    if (!use_figure_2) {
        b->figure_id = f->id();
    } else {
        b->figure_id2 = f->id();
    }
}

static void spawn_figure_hippodrome(building *b)
{
    check_labor_problem(b);
    if (b->prev_part_building_id) {
        return;
    }
    building *part = b;
    for (int i = 0; i < 2; i++) {
        part = building_next(part);
        if (part->id) {
            part->show_on_problem_overlay = b->show_on_problem_overlay;
        }
    }
    if (has_figure_of_type(b, FIGURE_CHARIOTEER)) {
        return;
    }
    map_point road;
    if (map_has_road_access_hippodrome_rotation(b->x, b->y, &road, b->subtype.orientation)) {
        if (building_local_workforce_access_score(Building(b)) <= 50) {
            generate_labor_seeker(b, road.x, road.y);
        }
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(15);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(30);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(50);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(80);
        } else {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Building owner(b);
            Figure *f = figure_runtime_create_profiled(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP, owner, "hippodrome_service");
            if (!f) {
                f = Figure::create(FIGURE_CHARIOTEER, road.x, road.y, DIR_0_TOP);
                if (!f) {
                    return;
                }
                f->action_state = FIGURE_ACTION_94_ENTERTAINER_ROAMING;
                attach_figure_to_building(f, b);
                figure_movement_init_roaming(f);
            }
            b->figure_id = f->id();

            if (!city_entertainment_hippodrome_has_race()) {
                // create mini-horses
                Figure *horse1 = Figure::create(FIGURE_HIPPODROME_HORSES, b->x + 2, b->y + 1, DIR_2_RIGHT);
                horse1->action_state = FIGURE_ACTION_200_HIPPODROME_HORSE_CREATED;
                attach_figure_to_building(horse1, b);
                horse1->resource_id = 0;
                horse1->speed_multiplier = 3;

                Figure *horse2 = Figure::create(FIGURE_HIPPODROME_HORSES, b->x + 2, b->y + 2, DIR_2_RIGHT);
                horse2->action_state = FIGURE_ACTION_200_HIPPODROME_HORSE_CREATED;
                attach_figure_to_building(horse2, b);
                horse2->resource_id = 1;
                horse2->speed_multiplier = 2;

                if (b->data.entertainment.days1 > 0) {
                    if (city_entertainment_show_message_hippodrome()) {
                        city_message_post(1, MESSAGE_HIPPODROME_WORKING_NEW, 0, 0);
                    }
                }
            }
        }
    }
}

static void spawn_figure_colosseum(building *b)
{
    check_labor_problem(b);
    if (has_figure_of_types(b, FIGURE_GLADIATOR, FIGURE_LION_TAMER)) {
        return;
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        if (building_local_workforce_access_score(Building(b)) <= 50) {
            generate_labor_seeker(b, road.x, road.y);
        }
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(6);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(12);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(20);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(40);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(70);
        } else {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;

            Figure *f;
            if (b->data.entertainment.days1 > 0) {
                f = Figure::create(FIGURE_LION_TAMER, road.x, road.y, DIR_0_TOP);
            } else {
                f = Figure::create(FIGURE_GLADIATOR, road.x, road.y, DIR_0_TOP);
            }
            f->action_state = FIGURE_ACTION_94_ENTERTAINER_ROAMING;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
            figure_movement_init_roaming(f);
            if (building_matches(b, "colosseum") && city_games_executions_active()) {
                f = Figure::create(FIGURE_LION_TAMER, road.x, road.y, DIR_0_TOP);
                f->action_state = FIGURE_ACTION_230_LION_TAMERS_HUNTING_ENEMIES;
                f = Figure::create(FIGURE_LION_TAMER, road.x, road.y, DIR_0_TOP);
                f->action_state = FIGURE_ACTION_230_LION_TAMERS_HUNTING_ENEMIES;
            }
            if (building_matches(b, "colosseum") &&
                (b->data.entertainment.days1 > 0 || b->data.entertainment.days2 > 0)) {
                if (city_entertainment_show_message_colosseum()) {
                    city_message_post(1, MESSAGE_COLOSSEUM_WORKING_NEW, 0, 0);
                }
            }
        }
    }
}

static void send_supplier_to_destination(Figure *f, Building destination)
{
    if (f->destination_building.id()) {
        f->last_destination_id = f->destination_building.id();
    }
    f->destination_building = destination;
    map_point road;
    int destination_found = 0;
    if (!destination.type) {
        destination_found = 0;
    } else if (destination.type->is_warehouse()) {
        if (map_has_road_access_warehouse(destination.x(), destination.y(), &road)) {
            destination_found = 1;
            f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
            f->destination_x = road.x;
            f->destination_y = road.y;
        }
    } else if (destination.type->is_granary()) {
        if (map_has_road_access_granary(destination.x(), destination.y(), &road)) {
            destination_found = 1;
            f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
            f->destination_x = road.x;
            f->destination_y = road.y;
        }
    } else if (destination.type->is_grand_temple_venus()) {
        if (map_has_road_access(destination.x(), destination.y(), destination.size(), &road)) {
            destination_found = 1;
            f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
            f->destination_x = road.x;
            f->destination_y = road.y;
        }
    }
    if (!destination_found) {
        f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
        f->destination_x = f->x;
        f->destination_y = f->y;
    }
}

static void spawn_temple_supplier(building *b, int x, int y)
{
    if (b->figure_id2) {
        Figure *f = Figure::get(b->figure_id2);
        if (f->state != FIGURE_STATE_ALIVE || (f->type != FIGURE_PRIEST_SUPPLIER && f->type != FIGURE_LABOR_SEEKER)) {
            b->figure_id2 = 0;
        }
        return;
    }
    Building temple(b);
    if (const building_type_registry_impl::Distribution *distribution = temple.type->distribution()) {
        distribution->update_demands(temple);
    }
    Building destination = building_temple_get_storage_destination(temple);
    if (!destination.id()) {
        return;
    }
    Figure *f = Figure::create(FIGURE_PRIEST_SUPPLIER, x, y, DIR_0_TOP);
    attach_figure_to_building(f, b);
    b->figure_id2 = f->id();
    f->collecting_item_id = b->data.market.fetch_inventory_id;
    send_supplier_to_destination(f, destination);
}

static void spawn_tavern_supplier(building *b, int x, int y)
{
    if (b->figure_id2) {
        Figure *f = Figure::get(b->figure_id2);
        if (f->state != FIGURE_STATE_ALIVE || (f->type != FIGURE_BARKEEP_SUPPLIER && f->type != FIGURE_LABOR_SEEKER)) {
            b->figure_id2 = 0;
        }
        return;
    }
    int dst_building_id = Tavern(Building(b)).storage_destination();
    if (dst_building_id == 0) {
        return;
    }
    Building destination(building_get(dst_building_id));
    Figure *f = Figure::create(FIGURE_BARKEEP_SUPPLIER, x, y, DIR_0_TOP);
    attach_figure_to_building(f, b);
    b->figure_id2 = f->id();
    f->collecting_item_id = b->data.market.fetch_inventory_id;
    send_supplier_to_destination(f, destination);
}

static void spawn_mess_hall_supplier(building *b, int x, int y, int figure_id_to_use)
{
    if (figure_id_to_use == 1 && b->figure_id) {
        Figure *f = Figure::get(b->figure_id);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_MESS_HALL_SUPPLIER) {
            b->figure_id = 0;
        }
        return;
    }
    if (figure_id_to_use == 4 && b->figure_id4) { // for mess hall 2nd QM
        Figure *f = Figure::get(b->figure_id4);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_MESS_HALL_SUPPLIER) {
            b->figure_id = 0;
        }
        return;
    }
    int dst_building_id = building_mess_hall_get_storage_destination(Building(b));
    if (dst_building_id == 0) {
        return;
    }
    Building destination(building_get(dst_building_id));
    Figure *f = Figure::create(FIGURE_MESS_HALL_SUPPLIER, x, y, DIR_0_TOP);
    attach_figure_to_building(f, b);
    if (figure_id_to_use == 1) {
        b->figure_id = f->id();
    } else if (figure_id_to_use == 4) {
        b->figure_id4 = f->id();
    }
    f->collecting_item_id = b->data.market.fetch_inventory_id;
    send_supplier_to_destination(f, destination);
}

static void spawn_market_supplier(building *b, int x, int y)
{
    if (b->figure_id2) {
        Figure *f = Figure::get(b->figure_id2);
        if (f->state != FIGURE_STATE_ALIVE || (f->type != FIGURE_MARKET_SUPPLIER && f->type != FIGURE_LABOR_SEEKER)) {
            b->figure_id2 = 0;
        }
        return;
    }
    Market market(b);
    if (const building_type_registry_impl::Distribution *distribution = market.type->distribution()) {
        distribution->update_demands(market);
    }
    int dst_building_id = market.storage_destination();
    if (dst_building_id == 0) {
        return;
    }
    Building destination(building_get(dst_building_id));
    Figure *f = Figure::create(FIGURE_MARKET_SUPPLIER, x, y, DIR_0_TOP);
    attach_figure_to_building(f, b);
    b->figure_id2 = f->id();
    f->collecting_item_id = b->data.market.fetch_inventory_id;
    send_supplier_to_destination(f, destination);
}

static void spawn_figure_market(building *b)
{
    Building(b).refresh_graphic();
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(2);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(5);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(10);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(20);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(30);
        } else {
            return;
        }
        // market trader
        if (!has_figure_of_type(b, FIGURE_MARKET_TRADER)) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay <= spawn_delay) {
                return;
            }
            b->figure_spawn_delay = 0;
            create_roaming_figure(b, road.x, road.y, FIGURE_MARKET_TRADER);
        }
        // market supplier or labor seeker
        spawn_market_supplier(b, road.x, road.y);
    }
}

static void spawn_figure_grand_temple_mars(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = 8;
        } else if (pct_workers >= 75) {
            spawn_delay = 12;
        } else if (pct_workers >= 50) {
            spawn_delay = 16;
        } else if (pct_workers >= 25) {
            spawn_delay = 32;
        } else if (pct_workers >= 1) {
            spawn_delay = 48;
        } else {
            return;
        }

        // recruitment penalty for no food
        if (city_data.mess_hall.food_stress_cumulative > 20) {
            spawn_delay += city_data.mess_hall.food_stress_cumulative - 20;
        }
        spawn_delay = game_time_scale_legacy_day_ticks(spawn_delay);

        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            map_has_road_access(b->x, b->y, b->size, &road);
            Barracks barracks(b);
            switch (barracks.priority()) {
                case PRIORITY_FORT:
                case PRIORITY_FORT_JAVELIN:
                case PRIORITY_FORT_MOUNTED:
                case PRIORITY_FORT_AUXILIA_INFANTRY:
                case PRIORITY_FORT_AUXILIA_ARCHERY:
                    if (!barracks.create_soldier(road.x, road.y)) {
                        barracks.create_tower_sentry(road.x, road.y);
                    }
                    break;
                default:
                    if (!barracks.create_tower_sentry(road.x, road.y)) {
                        barracks.create_soldier(road.x, road.y);
                    }
            }
            if (!has_figure_of_type(b, FIGURE_PRIEST)) {
                create_roaming_figure(b, road.x, road.y, FIGURE_PRIEST);
            }
        }

        // Pantheon Module 1 Bonus
        if (!b->figure_id4 && building_monument_pantheon_module_is_active(PANTHEON_MODULE_1_DESTINATION_PRIESTS)) {
            Figure *f = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
            Building pantheon = working_pantheon();
            b->figure_id4 = f->id();
            f->destination_building = pantheon;
            attach_figure_to_building(f, b);
            f->action_state = FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED;
        }
    }
}

static void spawn_figure_tavern(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        if (building_local_workforce_access_score(Building(b)) <= 50) {
            generate_labor_seeker(b, road.x, road.y);
        }
        int spawn_delay = default_spawn_delay(Building(b)) * 2;
        if (!spawn_delay) {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Building(b).refresh_graphic();
            if (!has_figure_of_type(b, FIGURE_BARKEEP) && b->resources[resource_wine()] >= 20) {
                b->resources[resource_wine()] -= 20;
                int resource_decay = b->resources[resource_meat()] && b->resources[resource_fish()] ? 20 : 40;
                b->resources[resource_meat()] -= calc_bound(resource_decay, resource_decay, b->resources[resource_meat()]);
                b->resources[resource_fish()] -= calc_bound(resource_decay, resource_decay, b->resources[resource_fish()]);
                create_roaming_figure(b, road.x, road.y, FIGURE_BARKEEP);
            }
        }
        spawn_tavern_supplier(b, road.x, road.y);
    }
}

static void spawn_figure_temple(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        Building building_object(b);
        int pct_workers = worker_percentage(building_object);
        int spawn_delay;
        if (building_object.type->required_workers() <= 0) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(3);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(10);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(15);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(20);
        } else {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            if (!has_figure_of_type(b, FIGURE_PRIEST)) {
                create_roaming_figure(b, road.x, road.y, FIGURE_PRIEST);
            }
            // Neptune Module 1 Bonus
            if (building_is_neptune_temple(b->type) &&
                building_monument_gt_module_is_active(NEPTUNE_MODULE_1_HIPPODROME_ACCESS) && !b->figure_id2) {
                b->days_since_offering++;
                if (b->days_since_offering > 1) {
                    spawn_figure_chariot(b, road, 1);
                    b->days_since_offering = 0;
                }
            }
            b->figure_spawn_delay = 0;
        }

        // Mars Module 1 Bonus
        if (building_is_mars_temple(b->type) && building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
            Building mess_hall(building_get(city_buildings_get_mess_hall()));
            if (mess_hall.id() && mess_hall.type && mess_hall.type->is_mess_hall()) {
                Figure *f = Figure::get(b->figure_id2);
                if (f->state != FIGURE_STATE_ALIVE) {
                    b->figure_id2 = 0;
                }
                int food_to_deliver = building_temple_mars_food_to_deliver(Building(b), mess_hall);
                if (food_to_deliver >= 0) {
                    Figure *priest = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);

                    priest->collecting_item_id = food_to_deliver;
                    b->figure_id2 = priest->id();
                    priest->destination_building = mess_hall;
                    attach_figure_to_building(priest, b);
                    priest->action_state = FIGURE_ACTION_214_DESTINATION_MARS_PRIEST_CREATED;
                }
            }
        }

        // Ceres Module 2 Bonus
        if (building_is_ceres_temple(b->type) && building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD) && !b->figure_id2) {
            spawn_temple_supplier(b, road.x, road.y);
        }

        // Venus Module 1 Bonus
        if (building_is_venus_temple(b->type) && building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE) && !b->figure_id2) {
            spawn_temple_supplier(b, road.x, road.y);
        }

        // Pantheon Module 1 Bonus
        if (!building_matches(b, "pantheon") && !b->figure_id4 && building_monument_pantheon_module_is_active(PANTHEON_MODULE_1_DESTINATION_PRIESTS)) {
            Figure *f = Figure::create(FIGURE_PRIEST, road.x, road.y, DIR_4_BOTTOM);
            Building pantheon = working_pantheon();
            b->figure_id4 = f->id();
            f->destination_building = pantheon;
            attach_figure_to_building(f, b);
            f->action_state = FIGURE_ACTION_212_DESTINATION_PRIEST_CREATED;
        }
    }
}

static void spawn_figure_senate_forum(building *b)
{
    if (building_matches(b, "senate")) {
        Building(b).refresh_graphic();
    }
    if (building_matches(b, "forum")) {
        Building(b).refresh_graphic();
    }
    check_labor_problem(b);
    if (has_figure_of_type(b, FIGURE_TAX_COLLECTOR)) {
        return;
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(0);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(1);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(3);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(15);
        } else {
            return;
        }
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Figure *f = Figure::create(FIGURE_TAX_COLLECTOR, road.x, road.y, DIR_0_TOP);
            f->action_state = FIGURE_ACTION_40_TAX_COLLECTOR_CREATED;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
        }
    }
}

static void spawn_figure_mission_post(building *b)
{
    if (has_figure_of_type(b, FIGURE_MISSIONARY)) {
        return;
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        if (city_population() > 0) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(1)) {
                b->figure_spawn_delay = 0;
                create_roaming_figure(b, road.x, road.y, FIGURE_MISSIONARY);
            }
        }
    }
}

static void spawn_figure_industry(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        if (has_figure_of_type(b, FIGURE_CART_PUSHER)) {
            return;
        }
        if (building_industry_has_produced_resource(b)) {
            Building building_obj(b);
            const int output_cart_loads = building_obj.has_native_production() ?
                building_obj.native_production_output_cart_loads() : 1;
            building_industry_start_new_production(b);
            Figure *f = Figure::create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
            f->resource_id = b->output_resource_id;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
            f->wait_ticks = game_time_scale_legacy_day_ticks(30);
            f->loads_sold_or_carrying = static_cast<unsigned char>(output_cart_loads > 0 ? output_cart_loads : 1);
        }
    }
}

static void spawn_figure_wharf(building *b)
{
    check_labor_problem(b);
    if (b->data.industry.fishing_boat_id) {
        Figure *f = Figure::get(b->data.industry.fishing_boat_id);
        if (f->state != FIGURE_STATE_ALIVE || f->type != FIGURE_FISHING_BOAT) {
            b->data.industry.fishing_boat_id = 0;
        }
    }
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        if (has_figure_of_type(b, FIGURE_CART_PUSHER)) {
            return;
        }
        if (b->figure_spawn_delay) {
            b->figure_spawn_delay = 0;
            b->data.industry.has_fish = 0;
            Figure *f = Figure::create(FIGURE_CART_PUSHER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
            f->resource_id = resource_fish();
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
            f->wait_ticks = game_time_scale_legacy_day_ticks(30);
            f->loads_sold_or_carrying = 1;
        }
    }
}

static void spawn_figure_shipyard(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        if (has_figure_of_type(b, FIGURE_FISHING_BOAT)) {
            return;
        }
        int pct_workers = worker_percentage(Building(b));
        if (pct_workers >= 100) {
            b->data.industry.progress += 10;
        } else if (pct_workers >= 75) {
            b->data.industry.progress += 8;
        } else if (pct_workers >= 50) {
            b->data.industry.progress += 6;
        } else if (pct_workers >= 25) {
            b->data.industry.progress += 4;
        } else if (pct_workers >= 1) {
            b->data.industry.progress += 2;
        }
        if (b->data.industry.progress >= 160) {
            b->data.industry.progress = 0;
            map_point boat;
            if (map_water_can_spawn_fishing_boat(b->x, b->y, b->size, &boat)) {
                Figure *f = Figure::create(FIGURE_FISHING_BOAT, boat.x, boat.y, DIR_0_TOP);
                f->action_state = FIGURE_ACTION_190_FISHING_BOAT_CREATED;
                attach_figure_to_building(f, b);
                b->figure_id = f->id();
            }
        }
    }
}

static void spawn_figure_dock(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 50);
        int pct_workers = worker_percentage(Building(b));
        int max_dockers;
        if (pct_workers >= 75) {
            max_dockers = 3;
        } else if (pct_workers >= 50) {
            max_dockers = 2;
        } else if (pct_workers > 0) {
            max_dockers = 1;
        } else {
            max_dockers = 0;
        }
        // count existing dockers
        int existing_dockers = 0;
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                if (Figure::get(b->data.distribution.cartpusher_ids[i])->type == FIGURE_DOCKER) {
                    existing_dockers++;
                } else {
                    b->data.distribution.cartpusher_ids[i] = 0;
                }
            }
        }
        if (existing_dockers > max_dockers) {
            // too many dockers, kill one of them
            for (int i = 2; i >= 0; i--) {
                if (b->data.distribution.cartpusher_ids[i]) {
                    Figure::get(b->data.distribution.cartpusher_ids[i])->state = FIGURE_STATE_DEAD;
                    break;
                }
            }
        } else if (existing_dockers < max_dockers) {
            Figure *f = Figure::create(FIGURE_DOCKER, road.x, road.y, DIR_4_BOTTOM);
            f->action_state = FIGURE_ACTION_132_DOCKER_IDLING;
            attach_figure_to_building(f, b);
            for (int i = 0; i < 3; i++) {
                if (!b->data.distribution.cartpusher_ids[i]) {
                    b->data.distribution.cartpusher_ids[i] = f->id();
                    break;
                }
            }
        }
    }
}

static void spawn_figure_native_hut(building *b)
{
    if (building_matches(b, "native_hut")) {
        map_image_set(b->grid_offset, image_group(GROUP_BUILDING_NATIVE) + (map_random_get(b->grid_offset) & 1));
    } else {
        int image_group_id;
        switch (scenario_property_climate()) {
            case CLIMATE_NORTHERN:
                image_group_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Northern_01", "Native_Hut_Northern_01");
                break;
            case CLIMATE_DESERT:
                image_group_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Southern_01", "Native_Hut_Southern_01");
                break;
            default:
                image_group_id = assets_get_image_id("Terrain_Maps\\Native_Hut_Central_01", "Native_Hut_Central_01");
        }
        map_image_set(b->grid_offset, image_group_id + (map_random_get(b->grid_offset) & 1));
    }

    if (has_figure_of_type(b, FIGURE_INDIGENOUS_NATIVE)) {
        return;
    }
    int x_out, y_out;
    if (b->subtype.native_meeting_center_id > 0
        && map_terrain_get_adjacent_road_or_clear_land(b->x, b->y, b->size, &x_out, &y_out)) {
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(4)) {
            b->figure_spawn_delay = 0;
            Figure *f = Figure::create(FIGURE_INDIGENOUS_NATIVE, x_out, y_out, DIR_0_TOP);
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            attach_figure_to_building(f, b);
            b->figure_id = f->id();
        }
    }
}

static void spawn_figure_native_meeting(building *b)
{
    map_building_tiles_add(b->id, b->x, b->y, 2, building_image_get(b), TERRAIN_BUILDING);
    int pacified = b->sentiment.native_anger < 100;
    if (pacified && !has_figure_of_type(b, FIGURE_NATIVE_TRADER)) {
        int x_out, y_out;
        if (map_terrain_get_adjacent_road_or_clear_land(b->x, b->y, b->size, &x_out, &y_out)) {
            b->figure_spawn_delay++;
            if (b->figure_spawn_delay > game_time_scale_legacy_day_ticks(8)) {
                b->figure_spawn_delay = 0;
                Figure *f = Figure::create(FIGURE_NATIVE_TRADER, x_out, y_out, DIR_0_TOP);
                f->action_state = FIGURE_ACTION_162_NATIVE_TRADER_CREATED;
                attach_figure_to_building(f, b);
                b->figure_id = f->id();
            }
        }
    }
}

static void spawn_figure_barracks(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
        int pct_workers = worker_percentage(Building(b));
        int spawn_delay;
        if (pct_workers >= 100) {
            spawn_delay = 8;
        } else if (pct_workers >= 75) {
            spawn_delay = 12;
        } else if (pct_workers >= 50) {
            spawn_delay = 16;
        } else if (pct_workers >= 25) {
            spawn_delay = 32;
        } else if (pct_workers >= 1) {
            spawn_delay = 48;
        } else {
            return;
        }

        // recruitment penalty for no food
        if (city_data.mess_hall.food_stress_cumulative > 20) {
            spawn_delay += city_data.mess_hall.food_stress_cumulative - 20;
        }
        spawn_delay = game_time_scale_legacy_day_ticks(spawn_delay);

        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            map_has_road_access(b->x, b->y, b->size, &road);
            Barracks barracks(b);
            switch (barracks.priority()) {
                case PRIORITY_FORT:
                case PRIORITY_FORT_JAVELIN:
                case PRIORITY_FORT_MOUNTED:
                case PRIORITY_FORT_AUXILIA_INFANTRY:
                case PRIORITY_FORT_AUXILIA_ARCHERY:
                    if (!barracks.create_soldier(road.x, road.y)) {
                        barracks.create_tower_sentry(road.x, road.y);
                    }
                    break;
                default:
                    if (!barracks.create_tower_sentry(road.x, road.y)) {
                        barracks.create_soldier(road.x, road.y);
                    }
            }
        }
    }
}

static void spawn_figure_military_academy(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
    }
}

static void spawn_figure_fort_supplier(building *fort)
{
    building *supply_post = building_get(city_buildings_get_mess_hall());

    if (!supply_post || fort->figure_id2 || !fort->distance_from_entry) {
        return;
    }

    if (supply_post->state != BUILDING_STATE_IN_USE) {
        return;
    }

    int total_food_in_mess_hall = 0;

    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        total_food_in_mess_hall += supply_post->resources[r];
    }

    if (!total_food_in_mess_hall) {
        return;
    }

    int spawn_delay = game_time_scale_legacy_day_ticks(20);
    fort->figure_spawn_delay++;
    if (fort->figure_spawn_delay <= spawn_delay) {
        return;
    }

    fort->figure_spawn_delay = 0;
    map_point road;
    if (map_has_road_access(supply_post->x, supply_post->y, supply_post->size, &road)) {
        Figure *f = Figure::create(FIGURE_MESS_HALL_FORT_SUPPLIER, road.x, road.y, DIR_4_BOTTOM);
        f->action_state = FIGURE_ACTION_236_SUPPLY_POST_GOING_TO_FORT;
        f->destination_x = fort->road_access_x;
        f->destination_y = fort->road_access_y;
        f->source_x = road.x;
        f->source_y = road.y;
        f->destination_building = Building(fort);
        attach_figure_to_building(f, supply_post);
        fort->figure_id2 = f->id();
    }

}

static void spawn_figure_mess_hall(building *b)
{
    check_labor_problem(b);
    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);
        int spawn_delay;
        int pct_workers = worker_percentage(Building(b));
        if (pct_workers >= 100) {
            spawn_delay = game_time_scale_legacy_day_ticks(7);
        } else if (pct_workers >= 75) {
            spawn_delay = game_time_scale_legacy_day_ticks(15);
        } else if (pct_workers >= 50) {
            spawn_delay = game_time_scale_legacy_day_ticks(29);
        } else if (pct_workers >= 25) {
            spawn_delay = game_time_scale_legacy_day_ticks(44);
        } else if (pct_workers >= 1) {
            spawn_delay = game_time_scale_legacy_day_ticks(59);
        } else {
            return;
        }

        spawn_mess_hall_supplier(b, road.x, road.y, 1);
        if (b->figure_id) {
            b->figure_spawn_delay++;
        } else {
            b->figure_spawn_delay = 0;
        }
        if (building_monument_working_grand_temple_for_god(GOD_MARS) && b->figure_spawn_delay >= spawn_delay) {
            spawn_mess_hall_supplier(b, road.x, road.y, 4);
            b->figure_spawn_delay = 0;
        }
    }
}

static void spawn_figure_depot(building *b)
{
    check_labor_problem(b);

    map_point road;
    if (map_has_road_access(b->x, b->y, b->size, &road)) {
        spawn_labor_seeker(b, road.x, road.y, 100);

        int pct_workers = worker_percentage(Building(b));
        int max_carts = 0;
        if (pct_workers >= 75) {
            max_carts = 3;
        } else if (pct_workers >= 50) {
            max_carts = 2;
        } else if (pct_workers > 0) {
            max_carts = 1;
        }

        int existing_carts = 0;
        for (int i = 0; i < 3; i++) {
            if (b->data.distribution.cartpusher_ids[i]) {
                Figure *f = Figure::get(b->data.distribution.cartpusher_ids[i]);
                if (f->type == FIGURE_DEPOT_CART_PUSHER &&
                    figure_belongs_to_building(f, b)) {
                    existing_carts++;
                } else {
                    b->data.distribution.cartpusher_ids[i] = 0;
                }
            }
        }

        if (existing_carts >= max_carts) {
            return;
        }

        int spawn_delay = default_spawn_delay(Building(b));
        b->figure_spawn_delay++;
        if (b->figure_spawn_delay > spawn_delay) {
            b->figure_spawn_delay = 0;
            Building owner(b);
            Figure *f = figure_runtime_create_profiled(
                FIGURE_DEPOT_CART_PUSHER,
                road.x,
                road.y,
                DIR_0_TOP,
                owner,
                "order_runner");
            if (!f) {
                return;
            }

            for (int i = 0; i < 3; i++) {
                if (!b->data.distribution.cartpusher_ids[i]) {
                    b->data.distribution.cartpusher_ids[i] = f->id();
                    break;
                }
            }
        }
    }
}

static void update_native_crop_progress(building *b)
{
    b->data.industry.progress++;
    if (b->data.industry.progress >= 5) {
        b->data.industry.progress = 0;
    }
    map_image_set(b->grid_offset, image_group(GROUP_BUILDING_FARM_CROPS) + b->data.industry.progress);
}

static int building_uses_runtime_spawn(const building *b)
{
    if (!b) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && !definition->spawn_groups().empty()) {
        return 1;
    }
    if (building_matches_any(b, {"senate", "forum"})) {
        return 1;
    }
    if (type_is_basic_temple(b->type)) {
        return b->monument.phase <= 0;
    }
    if (definition && definition->is_theater()) {
        return 1;
    }

    return type_uses_native_spawn(b->type);
}

void building_figure_generate(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(static_cast<unsigned int>(i));
        if (!b || !b->id) {
            continue;
        }

        Building building_object(b);
        if (b->state != BUILDING_STATE_IN_USE) {
            b->show_on_problem_overlay = 1;
            continue;
        }
        if (!building_object.is_main_part() || building_monument_is_unfinished_monument(b)) {
            continue;
        }

        b->show_on_problem_overlay = 0;
        if (building_uses_runtime_spawn(b)) {
            building_object.spawn_figure();
        } else if (building_is_raw_resource_producer(b->type) ||
            building_is_farm(b->type) || building_is_workshop(b->type)) {
            spawn_figure_industry(b);
        } else if (building_matches_any(b, {"senate_1_unused", "forum_2_unused"})) {
            spawn_figure_senate_forum(b);
        } else if (building_object.type->is_warehouse()) {
            spawn_figure_warehouse(b);
        } else if (building_object.type->is_granary()) {
            spawn_figure_granary(b);
        } else if (building_matches(b, "tower")) {
            spawn_figure_tower(b);
        } else if (building_matches(b, "hippodrome")) {
            spawn_figure_hippodrome(b);
        } else if (building_matches(b, "colosseum")) {
            spawn_figure_colosseum(b);
        } else if (building_matches(b, "market")) {
            spawn_figure_market(b);
        } else if (building_matches(b, "mission_post")) {
            spawn_figure_mission_post(b);
        } else if (std::strcmp(building_object.type->attr(), "dock") == 0) {
            spawn_figure_dock(b);
        } else if (building_matches(b, "wharf")) {
            spawn_figure_wharf(b);
        } else if (building_matches(b, "shipyard")) {
            spawn_figure_shipyard(b);
        } else if (building_matches_any(b, {"native_hut", "native_hut_alt"})) {
            spawn_figure_native_hut(b);
        } else if (building_matches(b, "native_meeting")) {
            spawn_figure_native_meeting(b);
        } else if (building_matches(b, "native_crops")) {
            update_native_crop_progress(b);
        } else if (type_is_fort(b->type)) {
            Building fort(*b);
            formation_legion_update_recruit_status(fort);
            spawn_figure_fort_supplier(b);
        } else if (building_matches(b, "barracks")) {
            spawn_figure_barracks(b);
        } else if (building_matches(b, "military_academy")) {
            spawn_figure_military_academy(b);
        } else if (building_object.type->is_mess_hall()) {
            spawn_figure_mess_hall(b);
        } else if (building_matches(b, "tavern")) {
            spawn_figure_tavern(b);
        } else if (building_matches(b, "cart_depot")) {
            spawn_figure_depot(b);
        }
    }
}
