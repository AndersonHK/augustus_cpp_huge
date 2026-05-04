#include "building/building_type_id_bridge.h"

#include "building/building_type_legacy_migration.h"
#include "building/building_type_registry_internal.h"

extern "C" {
#include "core/log.h"
#include "game/mod_manager.h"
}

#include "core/crash_context.h"

#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

constexpr int SAVE_TABLE_TEXT_VERSION = 1;
constexpr int SAVE_TABLE_OBJECT_VERSION = 2;
constexpr int SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION = 3;

static void fatal_current_save_table(const char *message, const char *detail)
{
    ErrorContextScope scope("building_type_id_bridge.load_save_table", detail);
    error_context_report_fatal_error_dialog("Vespasian Save Data Error", message, detail);
}
constexpr int SAVE_TABLE_VERSION = SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION;

struct SavedConstructionRequirement {
    resource_type resource = RESOURCE_NONE;
    int amount = 0;
};

struct SavedConstructionPhase {
    int index = 0;
    std::string path;
    std::string image;
    std::vector<SavedConstructionRequirement> requirements;
};

struct SavedConstructionDefinition {
    bool has_phased_construction = false;
    int road_update_radius = 0;
    std::vector<SavedConstructionPhase> phases;
};

struct SaveTableEntry {
    std::string text_id;
    uint16_t legacy_enum_hint = 0;
    building_type runtime_id = BUILDING_NONE;
    bool missing = false;
    std::string default_graphics_path;
    std::string default_graphics_image;
    SavedConstructionDefinition construction;
};

struct BridgeState {
    std::unordered_map<std::string, uint16_t> text_to_runtime;
    std::array<std::string, BUILDING_TYPE_MAX> runtime_to_text;
    std::array<uint16_t, BUILDING_TYPE_MAX> runtime_to_save = {};
    std::vector<building_type> save_to_runtime;
    std::vector<uint8_t> save_id_missing;
    std::vector<SaveTableEntry> save_entries;
    bool runtime_ready = false;
    bool save_table_ready = false;
};

BridgeState g_bridge;
std::unique_ptr<BridgeState> g_preview_bridge_snapshot;

std::string trim_copy(const std::string &text)
{
    size_t first = 0;
    while (first < text.size() && std::isspace(static_cast<unsigned char>(text[first]))) {
        ++first;
    }
    size_t last = text.size();
    while (last > first && std::isspace(static_cast<unsigned char>(text[last - 1]))) {
        --last;
    }
    return text.substr(first, last - first);
}

void register_text_id(building_type runtime_id, const std::string &text_id)
{
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX || text_id.empty()) {
        return;
    }

    uint16_t runtime = static_cast<uint16_t>(runtime_id);
    if (g_bridge.runtime_to_text[runtime].empty()) {
        g_bridge.runtime_to_text[runtime] = text_id;
    }
    g_bridge.text_to_runtime[text_id] = runtime;
}

void register_attr_tokens(building_type runtime_id, const char *attr)
{
    if (!attr || !*attr) {
        return;
    }

    std::string list(attr);
    size_t start = 0;
    while (start <= list.size()) {
        size_t end = list.find('|', start);
        std::string token = trim_copy(list.substr(start, end == std::string::npos ? std::string::npos : end - start));
        register_text_id(runtime_id, token);
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
}

building_type runtime_from_registered_text(const char *text_id)
{
    if (!text_id || !*text_id) {
        return BUILDING_NONE;
    }

    auto found = g_bridge.text_to_runtime.find(text_id);
    if (found == g_bridge.text_to_runtime.end()) {
        return BUILDING_NONE;
    }
    return static_cast<building_type>(found->second);
}

bool parse_uint16_text(const std::string &text, uint16_t *value)
{
    if (text.empty() || !value) {
        return false;
    }

    unsigned int parsed = 0;
    for (char ch : text) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
        parsed = parsed * 10 + static_cast<unsigned int>(ch - '0');
        if (parsed > std::numeric_limits<uint16_t>::max()) {
            return false;
        }
    }

    *value = static_cast<uint16_t>(parsed);
    return true;
}

struct RequirementSpec {
    resource_type resource;
    int amount;
};

struct PhaseSpec {
    const char *path;
    const char *image;
    const RequirementSpec *requirements;
    size_t requirement_count;
};

struct LegacyConstructionSpec {
    building_type legacy_type;
    const char *text_id;
    const char *default_path;
    const char *default_image;
    int road_update_radius;
    const PhaseSpec *phases;
    size_t phase_count;
    bool skip_for_julius;
};

static const RequirementSpec GRAND_TEMPLE_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 20}
};
static const RequirementSpec GRAND_TEMPLE_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_CONCRETE, 20}
};
static const RequirementSpec GRAND_TEMPLE_PHASE_3[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_MARBLE, 16}
};
static const RequirementSpec GRAND_TEMPLE_PHASE_4[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_BRICKS, 28}, {RESOURCE_MARBLE, 12}
};
static const RequirementSpec GRAND_TEMPLE_PHASE_5[] = {
    {RESOURCE_NONE, 4}
};

static const RequirementSpec LARGE_TEMPLE_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 4}, {RESOURCE_CONCRETE, 4}
};
static const RequirementSpec LARGE_TEMPLE_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_MARBLE, 2}
};

static const RequirementSpec ORACLE_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_CONCRETE, 2}, {RESOURCE_MARBLE, 2}
};
static const RequirementSpec ORACLE_PHASE_2[] = {
    {RESOURCE_NONE, 1}
};

static const RequirementSpec LIGHTHOUSE_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 16}
};
static const RequirementSpec LIGHTHOUSE_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_STONE, 16}
};
static const RequirementSpec LIGHTHOUSE_PHASE_3[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_STONE, 12}
};
static const RequirementSpec LIGHTHOUSE_PHASE_4[] = {
    {RESOURCE_NONE, 4}, {RESOURCE_TIMBER, 8}, {RESOURCE_BRICKS, 20}, {RESOURCE_STONE, 12}
};

static const RequirementSpec CARAVANSERAI_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 12}
};
static const RequirementSpec CARAVANSERAI_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_BRICKS, 12}, {RESOURCE_STONE, 12}
};

static const RequirementSpec PANTHEON_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 20}
};
static const RequirementSpec PANTHEON_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_STONE, 20}
};
static const RequirementSpec PANTHEON_PHASE_3[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_CONCRETE, 40}
};
static const RequirementSpec PANTHEON_PHASE_4[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_MARBLE, 32}, {RESOURCE_GOLD, 16}
};
static const RequirementSpec PANTHEON_PHASE_5[] = {
    {RESOURCE_NONE, 4}
};

static const RequirementSpec COLOSSEUM_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 12}
};
static const RequirementSpec COLOSSEUM_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 8}, {RESOURCE_CONCRETE, 16}
};
static const RequirementSpec COLOSSEUM_PHASE_3[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 12}, {RESOURCE_STONE, 16}
};
static const RequirementSpec COLOSSEUM_PHASE_4[] = {
    {RESOURCE_NONE, 4}, {RESOURCE_TIMBER, 12}, {RESOURCE_MARBLE, 20}, {RESOURCE_BRICKS, 16}
};

static const RequirementSpec HIPPODROME_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 32}
};
static const RequirementSpec HIPPODROME_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_CONCRETE, 32}
};
static const RequirementSpec HIPPODROME_PHASE_3[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 16}, {RESOURCE_STONE, 32}
};
static const RequirementSpec HIPPODROME_PHASE_4[] = {
    {RESOURCE_NONE, 4}, {RESOURCE_TIMBER, 32}, {RESOURCE_MARBLE, 48}, {RESOURCE_BRICKS, 32}
};

static const RequirementSpec SMALL_MAUSOLEUM_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 2}, {RESOURCE_CONCRETE, 4}
};
static const RequirementSpec SMALL_MAUSOLEUM_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_MARBLE, 2}
};

static const RequirementSpec NYMPHAEUM_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 12}, {RESOURCE_CONCRETE, 4}
};
static const RequirementSpec NYMPHAEUM_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_MARBLE, 2}
};

static const RequirementSpec LARGE_MAUSOLEUM_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 8}, {RESOURCE_CONCRETE, 4}
};
static const RequirementSpec LARGE_MAUSOLEUM_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_MARBLE, 4}
};

static const RequirementSpec CITY_MINT_PHASE_1[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_STONE, 8}
};
static const RequirementSpec CITY_MINT_PHASE_2[] = {
    {RESOURCE_NONE, 1}, {RESOURCE_TIMBER, 4}, {RESOURCE_IRON, 4}, {RESOURCE_BRICKS, 4}
};

#define PHASE(path, image, requirements) {path, image, requirements, sizeof(requirements) / sizeof(requirements[0])}

static const PhaseSpec GRAND_TEMPLE_CERES_PHASES[] = {
    PHASE("Environment\\Group_206", "Ceres Complex Const 01", GRAND_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Ceres Complex Const 02", GRAND_TEMPLE_PHASE_2),
    PHASE("Environment\\Group_206", "Ceres Complex Const 03", GRAND_TEMPLE_PHASE_3),
    PHASE("Environment\\Group_206", "Ceres Complex Const 04", GRAND_TEMPLE_PHASE_4),
    PHASE("Environment\\Group_206", "Ceres Complex Const 05", GRAND_TEMPLE_PHASE_5)
};
static const PhaseSpec GRAND_TEMPLE_NEPTUNE_PHASES[] = {
    PHASE("Environment\\Group_206", "Neptune Complex Const 01", GRAND_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Neptune Complex Const 02", GRAND_TEMPLE_PHASE_2),
    PHASE("Environment\\Group_206", "Neptune Complex Const 03", GRAND_TEMPLE_PHASE_3),
    PHASE("Environment\\Group_206", "Neptune Complex Const 04", GRAND_TEMPLE_PHASE_4),
    PHASE("Environment\\Group_206", "Neptune Complex Const 05", GRAND_TEMPLE_PHASE_5)
};
static const PhaseSpec GRAND_TEMPLE_MERCURY_PHASES[] = {
    PHASE("Environment\\Group_206", "Mercury Complex Const 01", GRAND_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Mercury Complex Const 02", GRAND_TEMPLE_PHASE_2),
    PHASE("Environment\\Group_206", "Mercury Complex Const 03", GRAND_TEMPLE_PHASE_3),
    PHASE("Environment\\Group_206", "Mercury Complex Const 04", GRAND_TEMPLE_PHASE_4),
    PHASE("Environment\\Group_206", "Mercury Complex Const 05", GRAND_TEMPLE_PHASE_5)
};
static const PhaseSpec GRAND_TEMPLE_MARS_PHASES[] = {
    PHASE("Environment\\Group_206", "Mars Complex Const 01", GRAND_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Mars Complex Const 02", GRAND_TEMPLE_PHASE_2),
    PHASE("Environment\\Group_206", "Mars Complex Const 03", GRAND_TEMPLE_PHASE_3),
    PHASE("Environment\\Group_206", "Mars Complex Const 04", GRAND_TEMPLE_PHASE_4),
    PHASE("Environment\\Group_206", "Mars Complex Const 05", GRAND_TEMPLE_PHASE_5)
};
static const PhaseSpec GRAND_TEMPLE_VENUS_PHASES[] = {
    PHASE("Environment\\Group_206", "Venus Complex Const 01", GRAND_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Venus Complex Const 02", GRAND_TEMPLE_PHASE_2),
    PHASE("Environment\\Group_206", "Venus Complex Const 03", GRAND_TEMPLE_PHASE_3),
    PHASE("Environment\\Group_206", "Venus Complex Const 04", GRAND_TEMPLE_PHASE_4),
    PHASE("Environment\\Group_206", "Venus Complex Const 05", GRAND_TEMPLE_PHASE_5)
};

static const PhaseSpec LARGE_TEMPLE_CERES_PHASES[] = {
    PHASE("Environment\\Group_206", "Ceres_LT_0", LARGE_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Ceres_LT_50", LARGE_TEMPLE_PHASE_2)
};
static const PhaseSpec LARGE_TEMPLE_NEPTUNE_PHASES[] = {
    PHASE("Environment\\Group_206", "Neptune_LT_0", LARGE_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Neptune_LT_50", LARGE_TEMPLE_PHASE_2)
};
static const PhaseSpec LARGE_TEMPLE_MERCURY_PHASES[] = {
    PHASE("Environment\\Group_206", "Mercury_LT_0", LARGE_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Mercury_LT_50", LARGE_TEMPLE_PHASE_2)
};
static const PhaseSpec LARGE_TEMPLE_MARS_PHASES[] = {
    PHASE("Environment\\Group_206", "Mars_LT_0", LARGE_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Mars_LT_50", LARGE_TEMPLE_PHASE_2)
};
static const PhaseSpec LARGE_TEMPLE_VENUS_PHASES[] = {
    PHASE("Environment\\Group_206", "Venus_LT_0", LARGE_TEMPLE_PHASE_1),
    PHASE("Environment\\Group_206", "Venus_LT_50", LARGE_TEMPLE_PHASE_2)
};

static const PhaseSpec ORACLE_PHASES[] = {
    PHASE("Environment\\Group_206", "Oracle_Construction_01", ORACLE_PHASE_1),
    PHASE("Monuments\\Oracle_Construction_02", "Oracle_Construction_02", ORACLE_PHASE_2)
};
static const PhaseSpec LIGHTHOUSE_PHASES[] = {
    PHASE("Environment\\Group_206", "Lighthouse_Construction_01", LIGHTHOUSE_PHASE_1),
    PHASE("Monuments\\Lighthouse_Construction_02", "Lighthouse_Construction_02", LIGHTHOUSE_PHASE_2),
    PHASE("Monuments\\Lighthouse_Construction_02", "Lighthouse_Construction_03", LIGHTHOUSE_PHASE_3),
    PHASE("Monuments\\Lighthouse_Construction_02", "Lighthouse_Construction_04", LIGHTHOUSE_PHASE_4)
};
static const PhaseSpec CARAVANSERAI_PHASES[] = {
    PHASE("Environment\\Group_206", "Caravanserai_Construction_01", CARAVANSERAI_PHASE_1),
    PHASE("Monuments\\Caravanserai_Construction_02", "Caravanserai_Construction_02", CARAVANSERAI_PHASE_2)
};
static const PhaseSpec PANTHEON_PHASES[] = {
    PHASE("Environment\\Group_206", "Pantheon Const 01", PANTHEON_PHASE_1),
    PHASE("Environment\\Group_206", "Pantheon Const 02", PANTHEON_PHASE_2),
    PHASE("Environment\\Group_206", "Pantheon Const 03", PANTHEON_PHASE_3),
    PHASE("Environment\\Group_206", "Pantheon Const 04", PANTHEON_PHASE_4),
    PHASE("Environment\\Group_206", "Pantheon Const 05", PANTHEON_PHASE_5)
};
static const PhaseSpec COLOSSEUM_PHASES[] = {
    PHASE("Environment\\Group_206", "Colosseum_Construction_01", COLOSSEUM_PHASE_1),
    PHASE("Monuments\\Colosseum_Construction_02", "Colosseum_Construction_02", COLOSSEUM_PHASE_2),
    PHASE("Monuments\\Colosseum_Construction_03", "Colosseum_Construction_03", COLOSSEUM_PHASE_3),
    PHASE("Monuments\\Colosseum_Construction_04", "Colosseum_Construction_04", COLOSSEUM_PHASE_4)
};
static const PhaseSpec HIPPODROME_PHASES[] = {
    PHASE("Health_Culture\\Hippodrome_1", "Image_0004", HIPPODROME_PHASE_1),
    PHASE("Health_Culture\\Hippodrome_1", "Image_0002", HIPPODROME_PHASE_2),
    PHASE("Health_Culture\\Hippodrome_1", "Image_0000", HIPPODROME_PHASE_3),
    PHASE("Health_Culture\\Hippodrome_1", "Image_0000", HIPPODROME_PHASE_4)
};
static const PhaseSpec SMALL_MAUSOLEUM_PHASES[] = {
    PHASE("Environment\\Group_206", "Mausoleum_Small_Construction_01", SMALL_MAUSOLEUM_PHASE_1),
    PHASE("Environment\\Group_206", "Mausoleum_Small_Construction_02", SMALL_MAUSOLEUM_PHASE_2)
};
static const PhaseSpec NYMPHAEUM_PHASES[] = {
    PHASE("Environment\\Group_206", "Pantheon_Const_00", NYMPHAEUM_PHASE_1),
    PHASE("Terrain_Maps\\Road", "Nymphaeum_Construction_02", NYMPHAEUM_PHASE_2)
};
static const PhaseSpec LARGE_MAUSOLEUM_PHASES[] = {
    PHASE("Environment\\Group_206", "Mausoleum L Cons", LARGE_MAUSOLEUM_PHASE_1),
    PHASE("Monuments\\Mausoleum_Large_Construction_02", "Mausoleum_Large_Construction_02", LARGE_MAUSOLEUM_PHASE_2)
};
static const PhaseSpec CITY_MINT_PHASES[] = {
    PHASE("Environment\\Group_206", "City_Mint_Construction_01", CITY_MINT_PHASE_1),
    PHASE("Monuments\\City_Mint_Construction_02", "City_Mint_Construction_02", CITY_MINT_PHASE_2)
};

#undef PHASE

#define PHASE_COUNT(phases) (sizeof(phases) / sizeof(phases[0]))

static const LegacyConstructionSpec LEGACY_MONUMENT_CONSTRUCTION[] = {
    {BUILDING_GRAND_TEMPLE_CERES, "grand_temple_ceres", "Monuments\\Ceres_worshippers", "Ceres Complex On", 9,
        GRAND_TEMPLE_CERES_PHASES, PHASE_COUNT(GRAND_TEMPLE_CERES_PHASES), false},
    {BUILDING_GRAND_TEMPLE_NEPTUNE, "grand_temple_neptune", "Monuments\\Neptune_worshippers", "Neptune Complex On", 9,
        GRAND_TEMPLE_NEPTUNE_PHASES, PHASE_COUNT(GRAND_TEMPLE_NEPTUNE_PHASES), false},
    {BUILDING_GRAND_TEMPLE_MERCURY, "grand_temple_mercury", "Monuments\\Mercury_Complex_Off", "Mercury Complex On", 9,
        GRAND_TEMPLE_MERCURY_PHASES, PHASE_COUNT(GRAND_TEMPLE_MERCURY_PHASES), false},
    {BUILDING_GRAND_TEMPLE_MARS, "grand_temple_mars", "Monuments\\Mars_Complex_Off", "Mars Complex On", 9,
        GRAND_TEMPLE_MARS_PHASES, PHASE_COUNT(GRAND_TEMPLE_MARS_PHASES), false},
    {BUILDING_GRAND_TEMPLE_VENUS, "grand_temple_venus", "Monuments\\Venus_Complex_Off", "Venus Complex On", 9,
        GRAND_TEMPLE_VENUS_PHASES, PHASE_COUNT(GRAND_TEMPLE_VENUS_PHASES), false},
    {BUILDING_LARGE_TEMPLE_CERES, "large_temple_ceres", "Health_Culture\\Temple_Ceres", "Image_0001", 5,
        LARGE_TEMPLE_CERES_PHASES, PHASE_COUNT(LARGE_TEMPLE_CERES_PHASES), false},
    {BUILDING_LARGE_TEMPLE_NEPTUNE, "large_temple_neptune", "Health_Culture\\Temple_Neptune", "Image_0001", 5,
        LARGE_TEMPLE_NEPTUNE_PHASES, PHASE_COUNT(LARGE_TEMPLE_NEPTUNE_PHASES), false},
    {BUILDING_LARGE_TEMPLE_MERCURY, "large_temple_mercury", "Health_Culture\\Temple_Mercury", "Image_0001", 5,
        LARGE_TEMPLE_MERCURY_PHASES, PHASE_COUNT(LARGE_TEMPLE_MERCURY_PHASES), false},
    {BUILDING_LARGE_TEMPLE_MARS, "large_temple_mars", "Health_Culture\\Temple_Mars", "Image_0001", 5,
        LARGE_TEMPLE_MARS_PHASES, PHASE_COUNT(LARGE_TEMPLE_MARS_PHASES), false},
    {BUILDING_LARGE_TEMPLE_VENUS, "large_temple_venus", "Health_Culture\\Temple_Venus", "Image_0001", 5,
        LARGE_TEMPLE_VENUS_PHASES, PHASE_COUNT(LARGE_TEMPLE_VENUS_PHASES), false},
    {BUILDING_ORACLE, "oracle", "Health_Culture\\Oracle", "Image_0000", 2,
        ORACLE_PHASES, PHASE_COUNT(ORACLE_PHASES), true},
    {BUILDING_LIGHTHOUSE, "lighthouse", "Monuments\\Lighthouse_OFF", "Lighthouse OFF", 5,
        LIGHTHOUSE_PHASES, PHASE_COUNT(LIGHTHOUSE_PHASES), false},
    {BUILDING_CARAVANSERAI, "caravanserai", "Monuments\\Caravanserai_C_OFF", "Caravanserai_C_OFF", 4,
        CARAVANSERAI_PHASES, PHASE_COUNT(CARAVANSERAI_PHASES), false},
    {BUILDING_PANTHEON, "pantheon", "Monuments\\Pantheon_Off", "Pantheon Off", 9,
        PANTHEON_PHASES, PHASE_COUNT(PANTHEON_PHASES), false},
    {BUILDING_COLOSSEUM, "colosseum", "Health_Culture\\Colosseum", "Coloseum ON", 7,
        COLOSSEUM_PHASES, PHASE_COUNT(COLOSSEUM_PHASES), false},
    {BUILDING_HIPPODROME, "hippodrome", "Health_Culture\\Hippodrome_1", "Image_0000", 7,
        HIPPODROME_PHASES, PHASE_COUNT(HIPPODROME_PHASES), false},
    {BUILDING_SMALL_MAUSOLEUM, "small_mausoleum", "Monuments\\Mausoleum_S", "Mausoleum S", 3,
        SMALL_MAUSOLEUM_PHASES, PHASE_COUNT(SMALL_MAUSOLEUM_PHASES), false},
    {BUILDING_NYMPHAEUM, "nymphaeum", "Monuments\\Nymphaeum_OFF", "Nymphaeum ON", 3,
        NYMPHAEUM_PHASES, PHASE_COUNT(NYMPHAEUM_PHASES), false},
    {BUILDING_LARGE_MAUSOLEUM, "large_mausoleum", "Monuments\\Mausoleum_L", "Mausoleum L", 3,
        LARGE_MAUSOLEUM_PHASES, PHASE_COUNT(LARGE_MAUSOLEUM_PHASES), false},
    {BUILDING_CITY_MINT, "city_mint", "Monuments\\City_Mint_OFF", "City_Mint_OFF", 3,
        CITY_MINT_PHASES, PHASE_COUNT(CITY_MINT_PHASES), false}
};

#undef PHASE_COUNT

building_type runtime_from_legacy_enum(uint16_t legacy_id)
{
    const char *text_id = building_type_legacy_migration_text_id_for_enum(legacy_id);
    building_type runtime_id = runtime_from_registered_text(text_id);
    if (runtime_id != BUILDING_NONE) {
        return runtime_id;
    }

    uint16_t xml_owned_legacy_id = building_type_legacy_migration_enum_for_text_id(text_id);
    if (xml_owned_legacy_id > BUILDING_NONE && xml_owned_legacy_id < BUILDING_TYPE_MAX &&
        xml_owned_legacy_id != BUILDING_LEGACY_SLOT_THEATER &&
        xml_owned_legacy_id != BUILDING_LEGACY_SLOT_WELL) {
        return static_cast<building_type>(xml_owned_legacy_id);
    }

    return BUILDING_NONE;
}

building_type runtime_from_save_table_text(const std::string &text_id)
{
    building_type runtime_id = runtime_from_registered_text(text_id.c_str());
    if (runtime_id != BUILDING_NONE) {
        return runtime_id;
    }

    uint16_t legacy_id = 0;
    if (parse_uint16_text(text_id, &legacy_id)) {
        return runtime_from_legacy_enum(legacy_id);
    }

    uint16_t xml_owned_legacy_id = building_type_legacy_migration_enum_for_text_id(text_id.c_str());
    if (xml_owned_legacy_id > BUILDING_NONE) {
        return runtime_from_legacy_enum(xml_owned_legacy_id);
    }

    return BUILDING_NONE;
}

uint16_t legacy_enum_hint_for_text(const std::string &text_id, building_type runtime_id)
{
    uint16_t legacy_id = 0;
    if (parse_uint16_text(text_id, &legacy_id)) {
        return legacy_id;
    }
    legacy_id = building_type_legacy_migration_enum_for_text_id(text_id.c_str());
    if (legacy_id > BUILDING_NONE) {
        return legacy_id;
    }
    return static_cast<uint16_t>(runtime_id);
}

building_type_registry_impl::BuildingType *mutable_definition_for_type(building_type type)
{
    if (type <= BUILDING_NONE || type >= BUILDING_TYPE_MAX) {
        return nullptr;
    }
    return building_type_registry_impl::g_building_types[static_cast<uint16_t>(type)].get();
}

const LegacyConstructionSpec *legacy_construction_spec_for(uint16_t legacy_id, const std::string &text_id)
{
    for (const LegacyConstructionSpec &spec : LEGACY_MONUMENT_CONSTRUCTION) {
        if ((legacy_id > BUILDING_NONE && spec.legacy_type == static_cast<building_type>(legacy_id)) ||
            (!text_id.empty() && text_id == spec.text_id)) {
            return &spec;
        }
    }
    return nullptr;
}

bool active_mod_is_julius()
{
    const char *mod_name = mod_manager_get_mod_name();
    return mod_name && std::strcmp(mod_name, "Julius") == 0;
}

bool legacy_construction_spec_is_active(const LegacyConstructionSpec &spec)
{
    return !(spec.skip_for_julius && active_mod_is_julius());
}

void ensure_default_graphics(building_type_registry_impl::BuildingType &definition, const char *path, const char *image)
{
    if (definition.has_graphic() || !path || !*path) {
        return;
    }

    definition.mark_graphics_default_node();
    building_type_registry_impl::GraphicsTarget &target = definition.default_graphics_target();
    target.set_path(path);
    if (image && *image) {
        target.set_image(image);
    }
}

void capture_default_graphics(
    const building_type_registry_impl::BuildingType &definition,
    std::string &path,
    std::string &image)
{
    const building_type_registry_impl::GraphicsTarget &target = definition.graphics().default_target();
    if (!target.has_path()) {
        return;
    }
    path = target.path();
    if (target.has_image()) {
        image = target.image();
    }
}

SavedConstructionDefinition construction_from_legacy_spec(const LegacyConstructionSpec &spec)
{
    SavedConstructionDefinition construction;
    construction.has_phased_construction = true;
    construction.road_update_radius = spec.road_update_radius;
    for (size_t phase_index = 0; phase_index < spec.phase_count; ++phase_index) {
        const PhaseSpec &phase_spec = spec.phases[phase_index];
        SavedConstructionPhase phase;
        phase.index = static_cast<int>(phase_index + 1);
        phase.path = phase_spec.path ? phase_spec.path : "";
        phase.image = phase_spec.image ? phase_spec.image : "";
        for (size_t requirement_index = 0; requirement_index < phase_spec.requirement_count; ++requirement_index) {
            phase.requirements.push_back({
                phase_spec.requirements[requirement_index].resource,
                phase_spec.requirements[requirement_index].amount
            });
        }
        construction.phases.push_back(std::move(phase));
    }
    return construction;
}

SavedConstructionDefinition construction_from_registry_definition(
    const building_type_registry_impl::BuildingType &definition)
{
    SavedConstructionDefinition construction;
    if (!definition.has_phased_construction()) {
        return construction;
    }

    construction.has_phased_construction = true;
    construction.road_update_radius = definition.construction().road_update_radius();
    for (const building_type_registry_impl::ConstructionPhase &source_phase : definition.construction().phases()) {
        SavedConstructionPhase phase;
        phase.index = source_phase.index;
        if (source_phase.graphics.has_path()) {
            phase.path = source_phase.graphics.path();
        }
        if (source_phase.graphics.has_image()) {
            phase.image = source_phase.graphics.image();
        }
        for (const building_type_registry_impl::ConstructionRequirement &requirement : source_phase.requirements) {
            phase.requirements.push_back({requirement.resource, requirement.amount});
        }
        construction.phases.push_back(std::move(phase));
    }
    return construction;
}

bool definition_has_instant_construction_requirements(
    const building_type_registry_impl::BuildingType &definition)
{
    if (definition.construction().mode() != building_type_registry_impl::ConstructionMode::Instant) {
        return false;
    }
    for (int resource = RESOURCE_NONE; resource < RESOURCE_MAX; ++resource) {
        if (definition.construction().instant_requirement_amount(static_cast<resource_type>(resource)) > 0) {
            return true;
        }
    }
    return false;
}

SavedConstructionDefinition construction_for_save_entry(
    building_type type,
    const std::string &text_id,
    uint16_t legacy_id,
    const building_type_registry_impl::BuildingType *definition,
    const LegacyConstructionSpec **legacy_spec)
{
    if (legacy_spec) {
        *legacy_spec = nullptr;
    }
    if (definition) {
        SavedConstructionDefinition construction = construction_from_registry_definition(*definition);
        if (construction.has_phased_construction) {
            return construction;
        }
        if (definition_has_instant_construction_requirements(*definition)) {
            return {};
        }
    }

    const LegacyConstructionSpec *spec = legacy_construction_spec_for(legacy_id, text_id);
    if (!spec && type > BUILDING_NONE) {
        spec = legacy_construction_spec_for(static_cast<uint16_t>(type), text_id);
    }
    if (!spec || !legacy_construction_spec_is_active(*spec)) {
        return {};
    }
    if (legacy_spec) {
        *legacy_spec = spec;
    }
    return construction_from_legacy_spec(*spec);
}

void apply_construction_to_definition(
    building_type_registry_impl::BuildingType &definition,
    const SavedConstructionDefinition &construction)
{
    if (!construction.has_phased_construction || construction.phases.empty()) {
        return;
    }

    definition.clear_construction();
    definition.set_construction_mode(building_type_registry_impl::ConstructionMode::Phased);
    definition.set_construction_road_update_radius(construction.road_update_radius);
    for (const SavedConstructionPhase &source_phase : construction.phases) {
        building_type_registry_impl::ConstructionPhase &phase =
            definition.add_construction_phase(source_phase.index);
        if (!source_phase.path.empty()) {
            phase.graphics.set_path(source_phase.path);
        }
        if (!source_phase.image.empty()) {
            phase.graphics.set_image(source_phase.image);
        }
        for (const SavedConstructionRequirement &requirement : source_phase.requirements) {
            definition.add_construction_requirement(requirement.resource, requirement.amount);
        }
    }
}

building_type_registry_impl::BuildingType *ensure_save_definition(
    building_type runtime_id,
    const std::string &text_id,
    const LegacyConstructionSpec *legacy_spec)
{
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX) {
        return nullptr;
    }

    building_type_registry_impl::BuildingType *definition = mutable_definition_for_type(runtime_id);
    if (definition) {
        return definition;
    }

    const char *attr = !text_id.empty() ? text_id.c_str() :
        building_type_legacy_migration_text_id_for_enum(static_cast<uint16_t>(runtime_id));
    if (!legacy_spec && (!attr || !*attr)) {
        return nullptr;
    }

    if (attr && *attr) {
        log_error("Building type referenced by save is not available in active mod; using save table compatibility definition",
            attr, static_cast<int>(runtime_id));
    }
    building_type_registry_impl::g_building_types[static_cast<uint16_t>(runtime_id)] =
        std::make_unique<building_type_registry_impl::BuildingType>(runtime_id, attr ? attr : "");
    definition = mutable_definition_for_type(runtime_id);
    if (definition && legacy_spec) {
        ensure_default_graphics(*definition, legacy_spec->default_path, legacy_spec->default_image);
    }
    if (definition && attr && *attr) {
        register_text_id(runtime_id, attr);
    }
    return definition;
}

bool apply_saved_construction_entry(
    building_type runtime_id,
    const std::string &text_id,
    const SavedConstructionDefinition &construction)
{
    if (!construction.has_phased_construction) {
        return mutable_definition_for_type(runtime_id) != nullptr;
    }

    building_type_registry_impl::BuildingType *definition = ensure_save_definition(runtime_id, text_id, nullptr);
    if (!definition) {
        return false;
    }

    apply_construction_to_definition(*definition, construction);
    return true;
}

bool apply_legacy_monument_entry(
    building_type runtime_id,
    const std::string &text_id,
    uint16_t legacy_id,
    savegame_version_t save_version,
    SavedConstructionDefinition *saved_construction)
{
    if (save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE) {
        return mutable_definition_for_type(runtime_id) != nullptr;
    }

    const LegacyConstructionSpec *spec = legacy_construction_spec_for(legacy_id, text_id);
    if (!spec || !legacy_construction_spec_is_active(*spec)) {
        return mutable_definition_for_type(runtime_id) != nullptr;
    }

    building_type_registry_impl::BuildingType *definition = ensure_save_definition(runtime_id, text_id, spec);
    if (!definition) {
        return false;
    }

    if (!definition->has_phased_construction()) {
        SavedConstructionDefinition construction = construction_from_legacy_spec(*spec);
        apply_construction_to_definition(*definition, construction);
        if (saved_construction) {
            *saved_construction = std::move(construction);
        }
    } else if (saved_construction) {
        *saved_construction = construction_from_registry_definition(*definition);
    }
    return true;
}

void ensure_runtime_table()
{
    if (g_bridge.runtime_ready) {
        return;
    }

    g_bridge.text_to_runtime.clear();
    for (std::string &text_id : g_bridge.runtime_to_text) {
        text_id.clear();
    }

    for (uint16_t id = 1; id < BUILDING_TYPE_MAX; ++id) {
        const char *legacy_text_id = building_type_legacy_migration_text_id_for_enum(id);
        if (id == BUILDING_LEGACY_SLOT_THEATER || id == BUILDING_LEGACY_SLOT_WELL) {
            continue;
        }
        register_text_id(static_cast<building_type>(id), legacy_text_id ? legacy_text_id : "");
    }

    for (const auto &definition : building_type_registry_impl::g_building_types) {
        if (!definition) {
            continue;
        }
        register_attr_tokens(definition->type(), definition->attr());
    }

    g_bridge.runtime_ready = true;
}

void clear_save_table()
{
    g_bridge.runtime_to_save.fill(0);
    g_bridge.save_to_runtime.clear();
    g_bridge.save_id_missing.clear();
    g_bridge.save_entries.clear();
    g_bridge.save_to_runtime.push_back(BUILDING_NONE);
    g_bridge.save_id_missing.push_back(0);
    g_bridge.save_entries.emplace_back();
}

void ensure_save_table()
{
    if (g_bridge.save_table_ready) {
        return;
    }

    building_type_id_bridge_prepare_new_save_table();
}

void append_save_id_mapping(uint16_t save_id, building_type runtime_id, bool missing, const SaveTableEntry *entry = nullptr)
{
    size_t index = static_cast<size_t>(save_id);
    if (g_bridge.save_to_runtime.size() <= index) {
        g_bridge.save_to_runtime.resize(index + 1, BUILDING_NONE);
        g_bridge.save_id_missing.resize(index + 1, 0);
        g_bridge.save_entries.resize(index + 1);
    }
    g_bridge.save_to_runtime[index] = runtime_id;
    g_bridge.save_id_missing[index] = missing ? 1 : 0;
    if (entry) {
        g_bridge.save_entries[index] = *entry;
    } else {
        g_bridge.save_entries[index] = SaveTableEntry();
        g_bridge.save_entries[index].runtime_id = runtime_id;
        g_bridge.save_entries[index].missing = missing;
        g_bridge.save_entries[index].legacy_enum_hint = static_cast<uint16_t>(runtime_id);
    }
    if (!missing && runtime_id > BUILDING_NONE && runtime_id < BUILDING_TYPE_MAX) {
        g_bridge.runtime_to_save[static_cast<uint16_t>(runtime_id)] = save_id;
    }
}

void load_legacy_save_table(savegame_version_t save_version, bool apply_definition_migration)
{
    ensure_runtime_table();
    clear_save_table();

    for (uint16_t legacy_id = 1; legacy_id < BUILDING_TYPE_MAX; ++legacy_id) {
        building_type runtime_id = runtime_from_legacy_enum(legacy_id);
        const char *text_id = building_type_legacy_migration_text_id_for_enum(legacy_id);
        SaveTableEntry entry;
        entry.text_id = text_id ? text_id : "";
        entry.legacy_enum_hint = legacy_id;
        entry.runtime_id = runtime_id;
        int has_definition = mutable_definition_for_type(runtime_id) != nullptr;
        if (apply_definition_migration) {
            has_definition = apply_legacy_monument_entry(
                runtime_id,
                entry.text_id,
                legacy_id,
                save_version,
                &entry.construction);
        }
        entry.missing = runtime_id == BUILDING_NONE ||
            (building_type_legacy_migration_text_id_is_xml_owned(text_id) && !has_definition);
        if (entry.missing && text_id && *text_id) {
            log_error("Building type referenced by legacy save is not available in active mod", text_id, legacy_id);
        }
        append_save_id_mapping(legacy_id, runtime_id, entry.missing, &entry);
    }

    g_bridge.save_table_ready = true;
}

}

extern "C" void building_type_id_bridge_reset_for_runtime(void)
{
    g_bridge.runtime_ready = false;
    g_bridge.save_table_ready = false;
    ensure_runtime_table();
}

extern "C" const char *building_type_id_bridge_text_from_runtime(building_type runtime_id)
{
    ensure_runtime_table();
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX ||
        g_bridge.runtime_to_text[static_cast<uint16_t>(runtime_id)].empty()) {
        return 0;
    }
    return g_bridge.runtime_to_text[static_cast<uint16_t>(runtime_id)].c_str();
}

extern "C" building_type building_type_id_bridge_runtime_from_text(const char *text_id)
{
    ensure_runtime_table();
    return runtime_from_registered_text(text_id);
}

extern "C" void building_type_id_bridge_prepare_new_save_table(void)
{
    ensure_runtime_table();
    clear_save_table();

    for (uint16_t runtime_id = 1; runtime_id < BUILDING_TYPE_MAX; ++runtime_id) {
        if (g_bridge.runtime_to_text[runtime_id].empty()) {
            continue;
        }
        uint16_t save_id = static_cast<uint16_t>(g_bridge.save_to_runtime.size());
        building_type type = static_cast<building_type>(runtime_id);
        SaveTableEntry entry;
        entry.text_id = g_bridge.runtime_to_text[runtime_id];
        entry.legacy_enum_hint = legacy_enum_hint_for_text(entry.text_id, type);
        entry.runtime_id = type;
        entry.missing = false;
        const LegacyConstructionSpec *legacy_spec = nullptr;
        if (building_type_registry_impl::BuildingType *definition = mutable_definition_for_type(type)) {
            capture_default_graphics(*definition, entry.default_graphics_path, entry.default_graphics_image);
            entry.construction = construction_for_save_entry(
                type,
                entry.text_id,
                entry.legacy_enum_hint,
                definition,
                &legacy_spec);
        } else {
            entry.construction = construction_for_save_entry(
                type,
                entry.text_id,
                entry.legacy_enum_hint,
                nullptr,
                &legacy_spec);
        }
        if (legacy_spec) {
            if (entry.default_graphics_path.empty()) {
                entry.default_graphics_path = legacy_spec->default_path ? legacy_spec->default_path : "";
            }
            if (entry.default_graphics_image.empty()) {
                entry.default_graphics_image = legacy_spec->default_image ? legacy_spec->default_image : "";
            }
        }
        append_save_id_mapping(save_id, type, false, &entry);
    }

    g_bridge.save_table_ready = true;
}

size_t serialized_string_size(const std::string &text)
{
    return sizeof(uint16_t) + text.size();
}

void write_string(buffer *buf, const std::string &text)
{
    size_t text_length = text.size();
    if (text_length > std::numeric_limits<uint16_t>::max()) {
        buffer_write_u16(buf, 0);
        return;
    }
    buffer_write_u16(buf, static_cast<uint16_t>(text_length));
    if (text_length) {
        buffer_write_raw(buf, text.data(), text_length);
    }
}

std::string read_string(buffer *buf)
{
    uint16_t text_length = buffer_read_u16(buf);
    std::string text(text_length, '\0');
    if (text_length) {
        buffer_read_raw(buf, &text[0], text_length);
    }
    return text;
}

size_t serialized_construction_size(const SaveTableEntry &entry)
{
    size_t size = sizeof(uint16_t) + serialized_string_size(entry.text_id) + sizeof(uint16_t) +
        sizeof(uint8_t) + serialized_string_size(entry.default_graphics_path) +
        serialized_string_size(entry.default_graphics_image);
    if (!entry.construction.has_phased_construction) {
        return size;
    }

    size += sizeof(uint16_t) * 2;
    for (const SavedConstructionPhase &phase : entry.construction.phases) {
        size += sizeof(uint16_t) + serialized_string_size(phase.path) + serialized_string_size(phase.image) +
            sizeof(uint16_t);
        size += phase.requirements.size() * sizeof(uint16_t) * 2;
    }
    return size;
}

void write_construction_entry(buffer *buf, uint16_t save_id, const SaveTableEntry &entry)
{
    buffer_write_u16(buf, save_id);
    write_string(buf, entry.text_id);
    buffer_write_u16(buf, entry.legacy_enum_hint);
    buffer_write_u8(buf, entry.construction.has_phased_construction ? 1 : 0);
    write_string(buf, entry.default_graphics_path);
    write_string(buf, entry.default_graphics_image);
    if (!entry.construction.has_phased_construction) {
        return;
    }

    buffer_write_u16(buf, static_cast<uint16_t>(entry.construction.road_update_radius));
    buffer_write_u16(buf, static_cast<uint16_t>(entry.construction.phases.size()));
    for (const SavedConstructionPhase &phase : entry.construction.phases) {
        buffer_write_u16(buf, static_cast<uint16_t>(phase.index));
        write_string(buf, phase.path);
        write_string(buf, phase.image);
        buffer_write_u16(buf, static_cast<uint16_t>(phase.requirements.size()));
        for (const SavedConstructionRequirement &requirement : phase.requirements) {
            buffer_write_u16(buf, static_cast<uint16_t>(requirement.resource));
            buffer_write_u16(buf, static_cast<uint16_t>(requirement.amount));
        }
    }
}

SaveTableEntry read_construction_entry(buffer *table)
{
    SaveTableEntry entry;
    entry.text_id = read_string(table);
    entry.legacy_enum_hint = buffer_read_u16(table);
    entry.construction.has_phased_construction = buffer_read_u8(table) ? true : false;
    entry.default_graphics_path = read_string(table);
    entry.default_graphics_image = read_string(table);
    if (!entry.construction.has_phased_construction) {
        return entry;
    }

    entry.construction.road_update_radius = buffer_read_u16(table);
    uint16_t phase_count = buffer_read_u16(table);
    for (uint16_t i = 0; i < phase_count && !buffer_at_end(table); ++i) {
        SavedConstructionPhase phase;
        phase.index = buffer_read_u16(table);
        phase.path = read_string(table);
        phase.image = read_string(table);
        uint16_t requirement_count = buffer_read_u16(table);
        for (uint16_t r = 0; r < requirement_count && !buffer_at_end(table); ++r) {
            SavedConstructionRequirement requirement;
            requirement.resource = static_cast<resource_type>(buffer_read_u16(table));
            requirement.amount = buffer_read_u16(table);
            phase.requirements.push_back(requirement);
        }
        entry.construction.phases.push_back(std::move(phase));
    }
    return entry;
}

void load_save_table_internal(
    buffer *buf,
    int has_save_table,
    savegame_version_t save_version,
    bool apply_definition_migration)
{
    if (!has_save_table || !buf || !buf->size) {
        if (save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE) {
            fatal_current_save_table(
                "0xb6 save is missing the BuildingType object table; this line is wrong and needs to be cleaned up",
                "load_save_table_internal");
        }
        load_legacy_save_table(save_version, apply_definition_migration);
        return;
    }

    ensure_runtime_table();
    clear_save_table();

    buffer table = *buf;
    if (buffer_load_dynamic(&table) < sizeof(uint32_t) * 2) {
        if (save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE) {
            fatal_current_save_table(
                "0xb6 BuildingType object table is invalid; legacy fallback is forbidden",
                "load_save_table_internal");
        }
        log_error("Building type save table is invalid; falling back to legacy enum migration", 0, 0);
        load_legacy_save_table(save_version, apply_definition_migration);
        return;
    }

    uint32_t version = buffer_read_u32(&table);
    uint32_t count = buffer_read_u32(&table);
    if (version != SAVE_TABLE_TEXT_VERSION && version != SAVE_TABLE_OBJECT_VERSION &&
        version != SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION) {
        if (save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE) {
            fatal_current_save_table(
                "0xb6 BuildingType object table has an unsupported version; legacy fallback is forbidden",
                "load_save_table_internal");
        }
        log_error("Unsupported building type save table version", 0, static_cast<int>(version));
        load_legacy_save_table(save_version, apply_definition_migration);
        return;
    }
    if (save_version > SAVE_GAME_LAST_NO_BUILDING_TYPE_OBJECT_TABLE &&
        version != SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION) {
        fatal_current_save_table(
            "0xb6 save attempted to load a pre-v3 BuildingType object table; this line is wrong and needs to be cleaned up",
            "BuildingType save table must be version 3");
    }

    for (uint32_t i = 0; i < count && !buffer_at_end(&table); ++i) {
        uint16_t save_id = buffer_read_u16(&table);
        SaveTableEntry entry;
        if (version == SAVE_TABLE_TEXT_VERSION) {
            entry.text_id = read_string(&table);
            entry.legacy_enum_hint = legacy_enum_hint_for_text(entry.text_id, BUILDING_NONE);
        } else {
            entry = read_construction_entry(&table);
        }

        building_type runtime_id = runtime_from_save_table_text(entry.text_id);
        if (runtime_id == BUILDING_NONE && entry.legacy_enum_hint > BUILDING_NONE) {
            runtime_id = runtime_from_legacy_enum(entry.legacy_enum_hint);
        }
        if (runtime_id == BUILDING_NONE && version >= SAVE_TABLE_OBJECT_VERSION &&
            entry.legacy_enum_hint > BUILDING_NONE && entry.legacy_enum_hint < BUILDING_TYPE_MAX) {
            runtime_id = static_cast<building_type>(entry.legacy_enum_hint);
        }

        entry.runtime_id = runtime_id;
        int has_definition = mutable_definition_for_type(runtime_id) != nullptr;
        const LegacyConstructionSpec *authoritative_spec =
            legacy_construction_spec_for(entry.legacy_enum_hint, entry.text_id);
        bool invalid_authoritative_monument =
            version >= SAVE_TABLE_MONUMENT_CONSTRUCTION_VERSION &&
            !entry.construction.has_phased_construction &&
            authoritative_spec &&
            legacy_construction_spec_is_active(*authoritative_spec);
        if (apply_definition_migration) {
            if (version >= SAVE_TABLE_OBJECT_VERSION && entry.construction.has_phased_construction) {
                has_definition = apply_saved_construction_entry(runtime_id, entry.text_id, entry.construction);
                if (building_type_registry_impl::BuildingType *definition = mutable_definition_for_type(runtime_id)) {
                    ensure_default_graphics(
                        *definition,
                        entry.default_graphics_path.c_str(),
                        entry.default_graphics_image.c_str());
                }
            } else if (invalid_authoritative_monument) {
                has_definition = 0;
            } else {
                has_definition = apply_legacy_monument_entry(
                    runtime_id,
                    entry.text_id,
                    entry.legacy_enum_hint,
                    save_version,
                    &entry.construction);
            }
        }

        const char *text = entry.text_id.empty() ? nullptr : entry.text_id.c_str();
        entry.missing = invalid_authoritative_monument || runtime_id == BUILDING_NONE ||
            (building_type_legacy_migration_text_id_is_xml_owned(text) && !has_definition);
        if (entry.missing && !entry.text_id.empty()) {
            log_error("Building type referenced by save is not available in active mod", entry.text_id.c_str(), save_id);
        }
        append_save_id_mapping(save_id, runtime_id, entry.missing, &entry);
    }

    g_bridge.save_table_ready = true;
}

extern "C" void building_type_id_bridge_save_table_save_state(buffer *buf)
{
    if (!buf) {
        return;
    }

    ensure_save_table();

    size_t payload_size = sizeof(uint32_t) * 2;
    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        const SaveTableEntry &entry = g_bridge.save_entries[save_id];
        if (entry.text_id.empty()) {
            continue;
        }
        payload_size += serialized_construction_size(entry);
    }

    buffer_init_dynamic(buf, payload_size);
    buffer_write_u32(buf, SAVE_TABLE_VERSION);
    buffer_write_u32(buf, static_cast<uint32_t>(g_bridge.save_to_runtime.size() - 1));

    for (size_t save_id = 1; save_id < g_bridge.save_to_runtime.size(); ++save_id) {
        const SaveTableEntry &entry = g_bridge.save_entries[save_id];
        if (entry.text_id.empty()) {
            continue;
        }
        if (entry.text_id.size() > std::numeric_limits<uint16_t>::max()) {
            log_error("Building type text id too long for save table", entry.text_id.c_str(),
                static_cast<int>(entry.text_id.size()));
            continue;
        }
        write_construction_entry(buf, static_cast<uint16_t>(save_id), entry);
    }
}

extern "C" void building_type_id_bridge_save_table_load_state(
    buffer *buf,
    int has_save_table,
    savegame_version_t save_version)
{
    load_save_table_internal(buf, has_save_table, save_version, true);
}

extern "C" void building_type_id_bridge_save_table_preview_load_state(
    buffer *buf,
    int has_save_table,
    savegame_version_t save_version)
{
    g_preview_bridge_snapshot = std::make_unique<BridgeState>(g_bridge);
    load_save_table_internal(buf, has_save_table, save_version, false);
}

extern "C" void building_type_id_bridge_save_table_preview_restore(void)
{
    if (!g_preview_bridge_snapshot) {
        return;
    }
    g_bridge = std::move(*g_preview_bridge_snapshot);
    g_preview_bridge_snapshot.reset();
}

extern "C" uint16_t building_type_id_bridge_save_id_from_runtime(building_type runtime_id)
{
    ensure_save_table();
    if (runtime_id <= BUILDING_NONE || runtime_id >= BUILDING_TYPE_MAX) {
        return 0;
    }
    uint16_t save_id = g_bridge.runtime_to_save[static_cast<uint16_t>(runtime_id)];
    if (!save_id) {
        log_error("Building type has no save table id", building_type_id_bridge_text_from_runtime(runtime_id),
            static_cast<int>(runtime_id));
    }
    return save_id;
}

extern "C" building_type building_type_id_bridge_runtime_from_save_id(uint16_t save_id)
{
    ensure_save_table();
    if (static_cast<size_t>(save_id) >= g_bridge.save_to_runtime.size()) {
        return runtime_from_legacy_enum(save_id);
    }
    building_type runtime_id = g_bridge.save_to_runtime[static_cast<size_t>(save_id)];
    if (runtime_id == BUILDING_NONE && save_id > BUILDING_NONE &&
        g_bridge.save_entries[static_cast<size_t>(save_id)].text_id.empty()) {
        runtime_id = runtime_from_legacy_enum(save_id);
    }
    return runtime_id;
}

extern "C" int building_type_id_bridge_save_id_is_missing(uint16_t save_id)
{
    ensure_save_table();
    if (save_id == 0) {
        return 0;
    }
    if (static_cast<size_t>(save_id) >= g_bridge.save_id_missing.size()) {
        return runtime_from_legacy_enum(save_id) == BUILDING_NONE ? 1 : 0;
    }
    if (g_bridge.save_id_missing[static_cast<size_t>(save_id)] &&
        g_bridge.save_entries[static_cast<size_t>(save_id)].text_id.empty() &&
        runtime_from_legacy_enum(save_id) != BUILDING_NONE) {
        return 0;
    }
    return g_bridge.save_id_missing[static_cast<size_t>(save_id)] ? 1 : 0;
}

extern "C" int building_type_id_bridge_save_id_is_in_table(uint16_t save_id)
{
    ensure_save_table();
    return static_cast<size_t>(save_id) < g_bridge.save_to_runtime.size() ? 1 : 0;
}

extern "C" int building_type_id_bridge_save_id_has_phased_construction(uint16_t save_id)
{
    ensure_save_table();
    if (save_id == 0 || static_cast<size_t>(save_id) >= g_bridge.save_entries.size()) {
        return 0;
    }
    return g_bridge.save_entries[static_cast<size_t>(save_id)].construction.has_phased_construction ? 1 : 0;
}
