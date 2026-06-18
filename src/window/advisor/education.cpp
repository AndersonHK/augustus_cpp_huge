#include "building/count.h"
#include "building/building_type_registry_internal.h"
#include "translation/translation.h"
#include "city/culture.h"
#include "city/houses.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"

#include "education.h"

extern "C" {
#include "building/building_type_api.h"
#include "city/population.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
}


#define ADVISOR_HEIGHT 17

static building_type type_from_attr(const char *text_id)
{
    return building_type_registry_impl::type_from_attr(text_id);
}

static int get_education_advice(void)
{
    const house_demands *demands = city_houses_demands();
    if (demands->education == 1) {
        return demands->requiring.school ? 1 : 0;
    } else if (demands->education == 2) {
        return demands->requiring.library ? 3 : 2;
    } else if (demands->education == 3) {
        return 4;
    }
    int advice_id;
    int coverage_school = city_culture_coverage_school();
    int coverage_academy = city_culture_coverage_academy();
    int coverage_library = city_culture_coverage_library();
    if (!demands->requiring.school) {
        advice_id = 5; // no demands yet
    } else if (!demands->requiring.library) {
        if (coverage_school >= 100 && coverage_academy >= 100) {
            advice_id = 6; // education is perfect
        } else if (coverage_school <= coverage_academy) {
            advice_id = 7; // build more schools
        } else {
            advice_id = 8; // build more academies
        }
    } else {
        // all education needed
        if (coverage_school >= 100 && coverage_academy >= 100 && coverage_library >= 100) {
            advice_id = 6;
        } else if (coverage_school <= coverage_academy && coverage_school <= coverage_library) {
            advice_id = 7; // build more schools
        } else if (coverage_academy <= coverage_school && coverage_academy <= coverage_library) {
            advice_id = 8; // build more academies
        } else if (coverage_library <= coverage_school && coverage_library <= coverage_academy) {
            advice_id = 9; // build more libraries
        } else {
            advice_id = 6; // unlikely event that all coverages are equal
        }
    }
    return advice_id;
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    Image::from_id(Image::group(GROUP_ADVISOR_ICONS) + 7).draw(10, 10);
    lang_text_draw("main_strings.57.0", 60, 12, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height)); // Education

    // x population, y school age, z academy age
    int width = text_draw_number(city_population(), '@', " ", 60, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    width += lang_text_draw("main_strings.57.1", 60 + width, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_number(city_population_school_age(), '@', " ", 60 + width, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    width += lang_text_draw("main_strings.57.2", 60 + width, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    width += text_draw_number(city_population_academy_age(), '@', " ", 60 + width, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    lang_text_draw("main_strings.57.3", 60 + width, 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    // table headers
    lang_text_draw_centered("main_strings.57.4", 139, 86, 160, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // Working
    lang_text_draw("main_strings.57.5", 287, 86, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));               // Can educate
    lang_text_draw_centered("main_strings.57.6", 440, 86, 160, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // City coverage

    inner_panel_draw(32, 100, 36, 5);

    // schools
    building_type school = type_from_attr("school");
    lang_text_draw_amount(current_string_amount_key(8, 18, building_count_total(school)), building_count_total(school), 40, 105, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(building_count_active(school), 170, 105, 100, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    width = text_draw_number(city_culture_module_capacity(building_type_registry_impl::CultureModuleType::School), '@', " ", 280, 105, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    lang_text_draw("main_strings.57.7", 280 + width, 105, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    int pct_school = city_culture_coverage_school();
    if (pct_school == 0) {
        lang_text_draw_centered("main_strings.57.10", 420, 105, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else if (pct_school < 100) {
        lang_text_draw_centered(current_string_key(57, pct_school / 10 + 11), 420, 105, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else {
        lang_text_draw_centered("main_strings.57.21", 420, 105, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }

    // academies
    building_type academy = type_from_attr("academy");
    lang_text_draw_amount(current_string_amount_key(8, 20, building_count_total(academy)), building_count_total(academy), 40, 125, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(building_count_active(academy), 170, 125, 100, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    width = text_draw_number(city_culture_module_capacity(building_type_registry_impl::CultureModuleType::Academy), '@', " ", 280, 125, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    lang_text_draw("main_strings.57.8", 280 + width, 125, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    int pct_academy = city_culture_coverage_academy();
    if (pct_academy == 0) {
        lang_text_draw_centered("main_strings.57.10", 420, 125, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else if (pct_academy < 100) {
        lang_text_draw_centered(current_string_key(57, pct_academy / 10 + 11), 420, 125, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else {
        lang_text_draw_centered("main_strings.57.21", 420, 125, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }

    // libraries
    building_type library = type_from_attr("library");
    lang_text_draw_amount(current_string_amount_key(8, 22, building_count_total(library)), building_count_total(library), 40, 145, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(building_count_active(library), 170, 145, 100, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    width = text_draw_number(city_culture_module_capacity(building_type_registry_impl::CultureModuleType::Library), '@', " ", 280, 145, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    lang_text_draw("main_strings.57.9", 280 + width, 145, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    int pct_library = city_culture_coverage_library();
    if (pct_library == 0) {
        lang_text_draw_centered("main_strings.57.10", 420, 145, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else if (pct_library < 100) {
        lang_text_draw_centered(current_string_key(57, pct_library / 10 + 11), 420, 145, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else {
        lang_text_draw_centered("main_strings.57.21", 420, 145, 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }

    // Mission posts
    building_type mission_post = type_from_attr("mission_post");
    int count = building_count_total(mission_post);
    width = text_draw_number(count, ' ', " ", 40, 165, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    if (count == 1) {
        text_draw(lang_get_building_type_string(mission_post), 40 + width, 165, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    } else {
        text_draw(translation_for_key("TR_WINDOW_ADVISOR_EDUCATION_MISSION_POSTS"), 40 + width, 165, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }

    text_draw_number_centered(building_count_active(mission_post), 170, 165, 100, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));


    lang_text_draw_multiline(current_string_key(57, 22 + get_education_advice()), 45, 195, 560, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    return ADVISOR_HEIGHT;
}

const advisor_window_type *window_advisor_education(void)
{
    static const advisor_window_type window = {
        draw_background,
        0,
        0,
        0
    };
    return &window;
}
