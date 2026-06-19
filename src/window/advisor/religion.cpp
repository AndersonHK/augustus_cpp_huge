#include "building/count.h"
#include "building/building_type_registry_internal.h"
#include "city/festival.h"
#include "city/houses.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "window/hold_festival.h"

#include "religion.h"

#include "city/god.h"
#include "graphics/advisor_text_button_widget.h"
#include "graphics/ui_runtime.h"

#include "game/settings.h"

#include "assets/assets.h"
#include "building/building_type_api.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"

static void button_hold_festival(const generic_button *button);
static void draw_hold_festival_widget(void);

static generic_button hold_festival_button[] = {
    {102, 340, 300, 20, button_hold_festival},
};

static unsigned int focus_button_id;

static building_type type_from_attr(const char *text_id)
{
    return building_type_registry_impl::type_from_attr(text_id);
}

static int get_religion_advice(void)
{
    int least_happy = city_god_least_happy();
    const house_demands *demands = city_houses_demands();
    if (least_happy >= 0 && city_god_wrath_bolts(least_happy) > 4) {
        return 6 + least_happy;
    } else if (demands->religion == 1) {
        return demands->requiring.religion ? 1 : 0;
    } else if (demands->religion == 2) {
        return 2;
    } else if (demands->religion == 3) {
        return 3;
    } else if (!demands->requiring.religion) {
        return 4;
    } else if (least_happy >= 0) {
        return 6 + least_happy;
    } else {
        return 5;
    }
}

static void draw_god_row(god_type god, int y_offset, building_type altar, building_type small_temple,
    building_type large_temple, building_type grand_temple)
{
    lang_text_draw(current_string_key(59, 11 + god), 24, y_offset + 2, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    lang_text_draw(current_string_key(59, 16 + god), 104, y_offset + 3, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));
    text_draw_number_centered(building_count_total(altar), 190, y_offset + 2, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(building_count_active(small_temple), 250, y_offset + 2, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    if (building_count_active(grand_temple)) {
        text_draw_number_centered(building_count_active(large_temple) + building_count_active(grand_temple),
            310, y_offset + 2, 50, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        text_draw_number_centered(building_count_active(large_temple), 310, y_offset + 2, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }
    text_draw_number_centered(city_god_months_since_festival(god), 375, y_offset + 2, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    int width = lang_text_draw(current_string_key(59, 32 + city_god_happiness(god) / 10), 450, y_offset + 2, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    int bolts = city_god_wrath_bolts(god);
    for (int i = 0; i < bolts / 10; i++) {
        Image::from_id(Image::group(GROUP_GOD_BOLT)).draw(10 * i + width + 450, y_offset - 2);
    }
    int happy_bolts = city_god_happy_bolts(god);
    for (int i = 0; i < happy_bolts; i++) {
        Image::from_id(assets_get_image_id("UI", "Happy God Icon")).draw(10 * i + width + 450, y_offset - 2);
    }
}

static void draw_oracle_row(void)
{
    building_type pantheon = type_from_attr("pantheon");
    int oracle_count = building_count_active(type_from_attr("oracle")) +
        building_count_active(type_from_attr("small_mausoleum"));
    int large_oracle_count = building_count_active(type_from_attr("nymphaeum")) +
        building_count_active(pantheon) + building_count_active(type_from_attr("large_mausoleum"));
    lang_text_draw("main_strings.59.8", 24, 168, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(building_count_total(type_from_attr("lararium")), 190, 168, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    text_draw_number_centered(oracle_count, 250, 168, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    if (building_count_active(pantheon)) {
        text_draw_number_centered(large_oracle_count, 310, 168, 50, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
    } else {
        text_draw_number_centered(large_oracle_count, 310, 168, 50, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }
}

static int get_festival_advice(void)
{
    int months_since_festival = city_festival_months_since_last();
    if (months_since_festival <= 1) {
        return 0;
    } else if (months_since_festival <= 6) {
        return 1;
    } else if (months_since_festival <= 12) {
        return 2;
    } else if (months_since_festival <= 18) {
        return 3;
    } else if (months_since_festival <= 24) {
        return 4;
    } else if (months_since_festival <= 30) {
        return 5;
    } else {
        return 6;
    }
}

static void draw_festival_info(void)
{
    inner_panel_draw(48, 302, 34, 6);
    Image::from_id(Image::group(GROUP_PANEL_WINDOWS) + 15).draw(460, 305);
    lang_text_draw("main_strings.58.17", 52, 274, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    int width = lang_text_draw_amount(current_string_amount_key(8, 4, city_festival_months_since_last()), city_festival_months_since_last(), 112, 315, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    lang_text_draw("main_strings.58.15", 112 + width, 315, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    if (city_festival_is_planned()) {
        lang_text_draw_centered("main_strings.58.34", 102, 339, 300, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
    }
    lang_text_draw_multiline(current_string_key(58, 18 + get_festival_advice()), 56, 360, 400, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
}

static int draw_background(void)
{
    int height_blocks;
    height_blocks = 27;
    outer_panel_draw(0, 0, 40, height_blocks);

    Image::from_id(Image::group(GROUP_ADVISOR_ICONS) + 9).draw(10, 10);

    lang_text_draw("main_strings.59.0", 60, 12, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height)); // Religion

    text_draw_centered(translation_for_key("TR_WINDOW_ADVISOR_RELIGION_ALTARS_HEADER"), 165, 46, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height), 0); // Altars
    lang_text_draw_centered("main_strings.59.5", 256, 32, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // Temples
    lang_text_draw_centered("main_strings.59.1", 226, 46, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // Small
    lang_text_draw_centered("main_strings.59.2", 285, 46, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // large
    lang_text_draw_centered("main_strings.59.6", 350, 18, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // Months
    lang_text_draw_centered("main_strings.59.9", 350, 32, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // since
    lang_text_draw_centered("main_strings.59.7", 350, 46, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // Festival
    lang_text_draw_centered("main_strings.59.3", 449, 46, 100, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height)); // The gods are

    inner_panel_draw(16, 60, 38, 8);

    // god rows
    draw_god_row(GOD_CERES, 66, type_from_attr("shrine_ceres"), type_from_attr("small_temple_ceres"),
        type_from_attr("large_temple_ceres"), type_from_attr("grand_temple_ceres"));
    draw_god_row(GOD_NEPTUNE, 86, type_from_attr("shrine_neptune"), type_from_attr("small_temple_neptune"),
        type_from_attr("large_temple_neptune"), type_from_attr("grand_temple_neptune"));
    draw_god_row(GOD_MERCURY, 106, type_from_attr("shrine_mercury"), type_from_attr("small_temple_mercury"),
        type_from_attr("large_temple_mercury"), type_from_attr("grand_temple_mercury"));
    draw_god_row(GOD_MARS, 126, type_from_attr("shrine_mars"), type_from_attr("small_temple_mars"),
        type_from_attr("large_temple_mars"), type_from_attr("grand_temple_mars"));
    draw_god_row(GOD_VENUS, 146, type_from_attr("shrine_venus"), type_from_attr("small_temple_venus"),
        type_from_attr("large_temple_venus"), type_from_attr("grand_temple_venus"));

    // oracles
    draw_oracle_row();

    city_gods_calculate_least_happy();

    lang_text_draw_multiline(current_string_key(59, 21 + get_religion_advice()), 52, 208, 540, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));

    draw_festival_info();

    return height_blocks;
}

static void draw_foreground(void)
{
    if (!city_festival_is_planned()) {
        draw_hold_festival_widget();
    }
}

static int handle_mouse(const mouse *m)
{
    return GenericButtonList(hold_festival_button, 1).handle_mouse(
        *m,
        0,
        0,
        &focus_button_id
    );
}

static void button_hold_festival(const generic_button *button)
{
    if (!city_festival_is_planned()) {
        window_hold_festival_show();
    }
}

static void draw_hold_festival_widget(void)
{
    UiPrimitives &primitives = shared_ui_runtime().primitives();

    UiTextSpec button_text = {};
    button_text.content_type = UiTextContentType::Language;
    button_text.alignment = UiTextAlignment::Center;
    button_text.text_group = 58;
    button_text.text_id = 16;
    button_text.x = 102;
    button_text.y = 339;
    button_text.box_width = 300;
    button_text.font = FONT_NORMAL_WHITE;

    AdvisorTextButtonWidget(primitives, 102, 335, 300, 20, focus_button_id == 1, button_text).draw();
}

static void get_tooltip_text(advisor_tooltip_result *r)
{
    if (focus_button_id) {
        r->text_id = 112;
    }
}

const advisor_window_type *window_advisor_religion(void)
{
    static const advisor_window_type window = {
        draw_background,
        draw_foreground,
        handle_mouse,
        get_tooltip_text
    };
    focus_button_id = 0;
    return &window;
}
