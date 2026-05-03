#include "building/building_type_legacy_migration.h"

extern "C" {
#include "building/properties.h"
}

#include <array>
#include <cctype>
#include <string>

namespace {

std::array<std::string, BUILDING_TYPE_MAX> g_legacy_text_ids;
bool g_legacy_text_ids_ready = false;

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
