#include "building/building_record.h"
#include "entertainment.h"

#include "building/building.h"
#include "building/monument.h"

#include <string_view>

static int is_show_venue(const Building &building)
{
    const building_type_registry_impl::BuildingType *type = building.type;
    if (!type) {
        return 0;
    }
    const std::string_view attr(type->attr());
    return attr == "theater" || attr == "amphitheater" || attr == "arena" ||
        attr == "colosseum" || attr == "hippodrome";
}

void building_entertainment_run_shows(void)
{
    for (int i = 1; i < building_count(); i++) {
        building *b = building_get(i);
        if (!b || b->state != BUILDING_STATE_IN_USE) {
            continue;
        }
        Building venue(b);
        if (!is_show_venue(venue)) {
            continue;
        }
        if (building_monument_is_monument(b) && b->monument.phase != MONUMENT_FINISHED) {
            b->data.entertainment.num_shows = 0;
            continue;
        }

        int shows = 0;
        if (b->data.entertainment.days1 > 0) {
            --b->data.entertainment.days1;
            ++shows;
        }
        if (b->data.entertainment.days2 > 0) {
            --b->data.entertainment.days2;
            ++shows;
        }
        b->data.entertainment.num_shows = shows;
    }
}
