#include "translation/translation.h"
#include "housing.h"

#include "building/count.h"
#include "building/building_type_registry_internal.h"
#include "building/house.h"
#include "building/house_population.h"
#include "building/properties.h"
#include "city/migration.h"
#include "city/population.h"
#include "city/resource.h"
#include "core/string.h"
#include "game/time.h"
#include "game/resource.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/lang_text.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "scenario/property.h"
#include "game/ResourceGraphics.h"
#include "graphics/image.h"

#define ADVISOR_HEIGHT 27

static building_type housing_type_for_level(int level_id)
{
    return building_type_registry_impl::building_type_for_housing_level(level_id, 1);
}

static void draw_housing_table(void)
{
    int y_offset = 68;
    int rows = 0;

    resource_list list = { 0 };
    for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
        resource_type r = static_cast<resource_type>(resource);
        if (resource_is_inventory_good(r)) {
            list.items[list.size++] = r;
        }
    }

    int x = 40;
    int total_residences = 0;
    int houses_using_goods[RESOURCE_SLOT_COUNT] = { 0 };

    const int housing_level_count = building_type_registry_impl::housing_type_level_count();
    for (int index = 0; index < housing_level_count; index++) {
        int level_id = building_type_registry_impl::housing_type_level_at(index);
        if (level_id < 0) {
            continue;
        }
        house_level level = static_cast<house_level>(level_id);
        building_type type = housing_type_for_level(level_id);
        if (type <= BUILDING_NONE) {
            continue;
        }
        int residences_at_level = building_count_active(type);
        if (!residences_at_level) {
            continue;
        }
        total_residences += residences_at_level;

        for (unsigned int i = 0; i < list.size; i++) {
            if (model_house_uses_inventory(level, list.items[i])) {
                houses_using_goods[list.items[i]] += residences_at_level;
            }
        }

        lang_text_draw(current_string_key(29, level_id), x, y_offset + (20 * rows), FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height));
        text_draw_number(residences_at_level, '@', " ", x + 180, y_offset + (20 * rows), FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
        if (rows == 11) {
            x += 280;
            rows = 0;
        } else {
            rows++;
        }
    }

    text_draw(translation_for_key("TR_ADVISOR_TOTAL_NUM_HOUSES"), 320, y_offset + 180, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    text_draw_number(total_residences, '@', " ", 500, y_offset + 180, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    text_draw(translation_for_key("TR_ADVISOR_AVAILABLE_HOUSING_CAPACITY"), 320, y_offset + 200, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    text_draw_number(city_population_open_housing_capacity(), '@', " ", 500, y_offset + 200, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    text_draw(translation_for_key("TR_ADVISOR_TOTAL_HOUSING_CAPACITY"), 320, y_offset + 220, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
    text_draw_number(city_population_total_housing_capacity(), '@', " ", 500, y_offset + 220, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);

    auto goods_label = [](resource_type resource) -> translation_key {
        if (resource == resource_pottery()) {
            return "TR_ADVISOR_RESIDENCES_USING_POTTERY";
        }
        if (resource == resource_furniture()) {
            return "TR_ADVISOR_RESIDENCES_USING_FURNITURE";
        }
        if (resource == resource_oil()) {
            return "TR_ADVISOR_RESIDENCES_USING_OIL";
        }
        if (resource == resource_wine()) {
            return "TR_ADVISOR_RESIDENCES_USING_WINE";
        }
        return {};
    };

    for (unsigned int i = 0; i < list.size; i++) {

        const ImageGroupEntryRef &icon = resource_graphics(list.items[i]).panel_icon();
        int base_width = (26 - icon.width()) / 2;
        int base_height = (26 - icon.height()) / 2;

        icon.draw(54 + base_width, y_offset + 260 + (23 * i) + base_height - 5);
        translation_key label = goods_label(list.items[i]);
        if (label) {
            text_draw(translation_for(label), 90, y_offset + 263 + (23 * i),
                FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            text_draw(resource_get_data(list.items[i])->text, 90, y_offset + 263 + (23 * i),
                FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        text_draw_number(houses_using_goods[list.items[i]], '@', " ", 499, y_offset + 263 + (23 * i),
            FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        icon.draw(550 + base_width, y_offset + 260 + (23 * i) + base_height - 5);
    }
}

static int draw_background(void)
{
    outer_panel_draw(0, 0, 40, ADVISOR_HEIGHT);
    inner_panel_draw(24, 60, 37, 16);

    text_draw(translation_for_key("TR_HEADER_HOUSING"), 60, 12, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    ImageGroupEntryRef::from_group("UI\\Housing_Advisor_Button", "Housing Advisor Button").draw(10, 10);

    int x_offset = text_get_number_width(city_population(), 0, "", FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    x_offset += lang_text_get_width("TR_ADVISOR_TOTAL_POPULATION", FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    x_offset = 620 - x_offset;

    int width = text_draw_number(city_population(), 0, "", x_offset, 25, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    text_draw(translation_for_key("TR_ADVISOR_TOTAL_POPULATION"), x_offset + width, 25, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

    draw_housing_table();

    return ADVISOR_HEIGHT;
}

const advisor_window_type *window_advisor_housing(void)
{
    static const advisor_window_type window = {
        draw_background
    };
    house_population_update_room();
    return &window;
}
