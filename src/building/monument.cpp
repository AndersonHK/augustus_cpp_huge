#include "building/image.h"
#include "map/building_tiles.h"
#include "map/orientation.h"
#include "map/road_access.h"
#include "monument.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/religion.h"
#include "city/culture.h"

#include "assets/assets.h"
#include "building/properties.h"
#include "building/building_type_id_bridge.h"
#include "city/finance.h"
#include "city/message.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/log.h"
#include "empire/city.h"
#include "game/resource_id_bridge.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/property.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#define ORIGINAL_DELIVERY_BUFFER_SIZE 16
#define MODULES_PER_TEMPLE 2
#define ARCHITECTS RESOURCE_NONE

#define MAX_PHASES 6

#define NOTHING 0
#define INFINITE 10000

typedef struct {
    int phases;
    int resources[MAX_PHASES][RESOURCE_SLOT_COUNT];
} monument_type;

#define RESOURCE_ROW(architects, timber, iron, marble, gold, stone, concrete, bricks) \
    { architects, 0, 0, 0, 0, 0, 0, timber, 0, 0, iron, marble, gold, 0, stone, 0, 0, 0, 0, 0, concrete, bricks }

static const monument_type pantheon = {
    6,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 20, 0, 0),
        RESOURCE_ROW(1, 8, 0, 0, 0, 20, 0, 0),
        RESOURCE_ROW(1, 16, 0, 0, 0, 0, 40, 0),
        RESOURCE_ROW(1, 16, 0, 32, 16, 0, 0, 0),
        RESOURCE_ROW(4, 0, 0, 0, 0, 0, 0, 0),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type colosseum = {
    5,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 12, 0, 0),
        RESOURCE_ROW(1, 8, 0, 0, 0, 0, 16, 0),
        RESOURCE_ROW(1, 12, 0, 0, 0, 16, 0, 0),
        RESOURCE_ROW(4, 12, 0, 20, 0, 0, 0, 16),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type hippodrome = {
    5,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 32, 0, 0),
        RESOURCE_ROW(1, 16, 0, 0, 0, 0, 32, 0),
        RESOURCE_ROW(1, 16, 0, 0, 0, 32, 0, 0),
        RESOURCE_ROW(4, 32, 0, 48, 0, 0, 0, 32),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type small_mausoleum = {
    3,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 2, 4, 0),
        RESOURCE_ROW(1, 0, 0, 2, 0, 0, 0, 0),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type nymphaeum = {
    3,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 12, 4, 0),
        RESOURCE_ROW(1, 0, 0, 2, 0, 0, 0, 0),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type large_mausoleum = {
    3,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 8, 4, 0),
        RESOURCE_ROW(1, 0, 0, 4, 0, 0, 0, 0),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

static const monument_type city_mint = {
    3,
    {
        RESOURCE_ROW(1, 0, 0, 0, 0, 8, 0, 0),
        RESOURCE_ROW(1, 4, 4, 0, 0, 0, 0, 4),
        RESOURCE_ROW(NOTHING, 0, 0, 0, 0, 0, 0, 0)
    }
};

#undef RESOURCE_ROW

static int type_is_grand_temple(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Grand);
}

static const monument_type *legacy_monument_type(building_type type)
{
    if (building_type_registry_impl::type_attr_is(type, "pantheon")) {
        return &pantheon;
    }
    if (building_type_registry_impl::type_attr_is(type, "colosseum")) {
        return &colosseum;
    }
    if (building_type_registry_impl::type_attr_is(type, "hippodrome")) {
        return &hippodrome;
    }
    if (building_type_registry_impl::type_attr_is(type, "nymphaeum")) {
        return &nymphaeum;
    }
    if (building_type_registry_impl::type_attr_is(type, "large_mausoleum")) {
        return &large_mausoleum;
    }
    if (building_type_registry_impl::type_attr_is(type, "small_mausoleum")) {
        return &small_mausoleum;
    }
    if (building_type_registry_impl::type_attr_is(type, "city_mint")) {
        return &city_mint;
    }
    return nullptr;
}

static const char *LEGACY_MONUMENT_TEXT_IDS[] = {
    "pantheon",
    "colosseum",
    "hippodrome",
    "nymphaeum",
    "large_mausoleum",
    "small_mausoleum",
    "city_mint"
};

static std::vector<std::string> MONUMENT_TEXT_IDS;
static int monument_text_ids_initialized;
static monument_type xml_monument_types[BUILDING_TYPE_MAX];
static int xml_monument_types_initialized[BUILDING_TYPE_MAX];

static int monument_text_id_exists(const char *text_id)
{
    if (!text_id || !*text_id) {
        return 0;
    }
    for (const std::string &monument_text_id : MONUMENT_TEXT_IDS) {
        if (monument_text_id == text_id) {
            return 1;
        }
    }
    return 0;
}

static void add_monument_text_id(const char *text_id)
{
    if (!text_id || !*text_id || monument_text_id_exists(text_id)) {
        return;
    }
    MONUMENT_TEXT_IDS.push_back(text_id);
}

static void ensure_monument_text_ids()
{
    if (monument_text_ids_initialized) {
        return;
    }

    MONUMENT_TEXT_IDS.clear();
    for (const char *text_id : LEGACY_MONUMENT_TEXT_IDS) {
        add_monument_text_id(text_id);
    }

    for (int type_id = 1; type_id < BUILDING_TYPE_MAX; type_id++) {
        building_type type = static_cast<building_type>(type_id);
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (!definition || !definition->has_construction()) {
            continue;
        }
        add_monument_text_id(building_type_id_bridge_text_from_runtime(type));
    }

    monument_text_ids_initialized = 1;
}

void building_monument_reset_runtime_bridge(void)
{
    MONUMENT_TEXT_IDS.clear();
    monument_text_ids_initialized = 0;
    for (int type_id = 0; type_id < BUILDING_TYPE_MAX; type_id++) {
        xml_monument_types_initialized[type_id] = 0;
    }
}

// XML phased monuments are projected into the legacy resource table shape so save-loaded
// building records can keep using the old monument code path without persisting construction objects.
static const monument_type *xml_monument_type(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return nullptr;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition || !definition->has_phased_construction()) {
        return nullptr;
    }

    const building_type_registry_impl::ConstructionDefinition &construction = definition->construction();
    int phase_count = construction.phase_count();
    if (phase_count <= 0 || phase_count + 1 > MAX_PHASES) {
        return nullptr;
    }

    int type_index = static_cast<int>(type);
    if (!xml_monument_types_initialized[type_index]) {
        monument_type &cached_type = xml_monument_types[type_index];
        cached_type.phases = phase_count + 1;
        for (int phase = 0; phase < MAX_PHASES; phase++) {
            for (int resource = 0; resource < RESOURCE_SLOT_COUNT; resource++) {
                cached_type.resources[phase][resource] = 0;
            }
        }
        for (int phase = 1; phase <= phase_count; phase++) {
            for (int resource = 0; resource < RESOURCE_SLOT_COUNT; resource++) {
                cached_type.resources[phase - 1][resource] =
                    construction.requirement_amount(static_cast<resource_type>(resource), phase);
            }
        }
        xml_monument_types_initialized[type_index] = 1;
    }

    return &xml_monument_types[type_index];
}

static const monument_type *monument_type_for(building_type type)
{
    const monument_type *legacy_type = legacy_monument_type(type);
    return legacy_type ? legacy_type : xml_monument_type(type);
}

struct monument_delivery {
    int walker_id;
    unsigned int destination_id;
    int resource;
    int cartloads;
};

static std::vector<monument_delivery> monument_deliveries;

static int type_is_monument(building_type type)
{
    if (building_monument_text_id_is_monument(building_type_id_bridge_text_from_runtime(type))) {
        return 1;
    }
    return monument_type_for(type) ? 1 : 0;
}

int building_monument_text_id_is_monument(const char *text_id)
{
    if (!text_id || !*text_id) {
        return 0;
    }
    ensure_monument_text_ids();
    for (const std::string &monument_text_id : MONUMENT_TEXT_IDS) {
        if (monument_text_id == text_id) {
            return 1;
        }
    }

    building_type type = building_type_registry_impl::type_from_attr(text_id);
    return monument_type_for(type) != nullptr;
}

int building_monument_deliver_resource(building *b, int resource)
{
    if (b->id <= 0 || !building_monument_is_monument(b) ||
        b->resources[resource] <= 0) {
        return 0;
    }

    while (b->prev_part_building_id) {
        b = building_get(b->prev_part_building_id);
    }

    b->resources[resource]--;

    while (b->next_part_building_id) {
        b = building_get(b->next_part_building_id);
        b->resources[resource]--;
    }
    return 1;
}

int building_monument_access_point(building *b, map_point *dst)
{
    if (b->size < 3 || (b && building_type_registry_impl::type_attr_is(b->type, "hippodrome"))) {
        dst->x = b->x;
        dst->y = b->y;
        return 1;
    }
    int dx = b->x - b->road_access_x;
    int dy = b->y - b->road_access_y;
    int half_size = b->size / 2;
    int even_size = b->size % 2;

    if (dx == -half_size && dy == -b->size) {
        dst->x = b->x + half_size;
        dst->y = b->y + b->size - 1;
        return 1;
    } if (dx == 1 && dy == -half_size) {
        dst->x = b->x;
        dst->y = b->y + half_size;
        return 1;
    }
    if (dx == -half_size && dy == 1) {
        dst->x = b->x + half_size;
        dst->y = b->y;
        return 1;
    }
    if (dx == -b->size && dy == -half_size) {
        dst->x = b->x + b->size - 1;
        dst->y = b->y + half_size;
        return 1;
    }
    if (!even_size) {
        return 0;
    }
    if (dx == -1 && dy == -b->size) {
        dst->x = b->x + 1;
        dst->y = b->y + b->size - 1;
        return 1;
    }
    if (dx == 1 && dy == -1) {
        dst->x = b->x;
        dst->y = b->y + 1;
        return 1;
    }
    if (dx == -1 && dy == 1) {
        dst->x = b->x + 1;
        dst->y = b->y;
        return 1;
    }
    if (dx == -b->size && dy == -1) {
        dst->x = b->x + b->size - 1;
        dst->y = b->y + 1;
        return 1;
    }
    return 0;
}

int building_monument_add_module(building *b, int module)
{
    if (!building_monument_is_monument(b) ||
        b->monument.phase != MONUMENT_FINISHED ||
        (b->monument.upgrades &&
            !(b && building_type_registry_impl::type_attr_is(b->type, "caravanserai")) &&
            !(b && building_type_registry_impl::type_attr_is(b->type, "lighthouse")))) {
        return 0;
    }
    b->monument.upgrades = module;
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && definition->has_phased_construction()) {
        Building(b).refresh_graphic();
    } else {
        map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
    }
    return 1;
}

int building_monument_is_limited(building_type type)
{
    return type_is_grand_temple(type) || building_type_registry_impl::type_attr_is_any(type, {
        "pantheon",
        "lighthouse",
        "caravanserai",
        "colosseum",
        "hippodrome",
        "city_mint"
    });
}

int building_monument_get_monument(int x, int y, int resource, int road_network_id, map_point *dst)
{
    if (city_resource_is_stockpiled(static_cast<resource_type>(resource))) {
        return 0;
    }
    int min_dist = INFINITE;
    building *min_building = 0;
    for (int type_id = 1; type_id < BUILDING_TYPE_MAX; type_id++) {
        building_type type = static_cast<building_type>(type_id);
        if (!type_is_monument(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->monument.phase == MONUMENT_FINISHED ||
                b->monument.phase < MONUMENT_START ||
                building_monument_is_construction_halted(b) ||
                (!resource && building_monument_needs_resources(b))) {
                continue;
            }
            short needed = b->resources[resource];
            if ((needed - building_monument_resource_in_delivery(b, resource)) <= 0) {
                continue;
            }
            if (!map_has_road_access(b->x, b->y, b->size, 0) ||
                b->distance_from_entry <= 0 || b->road_network_id != road_network_id) {
                continue;
            }
            int dist = calc_maximum_distance(b->x, b->y, x, y);
            if (dist < min_dist) {
                min_dist = dist;
                min_building = b;
            }
        }
    }
    if (min_building && min_dist < INFINITE) {
        if (dst) {
            map_point_store_result(min_building->road_access_x, min_building->road_access_y, dst);
        }
        return min_building->id;
    }
    return 0;
}

int building_monument_has_unfinished_monuments(void)
{
    for (int type_id = 1; type_id < BUILDING_TYPE_MAX; type_id++) {
        building_type type = static_cast<building_type>(type_id);
        if (!type_is_monument(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->monument.phase != MONUMENT_FINISHED) {
                return 1;
            }
        }
    }
    return 0;
}

int building_monument_resources_needed_for_monument_type(building_type type, int resource, int phase)
{
    const monument_type *type_data = monument_type_for(type);
    if (!type_data || phase < 1 || phase > type_data->phases ||
        resource < RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return 0;
    }
    return type_data->resources[phase - 1][resource];
}

void building_monument_set_phase(building *b, int phase)
{
    if (phase == building_monument_phases(b->type)) {
        phase = MONUMENT_FINISHED;
    }
    if (phase == b->monument.phase) {
        return;
    }
    city_culture_remove_building_module_capacity(b);
    b->monument.phase = phase;
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (definition && definition->has_phased_construction()) {
        Building(b).refresh_graphic();
    } else {
        map_building_tiles_add(b->id, b->x, b->y, b->size, building_image_get(b), TERRAIN_BUILDING);
    }
    if (b->monument.phase != MONUMENT_FINISHED) {
        for (int resource = 0; resource < RESOURCE_SLOT_COUNT; resource++) {
            b->resources[resource] =
              building_monument_resources_needed_for_monument_type(b->type, resource, b->monument.phase);
        }
    }
    city_culture_add_building_module_capacity(b);
}

int building_monument_is_monument(const building *b)
{
    return building_monument_type_is_monument(b->type);
}

int building_monument_type_is_monument(building_type type)
{
    return type_is_monument(type);
}

int building_monument_type_is_mini_monument(building_type type)
{
    if (!building_monument_type_is_monument(type)) {
        return 0;
    }
    return building_properties_for_type(type)->size < 5 ? 1 : 0;
}

int building_monument_is_grand_temple(building_type type)
{
    return type_is_grand_temple(type);
}

int building_monument_needs_resource(building *b, int resource)
{
    if (b->monument.phase == MONUMENT_FINISHED) {
        return 0;
    }
    return (b->resources[resource]);
}

void building_monuments_set_construction_phase(int phase)
{
    for (int type_id = 1; type_id < BUILDING_TYPE_MAX; type_id++) {
        building_type type = static_cast<building_type>(type_id);
        if (!type_is_monument(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            building_monument_set_phase(b, phase);
        }
    }
}

static int grand_temple_id_for_god(god_type god, int require_working)
{
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        const building_type_registry_impl::BuildingType *definition =
            building_type_registry_impl::definition_for_type(type);
        if (!definition || !definition->is_temple(god, building_type_registry_impl::ReligionTier::Grand)) {
            continue;
        }
        if (require_working) {
            int monument_id = building_monument_working(type);
            if (monument_id) {
                return monument_id;
            }
            continue;
        }
        building *monument = building_first_of_type(type);
        if (monument) {
            return monument->id;
        }
    }
    return 0;
}

int building_monument_get_grand_temple_for_god(god_type god)
{
    return grand_temple_id_for_god(god, 0);
}

int building_monument_get_venus_gt(void)
{
    return building_monument_get_grand_temple_for_god(GOD_VENUS);
}

int building_monument_get_neptune_gt(void)
{
    return building_monument_get_grand_temple_for_god(GOD_NEPTUNE);
}

int building_monument_phases(building_type type)
{
    const monument_type *type_data = monument_type_for(type);
    return type_data ? type_data->phases : 0;
}

void building_monument_finish_monuments(void)
{
    for (int type_id = 1; type_id < BUILDING_TYPE_MAX; type_id++) {
        building_type type = static_cast<building_type>(type_id);
        if (!type_is_monument(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            if (b->monument.phase == MONUMENT_FINISHED) {
                continue;
            }
            building_monument_set_phase(b, MONUMENT_FINISHED);
            for (int resource = 0; resource < RESOURCE_SLOT_COUNT; resource++) {
                b->resources[resource] = 0;
            }
        }
    }
}

int building_monument_needs_resources(building *b)
{
    if (b->monument.phase == MONUMENT_FINISHED) {
        return 0;
    }
    for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
        if (b->resources[resource] > 0) {
            return 1;
        }
    }
    return 0;
}

int building_monument_progress(building *b)
{
    if (building_monument_needs_resources(b)) {
        return 0;
    }
    if (b->monument.phase == MONUMENT_FINISHED) {
        return 0;
    }
    while (b->prev_part_building_id) {
        b = building_get(b->prev_part_building_id);
    }
    building_monument_set_phase(b, b->monument.phase + 1);

    while (b->next_part_building_id) {
        b = building_get(b->next_part_building_id);
        building_monument_set_phase(b, b->monument.phase + 1);
    }
    if (b->monument.phase == MONUMENT_FINISHED) {
        if (building_monument_is_grand_temple(b->type)) {
            city_message_post(1, MESSAGE_GRAND_TEMPLE_COMPLETE, 0, b->grid_offset);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "pantheon")) {
            city_message_post(1, MESSAGE_PANTHEON_COMPLETE, 0, b->grid_offset);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "lighthouse")) {
            city_message_post(1, MESSAGE_LIGHTHOUSE_COMPLETE, 0, b->grid_offset);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "colosseum")) {
            city_message_post(1, MESSAGE_COLOSSEUM_COMPLETE, 0, b->grid_offset);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "hippodrome")) {
            city_message_post(1, MESSAGE_HIPPODROME_COMPLETE, 0, b->grid_offset);
        } else if (b && building_type_registry_impl::type_attr_is(b->type, "caravanserai")) {
            city_message_post(1, MESSAGE_CARAVANSERAI_COMPLETE, 0, b->grid_offset);
        }
    }
    return 1;
}

void building_monument_initialize_deliveries(void)
{
    monument_deliveries.clear();
}

void building_monument_add_delivery(unsigned int monument_id, int figure_id, int resource_id, int num_loads)
{
    monument_deliveries.push_back({figure_id, monument_id, resource_id, num_loads});
}

int building_monument_has_delivery_for_worker(int figure_id)
{
    for (const monument_delivery &delivery : monument_deliveries) {
        if (delivery.walker_id == figure_id && delivery.destination_id > 0) {
            return 1;
        }
    }
    return 0;
}

void building_monument_remove_delivery(int figure_id)
{
    monument_deliveries.erase(
        std::remove_if(monument_deliveries.begin(), monument_deliveries.end(),
            [figure_id](const monument_delivery &delivery) {
                return delivery.walker_id == figure_id;
            }),
        monument_deliveries.end());
}

void building_monument_remove_all_deliveries(unsigned int monument_id)
{
    monument_deliveries.erase(
        std::remove_if(monument_deliveries.begin(), monument_deliveries.end(),
            [monument_id](const monument_delivery &delivery) {
                return delivery.destination_id == monument_id;
            }),
        monument_deliveries.end());
}

static int resource_in_delivery(unsigned int monument_id, int resource_id)
{
    int resources = 0;
    for (const monument_delivery &delivery : monument_deliveries) {
        if (delivery.destination_id == monument_id &&
            delivery.resource == resource_id) {
            resources += delivery.cartloads;
        }
    }
    return resources;
}

static int resource_in_delivery_multipart(building *b, int resource_id)
{
    int resources = 0;

    while (b->prev_part_building_id) {
        b = building_get(b->prev_part_building_id);
    }

    while (b->id) {
        for (const monument_delivery &delivery : monument_deliveries) {
            if (delivery.destination_id == b->id &&
                delivery.resource == resource_id) {
                resources += delivery.cartloads;
            }
        }
        b = building_get(b->next_part_building_id);
    }

    return resources;
}

int building_monument_resource_in_delivery(building *b, int resource_id)
{
    if (b->next_part_building_id || b->prev_part_building_id) {
        return resource_in_delivery_multipart(b, resource_id);
    } else {
        return resource_in_delivery(b->id, resource_id);
    }
}

int building_monument_get_id(building_type type)
{
    building *b = building_first_of_type(type);
    if (!building_monument_type_is_monument(type) || !b) {
        return 0;
    }
    return b->id;
}

int building_monument_count_grand_temples(void)
{
    int count = 0;
    for (building_type type = BUILDING_NONE; type < BUILDING_TYPE_MAX; type = static_cast<building_type>(type + 1)) {
        if (!type_is_grand_temple(type)) {
            continue;
        }
        for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
            count++;
        }
    }
    return count;
}

int building_monument_has_labour_problems(building *b)
{
    const model_building *model = model_get_building(b->type);

    if (b->num_workers < model->laborers) {
        return 1;
    } else {
        return 0;
    }
}

int building_monument_working(building_type type)
{
    unsigned int monument_id = building_monument_get_id(type);
    building *b = building_get(monument_id);
    if (!monument_id) {
        return 0;
    }
    if (b->monument.phase != MONUMENT_FINISHED || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }

    if (building_monument_has_labour_problems(b)) {
        return 0;
    }

    return monument_id;
}

int building_monument_working_grand_temple_for_god(god_type god)
{
    return grand_temple_id_for_god(god, 1);
}

int building_monument_requires_resource(building_type type, int resource)
{
    int phases = building_monument_phases(type);
    for (int phase = 1; phase < phases; phase++) {
        if (building_monument_resources_needed_for_monument_type(type, resource, phase) > 0) {
            return 1;
        }
    }
    return 0;
}

int building_monument_has_required_resources_to_build(building_type type)
{
    int phases = building_monument_phases(type);
    for (int phase = 1; phase < phases; phase++) {
        for (int resource_id = (RESOURCE_NONE + 1); resource_id < RESOURCE_SLOT_COUNT; resource_id++) {
            resource_type r = static_cast<resource_type>(resource_id);
            if (building_monument_resources_needed_for_monument_type(type, r, phase) > 0 &&
                !empire_can_produce_resource_potentially(r) && !empire_can_import_resource_potentially(r)) {
                return 0;
            }
        }
    }
    return 1;
}

int building_monument_upgraded(building_type type)
{
    unsigned int monument_id = building_monument_working(type);
    building *b = building_get(monument_id);
    if (!monument_id) {
        return 0;
    }
    if (!b->monument.upgrades) {
        return 0;
    }
    return monument_id;
}

int building_monument_module_type(building_type type)
{
    unsigned int monument_id = building_monument_working(type);

    if (!monument_id) {
        return 0;
    }

    building *b = building_get(monument_id);
    return b->monument.upgrades;
}

static building_type grand_temple_type_for_module_index(int module_index)
{
    if (module_index < 0) {
        return BUILDING_NONE;
    }

    for (const std::unique_ptr<building_type_registry_impl::BuildingType> &definition :
        building_type_registry_impl::g_building_types) {
        if (!definition || !definition->is_temple_tier(building_type_registry_impl::ReligionTier::Grand)) {
            continue;
        }

        const building_type_registry_impl::Religion *religion = definition->religion();
        if (!religion || religion->has_all_gods() || !religion->presentation().is_complete()) {
            continue;
        }
        if (religion->presentation().module_index() == module_index) {
            return definition->type();
        }
    }

    return BUILDING_NONE;
}

int building_monument_gt_module_is_active(int module)
{
    if (module < 0) {
        return 0;
    }
    int module_num = module % MODULES_PER_TEMPLE + 1;
    building_type temple_type = grand_temple_type_for_module_index(module / MODULES_PER_TEMPLE);
    if (temple_type == BUILDING_NONE) {
        return 0;
    }

    return building_monument_module_type(temple_type) == module_num;
}

int building_monument_pantheon_module_is_active(int module)
{
    return building_monument_module_type(building_type_registry_impl::type_from_attr("pantheon")) ==
        (module - (PANTHEON_MODULE_1_DESTINATION_PRIESTS - 1));
}

static void delivery_save(buffer *buf, monument_delivery *delivery)
{
    buffer_write_i32(buf, delivery->walker_id);
    buffer_write_i32(buf, delivery->destination_id);
    buffer_write_i32(buf, delivery->resource);
    buffer_write_i32(buf, delivery->cartloads);
}

static void delivery_load(buffer *buf, monument_delivery *delivery, int size)
{
    delivery->walker_id = buffer_read_i32(buf);
    delivery->destination_id = buffer_read_i32(buf);
    delivery->resource = resource_remap(buffer_read_i32(buf));
    delivery->cartloads = buffer_read_i32(buf);

    if (size > ORIGINAL_DELIVERY_BUFFER_SIZE) {
        buffer_skip(buf, size - ORIGINAL_DELIVERY_BUFFER_SIZE);
    }
}

void building_monument_delivery_save_state(buffer *buf)
{
    int buf_size = 4 + static_cast<int>(monument_deliveries.size()) * ORIGINAL_DELIVERY_BUFFER_SIZE;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, ORIGINAL_DELIVERY_BUFFER_SIZE);

    for (monument_delivery &delivery : monument_deliveries) {
        delivery_save(buf, &delivery);
    }
}

void building_monument_delivery_load_state(buffer *buf, int includes_delivery_buffer_size)
{
    int delivery_buf_size = ORIGINAL_DELIVERY_BUFFER_SIZE;
    size_t buf_size = buf->size;

    if (includes_delivery_buffer_size) {
        delivery_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
    }

    int deliveries_to_load = static_cast<int>(buf_size) / delivery_buf_size;

    monument_deliveries.clear();
    monument_deliveries.resize(deliveries_to_load);

    for (int i = 0; i < deliveries_to_load; i++) {
        delivery_load(buf, &monument_deliveries[i], delivery_buf_size);
    }
}

int building_monument_is_construction_halted(building *b)
{
    return building_main(b)->state == BUILDING_STATE_MOTHBALLED;
}

int building_monument_toggle_construction_halted(building *b)
{
    city_culture_remove_building_module_capacity(b);
    if (b->state == BUILDING_STATE_MOTHBALLED) {
        b->state = BUILDING_STATE_IN_USE;
        city_culture_add_building_module_capacity(b);
        return 0;
    } else {
        b->state = BUILDING_STATE_MOTHBALLED;
        city_culture_add_building_module_capacity(b);
        return 1;
    }
}

int building_monument_is_unfinished_monument(const building *b)
{
    if (!building_monument_is_monument(b)) {
        return 0;
    }
    return b->monument.phase != MONUMENT_FINISHED ? 1 : 0;
}
