#include "entertainment.h"

#include "building/building.h"
#include "building/building_type_api.h"
#include "building/monument.h"

static const building_type ENTERTAINMENT_BUILDINGS[] = {
    BUILDING_AMPHITHEATER,
    BUILDING_ARENA,
    BUILDING_COLOSSEUM,
    BUILDING_HIPPODROME
};

#define NUM_ENTERTAINMENT_BUILDINGS (sizeof(ENTERTAINMENT_BUILDINGS) / sizeof(building_type))

static void run_shows_for_type(building_type type)
{
    if (type <= BUILDING_NONE) {
        return;
    }
    for (building *b = building_first_of_type(type); b; b = b->next_of_type) {
        if (building_monument_is_monument(b) && b->monument.phase != MONUMENT_FINISHED) {
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

void building_entertainment_run_shows(void)
{
    run_shows_for_type(BUILDING_THEATER);
    for (size_t i = 0; i < NUM_ENTERTAINMENT_BUILDINGS; i++) {
        run_shows_for_type(ENTERTAINMENT_BUILDINGS[i]);
    }
}
