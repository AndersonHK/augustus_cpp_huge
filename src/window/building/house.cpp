#include "building/building.h"
#include "building/house.h"
#include "building/local_workforce.h"
#include "city/sentiment.h"
#include "game/resource_graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "map/road_access.h"

#include "house.h"

#include "translation/translation.h"
#include "window/building/figures.h"
#include "building/building_record.h"
#include "city/constants.h"
#include "city/finance.h"
#include "core/calc.h"
#include "core/image.h"
#include "core/string.h"
#include "game/resource.h"
#include "graphics/font.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "sound/speech.h"


static void draw_vacant_lot(building_info_context *c)
{
    window_building_prepare_figure_list(c);
    if (c->can_play_sound) {
        c->can_play_sound = 0;
        if (c->figure.count > 0) {
            window_building_play_figure_phrase(c);
        } else {
            sound_speech_play_file("wavs/empty_land.wav");
        }
    }
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.128.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_figure_list(c);

    int text_id = 2;
    building *b = building_get(c->building.id());
    if (map_closest_road_within_radius(b->x, b->y, 1, 2, 0, 0)) {
        text_id = 1;
    }
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 113, 128, text_id);
}

static void draw_population_info(building_info_context *c, int y_offset)
{
    building *b = building_get(c->building.id());
    int icon = 13;
    if (building_house_has_plebeian_residents(c->building)) {
        icon++;
    }

    const int image_id = Image::group(GROUP_CONTEXT_ICONS) + icon;
    const Image &icon_image = Image::from_id(image_id);
    const int icon_x = c->x_offset + 34;
    const int icon_y = y_offset + 4;
    const int text_x = icon_x + icon_image.width() + 8;
    const int line_height = font_definition_for(FONT_NORMAL_BROWN)->line_height;
    const int line_padding = 4;
    const int text_block_height = 2 * line_height + line_padding;
    const int text_y = icon_y + (icon_image.height() - text_block_height) / 2;
    const int workers_text_y = text_y + line_height + line_padding;

    icon_image.draw(icon_x, icon_y);
    int width = text_draw_number(b->house_population, '@', " ", text_x, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    width += lang_text_draw("main_strings.127.20", text_x + width, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));

    if (b->house_population_room < 0) {
        width += text_draw_number(-b->house_population_room, '@', " ",
            text_x + width, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        width += lang_text_draw("main_strings.127.21", text_x + width, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else if (b->house_population_room > 0 && !b->has_plague) {
        width += lang_text_draw("main_strings.127.22", text_x + width, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        width += text_draw_number(b->house_population_room, '@', " ",
            text_x + width, text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
    width = text_draw_number(
        building_local_workforce_house_available_workers(c->building), '@', " ", text_x, workers_text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    text_draw(string_from_ascii("available workers"), text_x + width, workers_text_y, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
}

static void draw_tax_info(building_info_context *c, int y_offset)
{
    building *b = building_get(c->building.id());
    if (b->house_tax_coverage) {
        int pct = calc_adjust_with_percentage(b->tax_income_or_storage / 2, city_finance_tax_percentage());
        int width = lang_text_draw("main_strings.127.24", c->x_offset + 36, y_offset, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        width += lang_text_draw_amount(current_string_amount_key(8, 0, pct), pct, c->x_offset + 36 + width, y_offset, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw("main_strings.127.25", c->x_offset + 36 + width, y_offset, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw("main_strings.127.23", c->x_offset + 36, y_offset, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
}

static void draw_happiness_info(building_info_context *c, int y_offset)
{
    building *b = building_get(c->building.id());
    int happiness = b->sentiment.house_happiness;
    static const translation_key sentiment_keys[] = {
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_1",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_2",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_3",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_4",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_5",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_6",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_7",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_8",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_9",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_10",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_11",
        "TR_BUILDING_WINDOW_HOUSE_SENTIMENT_12"
    };
    int sentiment_index = 0;
    if (happiness > 0) {
        sentiment_index = calc_bound(happiness / 10 + 1, 1, 11);
    }
    text_draw(translation_for(sentiment_keys[sentiment_index]), c->x_offset + 36, y_offset, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);

    int message = b->house_sentiment_message;
    switch (message) {
        case LOW_MOOD_CAUSE_NO_JOBS:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_UPSET_UNEMPLOYMENT"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case LOW_MOOD_CAUSE_HIGH_TAXES:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_UPSET_HIGH_TAXES"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case LOW_MOOD_CAUSE_LOW_WAGES:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_UPSET_LOW_WAGES"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case LOW_MOOD_CAUSE_SQUALOR:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_UPSET_SQUALOR"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case SUGGEST_MORE_ENT:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_SUGGEST_ENTERTAINMENT"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case SUGGEST_MORE_FOOD:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_SUGGEST_FOOD"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        case SUGGEST_MORE_DESIRABILITY:
            text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_SUGGEST_DESIRABILITY"),
                c->x_offset + 36, y_offset + 20, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            break;
        default:
            break;
    }

    if (city_sentiment_get_blessing_festival_boost() > 3) {
        text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_RECENT_EVENT_POSITIVE"),
            c->x_offset + 36, y_offset + 40, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    } else if (city_sentiment_get_blessing_festival_boost() < -3) {
        text_draw(translation_for_key("TR_BUILDING_WINDOW_HOUSE_RECENT_EVENT_NEGATIVE"),
            c->x_offset + 36, y_offset + 40, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
}

void window_building_draw_house(building_info_context *c)
{
    c->advisor_button = ADVISOR_HOUSING;
    c->help_id = 56;
    building *b = building_get(c->building.id());
    if (b->house_population <= 0) {
        draw_vacant_lot(c);
        return;
    }
    window_building_play_sound(c, "wavs/housing.wav");
    int level = building_house_legacy_level(Building(b));
    if (level < HOUSE_MIN) {
        level = HOUSE_SMALL_TENT;
    }
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(current_string_key(29, level), c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    inner_panel_draw(c->x_offset + 16, c->y_offset + 128, c->width_blocks - 2, 13);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 136);

    draw_population_info(c, c->y_offset + 134);
    draw_tax_info(c, c->y_offset + 174);
    draw_happiness_info(c, c->y_offset + 194);

    int x_offset = 32;
    int y_content = 259;
    int y_amount = 263;

    // food inventory
    const model_house *house_model = building_house_get_model(Building(b));
    if (house_model && house_model->food_types) {
        const resource_list *list = city_resource_get_available_foods();
        int total_food_types = 0;
        int total_food_amount = 0;
        if (list->size > 4) {
            for (unsigned int i = 0; i < list->size; i++) {
                resource_type r = list->items[i];
                if (resource_is_food(r) && b->resources[r]) {
                    total_food_types++;
                    total_food_amount += b->resources[r];
                }
            }
        }
        if (total_food_types > 5) {
            x_offset += lang_text_draw("TR_BUILDING_INFO_TOTAL_FOOD",
                c->x_offset + x_offset, c->y_offset + y_content, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            x_offset += text_draw_number(total_food_amount, '@', " ", c->x_offset + x_offset, c->y_offset + y_content,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            x_offset += text_draw(string_from_ascii("("), c->x_offset + x_offset, c->y_offset + y_content,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            for (unsigned int i = 0; i < list->size; i++) {
                resource_type r = list->items[i];
                if (!resource_is_food(r) || !b->resources[r]) {
                    continue;
                }
                const ImageGroupEntryRef &icon = resource_graphics(r).panel_icon();
                int base_width = (25 - icon.width()) / 2;
                int base_height = (25 - icon.height()) / 2;
                icon.draw(c->x_offset + x_offset + base_width, c->y_offset + y_content + base_height);
                x_offset += icon.width() + 6;
            }
            text_draw(string_from_ascii(")"), c->x_offset + x_offset, c->y_offset + y_content + 2,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        } else {
            for (unsigned int i = 0; i < list->size; i++) {
                resource_type r = list->items[i];
                if (!resource_is_food(r) || (list->size > 4 && !b->resources[r])) {
                    continue;
                }
                const ImageGroupEntryRef &icon = resource_graphics(r).panel_icon();
                int base_width = (25 - icon.width()) / 2;
                int base_height = (25 - icon.height()) / 2;
                icon.draw(c->x_offset + x_offset + base_width, c->y_offset + y_content + base_height);
                text_draw_number(b->resources[r], '@', " ",
                    c->x_offset + x_offset + 25, c->y_offset + y_amount + 2, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                x_offset += 85;
            }
        }
    } else {
        // no food necessary
        lang_text_draw_multiline("main_strings.127.33", c->x_offset + x_offset + 4, c->y_offset + y_content, BLOCK_SIZE * (c->width_blocks - 6), FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
    // goods inventory
    x_offset = 32;
    y_content += 35;
    y_amount += 35;

    for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
        const resource_type r = static_cast<resource_type>(resource);
        if (!resource_is_inventory_good(r)) {
            continue;
        }
        const ImageGroupEntryRef &icon = resource_graphics(r).panel_icon();
        int base_width = (25 - icon.width()) / 2;
        int base_height = (25 - icon.height()) / 2;
        icon.draw(c->x_offset + x_offset + base_width, c->y_offset + y_content + base_height);
        text_draw_number(b->resources[r], '@', " ",
            c->x_offset + x_offset + 25, c->y_offset + y_amount + 2, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        x_offset += 85;
    }

    if (b->has_plague) {
        lang_text_draw_multiline("TR_BUILDING_HOUSE_DISEASE_DESC",
            c->x_offset + 32, c->y_offset + 56, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else if (b->data.house.evolve_text_id == 62) {
        int width = lang_text_draw(current_string_key(127, 40 + b->data.house.evolve_text_id), c->x_offset + 32, c->y_offset + 56, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
        width += text_draw(lang_get_building_type_string(c->worst_desirability_building_type), c->x_offset + 32 + width, c->y_offset + 56, FONT_NORMAL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_PLAIN)->line_height), COLOR_FONT_RED);
        text_draw((uint8_t *) ")", c->x_offset + 32 + width, c->y_offset + 56, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        lang_text_draw_multiline(current_string_key(127, 41 + b->data.house.evolve_text_id), c->x_offset + 32, c->y_offset + 72, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else if (b->data.house.evolve_text_id == 67) { // latrine devolve
        lang_text_draw_multiline("TR_BUILDING_LATRINES_MISSING_DEVOLVE",
            c->x_offset + 32, c->y_offset + 56, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else if (b->data.house.evolve_text_id == 68) { // latrine evolve
        lang_text_draw_multiline("TR_BUILDING_LATRINES_MISSING_EVOLVE",
            c->x_offset + 32, c->y_offset + 56, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else {
        lang_text_draw_multiline(current_string_key(127, 40 + b->data.house.evolve_text_id), c->x_offset + 32, c->y_offset + 56, BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }
}

const uint8_t *window_building_house_get_tooltip(const building_info_context *c)
{
    const mouse *m = mouse_get();

    if (m->x <  c->x_offset + 16 || m->x > c->x_offset + (c->width_blocks - 2) * BLOCK_SIZE ||
        m->y < c->y_offset + 270 || m->y > c->y_offset + 295) {
        return 0;
    }

    building *b = building_get(c->building.id());

    const model_house *house_model = building_house_get_model(c->building);
    if (!house_model || !house_model->food_types) {
        return 0;
    }
    const resource_list *list = city_resource_get_available_foods();
    if (list->size <= 4) {
        return 0;
    }
    int total_food_types = 0;

    if (list->size > 4) {
        for (unsigned int i = 0; i < list->size; i++) {
            resource_type r = list->items[i];
            if (resource_is_food(r) && b->resources[r]) {
                total_food_types++;
            }
        }
    }
    if (total_food_types <= 4) {
        return 0;
    }
    static uint8_t text[400];
    uint8_t *cursor = text;
    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];
        if (resource_is_food(r) && b->resources[r]) {
            if (cursor != text) {
                cursor = string_copy(string_from_ascii("\n"), cursor, 400 - (cursor - text));
            }
            cursor += string_from_int(cursor, b->resources[r], 0);
            cursor = string_copy(string_from_ascii(" "), cursor, 400 - (cursor - text));
            cursor = string_copy(resource_get_data(r)->text, cursor, 400 - (cursor - text));
        }
    }

    return text;
}
