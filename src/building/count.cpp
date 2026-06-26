#include "building/building_record.h"
#include "count.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/industry.h"
#include "city/buildings.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/sprite.h"
#include "map/terrain.h"

#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

static int is_any_type(building_type type)
{
    return type == BUILDING_NONE || building_type_registry_impl::type_attr_is(type, "any");
}

static int is_fort_menu_type(building_type type)
{
    return building_type_registry_impl::type_attr_is(type, "fort");
}

static constexpr std::string_view building_set_workshops[] = {
    "wine_workshop",
    "oil_workshop",
    "weapons_workshop",
    "furniture_workshop",
    "pottery_workshop",
    "concrete_maker",
    "brickworks",
    "city_mint",
};

static constexpr std::string_view building_set_small_temples[] = {
    "small_temple_ceres",
    "small_temple_neptune",
    "small_temple_mercury",
    "small_temple_mars",
    "small_temple_venus",
};

static constexpr std::string_view building_set_large_temples[] = {
    "large_temple_ceres",
    "large_temple_neptune",
    "large_temple_mercury",
    "large_temple_mars",
    "large_temple_venus",
};

static constexpr std::string_view building_set_grand_temples[] = {
    "grand_temple_ceres",
    "grand_temple_neptune",
    "grand_temple_mercury",
    "grand_temple_mars",
    "grand_temple_venus",
    "pantheon",
};

static constexpr std::string_view building_set_deco_trees[] = {
    "pine_tree",
    "fir_tree",
    "oak_tree",
    "elm_tree",
    "fig_tree",
    "plum_tree",
    "palm_tree",
    "date_tree",
};

static constexpr std::string_view building_set_deco_paths[] = {
    "pine_path",
    "fir_path",
    "oak_path",
    "elm_path",
    "fig_path",
    "plum_path",
    "palm_path",
    "date_path",
    "garden_path",
};

static constexpr std::string_view building_set_deco_statues[] = {
    "gardens",
    "grand_garden",
    "small_statue",
    "medium_statue",
    "large_statue",
    "goddess_statue",
    "senator_statue",
    "legion_statue",
    "gladiator_statue",
    "small_pond",
    "large_pond",
    "dolphin_fountain",
};

struct fort_type_entry {
    std::string_view attr;
    figure_type figure;
};

static constexpr fort_type_entry fort_types[] = {
    {"fort_legionaries", FIGURE_FORT_LEGIONARY},
    {"fort_javelin", FIGURE_FORT_JAVELIN},
    {"fort_mounted", FIGURE_FORT_MOUNTED},
    {"fort_swords", FIGURE_FORT_INFANTRY},
    {"fort_archers", FIGURE_FORT_ARCHER},
};

int building_count_forts(int active_only);
static int count_all_types_in_set(int active_only, std::span<const std::string_view> set);

int building_count_grand_temples(void)
{
    return count_all_types_in_set(0, building_set_grand_temples);
}

int building_count_grand_temples_active(void)
{
    return count_all_types_in_set(1, building_set_grand_temples);
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
        if (b != building_main(b)) {
            continue;
        }
        if (active_only ? building_is_active(b) :
            (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED ||
                b->state == BUILDING_STATE_MOTHBALLED)) {
            total++;
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

template <typename Predicate>
static int count_unique_buildings_in_area(int minx, int miny, int maxx, int maxy, Predicate accepts)
{
    std::vector<unsigned int> found_buildings;
    found_buildings.reserve(static_cast<size_t>(building_count()));
    for (int x = minx; x <= maxx; x++) {
        for (int y = miny; y <= maxy; y++) {
            const int building_id = map_building_at(map_grid_offset(x, y));
            if (!building_id) {
                continue;
            }
            building *b = building_main(building_get(building_id));
            if (!accepts(b) || std::find(found_buildings.begin(), found_buildings.end(), b->id) != found_buildings.end()) {
                continue;
            }
            found_buildings.push_back(b->id);
        }
    }
    return static_cast<int>(found_buildings.size());
}

int building_count_in_area(building_type type, int minx, int miny, int maxx, int maxy)
{
    if (building_is_fort(type) || is_fort_menu_type(type)) {
        return building_count_fort_type_in_area(minx, miny, maxx, maxy, type);
    }
    return count_unique_buildings_in_area(minx, miny, maxx, maxy, [type](building *b) {
        return (is_any_type(type) || b->type == type) &&
            (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED);
    });
}

int building_count_fort_type_in_area(int minx, int miny, int maxx, int maxy, building_type type)
{
    const bool all_forts = is_fort_menu_type(type);
    const figure_type figure_type = building_count_forts_get_figure_type_from_building(type);
    return count_unique_buildings_in_area(minx, miny, maxx, maxy, [all_forts, figure_type](building *b) {
        return building_is_fort(b->type) &&
            (all_forts || b->subtype.fort_figure_type == figure_type) &&
            (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_CREATED);
    });
}

static int building_count_with_active_check(building_type type, int active_only)
{
    return active_only ? building_count_active(type) : building_count_total(type);
}

static int count_all_types_in_set(int active_only, std::span<const std::string_view> set)
{
    int total = 0;
    for (std::string_view attr : set) {
        building_type type = building_type_registry_impl::type_from_attr(attr);
        if (type != BUILDING_NONE) {
            total += building_count_with_active_check(type, active_only);
        }
    }
    return total;
}

static int count_all_types_in_set_in_area(std::span<const std::string_view> set, int minx, int miny, int maxx, int maxy)
{
    int total = 0;
    for (std::string_view attr : set) {
        building_type type = building_type_registry_impl::type_from_attr(attr);
        if (type != BUILDING_NONE) {
            total += building_count_in_area(type, minx, miny, maxx, maxy);
        }
    }
    return total;
}

template <typename Count>
static int count_farm_types(Count count)
{
    int total = 0;
    for (const auto &definition : building_type_registry_impl::g_building_types) {
        if (definition && definition->is_farm()) {
            total += count(definition->type());
        }
    }
    return total;
}

int building_set_count_farms(int active_only)
{
    return count_farm_types([active_only](building_type type) {
        return building_count_with_active_check(type, active_only);
    });
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
    return count_all_types_in_set(active_only, building_set_workshops);
}

int building_set_count_small_temples(int active_only)
{
    return count_all_types_in_set(active_only, building_set_small_temples);
}

int building_set_count_large_temples(int active_only)
{
    return count_all_types_in_set(active_only, building_set_large_temples);
}

int building_set_count_deco_trees(void)
{
    return count_all_types_in_set(0, building_set_deco_trees);
}

int building_set_count_deco_paths(void)
{
    return count_all_types_in_set(0, building_set_deco_paths);
}

int building_set_count_deco_statues(void)
{
    return count_all_types_in_set(0, building_set_deco_statues);
}

int building_set_area_count_farms(int minx, int miny, int maxx, int maxy)
{
    return count_farm_types([minx, miny, maxx, maxy](building_type type) {
        return building_count_in_area(type, minx, miny, maxx, maxy);
    });
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
    return count_all_types_in_set_in_area(building_set_workshops, minx, miny, maxx, maxy);
}

int building_set_area_count_small_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_small_temples, minx, miny, maxx, maxy);
}

int building_set_area_count_large_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_large_temples, minx, miny, maxx, maxy);
}

int building_set_area_count_grand_temples(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_grand_temples, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_trees(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_trees, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_paths(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_paths, minx, miny, maxx, maxy);
}

int building_set_area_count_deco_statues(int minx, int miny, int maxx, int maxy)
{
    return count_all_types_in_set_in_area(building_set_deco_statues, minx, miny, maxx, maxy);
}

static int count_forts_per_type(building_type type, int active_only)
{
    int count = 0;
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (!active_only && b->state != BUILDING_STATE_IN_USE && b->state != BUILDING_STATE_CREATED) {
            continue;
        }
        if (building_is_active(b) >= active_only && b == building_main(b) &&
            b->subtype.fort_figure_type == building_count_forts_get_figure_type_from_building(type)) {
            count++;
        }
    }
    return count;
}

figure_type building_count_forts_get_figure_type_from_building(building_type type)
{
    for (const fort_type_entry &entry : fort_types) {
        if (building_type_registry_impl::type_attr_is(type, entry.attr)) {
            return entry.figure;
        }
    }
    return FIGURE_NONE;
}

int building_count_forts(int active_only)
{
    int total = 0;
    for (const fort_type_entry &entry : fort_types) {
        building_type type = building_type_registry_impl::type_from_attr(entry.attr);
        if (type != BUILDING_NONE) {
            total += count_forts_per_type(type, active_only);
        }
    }
    return total;
}

int building_count_terrain_in_area(int minx, int miny, int maxx, int maxy, int terrain, int (*condition)(int))
{
    int total = 0;
    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
            const int grid_offset = map_grid_offset(x, y);
            if (map_terrain_is(grid_offset, terrain) && condition(grid_offset)) {
                total++;
            }
        }
    }
    return total;
}

int building_count_terrain(int terrain, int (*condition)(int))
{
    const int min_x = map_grid_offset_to_x(map_data.start_offset);
    const int min_y = map_grid_offset_to_y(map_data.start_offset);
    return building_count_terrain_in_area(min_x, min_y, min_x + map_data.width, min_y + map_data.height, terrain, condition);
}

int building_count_bridges(int ship)
{
    const int min_x = map_grid_offset_to_x(map_data.start_offset);
    const int min_y = map_grid_offset_to_y(map_data.start_offset);
    int bridge_tiles = 0;
    for (int y = min_y; y < min_y + map_data.height; y++) {
        for (int x = min_x; x < min_x + map_data.width; x++) {
            const int grid_offset = map_grid_offset(x, y);
            const int bridge_sprite = map_sprite_bridge_at(grid_offset);
            if (bridge_sprite >= 1 + ship * 6 && bridge_sprite <= 4 + ship * 6) {
                bridge_tiles++;
            }
        }
    }
    return bridge_tiles / 2;
}

int building_count_bridges_in_area(int minx, int miny, int maxx, int maxy, int ship)
{
    std::vector<unsigned int> bridge_ids;
    for (int y = miny; y < maxy; y++) {
        for (int x = minx; x < maxx; x++) {
            const int grid_offset = map_grid_offset(x, y);
            const int building_id = map_building_at(grid_offset);
            if (!building_id) {
                continue;
            }
            building *b = building_main(building_get(building_id));
            if (!b) {
                continue;
            }
            if (building_type_registry_impl::type_attr_is(b->type, ship ? "ship_bridge" : "low_bridge") &&
                map_is_bridge(grid_offset)) {
                if (std::find(bridge_ids.begin(), bridge_ids.end(), b->id) == bridge_ids.end()) {
                    bridge_ids.push_back(b->id);
                }
            }
        }
    }
    return static_cast<int>(bridge_ids.size());
}
