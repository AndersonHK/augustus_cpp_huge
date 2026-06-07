#include "building/building_record.h"
#include "building/building_type_legacy_migration.h"

#include <array>
#include <string>
#include <string_view>

namespace {

std::array<std::string, BUILDING_TYPE_MAX> g_legacy_text_ids;
bool g_legacy_text_ids_ready = false;

struct LegacyBuildingTypeTextId {
    uint16_t legacy_type;
    const char *text_id;
};

constexpr LegacyBuildingTypeTextId LEGACY_BUILDING_TYPE_TEXT_IDS[] = {
    {2, "farms"},
    {3, "raw_materials"},
    {4, "workshops"},
    {5, "road"},
    {6, "wall"},
    {7, "draggable_reservoir"},
    {8, "aqueduct"},
    {9, "clear_land"},
    {10, "vacant_lot"},
    {11, "house_large_tent"},
    {12, "house_small_shack"},
    {13, "house_large_shack"},
    {14, "house_small_hovel"},
    {15, "house_large_hovel"},
    {16, "house_small_casa"},
    {17, "house_large_casa"},
    {18, "house_small_insula"},
    {19, "house_medium_insula"},
    {20, "house_large_insula"},
    {21, "house_grand_insula"},
    {22, "house_small_villa"},
    {23, "house_medium_villa"},
    {24, "house_large_villa"},
    {25, "house_grand_villa"},
    {26, "house_small_palace"},
    {27, "house_medium_palace"},
    {28, "house_large_palace"},
    {29, "house_luxury_palace"},
    {30, "amphitheater"},
    {31, "theater"},
    {32, "hippodrome"},
    {33, "colosseum"},
    {34, "gladiator_school"},
    {35, "lion_house"},
    {36, "actor_colony"},
    {37, "chariot_maker"},
    {38, "plaza"},
    {39, "gardens"},
    {40, "fort_legionaries"},
    {41, "small_statue"},
    {42, "medium_statue"},
    {43, "large_statue"},
    {44, "fort_javelin"},
    {45, "fort_mounted"},
    {46, "doctor"},
    {47, "hospital"},
    {48, "bathhouse"},
    {49, "barber"},
    {50, "distribution_center_unused"},
    {51, "school"},
    {52, "academy"},
    {53, "library"},
    {54, "fort_ground"},
    {55, "prefecture"},
    {56, "triumphal_arch"},
    {57, "fort"},
    {58, "gatehouse"},
    {59, "tower"},
    {60, "small_temple_ceres"},
    {61, "small_temple_neptune"},
    {62, "small_temple_mercury"},
    {63, "small_temple_mars"},
    {64, "small_temple_venus"},
    {65, "large_temple_ceres"},
    {66, "large_temple_neptune"},
    {67, "large_temple_mercury"},
    {68, "large_temple_mars"},
    {69, "large_temple_venus"},
    {70, "market"},
    {71, "granary"},
    {72, "warehouse"},
    {73, "warehouse_space"},
    {74, "shipyard"},
    {75, "dock"},
    {76, "wharf"},
    {77, "governors_house"},
    {78, "governors_villa"},
    {79, "governors_palace"},
    {80, "mission_post"},
    {81, "engineers_post"},
    {82, "low_bridge"},
    {83, "ship_bridge"},
    {84, "senate_1_unused"},
    {85, "senate"},
    {86, "forum"},
    {87, "forum_2_unused"},
    {88, "native_hut"},
    {89, "native_meeting"},
    {90, "reservoir"},
    {91, "fountain"},
    {92, "well"},
    {93, "native_crops"},
    {94, "military_academy"},
    {95, "barracks"},
    {96, "small_temples"},
    {97, "large_temples"},
    {98, "oracle"},
    {99, "burning_ruin"},
    {100, "wheat_farm"},
    {101, "vegetable_farm"},
    {102, "fruit_farm"},
    {103, "olive_farm"},
    {104, "vines_farm"},
    {105, "pig_farm"},
    {106, "marble_quarry"},
    {107, "iron_mine"},
    {108, "timber_yard"},
    {109, "clay_pit"},
    {110, "wine_workshop"},
    {111, "oil_workshop"},
    {112, "weapons_workshop"},
    {113, "furniture_workshop"},
    {114, "pottery_workshop"},
    {115, "roadblock"},
    {116, "workcamp"},
    {117, "grand_temple_ceres"},
    {118, "grand_temple_neptune"},
    {119, "grand_temple_mercury"},
    {120, "grand_temple_mars"},
    {121, "grand_temple_venus"},
    {122, "grand_temples"},
    {123, "trees"},
    {124, "paths"},
    {125, "parks"},
    {126, "small_pond"},
    {127, "large_pond"},
    {128, "pine_tree"},
    {129, "fir_tree"},
    {130, "oak_tree"},
    {131, "elm_tree"},
    {132, "fig_tree"},
    {133, "plum_tree"},
    {134, "palm_tree"},
    {135, "date_tree"},
    {136, "pine_path"},
    {137, "fir_path"},
    {138, "oak_path"},
    {139, "elm_path"},
    {140, "fig_path"},
    {141, "plum_path"},
    {142, "palm_path"},
    {143, "date_path"},
    {144, "pavilion_blue"},
    {145, "pavilion_red"},
    {146, "pavilion_orange"},
    {147, "pavilion_yellow"},
    {148, "pavilion_green"},
    {149, "goddess_statue"},
    {150, "senator_statue"},
    {151, "obelisk"},
    {152, "pantheon"},
    {153, "architect_guild"},
    {154, "mess_hall"},
    {155, "lighthouse"},
    {156, "statues"},
    {157, "governor_home"},
    {158, "tavern"},
    {159, "grand_garden"},
    {160, "arena"},
    {161, "horse_statue"},
    {162, "dolphin_fountain"},
    {163, "hedge_dark"},
    {164, "hedge_light"},
    {165, "looped_garden_wall"},
    {166, "legion_statue"},
    {167, "decorative_column"},
    {168, "colonnade"},
    {169, "lararium"},
    {170, "nymphaeum"},
    {171, "small_mausoleum"},
    {172, "large_mausoleum"},
    {173, "watchtower"},
    {174, "palisade"},
    {175, "garden_path"},
    {176, "caravanserai"},
    {177, "roofed_garden_wall"},
    {178, "garden_wall_gate"},
    {179, "hedge_gate_dark"},
    {180, "hedge_gate_light"},
    {181, "palisade_gate"},
    {182, "gladiator_statue"},
    {183, "highway"},
    {184, "gold_mine"},
    {185, "city_mint"},
    {186, "cart_depot"},
    {187, "sand_pit"},
    {188, "stone_quarry"},
    {189, "concrete_maker"},
    {190, "brickworks"},
    {191, "panelled_garden_wall"},
    {192, "panelled_garden_gate"},
    {193, "looped_garden_gate"},
    {194, "shrine_ceres"},
    {195, "shrine_neptune"},
    {196, "shrine_mercury"},
    {197, "shrine_mars"},
    {198, "shrine_venus"},
    {199, "shrines"},
    {200, "all_gardens"},
    {201, "overgrown_gardens"},
    {202, "fort_swords"},
    {203, "armoury"},
    {204, "fort_archers"},
    {205, "latrines"},
    {206, "native_hut_alt"},
    {207, "native_watchtower"},
    {208, "native_monument"},
    {209, "native_decor"},
    {210, "repair_land"},
    {211, "clear_trees"},
};

void build_legacy_text_ids()
{
    if (g_legacy_text_ids_ready) {
        return;
    }

    for (const LegacyBuildingTypeTextId &mapping : LEGACY_BUILDING_TYPE_TEXT_IDS) {
        g_legacy_text_ids[mapping.legacy_type] = mapping.text_id;
    }
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
    for (const LegacyBuildingTypeTextId &mapping : LEGACY_BUILDING_TYPE_TEXT_IDS) {
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
    for (const LegacyBuildingTypeTextId &mapping : LEGACY_BUILDING_TYPE_TEXT_IDS) {
        if (requested_id == mapping.text_id) {
            return 1;
        }
    }
    return 0;
}
