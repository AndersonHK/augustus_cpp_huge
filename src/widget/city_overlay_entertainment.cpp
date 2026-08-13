#include "translation/translation.h"
#include "city_overlay_entertainment.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "game/state.h"
#include "map/building.h"

#include <exception>

static Building &runtime_building_for_overlay_record(const building *record)
{
    if (!record || !map_building_exists_at(record->grid_offset)) {
        std::terminate();
    }
    Building &tile_building = map_building_at(record->grid_offset);
    if (tile_building.id == record->id) {
        return tile_building;
    }
    Building *main_building = tile_building.Composition ? tile_building.Composition->owner() : &tile_building;
    if (main_building && main_building->id == record->id) {
        return *main_building;
    }
    std::terminate();
}

static const HousingState &housing_state_for_overlay_record(const building *record)
{
    static const HousingState empty;
    const Building &building = runtime_building_for_overlay_record(record);
    return building.Housing ? building.Housing->state() : empty;
}

static int show_building_entertainment(const building *b)
{
    const Building &building = runtime_building_for_overlay_record(b);
    const building_type_registry_impl::BuildingType *type = building.type;
    return type && (
        type->attr_is("actor_colony") || type->is_theater() ||
        type->attr_is("gladiator_school") || type->attr_is("amphitheater") ||
        type->attr_is("lion_house") || type->attr_is("colosseum") ||
        type->attr_is("chariot_maker") || type->attr_is("hippodrome") ||
        type->attr_is("tavern") || type->attr_is("arena"));
}

static int show_building_theater(const building *b)
{
    const Building &building = runtime_building_for_overlay_record(b);
    const building_type_registry_impl::BuildingType *type = building.type;
    return type && (type->attr_is("actor_colony") || type->is_theater());
}

static int show_building_amphitheater(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(
        b->type, {"actor_colony", "gladiator_school", "amphitheater"});
}

static int show_building_arena(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(b->type, {"gladiator_school", "lion_house", "arena"});
}

static int show_building_colosseum(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(b->type, {"gladiator_school", "lion_house", "colosseum"});
}

static int show_building_hippodrome(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(b->type, {"chariot_maker", "hippodrome"});
}

static int show_building_tavern(const building *b)
{
    return building_type_registry_impl::type_attr_is(b->type, "tavern");
}

static const Building *get_entertainment_building(const Figure &f)
{
    return f.action_state == FIGURE_ACTION_94_ENTERTAINER_ROAMING ||
        f.action_state == FIGURE_ACTION_95_ENTERTAINER_RETURNING ?
        f.building :
        f.destination_building;
}

static int show_figure_entertainment(const Figure *f)
{
    switch (f->type) {
        case FIGURE_ACTOR:
        case FIGURE_GLADIATOR:
        case FIGURE_LION_TAMER:
        case FIGURE_CHARIOTEER:
        case FIGURE_BARKEEP:
        case FIGURE_BARKEEP_SUPPLIER:
            return 1;
        default:
            return 0;
    }
}

static int show_figure_theater(const Figure *f)
{
    if (f->type == FIGURE_ACTOR) {
        const Building *building = get_entertainment_building(*f);
        return building && building->type && building->type->is_theater();
    }
    return 0;
}

static int show_figure_amphitheater(const Figure *f)
{
    if (f->type == FIGURE_ACTOR || f->type == FIGURE_GLADIATOR) {
        const Building *building = get_entertainment_building(*f);
        return building && building->type && building->type->attr_is("amphitheater");
    }
    return 0;
}

static int show_figure_arena(const Figure *f)
{
    if (f->type == FIGURE_GLADIATOR || f->type == FIGURE_LION_TAMER) {
        const Building *building = get_entertainment_building(*f);
        return building && building->type && building->type->attr_is("arena");
    } 
    return 0;
}

static int show_figure_colosseum(const Figure *f)
{
    if (f->type == FIGURE_GLADIATOR || f->type == FIGURE_LION_TAMER) {
        const Building *building = get_entertainment_building(*f);
        return building && building->type && building->type->attr_is("colosseum");
    }
    return 0;
}

static int show_figure_hippodrome(const Figure *f)
{
    return f->type == FIGURE_CHARIOTEER;
}

static int show_figure_tavern(const Figure *f)
{
    return f->type == FIGURE_BARKEEP || f->type == FIGURE_BARKEEP_SUPPLIER;
}

static int get_column_height_entertainment(const building *b)
{
    const auto &services = housing_state_for_overlay_record(b).services;
    return services.entertainment ? services.entertainment / 10 : NO_COLUMN;
}

static int get_column_height_theater(const building *b)
{
    const auto &services = housing_state_for_overlay_record(b).services;
    return services.theater ? services.theater / 10 : NO_COLUMN;
}

static int get_column_height_amphitheater(const building *b)
{
    const auto &services = housing_state_for_overlay_record(b).services;
    return services.amphitheater_actor ? services.amphitheater_actor / 10 : NO_COLUMN;
}

static int get_column_height_arena(const building *b)
{
    const auto &state = housing_state_for_overlay_record(b);
    return state.arena_gladiator ? state.arena_gladiator / 10 : NO_COLUMN;
}

static int get_column_height_colosseum(const building *b)
{
    const auto &services = housing_state_for_overlay_record(b).services;
    return services.colosseum_gladiator ? services.colosseum_gladiator / 10 : NO_COLUMN;
}

static int get_column_height_hippodrome(const building *b)
{
    const auto &services = housing_state_for_overlay_record(b).services;
    return services.hippodrome ? services.hippodrome / 10 : NO_COLUMN;
}

static int get_column_height_tavern(const building *b)
{
    const auto &state = housing_state_for_overlay_record(b);
    if (!state.tavern_wine_access) {
        return NO_COLUMN;
    }
    return (((state.tavern_wine_access / 10) * 2) + (state.tavern_food_access / 10)) / 3;
}

static int get_tooltip_entertainment(tooltip_context *c, const building *b)
{
    (void)c;
    const int entertainment = housing_state_for_overlay_record(b).services.entertainment;

    if (entertainment <= 0) {
        return 64;
    } else if (entertainment < 10) {
        return 65;
    } else if (entertainment < 20) {
        return 66;
    } else if (entertainment < 30) {
        return 67;
    } else if (entertainment < 40) {
        return 68;
    } else if (entertainment < 50) {
        return 69;
    } else if (entertainment < 60) {
        return 70;
    } else if (entertainment < 70) {
        return 71;
    } else if (entertainment < 80) {
        return 72;
    } else if (entertainment < 90) {
        return 73;
    } else {
        return 74;
    }
}

static int get_tooltip_theater(tooltip_context *c, const building *b)
{
    (void)c;
    const int theater = housing_state_for_overlay_record(b).services.theater;

    if (theater <= 0) {
        return 75;
    } else if (theater >= 80) {
        return 76;
    } else if (theater >= 20) {
        return 77;
    } else {
        return 78;
    }
}

static int get_tooltip_amphitheater(tooltip_context *c, const building *b)
{
    (void)c;
    const int amphitheater = housing_state_for_overlay_record(b).services.amphitheater_actor;

    if (amphitheater <= 0) {
        return 79;
    } else if (amphitheater >= 80) {
        return 80;
    } else if (amphitheater >= 20) {
        return 81;
    } else {
        return 82;
    }
}

static int get_tooltip_colosseum(tooltip_context *c, const building *b)
{
    const HousingState &state = housing_state_for_overlay_record(b);
    if (state.services.colosseum_gladiator && state.services.colosseum_lion) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_ARENA_COL_5";
    } else if (state.services.colosseum_gladiator) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_ARENA_COL_4";
    } else if (state.arena_gladiator && state.arena_lion) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_ARENA_COL_3";
    } else if (state.arena_gladiator) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_ARENA_COL_2";
    } else {
        c->translation_key = "TR_TOOLTIP_OVERLAY_ARENA_COL_1";
    }
    return 0;
}

static int get_tooltip_hippodrome(tooltip_context *c, const building *b)
{
    (void)c;
    const int hippodrome = housing_state_for_overlay_record(b).services.hippodrome;

    if (hippodrome <= 0) {
        return 87;
    } else if (hippodrome >= 80) {
        return 88;
    } else if (hippodrome >= 20) {
        return 89;
    } else {
        return 90;
    }
}

static int get_tooltip_tavern(tooltip_context *c, const building *b)
{
    const HousingState &state = housing_state_for_overlay_record(b);
    if (state.tavern_wine_access <= 0) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_1";
    } else if (state.tavern_wine_access <= 20) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_2";
    } else if (state.services.hippodrome <= 80) {
        if (!state.tavern_food_access) {
            c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_3";
        } else {
            c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_4";
        }
    } else {
        if (!state.tavern_food_access) {
            c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_5";
        } else {
            c->translation_key = "TR_TOOLTIP_OVERLAY_TAVERN_6";
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
