#include "building/building_record.h"
#include "count.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "building/monument.h"
#include "core/array.h"
#include "city/buildings.h"
#include "city/health.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "map/property.h"

#include <cstdlib>
#include <cstring>

struct building_type_set_entry {
    building_type type;
    const char *text_id;
};

static building_type resolve_set_entry(const building_type_set_entry &entry)
{
    return entry.text_id ? building_type_registry_impl::runtime_id_from_text(entry.text_id) : entry.type;
}

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_impl::runtime_id_from_text(text_id);
}

static int type_matches_text(building_type type, const char *text_id)
{
    return type == runtime_type(text_id);
}

static int is_any_type(building_type type)
{
    return type == BUILDING_NONE || type_matches_text(type, "any");
}

static int is_fort_menu_type(building_type type)
{
    return type_matches_text(type, "fort");
}

static const building_type_set_entry building_set_farms[] = {
    {BUILDING_NONE, "wheat_farm"},
    {BUILDING_NONE, "vegetable_farm"},
    {BUILDING_NONE, "fruit_farm"},
    {BUILDING_NONE, "olive_farm"},
    {BUILDING_NONE, "vines_farm"},
    {BUILDING_NONE, "pig_farm"}
};

#define BUILDING_SET_SIZE_FARMS (sizeof(building_set_farms) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_workshops[] = {
    {BUILDING_NONE, "wine_workshop"},
    {BUILDING_NONE, "oil_workshop"},
    {BUILDING_NONE, "weapons_workshop"},
    {BUILDING_NONE, "furniture_workshop"},
    {BUILDING_NONE, "pottery_workshop"},
    {BUILDING_NONE, "concrete_maker"},
    {BUILDING_NONE, "brickworks"},
    {BUILDING_NONE, "city_mint"}
};

#define BUILDING_SET_SIZE_WORKSHOPS (sizeof(building_set_workshops) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_small_temples[] = {
    {BUILDING_NONE, "small_temple_ceres"},
    {BUILDING_NONE, "small_temple_neptune"},
    {BUILDING_NONE, "small_temple_mercury"},
    {BUILDING_NONE, "small_temple_mars"},
    {BUILDING_NONE, "small_temple_venus"}
};

#define BUILDING_SET_SIZE_SMALL_TEMPLES (sizeof(building_set_small_temples) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_large_temples[] = {
    {BUILDING_NONE, "large_temple_ceres"},
    {BUILDING_NONE, "large_temple_neptune"},
    {BUILDING_NONE, "large_temple_mercury"},
    {BUILDING_NONE, "large_temple_mars"},
    {BUILDING_NONE, "large_temple_venus"}
};

#define BUILDING_SET_SIZE_LARGE_TEMPLES (sizeof(building_set_large_temples) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_grand_temples[] = {
    {BUILDING_NONE, "grand_temple_ceres"},
    {BUILDING_NONE, "grand_temple_neptune"},
    {BUILDING_NONE, "grand_temple_mercury"},
    {BUILDING_NONE, "grand_temple_mars"},
    {BUILDING_NONE, "grand_temple_venus"},
    {BUILDING_NONE, "pantheon"}
};

#define BUILDING_SET_SIZE_GRAND_TEMPLES (sizeof(building_set_grand_temples) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_deco_trees[] = {
    {BUILDING_NONE, "pine_tree"},
    {BUILDING_NONE, "fir_tree"},
    {BUILDING_NONE, "oak_tree"},
    {BUILDING_NONE, "elm_tree"},
    {BUILDING_NONE, "fig_tree"},
    {BUILDING_NONE, "plum_tree"},
    {BUILDING_NONE, "palm_tree"},
    {BUILDING_NONE, "date_tree"}
};

#define BUILDING_SET_SIZE_DECO_TREES (sizeof(building_set_deco_trees) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_deco_paths[] = {
    {BUILDING_NONE, "pine_path"},
    {BUILDING_NONE, "fir_path"},
    {BUILDING_NONE, "oak_path"},
    {BUILDING_NONE, "elm_path"},
    {BUILDING_NONE, "fig_path"},
    {BUILDING_NONE, "plum_path"},
    {BUILDING_NONE, "palm_path"},
    {BUILDING_NONE, "date_path"},
    {BUILDING_NONE, "garden_path"}
};

#define BUILDING_SET_SIZE_DECO_PATHS (sizeof(building_set_deco_paths) / sizeof(building_type_set_entry))

static const building_type_set_entry building_set_deco_statues[] = {
    {BUILDING_NONE, "gardens"},
    {BUILDING_NONE, "grand_garden"},
    {BUILDING_NONE, "small_statue"},
    {BUILDING_NONE, "medium_statue"},
    {BUILDING_NONE, "large_statue"},
    {BUILDING_NONE, "goddess_statue"},
    {BUILDING_NONE, "senator_statue"},
    {BUILDING_NONE, "legion_statue"},
    {BUILDING_NONE, "gladiator_statue"},
    {BUILDING_NONE, "small_pond"},
    {BUILDING_NONE, "large_pond"},
    {BUILDING_NONE, "dolphin_fountain"}
};

#define BUILDING_SET_SIZE_DECO_STATUES (sizeof(building_set_deco_statues) / sizeof(building_type_set_entry))

static const building_type_set_entry all_fort_types[] = {
    {BUILDING_NONE, "fort_legionaries"},
    {BUILDING_NONE, "fort_javelin"},
    {BUILDING_NONE, "fort_mounted"},
    {BUILDING_NONE, "fort_swords"},
    {BUILDING_NONE, "fort_archers"},
};

#define BUILDING_SET_SIZE_FORTS (sizeof(all_fort_types) / sizeof(building_type_set_entry))

int building_count_forts(int active_only);
static int count_all_types_in_set(int active_only, const building_type_set_entry *set, int count);

int building_count_grand_temples(void)
{
    return count_all_types_in_set(0, building_set_grand_temples, BUILDING_SET_SIZE_GRAND_TEMPLES);
}

int building_count_grand_temples_active(void)
{
    return count_all_types_in_set(1, building_set_grand_temples, BUILDING_SET_SIZE_GRAND_TEMPLES);
}

static int count_forts_per_type(building_type type, int active_only);

int building_count_active(building_type type)
{
    if (is_fort_menu_type(type)) {
        return building_count_forts(1); // 1 means check active only
    }
    if (building_is_fort(type)) {
        return count_forts_per_type(type, 1);
    }
    int active = 0;
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (building_is_active(b) && b == building_main(b)) {
            active++;
        }
    }
    return active;
}

int building_count_total(building_type type)
{
    if (is_fort_menu_type(type)) {
        return building_count_forts(0); // 0 means do not check active only
    }
    if (building_is_fort(type)) {
        return count_forts_per_type(type, 0);
    }
    int total = 0;
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if ((b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED ||
            b->state == BUILDING_STATE_MOTHBALLED) && b == building_main(b)) {
            total++;
        }
    }
    return total;
}

int building_count_any_total(int active_only)
{
    int total = 0;
    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        if (b == building_main(b)) {
            if (active_only) {
                if (building_is_active(b)) {
                    total++;
                }
            } else {
                if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED ||
                    b->state == BUILDING_STATE_MOTHBALLED) {
                    total++;
                }
            }
        }
    }
    return total;
}

int building_count_upgraded(building_type type)
{
    int upgraded = 0;
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if ((b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED) && b->upgrade_level > 0 && b == building_main(b)) {
            upgraded++;
        }
    }
    return upgraded;
}

int building_count_in_area(building_type type, int minx, int miny, int maxx, int maxy)
{
    if (building_is_fort(type) || is_fort_menu_type(type)) {
        return building_count_fort_type_in_area(minx, miny, maxx, maxy, type);
    }
    int grid_area = (abs(maxx - minx) + 1) * (abs(maxy - miny) + 1);
    int array_size = grid_area < building_count() ? grid_area : building_count();
    unsigned int *found_buildings = (unsigned int *) malloc(array_size * sizeof(unsigned int));
    memset(found_buildings, 0, array_size * sizeof(unsigned int));

    int total = 0;
    for (int x = minx; x <= maxx; x++) {
        for (int y = miny; y <= maxy; y++) {
            int grid_offset = map_grid_offset(x, y);
            int building_id = map_building_at(grid_offset);
            if (building_id) {
                building *b = building_main(building_get(building_id));
                if (!is_any_type(type) && b->type != type) {
                    continue;
                }
                if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_CREATED) {
                    continue;
                }

                int building_already_counted = 0;

                for (int i = 0; i < total; i++) {
                    if (found_buildings[i] == b->id) {
                        building_already_counted = 1;
                        break;
                    }
                }

                if (building_already_counted) {
                    continue;
                }

                found_buildings[total] = b->id;
                total++;
            }
        }
    }

    free(found_buildings);
    return total;
}

int building_count_fort_type_in_area(int minx, int miny, int maxx, int maxy, building_type type)
{
    int grid_area = abs((maxx - minx) * (maxy - miny));
    int array_size = grid_area < building_count() ? grid_area : building_count();
    int *found_buildings = (int *) malloc(array_size * sizeof(int));

    int total = 0;
    for (int x = minx; x <= maxx; x++) {
        for (int y = miny; y <= maxy; y++) {
            int grid_offset = map_grid_offset(x, y);
            int building_id = map_building_at(grid_offset);
            if (building_id) {
                building *b = building_main(building_get(building_id));
                if (!building_is_fort(b->type) || (b->subtype.fort_figure_type != 
                    building_count_forts_get_figure_type_from_building(type) && !is_fort_menu_type(type))) {
                    continue;
                }
                if (b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_CREATED) {
                    continue;
                }

                int building_already_counted = 0;

                for (int i = 0; i < total; i++) {
                    if ((unsigned int) found_buildings[i] == b->id) {
                        building_already_counted = 1;
                        break;
                    }
                }

                if (building_already_counted) {
                    continue;
                }

                found_buildings[total] = b->id;
                total++;
            }
        }
    }

    free(found_buildings);
    return total;
}

static int building_count_with_active_check(building_type type, int active_only)
{
    if (active_only) {
        return building_count_active(type);
    } else {
        return building_count_total(type);
    }
}

static int count_all_types_in_set(int active_only, const building_type_set_entry *set, int count)
{
    int total = 0;
    for (int i = 0; i < count; i++) {
        building_type type = resolve_set_entry(set[i]);
        if (type != BUILDING_NONE) {
            total += building_count_with_active_check(type, active_only);
        }
    }
    return total;
}

static int count_all_types_in_set_in_area(const building_type_set_entry *set, int count, int minx, int miny, int maxx, int maxy)
{
    int total = 0;
    for (int i = 0; i < count; i++) {
        building_type type = resolve_set_entry(set[i]);
        if (type != BUILDING_NONE) {
            total += building_count_in_area(type, minx, miny, maxx, maxy);
        }
    }
    return total;
}

int building_set_count_farms(int active_only)
{
    return count_all_types_in_set(active_only, building_set_farms, BUILDING_SET_SIZE_FARMS);
}

int building_set_count_raw_materials(int active_only)
{
    int total = 0;
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
        resource = (resource_type) ((int) resource + 1)) {
        if (resource_is_raw_material(resource)) {
            total += building_count_with_active_check(building_producer_for_resource(resource), active_only);
        }
    }
    return total;
}

int building_set_count_workshops(int active_only)
{
    return count_all_types_in_set(active_only, building_set_workshops, BUILDING_SET_SIZE_WORKSHOPS);
}

int building_set_count_small_temples(int active_only)
{
    return count_all_types_in_set(active_only, building_set_small_temples, BUILDING_SET_SIZE_SMALL_TEMPLES);
}

int building_set_count_large_temples(int active_only)
{
    return count_all_types_in_set(active_only, building_set_large_temples, BUILDING_SET_SIZE_LARGE_TEMPLES);
}

int building_set_count_deco_trees(void)
{
    return count_all_types_in_set(0, building_set_deco_trees, BUILDING_SET_SIZE_DECO_TREES);
}

int building_set_count_deco_paths(void)
{
    return count_all_types_in_set(0, building_set_deco_paths, BUILDING_SET_SIZE_DECO_PATHS);
}

int building_set_count_deco_statues(void)
{
    return count_all_types_in_set(0, building_set_deco_statues, BUILDING_SET_SIZE_DECO_STATUES);
}

int building_set_area_count_farms(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_farms, BUILDING_SET_SIZE_FARMS, minx, miny, maxx, maxy);
}

int building_set_area_count_raw_materials(int minx, int miny, int maxx, int maxy)
{
    int total = 0;
    for (resource_type resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT;
        resource = (resource_type) ((int) resource + 1)) {
        if (resource_is_raw_material(resource)) {
            total += building_count_in_area(building_producer_for_resource(resource), minx, miny, maxx, maxy);
        }
    }
    return total;
}

int building_set_area_count_workshops(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_workshops, BUILDING_SET_SIZE_WORKSHOPS, minx, miny, maxx, maxy);
}

int building_set_area_count_small_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_small_temples, BUILDING_SET_SIZE_SMALL_TEMPLES, minx, miny, maxx, maxy);
}

int building_set_area_count_large_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_large_temples, BUILDING_SET_SIZE_LARGE_TEMPLES, minx, miny, maxx, maxy);
}

int building_set_area_count_grand_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_grand_temples, BUILDING_SET_SIZE_GRAND_TEMPLES, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_trees(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_trees, BUILDING_SET_SIZE_DECO_TREES, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_paths(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_paths, BUILDING_SET_SIZE_DECO_PATHS, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_statues(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_statues, BUILDING_SET_SIZE_DECO_STATUES, minx, miny, maxx, maxy);
}

static int count_forts_per_type(building_type type, int active_only)
{
    int count = 0;
    //if active_only is 1, only count buildings that are active, otherwise count all valid forts of the type
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (!active_only) {
            if (!(b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED)) {
                continue; // if not couting active buildings, check if building at least in use or created
            }
        }
        int increase_count = ((building_is_active(b) >= active_only) && b == building_main(b));
        if (b->subtype.fort_figure_type == building_count_forts_get_figure_type_from_building(type) && increase_count) {
            count++;
        }
    }
    return count;
}

figure_type building_count_forts_get_figure_type_from_building(building_type type)
{
    struct fort_figure_type {
        const char *text_id;
        figure_type figure;
    };
    static const fort_figure_type fort_figure_types[] = {
        {"fort_legionaries", FIGURE_FORT_LEGIONARY},
        {"fort_javelin", FIGURE_FORT_JAVELIN},
        {"fort_mounted", FIGURE_FORT_MOUNTED},
        {"fort_swords", FIGURE_FORT_INFANTRY},
        {"fort_archers", FIGURE_FORT_ARCHER},
    };
    for (const fort_figure_type &entry : fort_figure_types) {
        if (type_matches_text(type, entry.text_id)) {
            return entry.figure;
        }
    }
    return FIGURE_NONE;
}

int building_count_forts(int active_only)
{
    int total = 0;
    for (size_t i = 0; i < BUILDING_SET_SIZE_FORTS; i++) {
        building_type type = resolve_set_entry(all_fort_types[i]);
        if (type != BUILDING_NONE) {
            total += count_forts_per_type(type, active_only);
        }
    }
    return total;
}

int building_count_terrain_in_area(int minx, int miny, int maxx, int maxy, int terrain, int (*condition)(int))
{
    int total = 0;
    int grid_offset;
    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
            grid_offset = map_grid_offset(x, y);

            if (map_terrain_is(grid_offset, terrain) && condition(grid_offset)) {
                total++;
            }
        }
    }
    return total;
}

static int min_x;
static int min_y;

static void get_min_map_xy(void)
{
    min_x = map_grid_offset_to_x(map_data.start_offset);
    min_y = map_grid_offset_to_y(map_data.start_offset);
}

int building_count_terrain(int terrain, int (*condition)(int))
{
    get_min_map_xy();
    return building_count_terrain_in_area(min_x, min_y, min_x + map_data.width, min_y + map_data.height, terrain, condition);
}

int building_count_bridges(int ship)
{
    get_min_map_xy();
    float total = 0;
    int grid_offset;
    for (int y = min_y; y < min_y + map_data.height; y++) {
        for (int x = min_x; x < min_x + map_data.width; x++) {
            grid_offset = map_grid_offset(x, y);
            int bridge_sprite = map_sprite_bridge_at(grid_offset);
            if (bridge_sprite >= 1 + ship * 6 && bridge_sprite <= 4 + ship * 6) {
                total += 0.5;
            }
        }
    }
    return (int) total;
}

int building_count_bridges_in_area(int minx, int miny, int maxx, int maxy, int ship)
{
    array(int) bridge_ids = { 0 };
    array_init(bridge_ids, 4, NULL, NULL);
    int grid_offset;
    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
            grid_offset = map_grid_offset(x, y);
            int building_id = map_building_at(grid_offset);
            if (!building_id) {
                continue;
            }
            building *b = building_main(building_get(building_id));
            if (!b) {
                continue;
            }
            if (type_matches_text(b->type, ship ? "ship_bridge" : "low_bridge") && map_is_bridge(grid_offset)) {
                int found = 0;
                int *item;
                array_foreach(bridge_ids, item)
                {
                    if ((unsigned int) *item == b->id) {
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    int *bridge_id;
                    array_new_item(bridge_ids, bridge_id);
                    if (bridge_id) {
                        *bridge_id = b->id;
                    }
                }
            }
        }
    }
    int total = bridge_ids.size;
    array_clear(bridge_ids);
    return total;
}
