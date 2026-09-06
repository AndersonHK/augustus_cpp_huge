#include "building/building_record.h"
#include "menu.h"

#include "building/building_type_registry_internal.h"
#include "building/industry.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/buildings.h"
#include "city/monument_gifts.h"
#include "core/config.h"
#include "empire/city.h"
#include "game/tutorial.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"

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

static std::array<std::vector<menu_entry>, BUILD_MENU_MAX> menu_entries;

static int menu_catalog_built = 0;
static int changed = 1;

struct submenu_expander_mapping {
    build_menu_group submenu;
    const char *text_id;
};

static const submenu_expander_mapping SUBMENU_EXPANDERS[] = {
    {BUILD_MENU_FARMS, "farms"},
    {BUILD_MENU_RAW_MATERIALS, "raw_materials"},
    {BUILD_MENU_WORKSHOPS, "workshops"},
    {BUILD_MENU_SMALL_TEMPLES, "small_temples"},
    {BUILD_MENU_LARGE_TEMPLES, "large_temples"},
    {BUILD_MENU_FORTS, "fort"},
    {BUILD_MENU_GRAND_TEMPLES, "grand_temples"},
    {BUILD_MENU_PARKS, "parks"},
    {BUILD_MENU_TREES, "trees"},
    {BUILD_MENU_PATHS, "paths"},
    {BUILD_MENU_GOV_RES, "governor_home"},
    {BUILD_MENU_STATUES, "statues"},
    {BUILD_MENU_SHRINES, "shrines"},
    {BUILD_MENU_GARDENS, "all_gardens"},
};

static building_type submenu_expander_type(int submenu)
{
    for (const submenu_expander_mapping &mapping : SUBMENU_EXPANDERS) {
        if (mapping.submenu == submenu) {
            return building_type_registry_impl::type_from_attr(mapping.text_id);
        }
    }
    return BUILDING_NONE;
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
        {"tools", BUILD_MENU_CLEAR_LAND},
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

static void rebuild_menu_catalog(void)
{
    for (std::vector<menu_entry> &entries : menu_entries) {
        entries.clear();
    }

    using namespace building_type_registry_impl;
    for (const std::unique_ptr<BuildingType> &definition : g_building_types) {
        if (!definition) {
            continue;
        }
        for (const BuildButtonDefinition &button : definition->buttons()) {
            if (!button.has_group()) {
                continue;
            }
            build_menu_group submenu = button_group_from_string(button.group());
            int order = button.has_order() ? button.order() : 10000;
            add_menu_entry(submenu, definition->type(), order);
        }
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
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(menu_building_type);
    if (definition && definition->has_housing()) {
        *enabled = 1;
    }
}

static void enable_clear(int *enabled, building_type menu_building_type)
{
    if (building_type_registry_impl::type_attr_is(menu_building_type, "clear_land")) {
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

static int can_get_required_resource(
    building_type type,
    const building_type_registry_impl::BuildingType *definition)
{
    if (definition->attr_is("shipyard")) {
        return empire_can_produce_resource_naturally(resource_fish());
    } else if (definition && definition->is_farm()) {
        const resource_type output_resource = building_output_resource(definition);
        return output_resource > RESOURCE_NONE && empire_can_produce_resource_naturally(output_resource);
    } else if (definition->attr_is("tavern")) {
        return empire_can_produce_resource_potentially(resource_wine()) ||
            empire_can_import_resource_potentially(resource_wine());
    } else if (definition->attr_is("lighthouse")) {
        return (empire_can_produce_resource_potentially(resource_timber()) ||
            empire_can_import_resource_potentially(resource_timber())) &&
            building_monument_has_required_resources_to_build(type);
    } else if (definition->attr_is("city_mint")) {
        building_type senate = building_type_registry_impl::type_from_attr("senate");
        return senate != BUILDING_NONE && is_building_type_allowed(senate) &&
            building_monument_has_required_resources_to_build(type);
    }
    return building_monument_has_required_resources_to_build(type);
}

static int is_building_type_allowed(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return scenario_allowed_building(definition) &&
        can_get_required_resource(type, definition);
}

static void enable_if_allowed(int *enabled, building_type menu_building_type, building_type type)
{
    if (type <= BUILDING_NONE) {
        return;
    }
    if (menu_building_type != type) {
        return;
    }
    if (building_type_registry_impl::type_attr_is(type, "small_temples") ||
        building_type_registry_impl::type_attr_is(type, "large_temples") ||
        building_type_registry_impl::type_attr_is(type, "fort")) {
        *enabled = 1;
        enable_cycling_temples_if_allowed(type);
    } else {
        *enabled = is_building_type_allowed(type);
    }
}

static void enable_if_allowed(int *enabled, building_type menu_building_type, const char *type_text_id)
{
    enable_if_allowed(enabled, menu_building_type, building_type_registry_impl::type_from_attr(type_text_id));
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
    if (scenario_is_tutorial_2() && resource == resource_sand()) {
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
    if (scenario_is_tutorial_2() && (resource != resource_pottery() && resource != resource_weapons())) {
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

    if (!city_monument_gift_available(type)) {
        *enabled = 0;
    }
}

static void enable_tutorial1_start(int *enabled, building_type type)
{
    enable_house(enabled, type);
    enable_clear(enabled, type);
    enable_if_allowed(enabled, type, "well");
    enable_if_allowed(enabled, type, "road");
    enable_if_allowed(enabled, type, "roadblock");
}

static void enable_tutorial1_after_fire(int *enabled, building_type type)
{
    enable_tutorial1_start(enabled, type);
    enable_if_allowed(enabled, type, "prefecture");
    enable_if_allowed(enabled, type, "market");
}

static void enable_tutorial1_after_collapse(int *enabled, building_type type)
{
    enable_tutorial1_after_fire(enabled, type);
    enable_if_allowed(enabled, type, "engineers_post");
    enable_if_allowed(enabled, type, "senate");
}

static void enable_tutorial1_after_senate(int *enabled, building_type type)
{
    enable_tutorial1_after_collapse(enabled, type);
    enable_if_allowed(enabled, type, "small_temples");
    enable_if_allowed(enabled, type, "small_temple_ceres");
    enable_if_allowed(enabled, type, "small_temple_neptune");
    enable_if_allowed(enabled, type, "small_temple_mercury");
    enable_if_allowed(enabled, type, "small_temple_mars");
    enable_if_allowed(enabled, type, "small_temple_venus");
}

static void enable_tutorial2_start(int *enabled, building_type type)
{
    enable_house(enabled, type);
    enable_clear(enabled, type);
    enable_if_allowed(enabled, type, "well");
    enable_if_allowed(enabled, type, "road");
    enable_if_allowed(enabled, type, "roadblock");
    enable_if_allowed(enabled, type, "prefecture");
    enable_if_allowed(enabled, type, "engineers_post");
    enable_if_allowed(enabled, type, "senate");
    enable_if_allowed(enabled, type, "market");
    enable_if_allowed(enabled, type, "granary");
    enable_if_allowed(enabled, type, "farms");
    enable_if_allowed(enabled, type, "wheat_farm");
    enable_if_allowed(enabled, type, "vegetable_farm");
    enable_if_allowed(enabled, type, "fruit_farm");
    enable_if_allowed(enabled, type, "pig_farm");
    enable_if_allowed(enabled, type, "small_temples");
    enable_if_allowed(enabled, type, "small_temple_ceres");
    enable_if_allowed(enabled, type, "small_temple_neptune");
    enable_if_allowed(enabled, type, "small_temple_mercury");
    enable_if_allowed(enabled, type, "small_temple_mars");
    enable_if_allowed(enabled, type, "small_temple_venus");
}

static void enable_tutorial2_up_to_250(int *enabled, building_type type)
{
    enable_tutorial2_start(enabled, type);
    enable_if_allowed(enabled, type, "reservoir");
    enable_if_allowed(enabled, type, "aqueduct");
    enable_if_allowed(enabled, type, "fountain");
}

static void enable_tutorial2_up_to_450(int *enabled, building_type type)
{
    enable_tutorial2_up_to_250(enabled, type);
    enable_if_allowed(enabled, type, "all_gardens");
    enable_if_allowed(enabled, type, "gardens");
    enable_if_allowed(enabled, type, "overgrown_gardens");
    enable_if_allowed(enabled, type, "actor_colony");
    enable_if_allowed(enabled, type, "theater");
    enable_if_allowed(enabled, type, "bathhouse");
    enable_if_allowed(enabled, type, "school");
}

static void enable_tutorial2_after_450(int *enabled, building_type type)
{
    enable_tutorial2_up_to_450(enabled, type);
    enable_if_allowed(enabled, type, "raw_materials");
    enable_submenu_entries_if_allowed(enabled, type, BUILD_MENU_RAW_MATERIALS);
    enable_if_allowed(enabled, type, "workshops");
    enable_if_allowed(enabled, type, "wine_workshop");
    enable_if_allowed(enabled, type, "oil_workshop");
    enable_if_allowed(enabled, type, "weapons_workshop");
    enable_if_allowed(enabled, type, "furniture_workshop");
    enable_if_allowed(enabled, type, "pottery_workshop");
    enable_if_allowed(enabled, type, "brickworks");
    enable_if_allowed(enabled, type, "concrete_maker");
    enable_if_allowed(enabled, type, "warehouse");
    enable_if_allowed(enabled, type, "forum");
    enable_if_allowed(enabled, type, "amphitheater");
    enable_if_allowed(enabled, type, "gladiator_school");
}

static void disable_resources(int *enabled, building_type type)
{
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(static_cast<int>(r) + 1)) {
        if (resource_is_food(r) || resource_is_raw_material(r)) {
            disable_raw(enabled, type, building_producer_for_resource(r), r);
        } else {
            disable_finished(enabled, type, building_producer_for_resource(r), r);
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
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("small_temples"));
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("large_temples"));
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("shrines"));
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("trees"));
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("paths"));
    enable_cycling_temples_if_allowed(building_type_registry_impl::type_from_attr("all_gardens"));
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

int building_menu_is_enabled(const building_type_registry_impl::BuildingType *type)
{
    if (!type) {
        return 0;
    }
    const building_type type_id = type->type();
    ensure_menu_catalog();
    for (int sub = 0; sub < BUILD_MENU_MAX; sub++) {
        for (const menu_entry &entry : menu_entries[sub]) {
            if (entry.type != type_id) {
                continue;
            }
            if (!entry.enabled) {
                return 0;
            }
            building_type expander = submenu_expander_type(sub);
            if (expander != BUILDING_NONE) {
                return building_menu_is_enabled(building_type_registry_impl::definition_for_type(expander));
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
    for (const submenu_expander_mapping &mapping : SUBMENU_EXPANDERS) {
        if (building_type_registry_impl::type_attr_is(type, mapping.text_id)) {
            return mapping.submenu;
        }
    }
    return 0;
}

