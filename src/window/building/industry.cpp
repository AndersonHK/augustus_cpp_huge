#include "building/building.h"
#include "building/count.h"
#include "building/industry.h"
#include "building/water_access_runtime.h"
#include "game/resource_graphics.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"

#include "industry.h"

#include "translation/translation.h"
#include "window/popup_dialog.h"
extern "C" {

#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/building_record.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/file.h"
#include "core/string.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/mouse.h"
#include "scenario/allowed_building.h"
#include "scenario/property.h"
}

#include "building/building_type_registry_internal.h"

#include <stdio.h>

static void set_city_mint_conversion(const generic_button *button);

static generic_button mint_conversion_buttons[] = {
    {16, 0, 20, 20, set_city_mint_conversion, 0, resource_denarii()},
    {16, 24, 20, 20, set_city_mint_conversion, 0, resource_gold()},
};

static struct {
    int city_mint_id;
    unsigned int focus_button_id;
} data;

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    return type == runtime_type(text_id);
}

struct industry_text {
    int legacy_group = 0;
    int legacy_offset = 0;
    const translation_key *keys = nullptr;
};

static industry_text legacy_industry_text(int group, int offset)
{
    return { group, offset, nullptr };
}

static industry_text named_industry_text(const translation_key *keys)
{
    return { 0, 0, keys };
}

static int industry_text_draw(const industry_text &text, int offset, int x, int y, font_t font, int pixel_size)
{
    return text.keys ? lang_text_draw(text.keys[offset], x, y, font, pixel_size) :
        lang_text_draw(text.legacy_group, text.legacy_offset + offset, x, y, font, pixel_size);
}

static int industry_text_get_width(const industry_text &text, int offset, font_t font, int pixel_size)
{
    return text.keys ? lang_text_get_width(text.keys[offset], font, pixel_size) :
        lang_text_get_width(text.legacy_group, text.legacy_offset + offset, font, pixel_size);
}

static void industry_text_draw_centered(
    const industry_text &text, int offset, int x, int y, int width, font_t font, int pixel_size)
{
    if (text.keys) {
        lang_text_draw_centered(text.keys[offset], x, y, width, font, pixel_size);
    } else {
        lang_text_draw_centered(text.legacy_group, text.legacy_offset + offset, x, y, width, font, pixel_size);
    }
}

static int industry_text_draw_description_at(
    building_info_context *c, int y_offset, const industry_text &text, int offset)
{
    return text.keys ? window_building_draw_description_at(c, y_offset, text.keys[offset]) :
        window_building_draw_description_at(c, y_offset, text.legacy_group, text.legacy_offset + offset);
}

static int building_type_requires_water_access(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && definition->water_access().has_requirements();
}

static void draw_farm(building_info_context *c, int help_id, const char *sound_file, int group_id,
    resource_type resource)
{
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = help_id;
    window_building_play_sound(c, sound_file);

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    resource_graphics(resource).panel_icon().draw(c->x_offset + 10, c->y_offset + 10);
    lang_text_draw_centered(group_id, 0, c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);
    int pct_grown = calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b));
    int width = lang_text_draw(group_id, 2, c->x_offset + 32, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_percentage(pct_grown, c->x_offset + 32 + width, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    lang_text_draw(group_id, 3, c->x_offset + 32 + width, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    int efficiency = building_get_efficiency(b);
    if (efficiency < 0) {
        efficiency = 0;
    }

    width = lang_text_draw("TR_BUILDING_WINDOW_INDUSTRY_EFFICIENCY",
        c->x_offset + 32, c->y_offset + 70, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    text_draw_percentage(efficiency, c->x_offset + 32 + width, c->y_offset + 70,
        efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 96, 69, 25);
    } else if (city_resource_is_mothballed(resource)) {
        window_building_draw_description_at(c, 96, group_id, 4);
    } else if (b->data.industry.curse_days_left > 4) {
        window_building_draw_description_at(c, 96, group_id, 11);
    } else if (b->num_workers <= 0) {
        window_building_draw_description_at(c, 96, group_id, 5);
    } else if (c->worker_percentage < 25) {
        window_building_draw_description_at(c, 96, group_id, 10);
    } else if (c->worker_percentage < 50) {
        window_building_draw_description_at(c, 96, group_id, 9);
    } else if (c->worker_percentage < 75) {
        window_building_draw_description_at(c, 96, group_id, 8);
    } else if (c->worker_percentage < 100) {
        window_building_draw_description_at(c, 96, group_id, 7);
    } else if (efficiency < 80) {
        window_building_draw_description_at(c, 96, "TR_BUILDING_WINDOW_INDUSTRY_LOW_EFFICIENCY_RAW_MATERIALS");
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description_at(c, 96, group_id, 6);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 162, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 166);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 170);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, group_id, 1);
}

void window_building_draw_wheat_farm(building_info_context *c)
{
    draw_farm(c, 89, "wavs/wheat_farm.wav", 112, resource_wheat());
}

void window_building_draw_vegetable_farm(building_info_context *c)
{
    draw_farm(c, 90, "wavs/veg_farm.wav", 113, resource_vegetables());
}

void window_building_draw_fruit_farm(building_info_context *c)
{
    draw_farm(c, 90, "wavs/figs_farm.wav", 114, resource_fruit());
}

void window_building_draw_olive_farm(building_info_context *c)
{
    draw_farm(c, 91, "wavs/olives_farm.wav", 115, resource_olives());
}

void window_building_draw_vines_farm(building_info_context *c)
{
    draw_farm(c, 91, "wavs/vines_farm.wav", 116, resource_vines());
}

void window_building_draw_pig_farm(building_info_context *c)
{
    draw_farm(c, 90, "wavs/meat_farm.wav", 117, resource_meat());
}

static void draw_raw_material(
    building_info_context *c, int help_id, const char *sound_file, industry_text text, resource_type resource)
{
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = help_id;
    window_building_play_sound(c, sound_file);

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    resource_graphics(resource).panel_icon().draw(c->x_offset + 10, c->y_offset + 10);
    industry_text_draw_centered(text, 0, c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);
    int pct_done = calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b));
    int width = industry_text_draw(text, 2, c->x_offset + 32, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_percentage(pct_done, c->x_offset + 32 + width, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    industry_text_draw(text, 3, c->x_offset + 32 + width, c->y_offset + 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    int efficiency = building_get_efficiency(b);
    if (efficiency < 0) {
        efficiency = 0;
    }

    width = lang_text_draw("TR_BUILDING_WINDOW_INDUSTRY_EFFICIENCY",
        c->x_offset + 32, c->y_offset + 70, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    text_draw_percentage(efficiency, c->x_offset + 32 + width, c->y_offset + 70,
        efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 96, 69, 25);
    } else if (b->strike_duration_days > 0) {
        window_building_draw_description_at(c, 96, "TR_WINDOW_BUILDING_WORKSHOP_STRIKING");
    } else if (city_resource_is_mothballed(resource)) {
        industry_text_draw_description_at(c, 96, text, 4);
    } else if (b->num_workers <= 0) {
        industry_text_draw_description_at(c, 96, text, 5);
    } else if (c->worker_percentage < 25) {
        industry_text_draw_description_at(c, 96, text, 10);
    } else if (c->worker_percentage < 50) {
        industry_text_draw_description_at(c, 96, text, 9);
    } else if (c->worker_percentage < 75) {
        industry_text_draw_description_at(c, 96, text, 8);
    } else if (c->worker_percentage < 100) {
        industry_text_draw_description_at(c, 96, text, 7);
    } else if (efficiency < 80) {
        window_building_draw_description_at(c, 96, "TR_BUILDING_WINDOW_INDUSTRY_LOW_EFFICIENCY_RAW_MATERIALS");
    } else {
        industry_text_draw_description_at(c, 96, text, 6);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 162, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 166);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 170);
    industry_text_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, text, 1);
}

void window_building_draw_marble_quarry(building_info_context *c)
{
    draw_raw_material(c, 95, "wavs/quarry.wav", legacy_industry_text(118, 0), resource_marble());
}

void window_building_draw_iron_mine(building_info_context *c)
{
    draw_raw_material(c, 93, "wavs/mine.wav", legacy_industry_text(119, 0), resource_iron());
}

void window_building_draw_gold_mine(building_info_context *c)
{
    static constexpr translation_key gold_mine_text[] = {
        "TR_BUILDING_GOLD_MINE",
        "TR_BUILDING_GOLD_MINE_DESC",
        "TR_BUILDING_GOLD_MINE_PRODUCTION",
        "TR_BUILDING_GOLD_MINE_COMPLETE",
        "TR_BUILDING_GOLD_MINE_HALTED",
        "TR_BUILDING_GOLD_MINE_NO_EMPLOYEES",
        "TR_BUILDING_GOLD_MINE_FULL_EMPLOYEES",
        "TR_BUILDING_GOLD_MINE_MANY_EMPLOYEES",
        "TR_BUILDING_GOLD_MINE_HALF_EMPLOYEES",
        "TR_BUILDING_GOLD_MINE_SOME_EMPLOYEES",
        "TR_BUILDING_GOLD_MINE_FEW_EMPLOYEES",
    };
    draw_raw_material(c, 93, "wavs/mine.wav", named_industry_text(gold_mine_text), resource_gold());
}

void window_building_draw_stone_quarry(building_info_context *c)
{
    static constexpr translation_key stone_quarry_text[] = {
        "TR_BUILDING_STONE_QUARRY",
        "TR_BUILDING_STONE_QUARRY_DESC",
        "TR_BUILDING_STONE_QUARRY_PRODUCTION",
        "TR_BUILDING_STONE_QUARRY_COMPLETE",
        "TR_BUILDING_STONE_QUARRY_HALTED",
        "TR_BUILDING_STONE_QUARRY_NO_EMPLOYEES",
        "TR_BUILDING_STONE_QUARRY_FULL_EMPLOYEES",
        "TR_BUILDING_STONE_QUARRY_MANY_EMPLOYEES",
        "TR_BUILDING_STONE_QUARRY_HALF_EMPLOYEES",
        "TR_BUILDING_STONE_QUARRY_SOME_EMPLOYEES",
        "TR_BUILDING_STONE_QUARRY_FEW_EMPLOYEES",
    };
    draw_raw_material(c, 93, "wavs/quarry.wav", named_industry_text(stone_quarry_text), resource_stone());
}

void window_building_draw_sand_pit(building_info_context *c)
{
    static constexpr translation_key sand_pit_text[] = {
        "TR_BUILDING_SAND_PIT",
        "TR_BUILDING_SAND_PIT_DESC",
        "TR_BUILDING_SAND_PIT_PRODUCTION",
        "TR_BUILDING_SAND_PIT_COMPLETE",
        "TR_BUILDING_SAND_PIT_HALTED",
        "TR_BUILDING_SAND_PIT_NO_EMPLOYEES",
        "TR_BUILDING_SAND_PIT_FULL_EMPLOYEES",
        "TR_BUILDING_SAND_PIT_MANY_EMPLOYEES",
        "TR_BUILDING_SAND_PIT_HALF_EMPLOYEES",
        "TR_BUILDING_SAND_PIT_SOME_EMPLOYEES",
        "TR_BUILDING_SAND_PIT_FEW_EMPLOYEES",
    };
    draw_raw_material(c, 93, "wavs/clay.wav", named_industry_text(sand_pit_text), resource_sand());
}

void window_building_draw_timber_yard(building_info_context *c)
{
    draw_raw_material(c, 94, "wavs/timber.wav", legacy_industry_text(120, 0), resource_timber());
}

void window_building_draw_clay_pit(building_info_context *c)
{
    draw_raw_material(c, 92, "wavs/clay.wav", legacy_industry_text(121, 0), resource_clay());
}

static int no_target_for_resource(const building *b, resource_type resource)
{
    return !resource_is_storable(resource) && b->data.industry.progress == 0 &&
        !building_has_workshop_for_raw_material_with_room(resource, b->road_network_id) &&
        !building_monument_get_monument(b->x, b->y, resource, b->road_network_id, 0);
}

static void draw_workshop(
    building_info_context *c, int help_id, const char *sound_file, industry_text text, resource_type resource)
{
    c->advisor_button = ADVISOR_TRADE;
    c->help_id = help_id;
    window_building_play_sound(c, sound_file);

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    resource_graphics(resource).panel_icon().draw(c->x_offset + 10, c->y_offset + 10);
    industry_text_draw_centered(text, 0, c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);
    int pct_done = calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b));
    int width = industry_text_draw(text, 2, c->x_offset + 32, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_percentage(pct_done, c->x_offset + 32 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    industry_text_draw(text, 3, c->x_offset + 32 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    int resources_y_offset = 0;

    if (!b->strike_duration_days) {
        resource_supply_chain chain[RESOURCE_SUPPLY_CHAIN_MAX_SIZE];
        int num_raw_materials = resource_get_supply_chain_for_good(chain, resource);
        if (num_raw_materials > 0) {
            width = 0;
            for (int i = 0; i < num_raw_materials; i++) {
                int current_width = industry_text_get_width(text, 12 + i, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
                if (current_width > width) {
                    width = current_width;
                }
            }
            for (int i = 0; i < num_raw_materials; i++) {
                font_t font = chain[i].raw_amount > b->resources[chain[i].raw_material] ?
                    FONT_NORMAL_RED : FONT_NORMAL_BLACK;
                resource_graphics(chain[i].raw_material).panel_icon().draw(c->x_offset + 32, c->y_offset + 56 + resources_y_offset);
                industry_text_draw(text, 12 + i,
                    c->x_offset + 60, c->y_offset + 60 + resources_y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
                int extra_width = lang_text_draw_amount(8, 10, b->resources[chain[i].raw_material],
                    c->x_offset + 60 + width, c->y_offset + 60 + resources_y_offset, font, screen_ui_to_pixel(font_definition_for(font)->line_height));
                text_draw_number(chain[i].raw_amount, '(',
                    reinterpret_cast<const char *>(translation_for_key("TR_BUILDING_WINDOW_INDUSTRY_NEEDED")),
                    c->x_offset + 60 + width + extra_width, c->y_offset + 60 + resources_y_offset,
                    FONT_NORMAL_BLACK,
                    screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height),
                    COLOR_MASK_NONE);
                resources_y_offset += 20;
            }
        }
    }

    int efficiency = building_get_efficiency(b);
    if (efficiency < 0) {
        efficiency = 0;
    }

    width = lang_text_draw("TR_BUILDING_WINDOW_INDUSTRY_EFFICIENCY",
        c->x_offset + 32, c->y_offset + 78 + resources_y_offset, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    text_draw_percentage(efficiency, c->x_offset + 32 + width, c->y_offset + 78 + resources_y_offset,
        efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 96 + resources_y_offset, 69, 25);
    } else if (b->strike_duration_days > 0) {
        window_building_draw_description_at(c, 96 + resources_y_offset, "TR_WINDOW_BUILDING_WORKSHOP_STRIKING");
    } else if (city_resource_is_mothballed(resource)) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 4);
    } else if (no_target_for_resource(b, resource)) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 13);
    } else if (b->num_workers <= 0) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 5);
    } else if (building_type_requires_water_access(b->type) && !b->has_water_access) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 11);
    } else if (!building_industry_has_raw_materials_for_production(b)) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 11);
    } else if (c->worker_percentage < 25) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 10);
    } else if (c->worker_percentage < 50) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 9);
    } else if (c->worker_percentage < 75) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 8);
    } else if (c->worker_percentage < 100) {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 7);
    } else if (type_matches(static_cast<building_type>(b->type), "concrete_maker") && b->has_water_access &&
        !water_access_runtime_building_area_has_access(b, "reservoir")) {
        window_building_draw_description_at(c, 96 + resources_y_offset, "TR_BUILDING_CONCRETE_MAKER_IMPROVE_WATER_ACCESS");
    } else if (efficiency < 70) {
        window_building_draw_description_at(c, 96 + resources_y_offset, "TR_BUILDING_WINDOW_INDUSTRY_LOW_EFFICIENCY_WORKSHOPS");
    } else {
        industry_text_draw_description_at(c, 96 + resources_y_offset, text, 6);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146 + resources_y_offset, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 150 + resources_y_offset);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76,
        c->y_offset + 154 + resources_y_offset);
    industry_text_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 126 + resources_y_offset, text, 1);
}

void window_building_draw_wine_workshop(building_info_context *c)
{
    draw_workshop(c, 96, "wavs/wine_workshop.wav", legacy_industry_text(122, 0), resource_wine());
}

void window_building_draw_oil_workshop(building_info_context *c)
{
    draw_workshop(c, 97, "wavs/oil_workshop.wav", legacy_industry_text(123, 0), resource_oil());
}

void window_building_draw_weapons_workshop(building_info_context *c)
{
    draw_workshop(c, 98, "wavs/weapons_workshop.wav", legacy_industry_text(124, 0), resource_weapons());
}

void window_building_draw_furniture_workshop(building_info_context *c)
{
    draw_workshop(c, 99, "wavs/furniture_workshop.wav", legacy_industry_text(125, 0), resource_furniture());
}

void window_building_draw_pottery_workshop(building_info_context *c)
{
    draw_workshop(c, 1, "wavs/pottery_workshop.wav", legacy_industry_text(126, 0), resource_pottery());
}

void window_building_draw_brickworks(building_info_context *c)
{
    static constexpr translation_key brickworks_text[] = {
        "TR_BUILDING_BRICKWORKS",
        "TR_BUILDING_BRICKWORKS_DESC",
        "TR_BUILDING_BRICKWORKS_PRODUCTION",
        "TR_BUILDING_BRICKWORKS_COMPLETE",
        "TR_BUILDING_BRICKWORKS_HALTED",
        "TR_BUILDING_BRICKWORKS_NO_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_FULL_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_MANY_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_HALF_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_SOME_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_FEW_EMPLOYEES",
        "TR_BUILDING_BRICKWORKS_NO_RESOURCES",
        "TR_BUILDING_BRICKWORKS_STORED_SAND",
        "TR_BUILDING_BRICKWORKS_STORED_CLAY",
    };
    draw_workshop(c, 1, ASSETS_DIRECTORY "/Sounds/Brickworks.ogg", named_industry_text(brickworks_text),
        resource_bricks());
}

void window_building_draw_concrete_maker(building_info_context *c)
{
    static constexpr translation_key concrete_maker_text[] = {
        "TR_BUILDING_CONCRETE_MAKER",
        "TR_BUILDING_CONCRETE_MAKER_DESC",
        "TR_BUILDING_CONCRETE_MAKER_PRODUCTION",
        "TR_BUILDING_CONCRETE_MAKER_COMPLETE",
        "TR_BUILDING_CONCRETE_MAKER_HALTED",
        "TR_BUILDING_CONCRETE_MAKER_NO_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_FULL_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_MANY_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_HALF_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_SOME_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_FEW_EMPLOYEES",
        "TR_BUILDING_CONCRETE_MAKER_NO_RESOURCES",
        "TR_BUILDING_CONCRETE_MAKER_STORED_SAND",
        "TR_BUILDING_CONCRETE_MAKER_NO_TARGETS",
    };
    draw_workshop(c, 1, ASSETS_DIRECTORY "/Sounds/ConcreteMaker.ogg", named_industry_text(concrete_maker_text),
        resource_concrete());
}

static int governor_palace_is_allowed(void)
{
    return scenario_allowed_building(runtime_type("governors_house")) ||
        scenario_allowed_building(runtime_type("governors_villa")) ||
        scenario_allowed_building(runtime_type("governors_palace"));
}

void window_building_draw_city_mint(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/coin.wav");
    building *b = building_get(c->building_id);
    data.city_mint_id = 0;
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_FINANCIAL;
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        resource_graphics(resource_denarii()).panel_icon().draw(c->x_offset + 10, c->y_offset + 10);

        int pct_done = calc_percentage(b->data.industry.progress, building_industry_get_max_progress(b));
        int width = lang_text_draw("TR_BUILDING_GOLD_MINE_PRODUCTION",
            c->x_offset + 32, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        width += text_draw_percentage(pct_done, c->x_offset + 32 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        lang_text_draw("TR_BUILDING_GOLD_MINE_COMPLETE",
            c->x_offset + 32 + width, c->y_offset + 40, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

        resource_graphics(resource_gold()).panel_icon().draw(c->x_offset + 32, c->y_offset + 56);
        width = lang_text_draw("TR_BUILDING_CITY_MINT_STORED_GOLD",
            c->x_offset + 60, c->y_offset + 60, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        lang_text_draw_amount(8, 10, b->resources[resource_gold()],
            c->x_offset + 60 + width, c->y_offset + 60, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

        int efficiency = building_get_efficiency(b);
        if (efficiency < 0) {
            efficiency = 0;
        }

        width = lang_text_draw("TR_BUILDING_WINDOW_INDUSTRY_EFFICIENCY",
            c->x_offset + 32, c->y_offset + 98, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        text_draw_percentage(efficiency, c->x_offset + 32 + width, c->y_offset + 98,
            efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(efficiency >= 50 ? FONT_NORMAL_BLACK : FONT_NORMAL_RED)->line_height));

        if (!c->has_road_access) {
            window_building_draw_description_at(c, 116, 69, 25);
        } else if (building_count_active(runtime_type("senate")) == 0) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_NO_SENATE");
        } else if (b->num_workers <= 0) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_NO_EMPLOYEES");
        } else if (b->resources[resource_gold()] < BUILDING_INDUSTRY_CITY_MINT_GOLD_PER_COIN &&
            b->output_resource_id == resource_denarii()) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_NO_GOLD");
        } else if (c->worker_percentage < 25) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_FEW_EMPLOYEES");
        } else if (c->worker_percentage < 50) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_SOME_EMPLOYEES");
        } else if (c->worker_percentage < 75) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_HALF_EMPLOYEES");
        } else if (c->worker_percentage < 100) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_MANY_EMPLOYEES");
        } else if (efficiency < 70) {
            window_building_draw_description_at(c, 116, "TR_BUILDING_WINDOW_INDUSTRY_LOW_EFFICIENCY_WORKSHOPS");
        } else {
            window_building_draw_description_at(c, 116, "TR_BUILDING_CITY_MINT_FULL_EMPLOYEES");
        }

        inner_panel_draw(c->x_offset + 16, c->y_offset + 178, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 182);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 186);
        lang_text_draw("TR_BUILDING_CITY_MINT_CONVERT",
            c->x_offset + 32, c->y_offset + BLOCK_SIZE * c->height_blocks - 190, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        lang_text_draw("TR_BUILDING_CITY_MINT_GOLD_TO_DN",
            c->x_offset + 60, c->y_offset + BLOCK_SIZE * c->height_blocks - 166, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        lang_text_draw("TR_BUILDING_CITY_MINT_DN_TO_GOLD",
            c->x_offset + 60, c->y_offset + BLOCK_SIZE * c->height_blocks - 142, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 118,
            b->output_resource_id == resource_denarii() ?
                "TR_BUILDING_CITY_MINT_DESC" : "TR_BUILDING_CITY_MINT_DESC_ALTERNATIVE");
        if (governor_palace_is_allowed() && b->output_resource_id == resource_denarii()) {
            window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 78,
                city_buildings_has_governor_house() ?
                    "TR_BUILDING_CITY_MINT_DESC_PALACE" : "TR_BUILDING_CITY_MINT_DESC_NO_PALACE");
        }
        data.city_mint_id = b->id;
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        window_building_draw_monument_construction_process(c, "TR_BUILDING_CITY_MINT_PHASE_1",
            "TR_BUILDING_CITY_MINT_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
    }
    lang_text_draw_centered("TR_BUILDING_CITY_MINT", c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
}

void window_building_draw_city_mint_foreground(building_info_context *c)
{
    if (!data.city_mint_id) {
        return;
    }
    int x = c->x_offset + 32;
    int y = c->y_offset + BLOCK_SIZE * c->height_blocks - 171;
    button_border_draw(x, y, 20, 20, data.focus_button_id == 1);
    button_border_draw(x, y + 24, 20, 20, data.focus_button_id == 2);
    int selected_offset = building_get(data.city_mint_id)->output_resource_id == resource_denarii() ? 0 : 24;
    Image::from_id(assets_get_image_id("UI", "Denied_Walker_Checkmark")).draw(x + 4, y + 4 + selected_offset);
}

static int shipyard_boats_needed(void)
{
    for (const building *wharf = building_first_of_type(runtime_type("wharf")); wharf; wharf = wharf->next_of_type) {
        if (wharf->num_workers > 0 && !wharf->data.industry.fishing_boat_id) {
            return 1;
        }
    }
    return 0;
}

void window_building_draw_shipyard(building_info_context *c)
{
    c->help_id = 82;
    window_building_play_sound(c, "wavs/shipyard.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(100, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else {
        int pct_done = calc_percentage(b->data.industry.progress, 160);
        int width = lang_text_draw(100, 2, c->x_offset + 32, c->y_offset + 56, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        width += text_draw_percentage(pct_done, c->x_offset + 32 + width, c->y_offset + 56, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        lang_text_draw(100, 3, c->x_offset + 32 + width, c->y_offset + 56, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        if (shipyard_boats_needed()) {
            lang_text_draw_multiline(100, 5, c->x_offset + 32, c->y_offset + 80,
                BLOCK_SIZE * (c->width_blocks - 6), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        } else {
            lang_text_draw_multiline(100, 4, c->x_offset + 32, c->y_offset + 80,
                BLOCK_SIZE * (c->width_blocks - 6), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        }
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, 100, 1);
}

void window_building_draw_wharf(building_info_context *c)
{
    c->help_id = 84;
    c->advisor_button = ADVISOR_TRADE;
    window_building_play_sound(c, "wavs/wharf.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(102, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    resource_graphics(resource_fish()).panel_icon().draw(c->x_offset + 10, c->y_offset + 10);

    building *b = building_get(c->building_id);

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (city_resource_is_mothballed(resource_fish())) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_WHARF_MOTHBALLED");
    } else if (!b->data.industry.fishing_boat_id) {
        window_building_draw_description(c, 102, 2);
    } else {
        int text_id;
        switch (figure_get(b->data.industry.fishing_boat_id)->action_state) {
            case FIGURE_ACTION_191_FISHING_BOAT_GOING_TO_FISH: text_id = 3; break;
            case FIGURE_ACTION_192_FISHING_BOAT_FISHING: text_id = 4; break;
            case FIGURE_ACTION_193_FISHING_BOAT_GOING_TO_WHARF: text_id = 5; break;
            case FIGURE_ACTION_194_FISHING_BOAT_AT_WHARF: text_id = 6; break;
            case FIGURE_ACTION_195_FISHING_BOAT_RETURNING_WITH_FISH: text_id = 7; break;
            default: text_id = 8; break;
        }
        window_building_draw_description(c, 102, text_id);
    }

    int width = lang_text_draw("TR_BUILDING_WINDOW_INDUSTRY_WHARF_AVERAGE_CATCH",
        c->x_offset + 32, c->y_offset + 110, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_number(b->data.industry.average_production_per_month, '@', "",
        c->x_offset + 32 + width, c->y_offset + 110, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    resource_graphics(resource_fish()).panel_icon().draw(c->x_offset + 32 + width, c->y_offset + 110);

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, 102, 1);
}

static void city_mint_conversion_changed(int accepted, int checked)
{
    if (!accepted) {
        return;
    }
    building *city_mint = building_get(data.city_mint_id);
    if (city_mint->output_resource_id == resource_denarii()) {
        city_mint->output_resource_id = resource_gold();
    } else {
        city_mint->output_resource_id = resource_denarii();
    }
    city_mint->data.industry.progress = 0;
    city_mint->data.industry.age_months = 0;
    city_mint->data.industry.average_production_per_month = 0;
    city_mint->data.industry.production_current_month = 0;
}

static void set_city_mint_conversion(const generic_button *button)
{
    resource_type resource = static_cast<resource_type>(button->parameter1);
    if (building_get(data.city_mint_id)->output_resource_id != resource) {
        window_popup_dialog_show_confirmation(translation_for_key("TR_BUILDING_CITY_MINT_CHANGE_PRODUCTION"),
            translation_for_key("TR_BUILDING_CITY_MINT_PROGRESS_WILL_BE_LOST"), 0, city_mint_conversion_changed);
    }
}

int window_building_handle_mouse_city_mint(const mouse *m, building_info_context *c)
{
    if (!data.city_mint_id) {
        return 0;
    }
    if (GenericButtonList(mint_conversion_buttons, 2).handle_mouse(
        *m,
        c->x_offset + 16,
        c->y_offset + BLOCK_SIZE * c->height_blocks - 171,
        &data.focus_button_id
    )) {
        window_request_refresh();
        return 1;
    }
    return 0;
}

void window_building_industry_get_tooltip(building_info_context *c, translation_key *translation)
{
    building_type type = building_get(c->building_id)->type;
    int needed_resources = building_get_raw_materials_for_workshop(0, type);
    int y_correction;
    if (type_matches(type, "city_mint")) {
        y_correction = 8;
    } else if (needed_resources > 0) {
        y_correction = 8 + needed_resources * 20;
    } else {
        y_correction = 0;
    }

    const mouse *m = mouse_get();
    if (m->x >= c->x_offset + 16 && m->x < c->width_blocks * BLOCK_SIZE + c->x_offset - 16 &&
        m->y >= c->y_offset + 60 + y_correction && m->y < c->y_offset + 86 + y_correction) {
        *translation = "TR_BUILDING_WINDOW_INDUSTRY_EFFICIENCY_TOOLTIP";
    }
}
