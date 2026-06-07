#include "building/building.h"
#include "game/resource_graphics.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"

#include "government.h"

#include "translation/translation.h"
#include "window/building/figures.h"
extern "C" {
#include "assets/assets.h"
#include "building/building_record.h"
#include "city/constants.h"
#include "core/dir.h"
#include "game/resource.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
}


void window_building_draw_forum(building_info_context *c)
{
    c->advisor_button = ADVISOR_FINANCIAL;
    c->help_id = 76;
    window_building_play_sound(c, "wavs/forum.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.106.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    resource_graphics(resource_denarii()).panel_icon().draw(c->x_offset + 16, c->y_offset + 36);

    building *b = building_get(c->building_id);
    int width = lang_text_draw("main_strings.106.2", c->x_offset + 44, c->y_offset + 43, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    lang_text_draw_amount(current_string_amount_key(8, 0, b->tax_income_or_storage), b->tax_income_or_storage, c->x_offset + 44 + width, c->y_offset + 43, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 72, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description_at(c, 72, 106, 10);
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description_at(c, 72, 106, 5);
    } else if (c->worker_percentage >= 75) {
        window_building_draw_description_at(c, 72, 106, 6);
    } else if (c->worker_percentage >= 50) {
        window_building_draw_description_at(c, 72, 106, 7);
    } else if (c->worker_percentage >= 25) {
        window_building_draw_description_at(c, 72, 106, 8);
    } else {
        window_building_draw_description_at(c, 72, 106, 9);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 150);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, 230, 106, 1);
}

void window_building_draw_senate(building_info_context *c)
{
    c->advisor_button = ADVISOR_RATINGS;
    c->help_id = 77;
    window_building_play_sound(c, "wavs/senate.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.105.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    resource_graphics(resource_denarii()).panel_icon().draw(c->x_offset + 16, c->y_offset + 36);

    building *b = building_get(c->building_id);
    int width = lang_text_draw("main_strings.105.2", c->x_offset + 44, c->y_offset + 43, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    lang_text_draw_amount(current_string_amount_key(8, 0, b->tax_income_or_storage), b->tax_income_or_storage, c->x_offset + 44 + width, c->y_offset + 43, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 72, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description_at(c, 72, 106, 10);
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description_at(c, 72, 106, 5);
    } else if (c->worker_percentage >= 75) {
        window_building_draw_description_at(c, 72, 106, 6);
    } else if (c->worker_percentage >= 50) {
        window_building_draw_description_at(c, 72, 106, 7);
    } else if (c->worker_percentage >= 25) {
        window_building_draw_description_at(c, 72, 106, 8);
    } else {
        window_building_draw_description_at(c, 72, 106, 9);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 150);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, 230, 105, 1);
}

void window_building_draw_governor_home(building_info_context *c)
{
    c->help_id = 78;
    window_building_play_sound(c, "wavs/gov_palace.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.103.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 143, 103, 1);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 68, c->y_offset + 16);
}

void window_building_draw_garden(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, "wavs/park.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.79.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 158, 79, 1);
}

void window_building_draw_plaza(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    window_building_prepare_figure_list(c);
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.137.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_figure_list(c);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 113, 137, 1);
}

void window_building_draw_statue(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.80.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 158, 80, 1);
}

void window_building_draw_large_statue(building_info_context *c)
{
    building *b = building_get(c->building_id);
    window_building_draw_statue(c);
    if (!b->has_water_access) {
        lang_text_draw_multiline("TR_WINDOW_BUILDING_GOVERNMENT_LARGE_STATUE_WATER_WARNING",
            c->x_offset + 32, c->y_offset + 44, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }
}

void window_building_draw_triumphal_arch(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.80.2", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 328, 80, 3);
    Image::from_id(assets_get_image_id("UI", "Triumphal_Arch_Banner")).draw(c->x_offset + 37, c->y_offset + 125);
    ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 120);
}

void window_building_draw_pond(building_info_context *c)
{
    c->help_id = 80;
    window_building_play_sound(c, "wavs/fountain.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for_key("TR_BUILDING_WINDOW_POND"),
        c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    window_building_draw_description_at(c, 96, "TR_BUILDING_POND_DESC");
}

void window_building_draw_obelisk(building_info_context *c)
{
    c->help_id = 79;
    window_building_play_sound(c, "wavs/statue.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for_key("TR_BUILDING_OBELISK"),
        c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    window_building_draw_description_at(c, 96, "TR_BUILDING_OBELISK_DESC");
}
