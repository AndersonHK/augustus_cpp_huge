#include "menu.h"

#include "building/building_type_api.h"
#include "building/building_type_legacy_migration.h"
#include "building/building_type_registry_internal.h"

extern "C" {
#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "core/config.h"
#include "empire/city.h"
#include "game/tutorial.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"
}

#include <algorithm>
#include <array>
#include <string_view>
#include <vector>

#define MAIN_MENU_NUM_ENTRIES 11

struct menu_entry {
    building_type type = BUILDING_NONE;
    int order = 0;
    int enabled = 0;
};

static const building_type LEGACY_MENU_BUILDING_TYPE[BUILD_MENU_MAX][30] = {
    {BUILDING_HOUSE_VACANT_LOT, BUILDING_NONE},
    {BUILDING_CLEAR_LAND, BUILDING_REPAIR_LAND, BUILDING_NONE},
    {BUILDING_ROAD, BUILDING_HIGHWAY, BUILDING_ROADBLOCK, BUILDING_NONE},
    {BUILDING_DRAGGABLE_RESERVOIR, BUILDING_AQUEDUCT, BUILDING_NONE},
    {BUILDING_LATRINES, BUILDING_NONE},
    {BUILDING_MENU_SMALL_TEMPLES, BUILDING_MENU_LARGE_TEMPLES, BUILDING_MENU_GRAND_TEMPLES, BUILDING_MENU_SHRINES, BUILDING_LARARIUM,
        BUILDING_ORACLE, BUILDING_SMALL_MAUSOLEUM, BUILDING_LARGE_MAUSOLEUM, BUILDING_NYMPHAEUM, BUILDING_NONE},
    {BUILDING_MISSION_POST, BUILDING_NONE},
    {BUILDING_COLOSSEUM, BUILDING_HIPPODROME, BUILDING_CHARIOT_MAKER, BUILDING_NONE},
    {BUILDING_MENU_GARDENS, BUILDING_MENU_TREES, BUILDING_MENU_PATHS, BUILDING_MENU_PARKS, BUILDING_MENU_STATUES,
        BUILDING_MENU_GOV_RES, BUILDING_PLAZA, BUILDING_CITY_MINT,
        BUILDING_TRIUMPHAL_ARCH, BUILDING_NONE},
    {BUILDING_LOW_BRIDGE, BUILDING_SHIP_BRIDGE, BUILDING_SHIPYARD, BUILDING_DOCK, BUILDING_WHARF, BUILDING_NONE},
    {BUILDING_PALISADE, BUILDING_WALL, BUILDING_TOWER, BUILDING_GATEHOUSE,
        BUILDING_MENU_FORT, BUILDING_BARRACKS, BUILDING_MESS_HALL, BUILDING_MILITARY_ACADEMY, BUILDING_NONE},
    {BUILDING_MENU_FARMS, BUILDING_MENU_RAW_MATERIALS, BUILDING_MENU_WORKSHOPS,
        BUILDING_GRANARY, BUILDING_WAREHOUSE, BUILDING_DEPOT, BUILDING_NONE},
    {BUILDING_NONE},
    {BUILDING_NONE},
    {BUILDING_CONCRETE_MAKER, BUILDING_NONE},
    {BUILDING_MENU_SMALL_TEMPLES, BUILDING_NONE},
    {BUILDING_MENU_LARGE_TEMPLES, BUILDING_NONE},
    {BUILDING_FORT_LEGIONARIES, BUILDING_FORT_JAVELIN, BUILDING_FORT_MOUNTED, BUILDING_FORT_AUXILIA_INFANTRY, BUILDING_FORT_ARCHERS, BUILDING_NONE},
    {BUILDING_COLONNADE, BUILDING_HEDGE_LIGHT, BUILDING_HEDGE_DARK, BUILDING_LOOPED_GARDEN_WALL, BUILDING_ROOFED_GARDEN_WALL,
        BUILDING_PANELLED_GARDEN_WALL, BUILDING_PAVILION_BLUE, BUILDING_SMALL_POND, BUILDING_LARGE_POND, BUILDING_NONE},
    {BUILDING_MENU_TREES, BUILDING_PINE_TREE, BUILDING_FIR_TREE, BUILDING_OAK_TREE, BUILDING_ELM_TREE, BUILDING_FIG_TREE, BUILDING_PLUM_TREE,
        BUILDING_PALM_TREE, BUILDING_DATE_TREE, BUILDING_NONE},
    {BUILDING_MENU_PATHS, BUILDING_GARDEN_PATH, BUILDING_PINE_PATH , BUILDING_FIR_PATH, BUILDING_OAK_PATH, BUILDING_ELM_PATH,
        BUILDING_FIG_PATH, BUILDING_PLUM_PATH, BUILDING_PALM_PATH, BUILDING_DATE_PATH, BUILDING_NONE},
    {BUILDING_NONE},
    {BUILDING_SMALL_STATUE, BUILDING_GODDESS_STATUE, BUILDING_SENATOR_STATUE, BUILDING_GLADIATOR_STATUE, BUILDING_DECORATIVE_COLUMN,
        BUILDING_MEDIUM_STATUE, BUILDING_LEGION_STATUE, BUILDING_OBELISK, BUILDING_HORSE_STATUE, BUILDING_NONE},
    {BUILDING_GOVERNORS_HOUSE, BUILDING_GOVERNORS_VILLA, BUILDING_GOVERNORS_PALACE, BUILDING_NONE},
    {BUILDING_MENU_SHRINES, BUILDING_SHRINE_CERES, BUILDING_SHRINE_NEPTUNE, BUILDING_SHRINE_MERCURY, BUILDING_SHRINE_MARS, BUILDING_SHRINE_VENUS, BUILDING_NONE},
    {BUILDING_MENU_GARDENS, BUILDING_GARDENS, BUILDING_OVERGROWN_GARDENS, BUILDING_NONE}
};

static std::array<std::vector<menu_entry>, BUILD_MENU_MAX> menu_entries;

static int menu_catalog_built = 0;
static int changed = 1;

static building_type submenu_expander_type(int submenu)
{
    switch (submenu) {
        case BUILD_MENU_FARMS:
            return BUILDING_MENU_FARMS;
        case BUILD_MENU_RAW_MATERIALS:
            return BUILDING_MENU_RAW_MATERIALS;
        case BUILD_MENU_WORKSHOPS:
            return BUILDING_MENU_WORKSHOPS;
        case BUILD_MENU_SMALL_TEMPLES:
            return BUILDING_MENU_SMALL_TEMPLES;
        case BUILD_MENU_LARGE_TEMPLES:
            return BUILDING_MENU_LARGE_TEMPLES;
        case BUILD_MENU_FORTS:
            return BUILDING_MENU_FORT;
        case BUILD_MENU_GRAND_TEMPLES:
            return BUILDING_MENU_GRAND_TEMPLES;
        case BUILD_MENU_PARKS:
            return BUILDING_MENU_PARKS;
        case BUILD_MENU_TREES:
            return BUILDING_MENU_TREES;
        case BUILD_MENU_PATHS:
            return BUILDING_MENU_PATHS;
        case BUILD_MENU_GOV_RES:
            return BUILDING_MENU_GOV_RES;
        case BUILD_MENU_STATUES:
            return BUILDING_MENU_STATUES;
        case BUILD_MENU_SHRINES:
            return BUILDING_MENU_SHRINES;
        case BUILD_MENU_GARDENS:
            return BUILDING_MENU_GARDENS;
        default:
            return BUILDING_NONE;
    }
}

static int is_valid_submenu(int submenu)
{
    return submenu > SUBMENU_NONE && submenu < BUILD_MENU_MAX;
}

static build_menu_group button_group_from_string(const char *group)
{
    if (!group || !*group) {
        return SUBMENU_NONE;
    }

    struct group_mapping {
        const char *name;
        build_menu_group group;
    };
    static const group_mapping groups[] = {
        {"vacant_house", BUILD_MENU_VACANT_HOUSE},
        {"clear_land", BUILD_MENU_CLEAR_LAND},
        {"road", BUILD_MENU_ROAD},
        {"water", BUILD_MENU_WATER},
        {"health", BUILD_MENU_HEALTH},
        {"temples", BUILD_MENU_TEMPLES},
        {"education", BUILD_MENU_EDUCATION},
        {"entertainment", BUILD_MENU_ENTERTAINMENT},
        {"administration", BUILD_MENU_ADMINISTRATION},
        {"engineering", BUILD_MENU_ENGINEERING},
        {"security", BUILD_MENU_SECURITY},
        {"industry", BUILD_MENU_INDUSTRY},
        {"farms", BUILD_MENU_FARMS},
        {"raw_materials", BUILD_MENU_RAW_MATERIALS},
        {"workshops", BUILD_MENU_WORKSHOPS},
        {"small_temples", BUILD_MENU_SMALL_TEMPLES},
        {"large_temples", BUILD_MENU_LARGE_TEMPLES},
        {"forts", BUILD_MENU_FORTS},
        {"parks", BUILD_MENU_PARKS},
        {"trees", BUILD_MENU_TREES},
        {"paths", BUILD_MENU_PATHS},
        {"grand_temples", BUILD_MENU_GRAND_TEMPLES},
        {"statues", BUILD_MENU_STATUES},
        {"gov_res", BUILD_MENU_GOV_RES},
        {"shrines", BUILD_MENU_SHRINES},
        {"gardens", BUILD_MENU_GARDENS}
    };

    std::string_view requested_group(group);
    for (const group_mapping &mapping : groups) {
        if (requested_group == mapping.name) {
            return mapping.group;
        }
    }
    return SUBMENU_NONE;
}

static void add_menu_entry(build_menu_group submenu, building_type type, int order)
{
    if (submenu <= SUBMENU_NONE || submenu >= BUILD_MENU_MAX || type <= BUILDING_NONE) {
        return;
    }
    menu_entries[submenu].push_back({type, order, 0});
}

static building_type menu_tool_type_for_definition(building_type type)
{
    if (type == BUILDING_RESERVOIR) {
        return BUILDING_DRAGGABLE_RESERVOIR;
    }
    return type;
}

static int legacy_menu_entry_is_xml_owned(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return 0;
    }

    const char *text_id = building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(type));
    return building_type_legacy_migration_text_id_is_xml_owned(text_id);
}

static void rebuild_menu_catalog(void)
{
    for (std::vector<menu_entry> &entries : menu_entries) {
        entries.clear();
    }

    for (int submenu = 0; submenu < BUILD_MENU_MAX; submenu++) {
        for (int item = 0; LEGACY_MENU_BUILDING_TYPE[submenu][item] != BUILDING_NONE; item++) {
            building_type legacy_type = LEGACY_MENU_BUILDING_TYPE[submenu][item];
            if (legacy_menu_entry_is_xml_owned(legacy_type)) {
                continue;
            }
            add_menu_entry(static_cast<build_menu_group>(submenu), legacy_type, item * 100);
        }
    }

    using namespace building_type_registry_impl;
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition || !definition->has_button() || !definition->button().has_group()) {
            continue;
        }
        build_menu_group submenu = button_group_from_string(definition->button().group());
        int order = definition->button().has_order() ? definition->button().order() : 10000;
        add_menu_entry(submenu, menu_tool_type_for_definition(definition->type()), order);
    }

    for (std::vector<menu_entry> &entries : menu_entries) {
        std::sort(entries.begin(), entries.end(), [](const menu_entry &left, const menu_entry &right) {
            if (left.order != right.order) {
                return left.order < right.order;
            }
            return left.type < right.type;
        });
    }
    menu_catalog_built = 1;
}

static void ensure_menu_catalog(void)
{
    if (!menu_catalog_built) {
        rebuild_menu_catalog();
    }
}

void building_menu_invalidate_catalog(void)
{
    menu_catalog_built = 0;
    changed = 1;
}

static building_type well_type(void)
{
    return BUILDING_WELL;
}

static building_type theater_type(void)
{
    return BUILDING_THEATER;
}

void building_menu_enable_all(void)
{
    rebuild_menu_catalog();
    for (std::vector<menu_entry> &entries : menu_entries) {
        for (menu_entry &entry : entries) {
            entry.enabled = 1;
        }
    }
}

static void enable_house(int *enabled, building_type menu_building_type)
{
    if (menu_building_type >= BUILDING_HOUSE_VACANT_LOT && menu_building_type <= BUILDING_HOUSE_LUXURY_PALACE) {
        *enabled = 1;
    }
}

static void enable_clear(int *enabled, building_type menu_building_type)
{
    if (menu_building_type == BUILDING_CLEAR_LAND) {
        *enabled = 1;
    }
}

static void enable_cycling_temples_if_allowed(building_type type)
{
    int sub = building_menu_get_submenu_for_type(type);
    if (!is_valid_submenu(sub)) {
        return;
    }
    for (menu_entry &entry : menu_entries[sub]) {
        if (entry.type == type) {
            entry.enabled = building_menu_count_items(sub) > 1;
            return;
        }
    }
}

static int is_building_type_allowed(building_type type);

static int can_get_required_resource(building_type type)
{
    switch (type) {
        case BUILDING_SHIPYARD:
            return empire_can_produce_resource_naturally(RESOURCE_FISH);
        case BUILDING_TAVERN:
            return empire_can_produce_resource_potentially(RESOURCE_WINE) ||
                empire_can_import_resource_potentially(RESOURCE_WINE);
        case BUILDING_LIGHTHOUSE:
            return (empire_can_produce_resource_potentially(RESOURCE_TIMBER) ||
                empire_can_import_resource_potentially(RESOURCE_TIMBER)) &&
                building_monument_has_required_resources_to_build(type);
        case BUILDING_CITY_MINT:
            return is_building_type_allowed(BUILDING_SENATE) &&
                building_monument_has_required_resources_to_build(type);
        default:
            return building_monument_has_required_resources_to_build(type);
    }
}

static int is_building_type_allowed(building_type type)
{
    return scenario_allowed_building(type) && can_get_required_resource(type);
}

static void enable_if_allowed(int *enabled, building_type menu_building_type, building_type type)
{
    if (type <= BUILDING_NONE) {
        return;
    }
    if (menu_building_type != type) {
        return;
    }
    if (type == BUILDING_MENU_SMALL_TEMPLES || type == BUILDING_MENU_LARGE_TEMPLES || type == BUILDING_MENU_FORT) {
        *enabled = 1;
        enable_cycling_temples_if_allowed(type);
    } else {
        *enabled = is_building_type_allowed(type);
    }
}

static void enable_submenu_entries_if_allowed(int *enabled, building_type type, int submenu)
{
    if (!is_valid_submenu(submenu)) {
        return;
    }
    building_type expander = submenu_expander_type(submenu);
    for (const menu_entry &entry : menu_entries[submenu]) {
        if (entry.type == expander) {
            continue;
        }
        enable_if_allowed(enabled, type, entry.type);
    }
}

static void disable_raw(int *enabled, building_type menu_building_type, building_type type, int resource)
{
    if (type != menu_building_type) {
        return;
    }
    if (!empire_can_produce_resource_naturally(resource)) {
        *enabled = 0;
    }
    if (scenario_is_tutorial_2() && resource == RESOURCE_SAND) {
        *enabled = 0;
    }
}

static void disable_finished(int *enabled, building_type menu_building_type, building_type type, int resource)
{
    if (type != menu_building_type) {
        return;
    }
    if (!empire_can_produce_resource_potentially(resource)) {
        *enabled = 0;
    }
    if (scenario_is_tutorial_2() && (resource != RESOURCE_POTTERY && resource != RESOURCE_WEAPONS)) {
        *enabled = 0;
    }
}

static void disable_if_no_enabled_submenu_items(int *enabled, int submenu)
{
    if (!is_valid_submenu(submenu)) {
        *enabled = 0;
        return;
    }
    building_type expander = submenu_expander_type(submenu);
    for (const menu_entry &entry : menu_entries[submenu]) {
        if (entry.type == expander) {
            continue;
        }
        if (is_building_type_allowed(entry.type)) {
            return;
        }
    }
    *enabled = 0;
}

static void enable_normal(int *enabled, building_type type)
{
    for (building_type current_type = BUILDING_NONE; current_type < BUILDING_TYPE_MAX;
        current_type = static_cast<building_type>(static_cast<int>(current_type) + 1)) {
        enable_if_allowed(enabled, type, current_type);
    }

    if (type == BUILDING_TRIUMPHAL_ARCH && !city_buildings_triumphal_arch_available()) {
        *enabled = 0;
    }
}

static void enable_tutorial1_start(int *enabled, building_type type)
{
    enable_house(enabled, type);
    enable_clear(enabled, type);
    enable_if_allowed(enabled, type, well_type());
    enable_if_allowed(enabled, type, BUILDING_ROAD);
    enable_if_allowed(enabled, type, BUILDING_ROADBLOCK);
}

static void enable_tutorial1_after_fire(int *enabled, building_type type)
{
    enable_tutorial1_start(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_PREFECTURE);
    enable_if_allowed(enabled, type, BUILDING_MARKET);
}

static void enable_tutorial1_after_collapse(int *enabled, building_type type)
{
    enable_tutorial1_after_fire(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_ENGINEERS_POST);
    enable_if_allowed(enabled, type, BUILDING_SENATE);
}

static void enable_tutorial1_after_senate(int *enabled, building_type type)
{
    enable_tutorial1_after_collapse(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_MENU_SMALL_TEMPLES);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_CERES);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_NEPTUNE);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_MERCURY);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_MARS);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_VENUS);
}

static void enable_tutorial2_start(int *enabled, building_type type)
{
    enable_house(enabled, type);
    enable_clear(enabled, type);
    enable_if_allowed(enabled, type, well_type());
    enable_if_allowed(enabled, type, BUILDING_ROAD);
    enable_if_allowed(enabled, type, BUILDING_ROADBLOCK);
    enable_if_allowed(enabled, type, BUILDING_PREFECTURE);
    enable_if_allowed(enabled, type, BUILDING_ENGINEERS_POST);
    enable_if_allowed(enabled, type, BUILDING_SENATE);
    enable_if_allowed(enabled, type, BUILDING_MARKET);
    enable_if_allowed(enabled, type, BUILDING_GRANARY);
    enable_if_allowed(enabled, type, BUILDING_MENU_FARMS);
    enable_if_allowed(enabled, type, BUILDING_WHEAT_FARM);
    enable_if_allowed(enabled, type, BUILDING_VEGETABLE_FARM);
    enable_if_allowed(enabled, type, BUILDING_FRUIT_FARM);
    enable_if_allowed(enabled, type, BUILDING_PIG_FARM);
    enable_if_allowed(enabled, type, BUILDING_MENU_SMALL_TEMPLES);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_CERES);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_NEPTUNE);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_MERCURY);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_MARS);
    enable_if_allowed(enabled, type, BUILDING_SMALL_TEMPLE_VENUS);
}

static void enable_tutorial2_up_to_250(int *enabled, building_type type)
{
    enable_tutorial2_start(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_DRAGGABLE_RESERVOIR);
    enable_if_allowed(enabled, type, BUILDING_AQUEDUCT);
    enable_if_allowed(enabled, type, BUILDING_FOUNTAIN);
}

static void enable_tutorial2_up_to_450(int *enabled, building_type type)
{
    enable_tutorial2_up_to_250(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_MENU_GARDENS);
    enable_if_allowed(enabled, type, BUILDING_GARDENS);
    enable_if_allowed(enabled, type, BUILDING_OVERGROWN_GARDENS);
    enable_if_allowed(enabled, type, BUILDING_ACTOR_COLONY);
    enable_if_allowed(enabled, type, theater_type());
    enable_if_allowed(enabled, type, BUILDING_BATHHOUSE);
    enable_if_allowed(enabled, type, BUILDING_SCHOOL);
}

static void enable_tutorial2_after_450(int *enabled, building_type type)
{
    enable_tutorial2_up_to_450(enabled, type);
    enable_if_allowed(enabled, type, BUILDING_MENU_RAW_MATERIALS);
    enable_submenu_entries_if_allowed(enabled, type, BUILD_MENU_RAW_MATERIALS);
    enable_if_allowed(enabled, type, BUILDING_MENU_WORKSHOPS);
    enable_if_allowed(enabled, type, BUILDING_WINE_WORKSHOP);
    enable_if_allowed(enabled, type, BUILDING_OIL_WORKSHOP);
    enable_if_allowed(enabled, type, BUILDING_WEAPONS_WORKSHOP);
    enable_if_allowed(enabled, type, BUILDING_FURNITURE_WORKSHOP);
    enable_if_allowed(enabled, type, BUILDING_POTTERY_WORKSHOP);
    enable_if_allowed(enabled, type, BUILDING_BRICKWORKS);
    enable_if_allowed(enabled, type, BUILDING_CONCRETE_MAKER);
    enable_if_allowed(enabled, type, BUILDING_WAREHOUSE);
    enable_if_allowed(enabled, type, BUILDING_FORUM);
    enable_if_allowed(enabled, type, BUILDING_AMPHITHEATER);
    enable_if_allowed(enabled, type, BUILDING_GLADIATOR_SCHOOL);
}

static void disable_resources(int *enabled, building_type type)
{
    for (resource_type r = RESOURCE_MIN; r < RESOURCE_MAX; r = static_cast<resource_type>(static_cast<int>(r) + 1)) {
        if (resource_is_food(r) || resource_is_raw_material(r)) {
            disable_raw(enabled, type, resource_get_data(r)->industry, r);
        } else {
            disable_finished(enabled, type, resource_get_data(r)->industry, r);
        }
    }
}

void building_menu_update(void)
{
    rebuild_menu_catalog();
    tutorial_build_buttons tutorial_buttons = tutorial_get_build_buttons();
    for (int sub = 0; sub < BUILD_MENU_MAX; sub++) {
        for (menu_entry &entry : menu_entries[sub]) {
            building_type type = entry.type;
            int *menu_item = &entry.enabled;
            // first 12 items, parks and grand temples always disabled at the start
            if (sub <= MAIN_MENU_NUM_ENTRIES || sub == 18 || sub == 21) {
                *menu_item = 0;
            } else {
                *menu_item = 1;
            }
            switch (tutorial_buttons) {
                case TUT1_BUILD_START:
                    enable_tutorial1_start(menu_item, type);
                    break;
                case TUT1_BUILD_AFTER_FIRE:
                    enable_tutorial1_after_fire(menu_item, type);
                    break;
                case TUT1_BUILD_AFTER_COLLAPSE:
                    enable_tutorial1_after_collapse(menu_item, type);
                    break;
                case TUT1_BUILD_AFTER_SENATE:
                    enable_tutorial1_after_senate(menu_item, type);
                    break;
                case TUT2_BUILD_START:
                    enable_tutorial2_start(menu_item, type);
                    break;
                case TUT2_BUILD_UP_TO_250:
                    enable_tutorial2_up_to_250(menu_item, type);
                    break;
                case TUT2_BUILD_UP_TO_450:
                    enable_tutorial2_up_to_450(menu_item, type);
                    break;
                case TUT2_BUILD_AFTER_450:
                    enable_tutorial2_after_450(menu_item, type);
                    break;
                default:
                    enable_normal(menu_item, type);
                    break;
            }
            disable_resources(menu_item, type);

            if (*menu_item) {
                int submenu = building_menu_get_submenu_for_type(type);
                if (submenu) {
                    disable_if_no_enabled_submenu_items(menu_item, submenu);
                }
            }
        }
    }
    enable_cycling_temples_if_allowed(BUILDING_MENU_SMALL_TEMPLES);
    enable_cycling_temples_if_allowed(BUILDING_MENU_LARGE_TEMPLES);
    enable_cycling_temples_if_allowed(BUILDING_MENU_SHRINES);
    enable_cycling_temples_if_allowed(BUILDING_MENU_TREES);
    enable_cycling_temples_if_allowed(BUILDING_MENU_PATHS);
    enable_cycling_temples_if_allowed(BUILDING_MENU_GARDENS);
    changed = 1;
}

int building_menu_count_items(int submenu)
{
    ensure_menu_catalog();
    if (!is_valid_submenu(submenu)) {
        return 0;
    }
    int count = 0;
    for (const menu_entry &entry : menu_entries[submenu]) {
        if (entry.enabled && entry.type > BUILDING_NONE) {
            count++;
        }
    }
    return count;
}

int building_menu_count_all_items(int submenu)
{
    ensure_menu_catalog();
    if (!is_valid_submenu(submenu)) {
        return 0;
    }
    return static_cast<int>(menu_entries[submenu].size());
}

int building_menu_next_index(int submenu, int current_index)
{
    ensure_menu_catalog();
    if (!is_valid_submenu(submenu)) {
        return 0;
    }
    for (int i = current_index + 1; i < static_cast<int>(menu_entries[submenu].size()); i++) {
        if (menu_entries[submenu][i].enabled) {
            return i;
        }
    }
    return 0;
}

building_type building_menu_type(int submenu, int item)
{
    ensure_menu_catalog();
    if (!is_valid_submenu(submenu) || item < 0 || item >= static_cast<int>(menu_entries[submenu].size())) {
        return BUILDING_NONE;
    }
    return menu_entries[submenu][item].type;
}

build_menu_group building_menu_for_type(building_type type)
{
    ensure_menu_catalog();
    for (int sub = 0; sub < BUILD_MENU_MAX; sub++) {
        for (const menu_entry &entry : menu_entries[sub]) {
            if (entry.type == type) {
                return static_cast<build_menu_group>(sub);
            }
        }
    }
    return SUBMENU_NONE;
}

int building_menu_is_enabled(building_type type)
{
    ensure_menu_catalog();
    for (int sub = 0; sub < BUILD_MENU_MAX; sub++) {
        for (const menu_entry &entry : menu_entries[sub]) {
            if (entry.type != type) {
                continue;
            }
            if (!entry.enabled) {
                return 0;
            }
            building_type expander = submenu_expander_type(sub);
            if (expander != BUILDING_NONE) {
                return building_menu_is_enabled(expander);
            } else {
                return 1;
            }
        }
    }
    return 0;
}

int building_menu_has_changed(void)
{
    if (changed) {
        changed = 0;
        return 1;
    }
    return 0;
}

int building_menu_is_submenu(build_menu_group menu)
{
    return is_valid_submenu(menu) && submenu_expander_type(menu) != BUILDING_NONE;
}

int building_menu_get_submenu_for_type(building_type type)
{
    switch (type) {
        case BUILDING_MENU_FARMS:
            return BUILD_MENU_FARMS;
        case BUILDING_MENU_RAW_MATERIALS:
            return BUILD_MENU_RAW_MATERIALS;
        case BUILDING_MENU_WORKSHOPS:
            return BUILD_MENU_WORKSHOPS;
        case BUILDING_MENU_SMALL_TEMPLES:
            return BUILD_MENU_SMALL_TEMPLES;
        case BUILDING_MENU_LARGE_TEMPLES:
            return BUILD_MENU_LARGE_TEMPLES;
        case BUILDING_MENU_FORT:
            return BUILD_MENU_FORTS;
        case BUILDING_MENU_GRAND_TEMPLES:
            return BUILD_MENU_GRAND_TEMPLES;
        case BUILDING_MENU_PARKS:
            return BUILD_MENU_PARKS;
        case BUILDING_MENU_TREES:
            return BUILD_MENU_TREES;
        case BUILDING_MENU_PATHS:
            return BUILD_MENU_PATHS;
        case BUILDING_MENU_GOV_RES:
            return BUILD_MENU_GOV_RES;
        case BUILDING_MENU_STATUES:
            return BUILD_MENU_STATUES;
        case BUILDING_MENU_SHRINES:
            return BUILD_MENU_SHRINES;
        case BUILDING_MENU_GARDENS:
            return BUILD_MENU_GARDENS;
        default:
            return 0;
    }
}

