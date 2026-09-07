#include <array>

#include "game/resource_id_bridge.h"

#include "core/log.h"
#include "scenario/allowed_building.h"
#include "scenario/map.h"

#include "building/building_type_registry_internal.h"

#include <cstdint>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace {

constexpr int SAVE_TABLE_VERSION = 1;

enum LegacyResourceVersion {
    RESOURCE_ORIGINAL_VERSION = 0,
    RESOURCE_DYNAMIC_VERSION = 1,
    RESOURCE_REORDERED_VERSION = 2,
    RESOURCE_SEPARATE_FISH_AND_MEAT_VERSION = 3,
    RESOURCE_HAS_GOLD_VERSION = 4,
    RESOURCE_HAS_NEW_MONUMENT_ELEMENTS = 5,
    RESOURCE_CURRENT_VERSION = RESOURCE_HAS_NEW_MONUMENT_ELEMENTS
};

enum LegacyResourceId {
    RESOURCE_WHEAT = 1,
    RESOURCE_VEGETABLES,
    RESOURCE_FRUIT,
    RESOURCE_MEAT,
    RESOURCE_FISH,
    RESOURCE_CLAY,
    RESOURCE_TIMBER,
    RESOURCE_OLIVES,
    RESOURCE_VINES,
    RESOURCE_IRON,
    RESOURCE_MARBLE,
    RESOURCE_GOLD,
    RESOURCE_SAND,
    RESOURCE_STONE,
    RESOURCE_POTTERY,
    RESOURCE_FURNITURE,
    RESOURCE_OIL,
    RESOURCE_WINE,
    RESOURCE_WEAPONS,
    RESOURCE_CONCRETE,
    RESOURCE_BRICKS,
    RESOURCE_DENARII,
    RESOURCE_TROOPS
};

constexpr int LEGACY_INVENTORY_MAX = 8;
constexpr int kLegacyMaxFood = 7;
constexpr int kReorderedMaxFood = 5;
constexpr int kSeparateFishMaxFood = 6;
constexpr int kLegacyMaxResources = 16;
constexpr int kWithFishMaxResources = 17;
constexpr int kWithGoldMaxResources = 18;
constexpr int kWithMonumentResourcesMaxResources = 22;

using LegacyResourceNames = std::array<const char *, RESOURCE_SLOT_COUNT>;

struct LegacyMappingDefinition {
    const LegacyResourceNames *resources = nullptr;
    const char *const *inventory = nullptr;
    int total_resources = 0;
    int total_food_resources = 0;
    bool joined_meat_and_fish = false;
};

struct BridgeState {
    resource_version_t version = RESOURCE_CURRENT_VERSION;
    std::unordered_map<std::string, resource_type> text_to_runtime;
    std::vector<resource_type> save_to_runtime;
    std::vector<std::string> save_to_text;
    std::vector<uint8_t> save_id_has_mapping;
    const char *const *legacy_inventory = nullptr;
    int total_resources = 0;
    int total_food_resources = 0;
    bool joined_meat_and_fish = false;
    bool runtime_ready = false;
    bool save_table_ready = false;
};

const LegacyResourceNames kOriginalResourceNames = {
    "none", "wheat", "vegetables", "fruit", "olives", "vines",
    "meat", "wine", "oil", "iron", "timber", "clay", "marble",
    "weapons", "furniture", "pottery", "denarii", "troops"
};

const LegacyResourceNames kReorderedResourceNames = {
    "none", "wheat", "vegetables", "fruit", "meat",
    "clay", "timber", "olives", "vines", "iron", "marble",
    "pottery", "furniture", "oil", "wine", "weapons",
    "denarii", "troops"
};

const LegacyResourceNames kFishResourceNames = {
    "none", "wheat", "vegetables", "fruit", "meat", "fish",
    "clay", "timber", "olives", "vines", "iron", "marble",
    "pottery", "furniture", "oil", "wine", "weapons",
    "denarii", "troops"
};

const LegacyResourceNames kGoldResourceNames = {
    "none", "wheat", "vegetables", "fruit", "meat", "fish",
    "clay", "timber", "olives", "vines", "iron", "marble", "gold",
    "pottery", "furniture", "oil", "wine", "weapons",
    "denarii", "troops"
};

const LegacyResourceNames kMonumentResourceNames = {
    "none", "wheat", "vegetables", "fruit", "meat", "fish",
    "clay", "timber", "olives", "vines", "iron", "marble", "gold", "sand", "stone",
    "pottery", "furniture", "oil", "wine", "weapons", "concrete", "bricks",
    "denarii", "troops"
};

const char *const kLegacyInventoryResourceNames[LEGACY_INVENTORY_MAX] = {
    "wheat", "vegetables", "fruit", "meat",
    "wine", "oil", "furniture", "pottery"
};

BridgeState g_bridge;

void ensure_runtime_table();

void register_text_id(resource_type runtime_id, const char *text_id)
{
    if (runtime_id < RESOURCE_NONE || runtime_id >= RESOURCE_SLOT_COUNT || !text_id || !*text_id) {
        return;
    }
    g_bridge.text_to_runtime[text_id] = runtime_id;
}

void clear_save_table()
{
    g_bridge.save_to_runtime.clear();
    g_bridge.save_to_text.clear();
    g_bridge.save_id_has_mapping.clear();
    g_bridge.save_to_runtime.push_back(RESOURCE_NONE);
    g_bridge.save_to_text.push_back("");
    g_bridge.save_id_has_mapping.push_back(1);
    g_bridge.legacy_inventory = nullptr;
    g_bridge.total_resources = 1;
    g_bridge.total_food_resources = 1;
    g_bridge.joined_meat_and_fish = false;
}

bool fishing_industry_is_available()
{
    if (!scenario_map_has_fishing_points()) {
        return false;
    }
    building_type wharf = building_type_registry_impl::type_from_attr("wharf");
    return wharf != BUILDING_NONE &&
        scenario_allowed_building(building_type_registry_impl::definition_for_type(wharf));
}

resource_type runtime_from_text_id(const char *text_id)
{
    ensure_runtime_table();
    if (!text_id || !*text_id) {
        return RESOURCE_NONE;
    }

    if (g_bridge.joined_meat_and_fish && std::string_view(text_id) == "meat" &&
        fishing_industry_is_available()) {
        text_id = "fish";
    }

    const auto found = g_bridge.text_to_runtime.find(text_id);
    return found == g_bridge.text_to_runtime.end() ? RESOURCE_NONE : found->second;
}

resource_type runtime_from_raw_runtime_slot(int id)
{
    resource_type runtime_id = static_cast<resource_type>(id);
    if (!resource_is_declared(runtime_id)) {
        return RESOURCE_NONE;
    }
    if (g_bridge.joined_meat_and_fish) {
        const char *text_id = resource_text_id(runtime_id);
        if (text_id && std::string_view(text_id) == "meat" && fishing_industry_is_available()) {
            return runtime_from_text_id("fish");
        }
    }
    return runtime_id;
}

void append_save_id_mapping(uint16_t save_id, resource_type runtime_id, const char *text_id)
{
    const size_t index = static_cast<size_t>(save_id);
    if (g_bridge.save_to_runtime.size() <= index) {
        g_bridge.save_to_runtime.resize(index + 1, RESOURCE_NONE);
        g_bridge.save_to_text.resize(index + 1);
        g_bridge.save_id_has_mapping.resize(index + 1, 0);
    }
    g_bridge.save_to_runtime[index] = runtime_id;
    g_bridge.save_to_text[index] = text_id ? text_id : "";
    g_bridge.save_id_has_mapping[index] = 1;
}

bool save_id_has_mapping(uint16_t save_id)
{
    return static_cast<size_t>(save_id) < g_bridge.save_id_has_mapping.size() &&
        g_bridge.save_id_has_mapping[static_cast<size_t>(save_id)];
}

void update_totals_from_mapping()
{
    int max_resource_save_id = 0;
    int max_food_save_id = 0;
    for (size_t save_id = 0; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        if (!g_bridge.save_id_has_mapping[save_id]) {
            continue;
        }
        resource_type runtime_id = g_bridge.save_to_runtime[save_id];
        if (runtime_id == RESOURCE_NONE || !resource_is_special(runtime_id)) {
            max_resource_save_id = static_cast<int>(save_id);
        }
        if (runtime_id == RESOURCE_NONE || resource_is_food(runtime_id)) {
            max_food_save_id = static_cast<int>(save_id);
        }
    }
    g_bridge.total_resources = max_resource_save_id + 1;
    g_bridge.total_food_resources = max_food_save_id + 1;
}

void rebind_save_table_to_runtime()
{
    ensure_runtime_table();
    for (size_t save_id = 0; save_id < g_bridge.save_to_text.size(); ++save_id) {
        if (!g_bridge.save_id_has_mapping[save_id]) {
            continue;
        }
        const std::string &text_id = g_bridge.save_to_text[save_id];
        resource_type runtime_id = runtime_from_text_id(text_id.c_str());
        g_bridge.save_to_runtime[save_id] = runtime_id;
    }
}

const LegacyMappingDefinition legacy_mapping_for_version(resource_version_t version)
{
    switch (version) {
        case RESOURCE_ORIGINAL_VERSION:
            return { &kOriginalResourceNames, kLegacyInventoryResourceNames, kLegacyMaxResources, kLegacyMaxFood, true };
        case RESOURCE_DYNAMIC_VERSION:
            return { &kOriginalResourceNames, nullptr, kLegacyMaxResources, kLegacyMaxFood, true };
        case RESOURCE_REORDERED_VERSION:
            return { &kReorderedResourceNames, nullptr, kLegacyMaxResources, kReorderedMaxFood, true };
        case RESOURCE_SEPARATE_FISH_AND_MEAT_VERSION:
            return { &kFishResourceNames, nullptr, kWithFishMaxResources, kSeparateFishMaxFood, false };
        case RESOURCE_HAS_GOLD_VERSION:
            return { &kGoldResourceNames, nullptr, kWithGoldMaxResources, kSeparateFishMaxFood, false };
        case RESOURCE_HAS_NEW_MONUMENT_ELEMENTS:
        default:
            return { &kMonumentResourceNames, nullptr, kWithMonumentResourcesMaxResources, kSeparateFishMaxFood, false };
    }
}

void load_legacy_save_table(resource_version_t version)
{
    ensure_runtime_table();
    clear_save_table();

    const LegacyMappingDefinition mapping = legacy_mapping_for_version(version);
    g_bridge.legacy_inventory = mapping.inventory;
    g_bridge.total_resources = mapping.total_resources;
    g_bridge.total_food_resources = mapping.total_food_resources;
    g_bridge.joined_meat_and_fish = mapping.joined_meat_and_fish;

    for (uint16_t save_id = 0; save_id < RESOURCE_SLOT_COUNT && save_id < mapping.total_resources; ++save_id) {
        const char *text_id = mapping.resources ? (*mapping.resources)[save_id] : nullptr;
        append_save_id_mapping(save_id, runtime_from_text_id(text_id), text_id);
    }

    g_bridge.save_table_ready = true;
}

void load_current_runtime_save_table()
{
    ensure_runtime_table();
    clear_save_table();

    for (int i = 0; i < resource_loaded_count(); ++i) {
        resource_type runtime_id = resource_get_loaded(i);
        if (runtime_id < RESOURCE_NONE || runtime_id >= RESOURCE_SLOT_COUNT) {
            continue;
        }
        if (runtime_id != RESOURCE_NONE && resource_is_special(runtime_id)) {
            continue;
        }
        append_save_id_mapping(static_cast<uint16_t>(runtime_id), runtime_id, resource_text_id(runtime_id));
    }
    update_totals_from_mapping();
    g_bridge.save_table_ready = true;
}

void ensure_runtime_table()
{
    if (g_bridge.runtime_ready) {
        return;
    }

    g_bridge.text_to_runtime.clear();
    for (int i = 0; i < resource_loaded_count(); ++i) {
        resource_type runtime_id = resource_get_loaded(i);
        register_text_id(runtime_id, resource_text_id(runtime_id));
    }

    g_bridge.runtime_ready = true;
}

void ensure_save_table()
{
    if (!g_bridge.save_table_ready) {
        load_current_runtime_save_table();
    }
}

} // namespace

void resource_id_bridge_reset_for_runtime(void)
{
    const bool had_save_table = g_bridge.save_table_ready;
    g_bridge.runtime_ready = false;
    ensure_runtime_table();
    if (had_save_table) {
        rebind_save_table_to_runtime();
    }
}

void resource_id_bridge_clear_save_table(void)
{
    clear_save_table();
    g_bridge.save_table_ready = false;
}

void resource_id_bridge_save_table_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    load_current_runtime_save_table();

    size_t entry_count = 0;
    size_t payload_size = sizeof(uint32_t) * 2;
    for (size_t save_id = 0; save_id < g_bridge.save_to_text.size(); ++save_id) {
        if (!g_bridge.save_id_has_mapping[save_id] || g_bridge.save_to_text[save_id].empty()) {
            continue;
        }
        payload_size += sizeof(uint16_t) * 2 + g_bridge.save_to_text[save_id].size();
        entry_count++;
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_TABLE_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(entry_count));

    for (size_t save_id = 0; save_id < g_bridge.save_to_text.size(); ++save_id) {
        if (!g_bridge.save_id_has_mapping[save_id] || g_bridge.save_to_text[save_id].empty()) {
            continue;
        }
        const std::string &text_id = g_bridge.save_to_text[save_id];
        if (text_id.size() > std::numeric_limits<uint16_t>::max() ||
            save_id > std::numeric_limits<uint16_t>::max()) {
            log_error("Resource text id too long for save table", text_id.c_str(), static_cast<int>(text_id.size()));
            continue;
        }
        buffer_write_u16(buf, static_cast<uint16_t>(save_id));
        buffer_write_u16(buf, static_cast<uint16_t>(text_id.size()));
        buffer_write_raw(buf, text_id.c_str(), text_id.size());
    }
}

void resource_id_bridge_save_table_load_state(buffer *buf, int has_save_table)
{
    if (!has_save_table || !buf || !buf->size) {
        load_legacy_save_table(g_bridge.version);
        return;
    }

    ensure_runtime_table();
    clear_save_table();

    buffer table = *buf;
    if (buffer_load_dynamic(&table) < sizeof(uint32_t) * 2) {
        log_error("Resource save table is invalid; falling back to legacy ids", 0, 0);
        load_legacy_save_table(g_bridge.version);
        return;
    }

    uint32_t version = buffer_read_u32(&table);
    uint32_t count = buffer_read_u32(&table);
    if (version != SAVE_TABLE_VERSION) {
        log_error("Unsupported resource save table version", 0, static_cast<int>(version));
        load_legacy_save_table(g_bridge.version);
        return;
    }

    for (uint32_t i = 0; i < count && !buffer_at_end(&table); ++i) {
        uint16_t save_id = buffer_read_u16(&table);
        uint16_t text_length = buffer_read_u16(&table);
        std::string text_id(text_length, '\0');
        if (text_length) {
            buffer_read_raw(&table, &text_id[0], text_length);
        }

        resource_type runtime_id = runtime_from_text_id(text_id.c_str());
        if (runtime_id == RESOURCE_NONE && text_id != "none") {
            log_warning("Mapping unavailable imported resource to none; its quantities cannot be retained in this mod stack", text_id.c_str(), save_id);
        }
        append_save_id_mapping(save_id, runtime_id, text_id.c_str());
    }

    update_totals_from_mapping();
    g_bridge.save_table_ready = true;
}

resource_version_t resource_id_bridge_original_version(void)
{
    return RESOURCE_ORIGINAL_VERSION;
}

resource_version_t resource_id_bridge_current_version(void)
{
    return RESOURCE_CURRENT_VERSION;
}

int resource_id_bridge_legacy_resource_count(void)
{
    return kLegacyMaxResources;
}

int resource_id_bridge_legacy_food_count(void)
{
    return kLegacyMaxFood;
}

int resource_id_bridge_legacy_inventory_count(void)
{
    return LEGACY_INVENTORY_MAX;
}

int resource_id_bridge_mapping_joins_meat_and_fish(void)
{
    return g_bridge.version < RESOURCE_SEPARATE_FISH_AND_MEAT_VERSION;
}

static resource_type runtime_from_save_id(uint16_t save_id)
{
    ensure_save_table();
    if (save_id_has_mapping(save_id)) {
        return g_bridge.save_to_runtime[static_cast<size_t>(save_id)];
    }
    return RESOURCE_NONE;
}

void resource_set_mapping(resource_version_t version)
{
    g_bridge.version = version;
    g_bridge.save_table_ready = false;
    load_legacy_save_table(version);
}

resource_version_t resource_mapping_get_version(void)
{
    return g_bridge.version;
}

resource_type resource_map_legacy_inventory(int id)
{
    if (id < 0 || id >= LEGACY_INVENTORY_MAX) {
        return RESOURCE_NONE;
    }

    ensure_save_table();
    if (g_bridge.legacy_inventory) {
        return runtime_from_text_id(g_bridge.legacy_inventory[id]);
    }
    return runtime_from_raw_runtime_slot(id);
}

resource_type resource_remap(int id)
{
    if (id < 0) {
        return RESOURCE_NONE;
    }
    return runtime_from_save_id(static_cast<uint16_t>(id));
}

uint16_t resource_id_bridge_save_id_from_runtime(resource_type runtime_id)
{
    ensure_save_table();
    if (runtime_id == RESOURCE_NONE) {
        return 0;
    }
    for (size_t save_id = 0; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        if (g_bridge.save_id_has_mapping[save_id] && g_bridge.save_to_runtime[save_id] == runtime_id) {
            return static_cast<uint16_t>(save_id);
        }
    }

    log_error("Resource runtime id has no save mapping", resource_text_id(runtime_id), runtime_id);
    return 0;
}

int resource_total_mapped(void)
{
    ensure_save_table();
    return g_bridge.total_resources;
}

int resource_total_food_mapped(void)
{
    ensure_save_table();
    return g_bridge.total_food_resources;
}

std::vector<resource_type> resource_id_bridge_production_order()
{
    ensure_save_table();
    std::vector<resource_type> resources;
    for (size_t id = 0; id < g_bridge.save_to_runtime.size(); ++id) {
        if (!g_bridge.save_id_has_mapping[id]) continue;
        const auto resource = g_bridge.save_to_runtime[id];
        if (resource == RESOURCE_NONE || (resource_get_data(resource)->flags & RESOURCE_FLAG_SPECIAL) == RESOURCE_FLAG_SPECIAL) continue;
        resources.push_back(resource);
    }
    return resources;
}
