#include "building/industry.h"
#include "game/state.h"
#include "graphics/image.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/image.h"
#include "widget/city_draw.h"
#include "widget/city_draw_highway.h"

#include "city_overlay_risks.h"

#include "translation/translation.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "figure/figure.h"

#include "core/config.h"
#include "map/property.h"
#include "map/random.h"
#include "map/terrain.h"

enum crime_level {
    NO_CRIME = 0,
    MINOR_CRIME = 1,
    LOW_CRIME = 2,
    SOME_CRIME = 3,
    MEDIUM_CRIME = 4,
    LARGE_CRIME = 5,
    RAMPANT_CRIME = 6,
};

static int is_problem_cartpusher(int figure_id)
{
    if (figure_id) {
        Figure *fig = Figure::get(figure_id);
        return fig->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL && fig->min_max_seen;
    } else {
        return 0;
    }
}

void city_overlay_problems_prepare_building(building *b)
{
    Building *building = b && map_building_exists_at(b->grid_offset) ? &map_building_at(b->grid_offset).main() : nullptr;
    if (!building) {
        return;
    }
    b = const_cast<::building *>(building->record());
    const auto *type = building->type;

    if (b->strike_duration_days > 0) {
        b->show_on_problem_overlay = 1;
        return;
    }

    if (b->has_plague) {
        b->show_on_problem_overlay = 1;
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        b->show_on_problem_overlay = 1;
    } else if (!b->num_workers && type && static_cast<bool>(type->required_workers())) {
        b->show_on_problem_overlay = 1;
    } else if (type && type->water_access().has_requirements() && !b->has_water_access) {
        b->show_on_problem_overlay = 1;
    } else if (type->is_farm() || building_is_raw_resource_producer(type)) {
        if (is_problem_cartpusher(b->figure_id)) {
            b->show_on_problem_overlay = 1;
        }
    } else if (building_is_workshop(type)) {
        if (is_problem_cartpusher(b->figure_id)) {
            b->show_on_problem_overlay = 1;
        } else if (!building->native_production_has_raw_materials()) {
            b->show_on_problem_overlay = 1;
        }
    } else if (((type && type->is_theater()) || building_type_registry_impl::type_attr_is_any(
            b->type, {"amphitheater", "arena", "colosseum", "hippodrome"})) &&
        !b->data.entertainment.days1) {
        b->show_on_problem_overlay = 1;
    } else if (building_type_registry_impl::type_attr_is_any(b->type, {"arena", "colosseum"}) &&
        !b->data.entertainment.days2) {
        b->show_on_problem_overlay = 1;
    } else if (building_type_registry_impl::type_attr_is(b->type, "cart_depot") &&
        (!b->data.depot.current_order.src_storage_id ||
         !b->data.depot.current_order.dst_storage_id)) {
        b->show_on_problem_overlay = 1;
    } else if (b->has_road_access == 0 &&
        type && static_cast<bool>(type->required_workers()) && !type->is_latrines() && !type->is_fountain()) {
        b->show_on_problem_overlay = 1;
    }

    if (b->show_on_problem_overlay) {
        building->for_each_part([](Building part) {
            ::building *record = const_cast<::building *>(part.record());
            if (record) {
                record->show_on_problem_overlay = 1;
            }
        });
    }
}

static int show_building_fire_crime(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(b->type, {"prefecture", "burning_ruin"});
}

static int show_building_damage(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(b->type, {"engineers_post", "architect_guild"});
}

static int show_building_problems(const building *b)
{
    return b->show_on_problem_overlay;
}

static int show_building_native(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(
            b->type, {"native_hut", "native_hut_alt", "native_meeting", "mission_post"}) ||
        building_type_registry_impl::type_attr_is_any(
            b->type, {"native_crops", "native_decor", "native_monument", "native_watchtower"});
}

static int draw_footprint_enemy(int x, int y, float scale, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 0;
    }
    int drawn = 0;
    // 1. If there is a highway, draw it
    if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY) &&
        !map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
        city_draw_highway_footprint(x, y, scale, grid_offset, COLOR_MASK_NONE);
        drawn = 1;
    }
    // 2. On top: an aqueduct / wall
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
        Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        drawn = 1;
    }
    // If nothing was drawn, let the engine draw the ground itself
    return drawn;
}

static int draw_top_enemy(int x, int y, float scale, int grid_offset)
{
    if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
        Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, 0, scale);
        return 1;
    }
    return 0;
}

static int show_building_enemy(const building *b)
{
    return building_type_registry_impl::type_attr_is_any(
            b->type, {"prefecture", "watchtower", "tower", "fort_ground"}) ||
        building_is_fort(b->type) ||
        building_type_registry_impl::type_attr_is_any(
            b->type, {"barracks", "military_academy", "gatehouse", "palisade_gate"}) ||
        building_type_registry_impl::type_attr_is(b->type, "palisade");
}

static int show_figure_fire(const Figure *f)
{
    return f->type == FIGURE_PREFECT;
}

static int show_figure_damage(const Figure *f)
{
    return f->type == FIGURE_ENGINEER || f->type == FIGURE_WORK_CAMP_ARCHITECT;
}

static int show_figure_crime(const Figure *f)
{
    return f->is_category(FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_CRIMINAL | FIGURE_CATEGORY_PROJECTILE);
}

static int show_figure_problems(const Figure *f)
{
    if (f->type == FIGURE_LABOR_SEEKER) {
        const building *owner = f->building ? f->building->record() : nullptr;
        return owner && owner->show_on_problem_overlay;
    } else if (f->type == FIGURE_CART_PUSHER) {
        return f->action_state == FIGURE_ACTION_20_CARTPUSHER_INITIAL || f->min_max_seen;
    } else if (f->type == FIGURE_PROTESTER || f->type == FIGURE_BEGGAR) {
        return 1;
    } else {
        return 0;
    }
}

static int show_figure_native(const Figure *f)
{
    return f->type == FIGURE_INDIGENOUS_NATIVE || f->type == FIGURE_MISSIONARY ||
        f->type == FIGURE_NATIVE_TRADER;
}

static int show_figure_enemy(const Figure *f)
{
    return f->is_category(FIGURE_CATEGORY_HOSTILE | FIGURE_CATEGORY_AGGRESSIVE_ANIMAL |
            FIGURE_CATEGORY_ARMED | FIGURE_CATEGORY_PROJECTILE)
        || f->type == FIGURE_FORT_STANDARD;
}

static int get_column_height_fire(const building *b)
{
    return b->fire_risk > 0 ? b->fire_risk / 10 : NO_COLUMN;
}

static int get_column_height_damage(const building *b)
{
    return b->damage_risk > 0 ? b->damage_risk / 20 : NO_COLUMN;
}

static int get_crime_level(const building *b)
{
    if (b->house_size) {
        int happiness = b->sentiment.house_happiness;
        if (happiness <= 0) {
            return RAMPANT_CRIME;
        } else if (happiness <= 10 || b->house_criminal_active) {
            return LARGE_CRIME;
        } else if (happiness <= 20) {
            return MEDIUM_CRIME;
        } else if (happiness <= 30) {
            return SOME_CRIME;
        } else if (happiness <= 40) {
            return LOW_CRIME;
        } else if (happiness < 50) {
            return MINOR_CRIME;
        }
    }
    return 0;

}

static int get_column_height_crime(const building *b)
{
    if (b->house_size) {
        int crime = get_crime_level(b);
        if (crime == RAMPANT_CRIME) {
            return 10;
        } else if (crime == LARGE_CRIME) {
            return 8;
        } else if (crime == MEDIUM_CRIME) {
            return 6;
        } else if (crime == SOME_CRIME) {
            return 4;
        } else if (crime == LOW_CRIME) {
            return 2;
        } else if (crime == MINOR_CRIME) {
            return 1;
        }
    }
    return NO_COLUMN;
}

static int get_column_height_none(const building *b)
{
    (void) b;
    return NO_COLUMN;
}

static int get_tooltip_fire(tooltip_context *c, const building *b)
{
    (void) c;
    if (b->fire_risk <= 0) {
        return 46;
    } else if (b->fire_risk <= 20) {
        return 47;
    } else if (b->fire_risk <= 40) {
        return 48;
    } else if (b->fire_risk <= 60) {
        return 49;
    } else if (b->fire_risk <= 80) {
        return 50;
    } else {
        return 51;
    }
}

static int get_tooltip_damage(tooltip_context *c, const building *b)
{
    (void) c;
    if (b->damage_risk <= 0) {
        return 52;
    } else if (b->damage_risk <= 40) {
        return 53;
    } else if (b->damage_risk <= 80) {
        return 54;
    } else if (b->damage_risk <= 120) {
        return 55;
    } else if (b->damage_risk <= 160) {
        return 56;
    } else {
        return 57;
    }
}

static int get_tooltip_crime(tooltip_context *c, const building *b)
{
    (void) c;
    int crime = get_crime_level(b);
    if (crime == RAMPANT_CRIME) {
        return 63;
    } else if (crime == LARGE_CRIME) {
        return 62;
    } else if (crime == MEDIUM_CRIME) {
        return 61;
    } else if (crime == SOME_CRIME) {
        return 60;
    } else if (crime == LOW_CRIME || crime == MINOR_CRIME) {
        return 59;
    } else {
        return 58;
    }
}

static int get_tooltip_problems(tooltip_context *c, const building *b)
{
    Building *building = b && map_building_exists_at(b->grid_offset) ? &map_building_at(b->grid_offset).main() : nullptr;
    if (building) {
        if (const ::building *main_record = building->record()) {
            b = main_record;
        }
    }
    const auto *type = building ? building->type : nullptr;

    if (b->has_plague) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_PLAGUE";
    }
    if (b->strike_duration_days > 0) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_STRIKE";
    } else if (b->state == BUILDING_STATE_MOTHBALLED) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_MOTHBALLED";
    } else if (!b->num_workers && type && static_cast<bool>(type->required_workers())) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_NO_LABOR";
    } else if (type && type->water_access().has_requirements() && !b->has_water_access) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_NO_WATER_ACCESS";
    } else if (type && (type->is_farm() || building_is_raw_resource_producer(type))) {
        if (is_problem_cartpusher(b->figure_id)) {
            c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_CARTPUSHER";
        }
    } else if (building_is_workshop(type)) {
        if (is_problem_cartpusher(b->figure_id)) {
            c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_CARTPUSHER";
        } else if (building && !building->native_production_has_raw_materials()) {
            c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_NO_RESOURCES";
        }
    } else if (type && type->is_theater() && !b->data.entertainment.days1) {
        c->text_group = 72;
        return 5;
    } else if (building_type_registry_impl::type_attr_is(b->type, "amphitheater")) {
        if (!b->data.entertainment.days1) {
            c->text_group = 71;
            return 7;
        } else if (!b->data.entertainment.days2) {
            c->text_group = 71;
            return 9;
        }
    } else if (building_type_registry_impl::type_attr_is_any(b->type, {"arena", "colosseum"})) {
        if (!b->data.entertainment.days1) {
            c->text_group = 74;
            return 7;
        } else if (!b->data.entertainment.days2) {
            c->text_group = 74;
            return 9;
        }
    } else if (building_type_registry_impl::type_attr_is(b->type, "hippodrome") && !b->data.entertainment.days1) {
        c->text_group = 73;
        return 5;
    } else if (building_type_registry_impl::type_attr_is(b->type, "cart_depot") &&
        (!b->data.depot.current_order.src_storage_id ||
         !b->data.depot.current_order.dst_storage_id)) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_DEPOT_NO_INSTRUCTIONS";
    } else if (b->has_road_access == 0 &&
        type && static_cast<bool>(type->required_workers()) && !type->is_latrines() && !type->is_fountain()) {
        c->translation_key = "TR_TOOLTIP_OVERLAY_PROBLEMS_NO_ROAD_ACCESS";
    }
    if (c->translation_key) {
        return 1;
    }
    return 0;
}

const city_overlay *city_overlay_for_fire(void)
{
    static city_overlay overlay = {
        OVERLAY_FIRE,
        COLUMN_COLOR_RED_TO_GREEN,
        show_building_fire_crime,
        show_figure_fire,
        get_column_height_fire,
        0,
        get_tooltip_fire,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_damage(void)
{
    static city_overlay overlay = {
        OVERLAY_DAMAGE,
        COLUMN_COLOR_RED_TO_GREEN,
        show_building_damage,
        show_figure_damage,
        get_column_height_damage,
        0,
        get_tooltip_damage,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_crime(void)
{
    static city_overlay overlay = {
        OVERLAY_CRIME,
        COLUMN_COLOR_RED_TO_GREEN,
        show_building_fire_crime,
        show_figure_crime,
        get_column_height_crime,
        0,
        get_tooltip_crime,
        0,
        0
    };
    return &overlay;
}

const city_overlay *city_overlay_for_problems(void)
{
    static city_overlay overlay = {
        OVERLAY_PROBLEMS,
        COLUMN_COLOR_RED,
        show_building_problems,
        show_figure_problems,
        get_column_height_none,
        0,
        get_tooltip_problems,
        0,
        0
    };
    return &overlay;
}

static int terrain_on_native_overlay(void)
{
    return
        TERRAIN_TREE | TERRAIN_ROCK | TERRAIN_WATER | TERRAIN_SHRUB |
        TERRAIN_GARDEN | TERRAIN_ELEVATION | TERRAIN_ACCESS_RAMP | TERRAIN_RUBBLE;
}

static int draw_footprint_native(int x, int y, float scale, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 1;
    }
    if (map_is_bridge(grid_offset)) {
        int water_image = map_image_at(grid_offset);  // Get the water image for the bridge
        if (!water_image) {
            water_image = Image::group(GROUP_TERRAIN_WATER);  // fallback - first image in water group
        }
        Image::from_id(water_image).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
    }
    if (map_terrain_is(grid_offset, terrain_on_native_overlay())) {
        if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
            city_with_overlay_draw_building_footprint(x, y, grid_offset, 0);
        } else {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT | TERRAIN_WALL)) {
        //display flattened building tile 
        int image_id = Image::group(GROUP_TERRAIN_OVERLAY);
        Image::from_id(image_id).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
    } else if (map_terrain_is(grid_offset, TERRAIN_BUILDING)) {
        city_with_overlay_draw_building_footprint(x, y, grid_offset, 0);
    } else {
        if (map_property_is_native_land(grid_offset)) {
            Image::from_id(Image::group(GROUP_TERRAIN_DESIRABILITY) + 1).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        } else if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY) && !map_terrain_is(grid_offset, TERRAIN_GATEHOUSE)) {
            city_draw_highway_footprint(x, y, scale, grid_offset, COLOR_MASK_NONE);
        } else {
            Image::from_id(map_image_at(grid_offset)).draw_isometric_footprint_from_draw_tile(x, y, 0, scale);
        }
    }
    if (config_get(CONFIG_UI_SHOW_GRID) && map_property_is_draw_tile(grid_offset)
                                        && !map_building_exists_at(grid_offset)) {
        city_draw_grid_overlay(x, y, scale);
    }
    return 1;
}

static int draw_top_native(int x, int y, float scale, int grid_offset)
{
    if (!map_property_is_draw_tile(grid_offset)) {
        return 1;
    }
    if (map_terrain_is(grid_offset, terrain_on_native_overlay())) {
        if (!map_terrain_is(grid_offset, TERRAIN_BUILDING) || map_is_bridge(grid_offset)) {
            color_t color_mask = 0;
            if (map_property_is_deleted(grid_offset) && map_property_multi_tile_size(grid_offset) == 1) {
                color_mask = COLOR_MASK_RED;
            }
            Image::from_id(map_image_at(grid_offset)).draw_isometric_top_from_draw_tile(x, y, color_mask, scale);
        }

    } else if (map_building_exists_at(grid_offset)) {
        city_with_overlay_draw_building_top(x, y, grid_offset);
    }
    return 1;
}

const city_overlay *city_overlay_for_native(void)
{
    static city_overlay overlay = {
        OVERLAY_NATIVE,
        COLUMN_COLOR_RED,
        show_building_native,
        show_figure_native,
        get_column_height_none,
        0,
        0,
        draw_footprint_native,
        draw_top_native
    };
    return &overlay;
}

const city_overlay *city_overlay_for_enemy(void)
{
    static city_overlay overlay = {
        OVERLAY_ENEMY,
        COLUMN_COLOR_RED,
        show_building_enemy,
        show_figure_enemy,
        get_column_height_none,
        0,
        0,
        draw_footprint_enemy,
        draw_top_enemy
    };
    return &overlay;
}
