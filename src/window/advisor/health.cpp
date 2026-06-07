#include "building/count.h"
#include "translation/translation.h"
#include "city/culture.h"
#include "city/health.h"
#include "city/houses.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"

#include "health.h"

#include "graphics/advisor_text_button_widget.h"
#include "graphics/ui_runtime.h"

extern "C" {

#include "building/building_type_api.h"
#include "city/population.h"
#include "core/calc.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "graphics/window.h"
}

#define ADVISOR_HEIGHT 26

static unsigned int focus_button_id;
static int display_water_coverage = 0;

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static void button_water_buildings(const generic_button *button);
static void draw_coverage_toggle_widgets(void);

static generic_button generic_buttons[] = {
    {32, 104, 80, 20, button_water_buildings, 0, 0},
    {112, 104, 80, 20, button_water_buildings, 0, 1}
};

static int get_health_advice(void)
{
    house_demands *demands = city_houses_demands();
    switch (demands->health) {
        case 1:
            return demands->requiring.bathhouse ? 1 : 0;
        case 2:
            return demands->requiring.barber ? 3 : 2;
        case 3:
            return demands->requiring.clinic ? 5 : 4;
        case 4:
            return 6;
        default:
            return 7;
    }
}

static void draw_counted_building_name(building_type type, int count, int x, int y)
{
    int desc_offset_x = text_draw_number(count, ' ', " ", x, y, FONT_NORMAL_WHITE,
        screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    text_draw(lang_get_building_type_string(type), x + desc_offset_x, y, FONT_NORMAL_WHITE,
        screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
}

static void print_water_building_info(int y_offset, building_type type, int active_count, int population_served)
{
    int total_count = building_count_total(type);
    draw_counted_building_name(type, total_count, 40, y_offset);

    // working
    text_draw_number_centered(active_count, 180, y_offset, 100, FONT_NORMAL_WHITE,
        screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    // care for
    int width = text_draw_number(population_served, '@', " ", 305, y_offset, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    lang_text_draw(58, 5, 305 + width, y_offset, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    // city coverage
    text_draw_percentage_centered(calc_percentage(population_served, city_population()),
        440, y_offset, 160, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
}

static void print_health_building_info(int y_offset, building_type type, int population_served, int coverage)
{
    draw_counted_building_name(type, building_count_total(type), 40, y_offset);
    // working
    text_draw_number_centered(building_count_active(type), 180, y_offset, 100, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));

    // care for
    int width = text_draw_number(population_served, '@', " ", 305, y_offset, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    if (type == runtime_type("doctor") || type == runtime_type("hospital")) {
        lang_text_draw(56, 6, 305 + width, y_offset, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else {
        lang_text_draw(58, 5, 305 + width, y_offset, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }

    // city coverage
    if (coverage == 0) {
        lang_text_draw_centered(57, 10, 440, y_offset, 160, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else if (coverage < 100) {
        lang_text_draw_centered(57, coverage / 10 + 11, 440, y_offset, 160, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    } else {
        lang_text_draw_centered(57, 21, 440, y_offset, 160, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }
}

int window_advisor_health_get_rating_text_id(void)
{
    // the group id is 56
    return city_health() / 10 + 16;
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    Image::from_id(Image::group(GROUP_ADVISOR_ICONS) + 6).draw(10, 10);

    int sickness_level = city_health_get_global_sickness_level();

    lang_text_draw(56, 0, 60, 12, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height)); // City health

    int x_offset = lang_text_draw("TR_ADVISOR_HEALTH_RATING", 60, 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    text_draw_number(city_health(), 0, "", 60 + x_offset, 44, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

    if (city_population() >= 200) {
        lang_text_draw_multiline(56, city_health() / 10 + 16, 60, 65, 560, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else {
        lang_text_draw_multiline(56, 15, 60, 65, 560, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }
    lang_text_draw_centered(56, 3, 165, 110, 130, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));    // Working
    lang_text_draw(56, 4, 312, 110, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));                  // Care for
    lang_text_draw_centered(56, 5, 441, 110, 160, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));    // City coverage
    
    inner_panel_draw(32, 124, 36, 5);

    int population = city_population();
    if (display_water_coverage) {
        int people_covered = city_health_get_population_with_well_access();
        building_type well = building_type_registry_well_type();
        print_water_building_info(128, well, building_count_total(well), people_covered);

        people_covered = city_health_get_population_with_latrines_access();
        building_type latrines = runtime_type("latrines");
        print_water_building_info(148, latrines, building_count_active(latrines), people_covered);

        people_covered = city_health_get_population_with_water_access();
        building_type fountain = runtime_type("fountain");
        print_water_building_info(168, fountain, building_count_active(fountain), people_covered);
    } else {
        int people_covered = city_health_get_population_with_baths_access();
        print_health_building_info(128, runtime_type("bathhouse"), people_covered, calc_percentage(people_covered, population));

        people_covered = city_health_get_population_with_barber_access();
        print_health_building_info(148, runtime_type("barber"), people_covered, calc_percentage(people_covered, population));

        people_covered = city_health_get_population_with_clinic_access();
        building_type doctor = runtime_type("doctor");
        print_health_building_info(168, doctor, people_covered, calc_percentage(people_covered, population));

        building_type hospital = runtime_type("hospital");
        people_covered = 1000 * building_count_active(hospital);
        print_health_building_info(188, hospital, people_covered, city_culture_coverage_hospital());
    }
    int text_height = lang_text_draw_multiline(56, 7 + get_health_advice(), 45, 226, 560, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    lang_text_draw("TR_ADVISOR_HEALTH_SURVEILLANCE", 45, 246 + text_height, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    text_height += 16;
    static const translation_key sickness_level_keys[] = {
        "TR_ADVISOR_SICKNESS_LEVEL_LOW",
        "TR_ADVISOR_SICKNESS_LEVEL_MEDIUM",
        "TR_ADVISOR_SICKNESS_LEVEL_HIGH",
        "TR_ADVISOR_SICKNESS_LEVEL_PLAGUE"
    };
    sickness_level = calc_bound(sickness_level, 0, static_cast<int>(sizeof(sickness_level_keys) / sizeof(sickness_level_keys[0])) - 1);
    text_draw_multiline(translation_for(sickness_level_keys[sickness_level]),
        45, 246 + text_height, 560, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

    return ADVISOR_HEIGHT;
}

static void draw_foreground(void)
{
    draw_coverage_toggle_widgets();
}

static int handle_mouse(const mouse *m)
{
    return GenericButtonList(generic_buttons, 2).handle_mouse(
        *m,
        0,
        0,
        &focus_button_id
    );
}

static void button_water_buildings(const generic_button *button)
{
    display_water_coverage = button->parameter1;
    window_invalidate();
}

static void draw_coverage_toggle_widgets(void)
{
    UiPrimitives &primitives = shared_ui_runtime().primitives();

    UiTextSpec health_text = {};
    health_text.content_type = UiTextContentType::TranslationKey;
    health_text.alignment = UiTextAlignment::Center;
    health_text.text_key = "TR_ADVISOR_HEALTH_HEALTH_COVERAGE";
    health_text.x = generic_buttons[0].x;
    health_text.y = generic_buttons[0].y + 5;
    health_text.box_width = generic_buttons[0].width;
    health_text.font = FONT_SMALL_PLAIN;
    health_text.color = COLOR_BLACK;

    AdvisorTextButtonWidget(
        primitives,
        generic_buttons[0].x,
        generic_buttons[0].y,
        generic_buttons[0].width,
        generic_buttons[0].height,
        focus_button_id == 1 && display_water_coverage,
        health_text)
        .draw();

    UiTextSpec water_text = {};
    water_text.content_type = UiTextContentType::TranslationKey;
    water_text.alignment = UiTextAlignment::Center;
    water_text.text_key = "TR_ADVISOR_HEALTH_WATER_COVERAGE";
    water_text.x = generic_buttons[1].x;
    water_text.y = generic_buttons[1].y + 5;
    water_text.box_width = generic_buttons[1].width;
    water_text.font = FONT_SMALL_PLAIN;
    water_text.color = COLOR_BLACK;

    AdvisorTextButtonWidget(
        primitives,
        generic_buttons[1].x,
        generic_buttons[1].y,
        generic_buttons[1].width,
        generic_buttons[1].height,
        focus_button_id == 2 && !display_water_coverage,
        water_text)
        .draw();
}

const advisor_window_type *window_advisor_health(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        0
    };
    return &window;
}
