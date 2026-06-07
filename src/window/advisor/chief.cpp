#include "city/health.h"
#include "city/labor.h"
#include "city/migration.h"
#include "city/houses.h"
#include "city/sentiment.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"

#include "chief.h"

#include "translation/translation.h"
extern "C" {
#include "city/data_private.h"
#include "city/figures.h"
#include "city/finance.h"
#include "city/military.h"
#include "city/population.h"
#include "city/resource.h"
#include "core/calc.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "scenario/invasion.h"
#include "scenario/property.h"
}


#define ADVISOR_HEIGHT 26
#define X_OFFSET 225

static void draw_title(int y, int text_id)
{
    Image::from_id(Image::group(GROUP_BULLET)).draw(32, y + 1);
    lang_text_draw(current_string_key(61, text_id), 52, y, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
}

static int draw_background(void)
{
    int width;

    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    Image::from_id(Image::group(GROUP_ADVISOR_ICONS) + 11).draw(10, 10);

    lang_text_draw("main_strings.61.0", 60, 12, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    inner_panel_draw(24, 60, 37, 19);

    // workers
    draw_title(66, 1);
    if (city_labor_unemployment_percentage() > 0) {
        width = lang_text_draw("main_strings.61.12", X_OFFSET, 66, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        width += text_draw_percentage(city_labor_unemployment_percentage(), X_OFFSET + width, 66, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        text_draw_number(city_labor_workers_unemployed() - city_labor_workers_needed(), '(', ")",
            X_OFFSET + width, 66, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height), 0);
    } else if (city_labor_workers_needed() > 0) {
        width = lang_text_draw("main_strings.61.13", X_OFFSET, 66, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        lang_text_draw_amount(current_string_amount_key(8, 12, city_labor_workers_needed()), city_labor_workers_needed(), X_OFFSET + width, 66, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.14", X_OFFSET, 66, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // finance
    draw_title(86, 2);
    int treasury = city_finance_treasury();
    int balance_last_year = city_finance_overview_last_year()->balance;
    if (treasury > balance_last_year) {
        width = lang_text_draw("main_strings.61.15", X_OFFSET, 86, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        text_draw_money(treasury - balance_last_year, X_OFFSET + width, 86, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (treasury < balance_last_year) {
        width = lang_text_draw("main_strings.61.16", X_OFFSET, 86, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        text_draw_money(balance_last_year - treasury, X_OFFSET + width, 86, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.17", X_OFFSET, 86, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // migration
    draw_title(106, 3);
    if (city_figures_total_invading_enemies() > 3) {
        lang_text_draw("main_strings.61.79", X_OFFSET, 106, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (city_migration_newcomers() >= 5) {
        lang_text_draw("main_strings.61.25", X_OFFSET, 106, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (city_migration_no_room_for_immigrants()) {
        lang_text_draw("main_strings.61.18", X_OFFSET, 106, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_migration_percentage() >= 80) {
        lang_text_draw("main_strings.61.25", X_OFFSET, 106, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        int text_group = 61;
        int text_id;
        translation_key text_key;
        switch (city_migration_no_immigration_cause()) {
            case NO_IMMIGRATION_LOW_WAGES: text_id = 19; break;
            case NO_IMMIGRATION_NO_JOBS: text_id = 20; break;
            case NO_IMMIGRATION_NO_FOOD: text_id = 21; break;
            case NO_IMMIGRATION_HIGH_TAXES: text_id = 22; break;
            case NO_IMMIGRATION_MANY_TENTS: text_id = 70; break;
            case NO_IMMIGRATION_LOW_MOOD: text_id = 71; break;
            case NO_IMMIGRATION_SQUALOR:
                text_key = "TR_ADVISOR_CHIEF_NO_IMMIGRATION_SQUALOR";
                text_id = 0;
                break;
            default: text_id = 0; break;
        }
        if (text_key) {
            lang_text_draw(text_key, X_OFFSET, 106, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        } else if (text_id) {
            lang_text_draw(current_string_key(text_group, text_id), X_OFFSET, 106, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        }
    }

    // housing capacity
    Image::from_id(Image::group(GROUP_BULLET)).draw(32, 126 + 1);
    text_draw(translation_for_key("TR_HEADER_HOUSING"), 52, 126, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    if (!city_population_open_housing_capacity()) {
        width = text_draw(translation_for_key("TR_ADVISOR_HOUSING_NO_ROOM"), X_OFFSET, 126, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    } else {
        width = text_draw(translation_for_key("TR_ADVISOR_HOUSING_ROOM"), X_OFFSET, 126, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
        text_draw_number(city_population_open_housing_capacity(), '@', " ", X_OFFSET + width, 126, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    }

    // food stocks
    draw_title(146, 4);
    if (scenario_property_rome_supplies_wheat()) {
        lang_text_draw("main_strings.61.26", X_OFFSET, 146, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (city_resource_food_supply_months() > 0) {
        width = lang_text_draw("main_strings.61.28", X_OFFSET, 146, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        lang_text_draw_amount(current_string_amount_key(8, 4, city_resource_food_supply_months()), city_resource_food_supply_months(), X_OFFSET + width, 146, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        lang_text_draw("main_strings.61.27", X_OFFSET, 146, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    }

    // food consumption
    draw_title(166, 62);
    if (scenario_property_rome_supplies_wheat()) {
        lang_text_draw("main_strings.61.26", X_OFFSET, 166, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        int pct = city_resource_food_percentage_produced();
        if (pct > 150) {
            lang_text_draw("main_strings.61.63", X_OFFSET, 166, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        } else if (pct > 105) {
            lang_text_draw("main_strings.61.64", X_OFFSET, 166, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        } else if (pct > 95) {
            lang_text_draw("main_strings.61.65", X_OFFSET, 166, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        } else if (pct > 75) {
            lang_text_draw("main_strings.61.66", X_OFFSET, 166, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        } else if (pct > 30) {
            lang_text_draw("main_strings.61.67", X_OFFSET, 166, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        } else if (pct > 0) {
            lang_text_draw("main_strings.61.68", X_OFFSET, 166, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        } else {
            lang_text_draw("main_strings.61.69", X_OFFSET, 166, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        }
    }

    // military
    draw_title(186, 5);

    translation_key food_text;
    int food_stress = city_data.mess_hall.food_stress_cumulative;

    if (food_stress > 60) {
        food_text = "TR_ADVISOR_LEGION_FOOD_CRITICAL";
    } else if (food_stress > 40) {
        food_text = "TR_ADVISOR_LEGION_FOOD_NEEDED";
    }
    if (food_text && city_figures_soldiers() > 0) {
        text_draw(translation_for(food_text), X_OFFSET, 186, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height), 0);
    } else if (city_figures_imperial_soldiers()) {
        lang_text_draw("main_strings.61.76", X_OFFSET, 186, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_figures_enemies()) {
        lang_text_draw("main_strings.61.75", X_OFFSET, 186, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (scenario_invasion_exists_upcoming()) {
        lang_text_draw("main_strings.61.74", X_OFFSET, 186, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_military_distant_battle_roman_army_is_traveling()) {
        lang_text_draw("main_strings.61.78", X_OFFSET, 186, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (city_military_months_until_distant_battle() > 0) {
        lang_text_draw("main_strings.61.77", X_OFFSET, 186, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_figures_soldiers() > 0) { // FIXED was ">=0" (always true)
        lang_text_draw("main_strings.61.73", X_OFFSET, 186, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        lang_text_draw("main_strings.61.72", X_OFFSET, 186, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // crime
    draw_title(206, 6);
    if (city_figures_rioters()) {
        lang_text_draw("main_strings.61.33", X_OFFSET, 206, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_figures_criminals() > 10) {
        lang_text_draw("main_strings.61.32", X_OFFSET, 206, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_figures_criminals()) {
        lang_text_draw("main_strings.61.31", X_OFFSET, 206, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (city_figures_protesters() || city_sentiment_crime_cooldown()) {
        lang_text_draw("main_strings.61.30", X_OFFSET, 206, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.29", X_OFFSET, 206, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // health
    draw_title(226, 7);
    int health_rate = city_health();
    if (health_rate >= 40) {
        lang_text_draw(current_string_key(56, health_rate / 10 + 27), X_OFFSET, 226, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        lang_text_draw(current_string_key(56, health_rate / 10 + 27), X_OFFSET, 226, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    }

    // education
    house_demands *demands = city_houses_demands();
    draw_title(246, 8);
    if (demands->education == 1) {
        lang_text_draw("main_strings.61.39", X_OFFSET, 246, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (demands->education == 2) {
        lang_text_draw("main_strings.61.40", X_OFFSET, 246, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (demands->education == 3) {
        lang_text_draw("main_strings.61.41", X_OFFSET, 246, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.42", X_OFFSET, 246, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // religion
    draw_title(266, 9);
    if (demands->religion == 1) {
        lang_text_draw("main_strings.61.46", X_OFFSET, 266, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (demands->religion == 2) {
        lang_text_draw("main_strings.61.47", X_OFFSET, 266, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (demands->religion == 3) {
        lang_text_draw("main_strings.61.48", X_OFFSET, 266, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.49", X_OFFSET, 266, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // entertainment
    draw_title(286, 10);
    if (demands->entertainment == 1) {
        lang_text_draw("main_strings.61.43", X_OFFSET, 286, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (demands->entertainment == 2) {
        lang_text_draw("main_strings.61.44", X_OFFSET, 286, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else {
        lang_text_draw("main_strings.61.45", X_OFFSET, 286, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }

    // sentiment
    draw_title(306, 11);
    int sentiment = city_sentiment();
    if (sentiment <= 0) {
        lang_text_draw("main_strings.61.50", X_OFFSET, 306, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    } else if (sentiment >= 100) {
        lang_text_draw("main_strings.61.61", X_OFFSET, 306, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        lang_text_draw(current_string_key(61, sentiment / 10 + 51), X_OFFSET, 306, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    }
    // water coverage
    Image::from_id(Image::group(GROUP_BULLET)).draw(32, 326 + 1);
    lang_text_draw("TR_ADVISOR_CHIEF_WATER_COVERAGE", 52, 326, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    int population = city_population();
    int box_width = 37 * BLOCK_SIZE - X_OFFSET;
    if (calc_percentage(city_health_get_population_with_water_access(), population) > 50) {
        lang_text_draw_multiline("TR_ADVISOR_CHIEF_CLEAN_WATER", X_OFFSET, 326, box_width, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (calc_percentage(city_health_get_population_with_latrines_access(), population) > 50 &&
        calc_percentage(city_health_get_population_with_well_access(), population) > 50) {
        lang_text_draw_multiline("TR_ADVISOR_CHIEF_LATRINE_AND_WELL", X_OFFSET, 326, box_width, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else if (calc_percentage(city_health_get_population_with_well_access(), population) > 50) {
        lang_text_draw_multiline("TR_ADVISOR_CHIEF_WELL_WATER", X_OFFSET, 326, box_width, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        lang_text_draw_multiline("TR_ADVISOR_CHIEF_NO_WATER", X_OFFSET, 326, box_width, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
    }
    
    return ADVISOR_HEIGHT;
}

const advisor_window_type *window_advisor_chief(void)
{
    static const advisor_window_type window = {
        draw_background,
        0,
        0,
        0
    };
    return &window;
}
