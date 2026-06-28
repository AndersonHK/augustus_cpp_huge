#include "allowed_building.h"

#include "building/building_type_id_bridge.h"
#include "building/building_type_registry_internal.h"
#include "building/menu.h"
#include "building/monument.h"
#include "building/properties.h"
#include "core/log.h"
#include "scenario/data.h"

#include <string.h>

#define MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT (11 + 1) // 11 buildings + BUILDING_NONE
#define ALLOWED_BUILDINGS_KEYED_STATE_VERSION 1

static const char *const CONVERSION_FROM_ORIGINAL_TEXT[MAX_ORIGINAL_ALLOWED_BUILDINGS][MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT] = {
    { 0 },
    { "wheat_farm", "vegetable_farm", "fruit_farm", "olive_farm", "vines_farm", "pig_farm" },
    { "clay_pit", "marble_quarry", "iron_mine", "timber_yard", "gold_mine", "stone_quarry", "sand_pit" },
    { "wine_workshop", "oil_workshop", "weapons_workshop", "furniture_workshop", "pottery_workshop", "brickworks", "concrete_maker" },
    { "road" },
    { "wall", "palisade" },
    { "draggable_reservoir", "aqueduct", "fountain" },
    { 0 },
    { "amphitheater" },
    { 0 },
    { "hippodrome" },
    { "colosseum", "arena" },
    { "gladiator_school" },
    { "lion_house" },
    { "actor_colony" },
    { "chariot_maker" },
{ "gardens", "overgrown_gardens", "hedge_dark", "hedge_light", "pavilion", "colonnade", "looped_garden_wall", "roofed_garden_wall", "decorative_column" },
    { "plaza" },
    { "small_statue", "goddess_statue", "senator_statue", "gladiator_statue", "medium_statue", "legion_statue", "large_statue", "horse_statue", "obelisk", "small_pond", "large_pond" },
    { "doctor" },
    { "hospital" },
    { "bathhouse" },
    { "barber" },
    { "school" },
    { "academy" },
    { "library" },
    { "prefecture" },
    { "mess_hall", "fort_legionaries", "fort_javelin", "fort_mounted", "fort_swords", "fort_archers" },
    { "gatehouse", "palisade_gate" },
    { "tower", "watchtower" },
    { "small_temple_ceres", "small_temple_neptune", "small_temple_mercury", "small_temple_mars", "small_temple_venus" },
    { "large_temple_ceres", "large_temple_neptune", "large_temple_mercury", "large_temple_mars", "large_temple_venus", "grand_temple_ceres", "grand_temple_neptune", "grand_temple_mercury", "grand_temple_mars", "grand_temple_venus" },
    { "market" },
    { "granary" },
    { "warehouse" },
    { "triumphal_arch" },
    { "dock" },
    { "wharf", "shipyard" },
    { "governors_house", "governors_villa", "governors_palace" },
    { "engineers_post" },
    { "senate" },
    { "forum" },
    { 0 },
    { "oracle", "small_mausoleum", "large_mausoleum", "nymphaeum" },
    { "mission_post" },
    { "low_bridge", "ship_bridge" },
    { "barracks", "armoury" },
    { "military_academy" },
    { "grand_temple_ceres", "grand_temple_neptune", "grand_temple_mercury", "grand_temple_mars", "grand_temple_venus" },
};

static building_type conversion_from_original[MAX_ORIGINAL_ALLOWED_BUILDINGS][MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT];
static int conversion_from_original_initialized;

static uint8_t allowed_buildings[BUILDING_TYPE_MAX];

static void refresh_dynamic_original_allowed_slots(void)
{
    memset(conversion_from_original, 0, sizeof(conversion_from_original));
    for (unsigned int i = 0; i < MAX_ORIGINAL_ALLOWED_BUILDINGS; i++) {
        for (unsigned int j = 0; j < MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT - 1; j++) {
            const char *text_id = CONVERSION_FROM_ORIGINAL_TEXT[i][j];
            if (!text_id) {
                break;
            }
            conversion_from_original[i][j] = building_type_registry_impl::type_from_attr(text_id);
        }
    }

    conversion_from_original[9][0] = building_type_registry_impl::type_from_attr("theater");
    conversion_from_original[9][1] = BUILDING_NONE;
    conversion_from_original[42][0] = building_type_registry_impl::type_from_attr("well");
    conversion_from_original[42][1] = BUILDING_NONE;
    conversion_from_original[7][0] = building_type_registry_impl::vacant_lot_fill_type();
    conversion_from_original[7][1] = BUILDING_NONE;
    conversion_from_original_initialized = 1;
}

int scenario_allowed_building(const building_type_registry_impl::BuildingType *type)
{
    if (!type) {
        return 0;
    }
    const building_type type_id = type->type();
    if (type_id <= BUILDING_NONE || type_id >= BUILDING_TYPE_MAX) {
        return 0;
    }
    return allowed_buildings[type_id];
}

static int enable_submenu_buildings(building_type type, int allowed)
{
    //used for enabling entire submenu, not just one building
    int submenu = building_menu_get_submenu_for_type(type);
    if (!submenu) {
        return 0;
    }
    int all_items = building_menu_count_all_items(submenu);
    for (unsigned int i = 0; (int) i < all_items; i++) {
        building_type item = building_menu_type(submenu, i);
        if (item == type) {
            continue;
        }
        allowed_buildings[item] = allowed;
    }
    return 1;
}

void scenario_allowed_building_set(building_type type, int allowed)
{
    if (!enable_submenu_buildings(type, allowed)) {
        allowed_buildings[type] = allowed;
    }
}

void scenario_allowed_building_enable_all(void)
{
    for (unsigned int i = 0; i < BUILDING_TYPE_MAX; i++) {
        allowed_buildings[i] = 1;
    }
}

void scenario_allowed_building_disable_all(void)
{
    for (unsigned int i = 0; i < BUILDING_TYPE_MAX; i++) {
        allowed_buildings[i] = 0;
    }
}

const building_type *scenario_allowed_building_get_buildings_from_original_id(unsigned int original)
{
    if (!conversion_from_original_initialized) {
        refresh_dynamic_original_allowed_slots();
    }
    if (original >= MAX_ORIGINAL_ALLOWED_BUILDINGS) {
        return conversion_from_original[0];
    }
    return conversion_from_original[original];
}

void scenario_allowed_building_load_state(buffer *buf)
{
    scenario_allowed_building_enable_all();

    size_t buildings = buffer_load_dynamic_array(buf);

    for (size_t i = 0; i < buildings; i++) {
        int allowed = buffer_read_i8(buf);
        building_type type = building_type_id_bridge_runtime_from_save_id((uint16_t) i);
        if (type > BUILDING_NONE && type < BUILDING_TYPE_MAX) {
            allowed_buildings[type] = allowed;
        }
    }
}

void scenario_allowed_building_load_state_keyed(buffer *buf, int has_keyed_state)
{
    if (!has_keyed_state) {
        scenario_allowed_building_load_state(buf);
        return;
    }

    scenario_allowed_building_enable_all();
    if (!buf || !buf->size) {
        return;
    }

    buffer state = *buf;
    if (buffer_load_dynamic(&state) < sizeof(uint32_t) * 2) {
        log_error("Scenario allowed-building state is invalid; falling back to legacy enum migration", 0, 0);
        scenario_allowed_building_load_state(buf);
        return;
    }

    uint32_t version = buffer_read_u32(&state);
    uint32_t count = buffer_read_u32(&state);
    if (version != ALLOWED_BUILDINGS_KEYED_STATE_VERSION) {
        log_error("Unsupported scenario allowed-building state version; falling back to legacy enum migration", 0, version);
        scenario_allowed_building_load_state(buf);
        return;
    }

    for (uint32_t i = 0; i < count && !buffer_at_end(&state); i++) {
        uint16_t text_length = buffer_read_u16(&state);
        char text_id[256];
        if (text_length >= sizeof(text_id)) {
            log_error("Scenario allowed-building text id too long", 0, text_length);
            buffer_skip(&state, text_length);
            if (!buffer_at_end(&state)) {
                buffer_read_i8(&state);
            }
            continue;
        }
        if (text_length) {
            buffer_read_raw(&state, text_id, text_length);
        }
        text_id[text_length] = 0;
        int allowed = buffer_read_i8(&state);
        building_type type = building_type_id_bridge_runtime_from_text(text_id);
        if (type > BUILDING_NONE && type < BUILDING_TYPE_MAX) {
            allowed_buildings[type] = allowed;
        }
    }
}

void scenario_allowed_building_load_state_old_version(buffer *buf)
{
    scenario_allowed_building_enable_all();
    refresh_dynamic_original_allowed_slots();

    for (unsigned int i = 0; i < MAX_ORIGINAL_ALLOWED_BUILDINGS; i++) {
        short allowed = buffer_read_i16(buf);
        if (allowed) {
            continue;
        }
        for (unsigned int j = 0; j < MAX_BUILDINGS_PER_ORIGINAL_ALLOWED_SLOT; j++) {
            building_type type = conversion_from_original[i][j];
            if (type == BUILDING_NONE) {
                break;
            }
            allowed_buildings[type] = 0;
        }
    }
}

void scenario_allowed_building_save_state(buffer *buf)
{
    size_t count = 0;
    size_t payload_size = sizeof(uint32_t) * 2;
    for (unsigned int i = 1; i < BUILDING_TYPE_MAX; i++) {
        const char *text_id = building_type_id_bridge_text_from_runtime((building_type) i);
        if (!text_id || !*text_id) {
            continue;
        }
        size_t text_length = strlen(text_id);
        if (text_length > UINT16_MAX) {
            log_error("Scenario allowed-building text id too long", text_id, (int) text_length);
            continue;
        }
        payload_size += sizeof(uint16_t) + text_length + sizeof(int8_t);
        count++;
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, ALLOWED_BUILDINGS_KEYED_STATE_VERSION);
    buffer_write_u32(buf, (uint32_t) count);

    for (unsigned int i = 1; i < BUILDING_TYPE_MAX; i++) {
        const char *text_id = building_type_id_bridge_text_from_runtime((building_type) i);
        if (!text_id || !*text_id) {
            continue;
        }
        size_t text_length = strlen(text_id);
        if (text_length > UINT16_MAX) {
            continue;
        }
        buffer_write_u16(buf, (uint16_t) text_length);
        buffer_write_raw(buf, text_id, text_length);
        buffer_write_i8(buf, allowed_buildings[i]);
    }
}
