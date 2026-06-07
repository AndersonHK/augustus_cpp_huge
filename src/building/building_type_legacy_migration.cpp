#include "building/building_record.h"
#include "building/building_type_legacy_migration.h"

extern "C" {
#include "building/properties.h"
}

#include <array>
#include <cctype>
#include <string>
#include <string_view>

namespace {

std::array<std::string, BUILDING_TYPE_MAX> g_legacy_text_ids;
bool g_legacy_text_ids_ready = false;

constexpr uint16_t LEGACY_BUILDING_FORT_GROUND = 54;
constexpr uint16_t LEGACY_BUILDING_DISTRIBUTION_CENTER_UNUSED = 50;
constexpr uint16_t LEGACY_BUILDING_WAREHOUSE_SPACE = 73;
constexpr uint16_t LEGACY_BUILDING_BURNING_RUIN = 99;

struct LegacyBuildingTypeTextId {
    uint16_t legacy_type;
    const char *text_id;
};

constexpr LegacyBuildingTypeTextId XML_OWNED_BUILDING_TYPE_IDS[] = {
    {10, "vacant_lot"},
    {52, "academy"},
    {36, "actor_colony"},
    {30, "amphitheater"},
    {153, "architect_guild"},
    {160, "arena"},
    {203, "armoury"},
    {49, "barber"},
    {48, "bathhouse"},
    {190, "brickworks"},
    {176, "caravanserai"},
    {37, "chariot_maker"},
    {109, "clay_pit"},
    {33, "colosseum"},
    {189, "concrete_maker"},
    {46, "doctor"},
    {81, "engineers_post"},
    {86, "forum"},
    {91, "fountain"},
    {102, "fruit_farm"},
    {113, "furniture_workshop"},
    {34, "gladiator_school"},
    {184, "gold_mine"},
    {77, "governors_house"},
    {79, "governors_palace"},
    {78, "governors_villa"},
    {117, "grand_temple_ceres"},
    {120, "grand_temple_mars"},
    {119, "grand_temple_mercury"},
    {118, "grand_temple_neptune"},
    {121, "grand_temple_venus"},
    {32, "hippodrome"},
    {47, "hospital"},
    {107, "iron_mine"},
    {169, "lararium"},
    {43, "large_statue"},
    {127, "large_pond"},
    {65, "large_temple_ceres"},
    {68, "large_temple_mars"},
    {67, "large_temple_mercury"},
    {66, "large_temple_neptune"},
    {69, "large_temple_venus"},
    {205, "latrines"},
    {53, "library"},
    {155, "lighthouse"},
    {35, "lion_house"},
    {70, "market"},
    {106, "marble_quarry"},
    {111, "oil_workshop"},
    {103, "olive_farm"},
    {98, "oracle"},
    {152, "pantheon"},
    {105, "pig_farm"},
    {114, "pottery_workshop"},
    {55, "prefecture"},
    {90, "reservoir"},
    {187, "sand_pit"},
    {51, "school"},
    {85, "senate"},
    {126, "small_pond"},
    {60, "small_temple_ceres"},
    {63, "small_temple_mars"},
    {62, "small_temple_mercury"},
    {61, "small_temple_neptune"},
    {64, "small_temple_venus"},
    {188, "stone_quarry"},
    {158, "tavern"},
    {31, "theater"},
    {108, "timber_yard"},
    {101, "vegetable_farm"},
    {104, "vines_farm"},
    {173, "watchtower"},
    {112, "weapons_workshop"},
    {92, "well"},
    {100, "wheat_farm"},
    {110, "wine_workshop"},
    {116, "workcamp"}
};

std::string canonical_text_id_from_attr(const char *attr)
{
    if (!attr || !*attr) {
        return {};
    }

    std::string text(attr);
    size_t end = text.find('|');
    if (end != std::string::npos) {
        text.resize(end);
    }

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

void build_legacy_text_ids()
{
    if (g_legacy_text_ids_ready) {
        return;
    }

    for (uint16_t id = 1; id < BUILDING_TYPE_MAX; ++id) {
        const building_properties *properties = building_properties_for_type(static_cast<building_type>(id));
        if (!properties) {
            continue;
        }
        g_legacy_text_ids[id] = canonical_text_id_from_attr(properties->event_data.attr);
    }
    for (const LegacyBuildingTypeTextId &mapping : XML_OWNED_BUILDING_TYPE_IDS) {
        g_legacy_text_ids[mapping.legacy_type] = mapping.text_id;
    }
    g_legacy_text_ids[LEGACY_BUILDING_FORT_GROUND] = "fort_ground";
    g_legacy_text_ids[LEGACY_BUILDING_DISTRIBUTION_CENTER_UNUSED] = "distribution_center_unused";
    g_legacy_text_ids[LEGACY_BUILDING_WAREHOUSE_SPACE] = "warehouse_space";
    g_legacy_text_ids[LEGACY_BUILDING_BURNING_RUIN] = "burning_ruin";
    g_legacy_text_ids_ready = true;
}

}

extern "C" const char *building_type_legacy_migration_text_id_for_enum(uint16_t legacy_type)
{
    build_legacy_text_ids();
    if (legacy_type == 0 || legacy_type >= BUILDING_TYPE_MAX || g_legacy_text_ids[legacy_type].empty()) {
        return 0;
    }
    return g_legacy_text_ids[legacy_type].c_str();
}

extern "C" uint16_t building_type_legacy_migration_enum_for_text_id(const char *text_id)
{
    if (!text_id || !*text_id) {
        return 0;
    }
    std::string_view requested_id(text_id);
    for (const LegacyBuildingTypeTextId &mapping : XML_OWNED_BUILDING_TYPE_IDS) {
        if (requested_id == mapping.text_id) {
            return mapping.legacy_type;
        }
    }
    return 0;
}

extern "C" int building_type_legacy_migration_text_id_is_xml_owned(const char *text_id)
{
    if (!text_id || !*text_id) {
        return 0;
    }
    std::string_view requested_id(text_id);
    for (const LegacyBuildingTypeTextId &mapping : XML_OWNED_BUILDING_TYPE_IDS) {
        if (requested_id == mapping.text_id) {
            return 1;
        }
    }
    return 0;
}
