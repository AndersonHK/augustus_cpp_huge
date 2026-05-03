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

struct LegacyBuildingTypeTextId {
    uint16_t legacy_type;
    const char *text_id;
};

constexpr LegacyBuildingTypeTextId XML_OWNED_BUILDING_TYPE_IDS[] = {
    {BUILDING_ACADEMY, "academy"},
    {BUILDING_ACTOR_COLONY, "actor_colony"},
    {BUILDING_AMPHITHEATER, "amphitheater"},
    {BUILDING_ARCHITECT_GUILD, "architect_guild"},
    {BUILDING_ARENA, "arena"},
    {BUILDING_ARMOURY, "armoury"},
    {BUILDING_BARBER, "barber"},
    {BUILDING_BATHHOUSE, "bathhouse"},
    {BUILDING_BRICKWORKS, "brickworks"},
    {BUILDING_CARAVANSERAI, "caravanserai"},
    {BUILDING_CONCRETE_MAKER, "concrete_maker"},
    {BUILDING_DOCTOR, "doctor"},
    {BUILDING_ENGINEERS_POST, "engineers_post"},
    {BUILDING_FORUM, "forum"},
    {BUILDING_FOUNTAIN, "fountain"},
    {BUILDING_FRUIT_FARM, "fruit_farm"},
    {BUILDING_FURNITURE_WORKSHOP, "furniture_workshop"},
    {BUILDING_GLADIATOR_SCHOOL, "gladiator_school"},
    {BUILDING_GRAND_TEMPLE_CERES, "grand_temple_ceres"},
    {BUILDING_GRAND_TEMPLE_MARS, "grand_temple_mars"},
    {BUILDING_GRAND_TEMPLE_MERCURY, "grand_temple_mercury"},
    {BUILDING_GRAND_TEMPLE_NEPTUNE, "grand_temple_neptune"},
    {BUILDING_GRAND_TEMPLE_VENUS, "grand_temple_venus"},
    {BUILDING_HOSPITAL, "hospital"},
    {BUILDING_LARGE_STATUE, "large_statue"},
    {BUILDING_LARGE_POND, "large_pond"},
    {BUILDING_LARGE_TEMPLE_CERES, "large_temple_ceres"},
    {BUILDING_LARGE_TEMPLE_MARS, "large_temple_mars"},
    {BUILDING_LARGE_TEMPLE_MERCURY, "large_temple_mercury"},
    {BUILDING_LARGE_TEMPLE_NEPTUNE, "large_temple_neptune"},
    {BUILDING_LARGE_TEMPLE_VENUS, "large_temple_venus"},
    {BUILDING_LIBRARY, "library"},
    {BUILDING_LIGHTHOUSE, "lighthouse"},
    {BUILDING_LION_HOUSE, "lion_house"},
    {BUILDING_MARKET, "market"},
    {BUILDING_OIL_WORKSHOP, "oil_workshop"},
    {BUILDING_OLIVE_FARM, "olive_farm"},
    {BUILDING_ORACLE, "oracle"},
    {BUILDING_PANTHEON, "pantheon"},
    {BUILDING_PIG_FARM, "pig_farm"},
    {BUILDING_POTTERY_WORKSHOP, "pottery_workshop"},
    {BUILDING_PREFECTURE, "prefecture"},
    {BUILDING_RESERVOIR, "reservoir"},
    {BUILDING_SCHOOL, "school"},
    {BUILDING_SENATE, "senate"},
    {BUILDING_SMALL_POND, "small_pond"},
    {BUILDING_SMALL_TEMPLE_CERES, "small_temple_ceres"},
    {BUILDING_SMALL_TEMPLE_MARS, "small_temple_mars"},
    {BUILDING_SMALL_TEMPLE_MERCURY, "small_temple_mercury"},
    {BUILDING_SMALL_TEMPLE_NEPTUNE, "small_temple_neptune"},
    {BUILDING_SMALL_TEMPLE_VENUS, "small_temple_venus"},
    {BUILDING_TAVERN, "tavern"},
    {BUILDING_LEGACY_SLOT_THEATER, BUILDING_TEXT_ID_THEATER},
    {BUILDING_VEGETABLE_FARM, "vegetable_farm"},
    {BUILDING_VINES_FARM, "vines_farm"},
    {BUILDING_WATCHTOWER, "watchtower"},
    {BUILDING_WEAPONS_WORKSHOP, "weapons_workshop"},
    {BUILDING_LEGACY_SLOT_WELL, BUILDING_TEXT_ID_WELL},
    {BUILDING_WHEAT_FARM, "wheat_farm"},
    {BUILDING_WINE_WORKSHOP, "wine_workshop"},
    {BUILDING_WORKCAMP, "workcamp"}
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
    g_legacy_text_ids[BUILDING_FORT_GROUND] = "fort_ground";
    g_legacy_text_ids[BUILDING_DISTRIBUTION_CENTER_UNUSED] = "distribution_center_unused";
    g_legacy_text_ids[BUILDING_WAREHOUSE_SPACE] = "warehouse_space";
    g_legacy_text_ids[BUILDING_BURNING_RUIN] = "burning_ruin";
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
