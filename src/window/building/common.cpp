#include "building/building.h"
#include "building/house.h"
#include "building/local_workforce.h"
#include "city/labor.h"
#include "game/resource_graphics.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"

#include "translation/translation.h"
#include "common.h"

extern "C" {
#include "assets/assets.h"
#include "building/building_record.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/population.h"
#include "city/resource.h"
#include "city/view.h"
#include "core/calc.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "sound/speech.h"
}


#include <stdlib.h>
#include <math.h>

void window_building_set_possible_position(int *x_offset, int *y_offset, int width_blocks, int height_blocks)
{
    int dialog_width = BLOCK_SIZE * width_blocks;
    int dialog_height = BLOCK_SIZE * height_blocks;
    int stub;
    int width;
    city_view_get_viewport(&stub, &stub, &width, &stub);
    width = screen_pixel_to_ui(width) - MARGIN_POSITION;

    if (*y_offset + dialog_height > screen_height() - MARGIN_POSITION) {
        *y_offset -= dialog_height;
    }

    *y_offset = (*y_offset < MIN_Y_POSITION) ? MIN_Y_POSITION : *y_offset;

    if (*x_offset + dialog_width > width) {
        *x_offset = width - dialog_width;
    }
}

int window_building_get_vertical_offset(building_info_context *c, int new_window_height)
{
    new_window_height = new_window_height * BLOCK_SIZE;
    int old_window_height = c->height_blocks * BLOCK_SIZE;
    int y_offset = c->y_offset;

    int center = (old_window_height / 2) + y_offset;
    int new_window_y = center - (new_window_height / 2);

    if (new_window_y < MIN_Y_POSITION) {
        new_window_y = MIN_Y_POSITION;
    } else {
        int height = screen_height() - MARGIN_POSITION;

        if (new_window_y + new_window_height > height) {
            new_window_y = height - new_window_height;
        }
    }

    return new_window_y;
}

static int get_employment_info_text(const building *b, int consider_house_covering)
{
    int text_id;
    int local_workforce = building_local_workforce_is_workforce_building(b);
    int labor_access = building_local_workforce_access_score(b);
    int required_workers = building_get_laborers(b->type);

    if (b->num_workers >= required_workers) {
        text_id = 0;
    } else if (city_population() <= 0) {
        text_id = 16; // no people in city
    } else if (!consider_house_covering) {
        text_id = 19;
    } else if (labor_access <= 0) {
        text_id = 17; // no employees nearby
    } else if ((local_workforce && labor_access < required_workers) ||
        (!local_workforce && labor_access < 40)) {
        text_id = 20; // poor access to employees
    } else if (city_labor_category(b->labor_category)->workers_allocated <= 0) {
        text_id = 18; // no people allocated
    } else {
        text_id = 19; // too few people allocated
    }
    if (!text_id && consider_house_covering &&
        ((local_workforce && labor_access < required_workers) ||
        (!local_workforce && labor_access < 40))) {
        text_id = 20; // poor access to employees
    }
    return text_id;
}

void window_building_draw_levy(int amount, int x_offset, int y_offset)
{
    resource_graphics(resource_denarii()).panel_icon().draw(x_offset, y_offset + 5);
    int width = text_draw_money(abs(amount), x_offset + 20, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    if (amount > 0) {
        text_draw(translation_for_key("TR_BUILDING_INFO_MONTHLY_LEVY"),
            x_offset + 20 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
}

/***
 * UNUSED
 *
 * void window_building_draw_tourism(building_info_context *c, int x_offset, int y_offset)
{
    building *b = building_get(c->building_id);
    if (b->tourism_income_this_year > 0) {
        int width = text_draw_money(b->tourism_income_this_year, x_offset + 0, y_offset + 10, FONT_NORMAL_BROWN);
        text_draw(translation_for_key("TR_WINDOW_BUILDING_TOURISM_ANNUAL"),
            x_offset + 0 + width, y_offset + 10, FONT_NORMAL_BROWN, 0);
    } else if (b->tourism_disabled) {
        text_draw(translation_for_key("TR_WINDOW_BUILDING_TOURISM_DISABLED"),
            x_offset + 0, y_offset + 10, FONT_NORMAL_BROWN, 0);
    }
} ***/

static void draw_employment_details(building_info_context *c, building *b, int y_offset, int text_id)
{
    y_offset += c->y_offset;
    Image::from_id(Image::group(GROUP_CONTEXT_ICONS) + 14).draw(c->x_offset + 40, y_offset + 6);

    int levy = building_get_levy(b);
    if (levy) {
        y_offset -= 10;
    }

    int laborers_needed = building_get_laborers(b->type);
    if (laborers_needed) {
        if (b->state == BUILDING_STATE_MOTHBALLED) {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, b->num_workers), b->num_workers, c->x_offset + 60, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            text_draw(translation_for_key("TR_BUILDING_INFO_MOTHBALL_WARNING"),
                c->x_offset + 70, y_offset + 26, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        } else if (text_id) {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, b->num_workers), b->num_workers, c->x_offset + 60, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            lang_text_draw(current_string_key(69, text_id), c->x_offset + 70, y_offset + 26, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            y_offset += 6;
        } else {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, b->num_workers), b->num_workers, c->x_offset + 60, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }
    }
    if (levy) {
        window_building_draw_levy(levy, c->x_offset + 64, y_offset + 26);
    }
}

void window_building_draw_employment(building_info_context *c, int y_offset)
{
    building *b = building_get(c->building_id);
    int text_id = get_employment_info_text(b, 1);
    draw_employment_details(c, b, y_offset, text_id);
}

void window_building_draw_employment_without_house_cover(building_info_context *c, int y_offset)
{
    building *b = building_get(c->building_id);
    int text_id = get_employment_info_text(b, 0);
    draw_employment_details(c, b, y_offset, text_id);
}

void window_building_draw_description(building_info_context *c, int text_group, int text_id)
{
    lang_text_draw_multiline(current_string_key(text_group, text_id), c->x_offset + 32, c->y_offset + 56, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

void window_building_draw_description(building_info_context *c, translation_key key)
{
    lang_text_draw_multiline(key, c->x_offset + 32, c->y_offset + 56,
       BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

int window_building_draw_description_at(building_info_context *c, int y_offset, int text_group, int text_id)
{
    return lang_text_draw_multiline(current_string_key(text_group, text_id), c->x_offset + 32, c->y_offset + y_offset, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

int window_building_draw_description_at(building_info_context *c, int y_offset, translation_key key)
{
    return lang_text_draw_multiline(key, c->x_offset + 32, c->y_offset + y_offset,
        BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

void window_building_play_sound(building_info_context *c, const char *sound_file)
{
    if (c->can_play_sound) {
        sound_speech_play_file(sound_file);
        c->can_play_sound = 0;
    }
}

static void window_building_draw_monument_resources_needed(building_info_context *c)
{
    building *b = building_get(c->building_id);
    int y_offset = 95;
    inner_panel_draw(c->x_offset + 16, c->y_offset + y_offset, c->width_blocks - 2, 5);
    if (building_monument_needs_resources(b)) {
        for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
            const resource_type r = static_cast<resource_type>(resource);
            int resource_needed_amount = building_monument_resources_needed_for_monument_type(b->type, r,
                b->monument.phase);
            if (!resource_needed_amount) {
                continue;
            }
            int resource_delivered_amount = resource_needed_amount - b->resources[r];
            resource_graphics(r).panel_icon().draw(c->x_offset + 32, c->y_offset + y_offset + 10);
            int width = text_draw_number(resource_delivered_amount, '@', "/",
                c->x_offset + 64, c->y_offset + y_offset + 15, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            width += text_draw_number(resource_needed_amount, '@', "",
                c->x_offset + 64 + width, c->y_offset + y_offset + 15, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            text_draw(resource_get_data(r)->text, c->x_offset + 68 + width, c->y_offset + y_offset + 15,
                FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            y_offset += 20;
        }
    } else {
        text_draw_multiline(translation_for_key("TR_BUILDING_MONUMENT_CONSTRUCTION_ARCHITECT_NEEDED"),
            c->x_offset + 32, c->y_offset + y_offset + 10, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }
}

static translation_key phase_key_from(const translation_key *keys, int count, int phase)
{
    int index = phase - 1;
    if (index < 0 || index >= count) {
        return {};
    }
    return keys[index];
}

static translation_key monument_phase_name_key(translation_key first_phase, int phase)
{
    static const translation_key large_temple[] = { "TR_BUILDING_LARGE_TEMPLE_PHASE_1", "TR_BUILDING_LARGE_TEMPLE_PHASE_2" };
    static const translation_key oracle[] = { "TR_BUILDING_ORACLE_PHASE_1", "TR_BUILDING_ORACLE_PHASE_2" };
    static const translation_key nymphaeum[] = { "TR_BUILDING_NYMPHAEUM_PHASE_1", "TR_BUILDING_NYMPHAEUM_PHASE_2" };
    static const translation_key small_mausoleum[] = { "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1", "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_2" };
    static const translation_key large_mausoleum[] = { "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1", "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_2" };
    static const translation_key grand_temple[] = {
        "TR_BUILDING_GRAND_TEMPLE_PHASE_1", "TR_BUILDING_GRAND_TEMPLE_PHASE_2", "TR_BUILDING_GRAND_TEMPLE_PHASE_3",
        "TR_BUILDING_GRAND_TEMPLE_PHASE_4", "TR_BUILDING_GRAND_TEMPLE_PHASE_5"
    };
    static const translation_key colosseum[] = {
        "TR_BUILDING_COLOSSEUM_PHASE_1", "TR_BUILDING_COLOSSEUM_PHASE_2", "TR_BUILDING_COLOSSEUM_PHASE_3",
        "TR_BUILDING_COLOSSEUM_PHASE_4"
    };
    static const translation_key hippodrome[] = {
        "TR_BUILDING_HIPPODROME_PHASE_1", "TR_BUILDING_HIPPODROME_PHASE_2", "TR_BUILDING_HIPPODROME_PHASE_3",
        "TR_BUILDING_HIPPODROME_PHASE_4"
    };
    static const translation_key caravanserai[] = { "TR_BUILDING_CARAVANSERAI_PHASE_1", "TR_BUILDING_CARAVANSERAI_PHASE_2" };
    static const translation_key lighthouse[] = {
        "TR_BUILDING_LIGHTHOUSE_PHASE_1", "TR_BUILDING_LIGHTHOUSE_PHASE_2", "TR_BUILDING_LIGHTHOUSE_PHASE_3",
        "TR_BUILDING_LIGHTHOUSE_PHASE_4"
    };
    static const translation_key city_mint[] = { "TR_BUILDING_CITY_MINT_PHASE_1", "TR_BUILDING_CITY_MINT_PHASE_2" };

    if (first_phase == "TR_BUILDING_LARGE_TEMPLE_PHASE_1") return phase_key_from(large_temple, 2, phase);
    if (first_phase == "TR_BUILDING_ORACLE_PHASE_1") return phase_key_from(oracle, 2, phase);
    if (first_phase == "TR_BUILDING_NYMPHAEUM_PHASE_1") return phase_key_from(nymphaeum, 2, phase);
    if (first_phase == "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1") return phase_key_from(small_mausoleum, 2, phase);
    if (first_phase == "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1") return phase_key_from(large_mausoleum, 2, phase);
    if (first_phase == "TR_BUILDING_GRAND_TEMPLE_PHASE_1") return phase_key_from(grand_temple, 5, phase);
    if (first_phase == "TR_BUILDING_COLOSSEUM_PHASE_1") return phase_key_from(colosseum, 4, phase);
    if (first_phase == "TR_BUILDING_HIPPODROME_PHASE_1") return phase_key_from(hippodrome, 4, phase);
    if (first_phase == "TR_BUILDING_CARAVANSERAI_PHASE_1") return phase_key_from(caravanserai, 2, phase);
    if (first_phase == "TR_BUILDING_LIGHTHOUSE_PHASE_1") return phase_key_from(lighthouse, 4, phase);
    if (first_phase == "TR_BUILDING_CITY_MINT_PHASE_1") return phase_key_from(city_mint, 2, phase);
    return first_phase;
}

static translation_key monument_phase_text_key(translation_key first_phase_text, int phase)
{
    static const translation_key large_temple[] = { "TR_BUILDING_LARGE_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_LARGE_TEMPLE_PHASE_2_TEXT" };
    static const translation_key oracle[] = { "TR_BUILDING_ORACLE_PHASE_1_TEXT", "TR_BUILDING_ORACLE_PHASE_2_TEXT" };
    static const translation_key nymphaeum[] = { "TR_BUILDING_NYMPHAEUM_PHASE_1_TEXT", "TR_BUILDING_NYMPHAEUM_PHASE_2_TEXT" };
    static const translation_key small_mausoleum[] = { "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_2_TEXT" };
    static const translation_key large_mausoleum[] = { "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_2_TEXT" };
    static const translation_key grand_temple[] = {
        "TR_BUILDING_GRAND_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_2_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_3_TEXT",
        "TR_BUILDING_GRAND_TEMPLE_PHASE_4_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_5_TEXT"
    };
    static const translation_key colosseum[] = {
        "TR_BUILDING_COLOSSEUM_PHASE_1_TEXT", "TR_BUILDING_COLOSSEUM_PHASE_2_TEXT", "TR_BUILDING_COLOSSEUM_PHASE_3_TEXT",
        "TR_BUILDING_COLOSSEUM_PHASE_4_TEXT"
    };
    static const translation_key hippodrome[] = {
        "TR_BUILDING_HIPPODROME_PHASE_1_TEXT", "TR_BUILDING_HIPPODROME_PHASE_2_TEXT", "TR_BUILDING_HIPPODROME_PHASE_3_TEXT",
        "TR_BUILDING_HIPPODROME_PHASE_4_TEXT"
    };
    static const translation_key caravanserai[] = { "TR_BUILDING_CARAVANSERAI_PHASE_1_TEXT", "TR_BUILDING_CARAVANSERAI_PHASE_2_TEXT" };
    static const translation_key lighthouse[] = {
        "TR_BUILDING_LIGHTHOUSE_PHASE_1_TEXT", "TR_BUILDING_LIGHTHOUSE_PHASE_2_TEXT", "TR_BUILDING_LIGHTHOUSE_PHASE_3_TEXT",
        "TR_BUILDING_LIGHTHOUSE_PHASE_4_TEXT"
    };
    static const translation_key city_mint[] = { "TR_BUILDING_CITY_MINT_PHASE_1_TEXT", "TR_BUILDING_CITY_MINT_PHASE_2_TEXT" };

    if (first_phase_text == "TR_BUILDING_LARGE_TEMPLE_PHASE_1_TEXT") return phase_key_from(large_temple, 2, phase);
    if (first_phase_text == "TR_BUILDING_ORACLE_PHASE_1_TEXT") return phase_key_from(oracle, 2, phase);
    if (first_phase_text == "TR_BUILDING_NYMPHAEUM_PHASE_1_TEXT") return phase_key_from(nymphaeum, 2, phase);
    if (first_phase_text == "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1_TEXT") return phase_key_from(small_mausoleum, 2, phase);
    if (first_phase_text == "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1_TEXT") return phase_key_from(large_mausoleum, 2, phase);
    if (first_phase_text == "TR_BUILDING_GRAND_TEMPLE_PHASE_1_TEXT") return phase_key_from(grand_temple, 5, phase);
    if (first_phase_text == "TR_BUILDING_COLOSSEUM_PHASE_1_TEXT") return phase_key_from(colosseum, 4, phase);
    if (first_phase_text == "TR_BUILDING_HIPPODROME_PHASE_1_TEXT") return phase_key_from(hippodrome, 4, phase);
    if (first_phase_text == "TR_BUILDING_CARAVANSERAI_PHASE_1_TEXT") return phase_key_from(caravanserai, 2, phase);
    if (first_phase_text == "TR_BUILDING_LIGHTHOUSE_PHASE_1_TEXT") return phase_key_from(lighthouse, 4, phase);
    if (first_phase_text == "TR_BUILDING_CITY_MINT_PHASE_1_TEXT") return phase_key_from(city_mint, 2, phase);
    return first_phase_text;
}

void window_building_draw_monument_construction_process(building_info_context *c,
    translation_key tr_phase_name, translation_key tr_phase_name_text, translation_key tr_construction_desc)
{
    building *b = building_get(c->building_id);

    if (b->monument.phase != MONUMENT_FINISHED) {
        if (!c->has_road_access) {
            window_building_draw_description(c, "TR_WINDOW_BUILDING_INFO_WARNING_NO_MONUMENT_ROAD_ACCESS");
            text_draw_multiline(translation_for(tr_construction_desc),
                c->x_offset + 32, c->y_offset + 180, 16 * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            return;
        }
        int width = text_draw(translation_for_key("TR_CONSTRUCTION_PHASE"),
            c->x_offset + 32, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        width += text_draw_number_pair(b->monument.phase, building_monument_phases(b->type) - 1, '@', "/",
            c->x_offset + 32 + width, c->y_offset + 50, 0, 0, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw(translation_for(monument_phase_name_key(tr_phase_name, b->monument.phase)),
            c->x_offset + 32 + width, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw(translation_for_key("TR_REQUIRED_RESOURCES"), c->x_offset + 32, c->y_offset + 80, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        window_building_draw_monument_resources_needed(c);
        int height = text_draw_multiline(translation_for(monument_phase_text_key(tr_phase_name_text, b->monument.phase)),
            c->x_offset + 32, c->y_offset + 190, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

        if (building_monument_is_construction_halted(b)) {
            height += text_draw_multiline(translation_for_key("TR_BUILDING_MONUMENT_CONSTRUCTION_HALTED"),
                c->x_offset + 32, c->y_offset + 200 + height, BLOCK_SIZE * (c->width_blocks - 4),
                0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            height += text_draw_multiline(translation_for(tr_construction_desc),
                c->x_offset + 32, c->y_offset + 200 + height, BLOCK_SIZE * (c->width_blocks - 4),
                0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        if (c->height_blocks > 28) {
            int phase_offset = b->monument.phase % 2;
            const char *banner_entry = phase_offset ? "Construction_Banner_02" : "Construction_Banner_01";
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 216 + height);
            ImageGroupEntryRef::from_group(phase_offset ? "UI\\Construction_Banner_02" : "UI\\Construction_Banner_01", banner_entry).draw(c->x_offset + 37, c->y_offset + 221 + height);
        }
    }
}

static color_t get_color_for_risk(int risk_level)
{
    static color_t risk_colors[4] = { COLOR_RISK_ICON_LOW, COLOR_RISK_ICON_MEDIUM,
        COLOR_RISK_ICON_HIGH, COLOR_RISK_ICON_EXTREME };
    return risk_colors[calc_bound(risk_level, 0, 10) / 3];
}

void window_building_draw_risks(building_info_context *c, int x_offset, int y_offset)
{
    c->risk_icons.active = 1;
    c->risk_icons.x_offset = x_offset;
    c->risk_icons.y_offset = y_offset;

    const building *b = building_get(c->building_id);
    int risks_image_id = assets_lookup_image_id(ASSET_UI_RISKS);

    // Health risk
    if (b->house_size && b->house_population) {
        graphics_draw_inset_rect(x_offset - 28, y_offset, 24, 24,
            COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
        Image::from_id(risks_image_id + 2).draw(x_offset - 28, y_offset, get_color_for_risk(b->sickness_level / 10), SCALE_NONE);
    }

    // Fire risk
    graphics_draw_inset_rect(x_offset, y_offset, 24, 24, COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
    if (b->fire_proof) {
        Image::from_id(risks_image_id + 1).draw(x_offset, y_offset);
        Image::from_id(risks_image_id + 3).draw(x_offset, y_offset);
    } else {
        Image::from_id(risks_image_id + 1).draw(x_offset, y_offset, get_color_for_risk(b->fire_risk / 10), SCALE_NONE);
    }

    // Damage risk
    graphics_draw_inset_rect(x_offset + 28, y_offset, 24, 24,
        COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
    int house_level = building_house_legacy_level(Building::from_id(b->id));
    if (b->fire_proof || (b->house_size && house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_TENT)) {
        Image::from_id(risks_image_id).draw(x_offset + 28, y_offset);
        Image::from_id(risks_image_id + 3).draw(x_offset + 28, y_offset);
    } else {
        Image::from_id(risks_image_id).draw(x_offset + 28, y_offset, get_color_for_risk(b->damage_risk / 20), SCALE_NONE);
    }
}

void window_building_get_risks_tooltip(
    const building_info_context *c, int *group_id, int *text_id, translation_key *translation)
{
    if (!c->risk_icons.active) {
        return;
    }

    const building *b = building_get(c->building_id);
    const mouse *m = mouse_get();

    // Health tooltip
    if (b->house_size && b->house_population) {
        if (m->x >= c->risk_icons.x_offset - 28 && m->x < c->risk_icons.x_offset - 4 &&
            m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
            static const translation_key sickness_tooltips[] = {
                "TR_TOOLTIP_OVERLAY_SICKNESS_LOW",
                "TR_TOOLTIP_OVERLAY_SICKNESS_MEDIUM",
                "TR_TOOLTIP_OVERLAY_SICKNESS_HIGH",
                "TR_TOOLTIP_OVERLAY_SICKNESS_PLAGUE",
            };
            *translation = b->sickness_level <= 0 ? "TR_TOOLTIP_OVERLAY_SICKNESS_NONE" :
                sickness_tooltips[calc_bound(b->sickness_level, 0, 100) / 30];
            return;
        }
    }

    // Fire tooltip
    if (m->x >= c->risk_icons.x_offset && m->x < c->risk_icons.x_offset + 24 &&
        m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
        *group_id = 66;
        *text_id = 46 + calc_bound(b->fire_risk + 19, 0, 100) / 20;
        return;
    }

    // Damage tooltip
    if (m->x >= c->risk_icons.x_offset + 28 && m->x < c->risk_icons.x_offset + 52 &&
        m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
        *group_id = 66;
        *text_id = 52 + calc_bound(b->damage_risk + 39, 0, 200) / 40;
    }
}
