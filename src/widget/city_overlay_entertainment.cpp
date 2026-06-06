#include "translation/translation.h"
#include "city_overlay_entertainment.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "game/state.h"

static int type_matches(building_type type, const char *text_id)
{
    return type == building_type_registry_runtime_id_from_text(text_id);
}

static int show_building_entertainment(const building *b)
{
    return
        type_matches(b->type, "actor_colony") || building_type_registry_is_theater(b->type) ||
        type_matches(b->type, "gladiator_school") || type_matches(b->type, "amphitheater") ||
        type_matches(b->type, "lion_house") || type_matches(b->type, "colosseum") ||
        type_matches(b->type, "chariot_maker") || type_matches(b->type, "hippodrome") ||
        type_matches(b->type, "tavern") || type_matches(b->type, "arena");
}

static int show_building_theater(const building *b)
{
    return type_matches(b->type, "actor_colony") || building_type_registry_is_theater(b->type);
}

static int show_building_amphitheater(const building *b)
{
    return type_matches(b->type, "actor_colony")
        || type_matches(b->type, "gladiator_school")
        || type_matches(b->type, "amphitheater");
}

static int show_building_arena(const building *b)
{
    return type_matches(b->type, "gladiator_school") || type_matches(b->type, "lion_house") ||
        type_matches(b->type, "arena");
}

static int show_building_colosseum(const building *b)
{
    return type_matches(b->type, "gladiator_school") || type_matches(b->type, "lion_house") ||
        type_matches(b->type, "colosseum");
}

static int show_building_hippodrome(const building *b)
{
    return type_matches(b->type, "chariot_maker") || type_matches(b->type, "hippodrome");
}

static int show_building_tavern(const building *b)
{
    return type_matches(b->type, "tavern");
}

static building *get_entertainment_building(const figure *f)
{
    if (f->action_state == FIGURE_ACTION_94_ENTERTAINER_ROAMING ||
        f->action_state == FIGURE_ACTION_95_ENTERTAINER_RETURNING) {
        return building_get(f->building_id);
    } else {
        return building_get(f->destination_building_id);
    }
}

static int show_figure_entertainment(const figure *f)
{
    return f->type == FIGURE_ACTOR || f->type == FIGURE_GLADIATOR ||
        f->type == FIGURE_LION_TAMER || f->type == FIGURE_CHARIOTEER || f->type == FIGURE_BARKEEP_SUPPLIER || f->type == FIGURE_BARKEEP;
}

static int show_figure_theater(const figure *f)
{
    if (f->type == FIGURE_ACTOR) {
        return building_type_registry_is_theater(get_entertainment_building(f)->type);
    }
    return 0;
}

static int show_figure_amphitheater(const figure *f)
{
    if (f->type == FIGURE_ACTOR || f->type == FIGURE_GLADIATOR) {
        return type_matches(get_entertainment_building(f)->type, "amphitheater");
    }
    return 0;
}

static int show_figure_arena(const figure *f)
{
    if (f->type == FIGURE_GLADIATOR || f->type == FIGURE_LION_TAMER) {
        return type_matches(get_entertainment_building(f)->type, "arena");
    } 
    return 0;
}

static int show_figure_colosseum(const figure *f)
{
    if (f->type == FIGURE_GLADIATOR || f->type == FIGURE_LION_TAMER) {
        return type_matches(get_entertainment_building(f)->type, "colosseum");
    }
    return 0;
}

static int show_figure_hippodrome(const figure *f)
{
    return f->type == FIGURE_CHARIOTEER;
}

static int show_figure_tavern(const figure *f)
{
    return f->type == FIGURE_BARKEEP || f->type == FIGURE_BARKEEP_SUPPLIER;
}

static int get_column_height_entertainment(const building *b)
{
    return b->house_size && b->data.house.entertainment ? b->data.house.entertainment / 10 : NO_COLUMN;
}

static int get_column_height_theater(const building *b)
{
    return b->house_size && b->data.house.theater ? b->data.house.theater / 10 : NO_COLUMN;
}

static int get_column_height_amphitheater(const building *b)
{
    return b->house_size && b->data.house.amphitheater_actor ? b->data.house.amphitheater_actor / 10 : NO_COLUMN;
}

static int get_column_height_arena(const building *b)
{
    return b->house_size && b->house_arena_gladiator ? b->house_arena_gladiator / 10 : NO_COLUMN;
}

static int get_column_height_colosseum(const building *b)
{
    return b->house_size && b->data.house.colosseum_gladiator ? b->data.house.colosseum_gladiator / 10 : NO_COLUMN;
}

static int get_column_height_hippodrome(const building *b)
{
    return b->house_size && b->data.house.hippodrome ? b->data.house.hippodrome / 10 : NO_COLUMN;
}

static int get_column_height_tavern(const building *b)
{
    if (!b->house_size || !b->house_tavern_wine_access) {
        return NO_COLUMN;
    }
    return (((b->house_tavern_wine_access / 10) * 2) + (b->house_tavern_food_access / 10)) / 3;
}

static int get_tooltip_entertainment(tooltip_context *c, const building *b)
{
    if (b->data.house.entertainment <= 0) {
        return 64;
    } else if (b->data.house.entertainment < 10) {
        return 65;
    } else if (b->data.house.entertainment < 20) {
        return 66;
    } else if (b->data.house.entertainment < 30) {
        return 67;
    } else if (b->data.house.entertainment < 40) {
        return 68;
    } else if (b->data.house.entertainment < 50) {
        return 69;
    } else if (b->data.house.entertainment < 60) {
        return 70;
    } else if (b->data.house.entertainment < 70) {
        return 71;
    } else if (b->data.house.entertainment < 80) {
        return 72;
    } else if (b->data.house.entertainment < 90) {
        return 73;
    } else {
        return 74;
    }
}

static int get_tooltip_theater(tooltip_context *c, const building *b)
{
    if (b->data.house.theater <= 0) {
        return 75;
    } else if (b->data.house.theater >= 80) {
        return 76;
    } else if (b->data.house.theater >= 20) {
        return 77;
    } else {
        return 78;
    }
}

static int get_tooltip_amphitheater(tooltip_context *c, const building *b)
{
    if (b->data.house.amphitheater_actor <= 0) {
        return 79;
    } else if (b->data.house.amphitheater_actor >= 80) {
        return 80;
    } else if (b->data.house.amphitheater_actor >= 20) {
        return 81;
    } else {
        return 82;
    }
}

static int get_tooltip_colosseum(tooltip_context *c, const building *b)
{
    if (b->data.house.colosseum_gladiator && b->data.house.colosseum_lion) {
        c->translation_key = TR_TOOLTIP_OVERLAY_ARENA_COL_5;
    } else if (b->data.house.colosseum_gladiator) {
        c->translation_key = TR_TOOLTIP_OVERLAY_ARENA_COL_4;
    } else if (b->house_arena_gladiator && b->house_arena_lion) {
        c->translation_key = TR_TOOLTIP_OVERLAY_ARENA_COL_3;
    } else if (b->house_arena_gladiator) {
        c->translation_key = TR_TOOLTIP_OVERLAY_ARENA_COL_2;
    } else {
        c->translation_key = TR_TOOLTIP_OVERLAY_ARENA_COL_1;
    }
    return 0;
}

static int get_tooltip_hippodrome(tooltip_context *c, const building *b)
{
    if (b->data.house.hippodrome <= 0) {
        return 87;
    } else if (b->data.house.hippodrome >= 80) {
        return 88;
    } else if (b->data.house.hippodrome >= 20) {
        return 89;
    } else {
        return 90;
    }
}

static int get_tooltip_tavern(tooltip_context *c, const building *b)
{
    if (b->house_tavern_wine_access <= 0) {
        c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_1;
    } else if (b->house_tavern_wine_access <= 20) {
        c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_2;
    } else if (b->data.house.hippodrome <= 80) {
        if (!b->house_tavern_food_access) {
            c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_3;
        } else {
            c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_4;
        }
    } else {
        if (!b->house_tavern_food_access) {
            c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_5;
        } else {
            c->translation_key = TR_TOOLTIP_OVERLAY_TAVERN_6;
        }
    }

    return 0;
}

const city_overlay *city_overlay_for_entertainment(void)
{
    static city_overlay overlay = {
        OVERLAY_ENTERTAINMENT,
        COLUMN_COLOR_GREEN,
        show_building_entertainment,
        show_figure_entertainment,
        get_column_height_entertainment,
        0,
        get_tooltip_entertainment,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_theater(void)
{
    static city_overlay overlay = {
        OVERLAY_THEATER,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_theater,
        show_figure_theater,
        get_column_height_theater,
        0,
        get_tooltip_theater,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_amphitheater(void)
{
    static city_overlay overlay = {
        OVERLAY_AMPHITHEATER,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_amphitheater,
        show_figure_amphitheater,
        get_column_height_amphitheater,
        0,
        get_tooltip_amphitheater,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_arena(void)
{
    static city_overlay overlay = {
        OVERLAY_ARENA,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_arena,
        show_figure_arena,
        get_column_height_arena,
        0,
        get_tooltip_colosseum,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_colosseum(void)
{
    static city_overlay overlay = {
        OVERLAY_COLOSSEUM,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_colosseum,
        show_figure_colosseum,
        get_column_height_colosseum,
        0,
        get_tooltip_colosseum,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_hippodrome(void)
{
    static city_overlay overlay = {
        OVERLAY_HIPPODROME,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_hippodrome,
        show_figure_hippodrome,
        get_column_height_hippodrome,
        0,
        get_tooltip_hippodrome,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_tavern(void)
{
    static city_overlay overlay = {
        OVERLAY_TAVERN,
        COLUMN_COLOR_GREEN_TO_RED,
        show_building_tavern,
        show_figure_tavern,
        get_column_height_tavern,
        0,
        get_tooltip_tavern,
        0,
        0
    };
    return &overlay;
}
