#include "city/festival.h"
#include "game/resource_graphics.h"
#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"
#include "window/advisor/entertainment.h"
#include "window/hold_games.h"
#include "window/race_bet.h"

#include "translation/translation.h"
#include "window/building/culture.h"
#include "window/building/military.h"
#include "window/option_popup.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "building/caravanserai.h"
#include "building/distribution.h"
#include "building/market.h"

extern "C" {
#include "assets/assets.h"
#include "building/monument.h"
#include "city/buildings.h"
#include "city/constants.h"
#include "city/data_private.h"
#include "city/finance.h"
#include "city/trade_policy.h"
#include "core/dir.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "scenario/allowed_building.h"
#include "sound/speech.h"
}

#define GOD_PANTHEON 5
#define MODULE_COST 1000

static void button_add_module_prompt(const generic_button *button);
static void button_hold_games(const generic_button *button);
static void button_race_bet(const generic_button *button);


static void draw_temple(building_info_context *c, const char *sound_file, int group_id);

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int building_is_xml_type(const building *b, const char *text_id)
{
    return b && b->type == runtime_type(text_id);
}

static int building_accepts_resource(building *b, resource_type resource)
{
    return Building(b).accepts_good(resource);
}

static const building_type_registry_impl::Distribution *distribution_for(building *b)
{
    return Building(b).type().distribution();
}

static generic_button add_module_button[] = {
    { 0, 0, 304, 20, button_add_module_prompt}
};

static generic_button race_bet_button[] = {
    {0, 0, 300, 20, button_race_bet}
};

static generic_button hold_games_button[] = {
    { 0, 0, 300, 20, button_hold_games}
};

static const ImageGroupEntryRef HORSE_TEAM_IMAGES[] = {
    ImageGroupEntryRef::from_group("UI\\Hipp_Team_Blue", "Hipp_Team_Blue"),
    ImageGroupEntryRef::from_group("UI\\Hipp_Team_Blue", "Image_0020"),
    ImageGroupEntryRef::from_group("UI\\Hipp_Team_Blue", "Image_0021"),
    ImageGroupEntryRef::from_group("UI\\Hipp_Team_Blue", "Image_0022"),
};

static struct {
    unsigned int focus_button_id;
    int building_id;
    int god_id;
    int module_choices[2];
} data;

static struct {
    option_menu_item option;
    int requires_any_fort;
    const char *required_building_text_id;
    const char *image_id;
} temple_module_options[12] = {
    {
        { "TR_BUILDING_GRAND_TEMPLE_CERES_DESC_MODULE_1", "TR_BUILDING_GRAND_TEMPLE_CERES_MODULE_1_DESC" },
        0,
        nullptr,
        "Ceres M Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_CERES_DESC_MODULE_2", "TR_BUILDING_GRAND_TEMPLE_CERES_MODULE_2_DESC" },
        0,
        nullptr,
        "Ceres M2 Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC_MODULE_1", "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_MODULE_1_DESC" },
        0,
        "hippodrome",
        "Nept M2 Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC_MODULE_2", "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_MODULE_2_DESC" },
        0,
        nullptr,
        "Nept M Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC_MODULE_1", "TR_BUILDING_GRAND_TEMPLE_MERCURY_MODULE_1_DESC" },
        1,
        nullptr,
        "Merc M Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC_MODULE_2", "TR_BUILDING_GRAND_TEMPLE_MERCURY_MODULE_2_DESC" },
        0,
        nullptr,
        "Merc M2 Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_MARS_DESC_MODULE_1", "TR_BUILDING_GRAND_TEMPLE_MARS_MODULE_1_DESC" },
        1,
        nullptr,
        "Mars M2 Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_MARS_DESC_MODULE_2", "TR_BUILDING_GRAND_TEMPLE_MARS_MODULE_2_DESC" },
        0,
        nullptr,
        "Mars M Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_VENUS_DESC_MODULE_1", "TR_BUILDING_GRAND_TEMPLE_VENUS_MODULE_1_DESC" },
        0,
        nullptr,
        "Venus M Icon"
    },
    {
        { "TR_BUILDING_GRAND_TEMPLE_VENUS_DESC_MODULE_2", "TR_BUILDING_GRAND_TEMPLE_VENUS_MODULE_2_DESC" },
        0,
        nullptr,
        "Venus M2 Icon"
    },
    {
        { "TR_BUILDING_PANTHEON_DESC_MODULE_1", "TR_BUILDING_PANTHEON_MODULE_1_DESC" },
        0,
        nullptr,
        "Panth M Icon"
    },
    {
        { "TR_BUILDING_PANTHEON_DESC_MODULE_2", "TR_BUILDING_PANTHEON_MODULE_2_DESC" },
        0,
        nullptr,
        "Panth M2 Icon"
    }
};

static int scenario_allows_any_fort(void)
{
    for (int type = BUILDING_NONE + 1; type < BUILDING_TYPE_MAX; type++) {
        building_type building_type_id = static_cast<building_type>(type);
        if (building_is_fort(building_type_id) && scenario_allowed_building(building_type_id)) {
            return 1;
        }
    }
    return 0;
}

static int temple_module_option_is_allowed(int index)
{
    if (temple_module_options[index].requires_any_fort) {
        return scenario_allows_any_fort();
    }

    const char *required_building_text_id = temple_module_options[index].required_building_text_id;
    if (!required_building_text_id) {
        return 1;
    }

    building_type required_building = runtime_type(required_building_text_id);
    return required_building != BUILDING_NONE && scenario_allowed_building(required_building);
}

static void draw_culture_info(building_info_context *c, int help_id, const char *sound_file, int group_id)
{
    c->advisor_button = ADVISOR_HEALTH;
    c->help_id = help_id;
    window_building_play_sound(c, sound_file);
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(group_id, 0, c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (building_get(c->building_id)->num_workers <= 0) {
        window_building_draw_description(c, group_id, 2);
    } else {
        window_building_draw_description(c, group_id, 3);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 116, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 120);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 124);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 126, group_id, 1);
}

void window_building_draw_clinic(building_info_context *c)
{
    draw_culture_info(c, 65, "wavs/clinic.wav", 81);

}

void window_building_draw_hospital(building_info_context *c)
{
    draw_culture_info(c, 66, "wavs/hospital.wav", 82);
}

void window_building_draw_bathhouse(building_info_context *c)
{
    c->advisor_button = ADVISOR_HEALTH;
    c->help_id = 64;
    window_building_play_sound(c, "wavs/baths.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(83, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building_id);
    if (!b->has_water_access) {
        window_building_draw_description(c, 83, 4);
    } else if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, 83, 2);
    } else {
        window_building_draw_description(c, 83, 3);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 116, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 120);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 124);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 126, 83, 1);
}

void window_building_draw_barber(building_info_context *c)
{
    draw_culture_info(c, 63, "wavs/barber.wav", 84);
}

void window_building_draw_school(building_info_context *c)
{
    c->advisor_button = ADVISOR_EDUCATION;
    c->help_id = 68;
    window_building_play_sound(c, "wavs/school.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(85, 0, c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (building_get(c->building_id)->num_workers <= 0) {
        window_building_draw_description(c, 85, 2);
    } else if (building_get(c->building_id)->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_SCHOOL_UPGRADE_DESC");
    } else {
        window_building_draw_description(c, 85, 3);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 136, 85, 1);
}

void window_building_draw_academy(building_info_context *c)
{
    c->advisor_button = ADVISOR_EDUCATION;
    c->help_id = 69;
    window_building_play_sound(c, "wavs/academy.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(86, 0, c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (building_get(c->building_id)->num_workers <= 0) {
        window_building_draw_description(c, 86, 2);
    } else if (building_get(c->building_id)->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_ACADEMY_UPGRADE_DESC");
    } else {
        window_building_draw_description(c, 86, 3);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 136, 86, 1);
}

void window_building_draw_library(building_info_context *c)
{
    c->advisor_button = ADVISOR_EDUCATION;
    c->help_id = 70;
    window_building_play_sound(c, "wavs/library.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(87, 0, c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (building_get(c->building_id)->num_workers <= 0) {
        window_building_draw_description(c, 87, 2);
    } else if (building_get(c->building_id)->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_LIBRARY_UPGRADE_DESC");
    } else {
        window_building_draw_description(c, 87, 3);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 136, 87, 1);
}

static void draw_temple_info(building_info_context *c, int image_offset)
{
    if (!c->has_road_access) {
        window_building_draw_description_at(c, 55, 69, 25);
        return;
    }

    building *b = building_get(c->building_id);
    if (building_is_ceres_temple(b->type) && building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD)) {
        resource_type food = city_resource_ceres_temple_food();
        font_t font = building_accepts_resource(b, food) ?
            FONT_NORMAL_BLACK : FONT_NORMAL_RED;
        resource_graphics(food).panel_icon().draw(c->x_offset + 112, c->y_offset + 60);
        text_draw_number(b->resources[food], '@', " ",
            c->x_offset + 132, c->y_offset + 60, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
        resource_graphics(resource_oil()).panel_icon().draw(c->x_offset + 202, c->y_offset + 60);
        font = building_accepts_resource(b, resource_oil()) ? FONT_NORMAL_BLACK : FONT_NORMAL_RED;
        text_draw_number(b->resources[resource_oil()], '@', " ",
            c->x_offset + 222, c->y_offset + 60, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
        text_draw_multiline(translation_for_key("TR_BUILDING_CERES_TEMPLE_MODULE_DESC"),
            c->x_offset + 112, c->y_offset + 90, BLOCK_SIZE * c->width_blocks - 132, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        Image::from_id(image_offset + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 16, c->y_offset + 45);
        return;
    }

    if (building_is_venus_temple(b->type) && building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
        font_t font = building_accepts_resource(b, resource_wine()) ? FONT_NORMAL_BLACK : FONT_NORMAL_RED;
        resource_graphics(resource_wine()).panel_icon().draw(c->x_offset + 112, c->y_offset + 60);
        text_draw_number(b->resources[resource_wine()], '@', " ",
            c->x_offset + 132, c->y_offset + 60, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);
        text_draw_multiline(translation_for_key("TR_BUILDING_VENUS_TEMPLE_MODULE_DESC"),
            c->x_offset + 112, c->y_offset + 90, BLOCK_SIZE * c->width_blocks - 132, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        Image::from_id(image_offset + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + BLOCK_SIZE, c->y_offset + 45);

        return;
    }

    int x_offset = 112;
    if (building_is_mars_temple(b->type) && building_monument_gt_module_is_active(MARS_MODULE_1_MESS_HALL)) {
        const resource_list *list = city_resource_get_potential_foods();
        for (unsigned int i = 0; i < list->size; i++) {
            resource_type r = list->items[i];
            if (!resource_is_inventory(r) || (list->size > 4 && !b->resources[r])) {
                continue;
            }
            resource_graphics(r).panel_icon().draw(c->x_offset + x_offset, c->y_offset + 60);
            text_draw_number(b->resources[r], '@', " ",
                c->x_offset + x_offset + 20, c->y_offset + 66, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

            x_offset += 70;
        }

        if (city_buildings_get_mess_hall()) {
            text_draw_multiline(translation_for_key("TR_BUILDING_MARS_TEMPLE_MODULE_DESC"),
                c->x_offset + 112, c->y_offset + 90,
                BLOCK_SIZE * c->width_blocks - 132, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            text_draw_multiline(translation_for_key("TR_BUILDING_MARS_TEMPLE_MODULE_DESC_NO_MESS"),
                c->x_offset + 112, c->y_offset + 90,
                BLOCK_SIZE * c->width_blocks - 132, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }

        Image::from_id(image_offset + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 16, c->y_offset + 45);

        return;
    }

    Image::from_id(image_offset + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

void window_building_draw_temple_ceres(building_info_context *c)
{
    building *b = building_get(c->building_id);

    draw_temple(c, "wavs/temple_farm.wav", 92);
    if (b->monument.phase <= 0) {
        draw_temple_info(c, 21);
    }
}

void window_building_draw_temple_neptune(building_info_context *c)
{
    building *b = building_get(c->building_id);

    draw_temple(c, "wavs/temple_ship.wav", 93);
    if (b->monument.phase <= 0) {
        draw_temple_info(c, 22);
    }
}

void window_building_draw_temple_mercury(building_info_context *c)
{
    building *b = building_get(c->building_id);

    draw_temple(c, "wavs/temple_comm.wav", 94);
    if (b->monument.phase <= 0) {
        draw_temple_info(c, 23);
    }
}

void window_building_draw_temple_mars(building_info_context *c)
{
    building *b = building_get(c->building_id);

    draw_temple(c, "wavs/temple_war.wav", 95);
    if (b->monument.phase <= 0) {
        draw_temple_info(c, 24);
    }
}

void window_building_draw_temple_venus(building_info_context *c)
{
    building *b = building_get(c->building_id);

    draw_temple(c, "wavs/temple_love.wav", 96);
    if (b->monument.phase <= 0) {
        draw_temple_info(c, 25);
    }
}

void window_building_draw_theater(building_info_context *c)
{
    c->advisor_button = ADVISOR_ENTERTAINMENT;
    c->help_id = 71;
    window_building_play_sound(c, "wavs/theatre.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(72, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    building *b = building_get(c->building_id);
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, 72, 4);
    } else if (b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_THEATRE_UPGRADE_DESC");
    } else if (!b->data.entertainment.num_shows) {
        window_building_draw_description(c, 72, 2);
    } else if (b->data.entertainment.days1) {
        window_building_draw_description(c, 72, 3);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 6);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    if (b->data.entertainment.days1 > 0) {
        int width = lang_text_draw(72, 6, c->x_offset + 32, c->y_offset + 182, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw_amount(8, 44, b->data.entertainment.days1,
            c->x_offset + width + 32, c->y_offset + 182, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw(72, 7 + b->data.entertainment.play,
            c->x_offset + 32, c->y_offset + 202, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw(72, 5, c->x_offset + 32, c->y_offset + 182, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 100, 72, 1);
}

void window_building_draw_amphitheater(building_info_context *c)
{
    c->advisor_button = ADVISOR_ENTERTAINMENT;
    c->help_id = 72;
    // The file name has a typo
    window_building_play_sound(c, "wavs/ampitheatre.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(71, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    building *b = building_get(c->building_id);
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, 71, 6);
    } else if (!b->data.entertainment.num_shows && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_AMPHITHEATER_DESC_UPGRADED_NO_SHOWS");
    } else if (!b->data.entertainment.num_shows) {
        window_building_draw_description(c, 71, 2);
    } else if (b->data.entertainment.num_shows == 2 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_AMPHITHEATER_DESC_UPGRADED_BOTH_SHOWS");
    } else if (b->data.entertainment.num_shows == 2) {
        window_building_draw_description(c, 71, 3);
    } else if (b->data.entertainment.days1 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_AMPHITHEATER_DESC_UPGRADED_NO_ACTORS");
    } else if (b->data.entertainment.days1) {
        window_building_draw_description(c, 71, 4);
    } else if (b->data.entertainment.days2 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_AMPHITHEATER_DESC_UPGRADED_NO_GLADIATORS");
    } else if (b->data.entertainment.days2) {
        window_building_draw_description(c, 71, 5);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 168, c->width_blocks - 2, 7);
    window_building_draw_employment(c, 172);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 176);
    if (b->data.entertainment.days1 > 0) {
        int width = lang_text_draw(71, 8, c->x_offset + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw_amount(8, 44, b->data.entertainment.days1,
            c->x_offset + width + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw(71, 7, c->x_offset + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
    if (b->data.entertainment.days2 > 0) {
        int width = lang_text_draw(71, 10, c->x_offset + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw_amount(8, 44, b->data.entertainment.days2,
            c->x_offset + width + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw(72, 7 + b->data.entertainment.play,
            c->x_offset + 32, c->y_offset + 254, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw(71, 9, c->x_offset + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 90, 71, 1);
}

static void draw_entertainment_school(building_info_context *c, const char *sound_file, int group_id)
{
    c->advisor_button = ADVISOR_ENTERTAINMENT;
    c->help_id = 75;
    window_building_play_sound(c, sound_file);

    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(group_id, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (building_get(c->building_id)->num_workers <= 0) {
        window_building_draw_description(c, group_id, 7);
    } else if (c->worker_percentage >= 100) {
        window_building_draw_description(c, group_id, 2);
    } else if (c->worker_percentage >= 75) {
        window_building_draw_description(c, group_id, 3);
    } else if (c->worker_percentage >= 50) {
        window_building_draw_description(c, group_id, 4);
    } else if (c->worker_percentage >= 25) {
        window_building_draw_description(c, group_id, 5);
    } else {
        window_building_draw_description(c, group_id, 6);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 116, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 120);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 124);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 92, group_id, 1);
}

void window_building_draw_actor_colony(building_info_context *c)
{
    draw_entertainment_school(c, "wavs/art_pit.wav", 77);
}

void window_building_draw_gladiator_school(building_info_context *c)
{
    draw_entertainment_school(c, "wavs/glad_pit.wav", 75);
}

void window_building_draw_lion_house(building_info_context *c)
{
    draw_entertainment_school(c, "wavs/lion_pit.wav", 76);
}

void window_building_draw_chariot_maker(building_info_context *c)
{
    draw_entertainment_school(c, "wavs/char_pit.wav", 78);
}

static void window_building_draw_monument_large_temple_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_LARGE_TEMPLE_PHASE_1",
        "TR_BUILDING_LARGE_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_oracle_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_ORACLE_PHASE_1",
        "TR_BUILDING_ORACLE_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_nymphaeum_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_NYMPHAEUM_PHASE_1",
        "TR_BUILDING_NYMPHAEUM_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_small_mausoleum_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1",
        "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_large_mausoleum_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1",
        "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_temple_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_GRAND_TEMPLE_PHASE_1",
        "TR_BUILDING_GRAND_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_colosseum_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_COLOSSEUM_PHASE_1",
        "TR_BUILDING_COLOSSEUM_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void window_building_draw_monument_hippodrome_construction_process(building_info_context *c)
{
    window_building_draw_monument_construction_process(c, "TR_BUILDING_HIPPODROME_PHASE_1",
        "TR_BUILDING_HIPPODROME_PHASE_1_TEXT", "TR_BUILDING_MONUMENT_CONSTRUCTION_DESC");
}

static void draw_grand_temple_venus_wine(building_info_context *c)
{
    int y = 50;
    data.building_id = c->building_id;
    resource_graphics(resource_wine()).panel_icon().draw(c->x_offset + 24, c->y_offset + y - 5);
    building *b = building_get(c->building_id);
    if (b->resources[resource_wine()] < 1) {
        lang_text_draw_amount(8, 10, 0, c->x_offset + 52, c->y_offset + y, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else {
        lang_text_draw_amount(8, 10, b->resources[resource_wine()], c->x_offset + 52, c->y_offset + y, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }

}

static void draw_grand_temple_mars_military(building_info_context *c)
{
    int y = 60;
    data.building_id = c->building_id;
    resource_graphics(resource_weapons()).panel_icon().draw(c->x_offset + 25, c->y_offset + y - 5);
    building *b = building_get(c->building_id);
    if (b->resources[resource_weapons()] < 1) {
        lang_text_draw_amount(8, 10, 0, c->x_offset + 52, c->y_offset + y, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else {
        lang_text_draw_amount(8, 10, b->resources[resource_weapons()], c->x_offset + 52, c->y_offset + y, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }

    lang_text_draw("TR_WINDOW_BARRACKS_PRIORITY", c->x_offset + 25, c->y_offset + 88, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height)); // "Priority"

    inner_panel_draw(c->x_offset + 16, c->y_offset + 108, c->width_blocks - 2, 5);
    lang_text_draw("TR_WINDOW_BARRACKS_FORTS", c->x_offset + 50, c->y_offset + 115, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height)); // "Forts"
    lang_text_draw("TR_WINDOW_BARRACKS_TOWERS", c->x_offset + 327, c->y_offset + 115, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height)); // "Towers"
}

static void draw_temple(building_info_context *c, const char *sound_file, int group_id)
{
    c->help_id = 67;
    building *b = building_get(c->building_id);
    if (b->monument.phase <= 0) {
        c->advisor_button = ADVISOR_RELIGION;
        window_building_play_sound(c, sound_file);
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        lang_text_draw_centered(group_id, 0, c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 150);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, group_id, 1);
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        lang_text_draw_centered(group_id, 0, c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        window_building_draw_monument_large_temple_construction_process(c);
    }
}

void window_building_draw_oracle(building_info_context *c)
{
    c->help_id = 67;
    building *b = building_get(c->building_id);
    if (!building_monument_is_unfinished_monument(b)) {
        c->advisor_button = ADVISOR_RELIGION;
        window_building_play_sound(c, "wavs/oracle.wav");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        lang_text_draw_centered(110, 0, c->x_offset, c->y_offset + 12, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 148, 110, 1);
        inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 150);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
        text_draw_multiline(translation_for_key("TR_BUILDING_ORACLE_DESC"),
            c->x_offset + 32, c->y_offset + 56, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        lang_text_draw_centered(110, 0, c->x_offset, c->y_offset + 12, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        window_building_draw_monument_oracle_construction_process(c);
    }
}

void window_building_draw_lararium(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/oracle.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_LARARIUM"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, 56, "TR_WINDOW_BUILDING_LARARIUM_DESC");
}

void window_building_draw_shrine_ceres(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/temple_farm.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_SHRINE_CERES"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, 92, 1);
    Image::from_id(21 + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

void window_building_draw_shrine_neptune(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/temple_ship.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_SHRINE_NEPTUNE"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, 93, 1);
    Image::from_id(22 + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

void window_building_draw_shrine_mercury(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/temple_comm.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_SHRINE_MERCURY"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, 94, 1);
    Image::from_id(23 + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

void window_building_draw_shrine_mars(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/temple_war.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_SHRINE_MARS"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, 95, 1);
    Image::from_id(24 + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

void window_building_draw_shrine_venus(building_info_context *c)
{
    c->advisor_button = ADVISOR_RELIGION;
    c->help_id = 67;
    window_building_play_sound(c, "wavs/temple_love.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_SHRINE_VENUS"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 116, 96, 1);
    Image::from_id(25 + Image::group(GROUP_PANEL_WINDOWS)).draw(c->x_offset + 180, c->y_offset + 45);
}

static void draw_grand_temple(building_info_context *c, const char *sound_file,
    translation_key name, translation_key bonus_desc, int banner_id, translation_key quote,
    int temple_god_id, int extra_y)
{
    building *b = building_get(c->building_id);
    window_building_play_sound(c, sound_file);
    data.god_id = temple_god_id;
    if (b->monument.phase == MONUMENT_FINISHED) {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        c->advisor_button = ADVISOR_RELIGION;
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        window_building_draw_monument_temple_construction_process(c);
    }
    if (b->monument.upgrades) {
        const translation_key module_name = temple_module_options[data.god_id * 2 + (b->monument.upgrades - 1)].option.header;
        text_draw_centered(translation_for(module_name),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    } else {
        text_draw_centered(translation_for(name),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    }
    int height = 0;
    if (b->monument.phase == MONUMENT_FINISHED) {
        if (building_monument_has_labour_problems(b)) {
            height = text_draw_multiline(translation_for_key("TR_BUILDING_GRAND_TEMPLE_NEEDS_WORKERS"),
                c->x_offset + 22, c->y_offset + 56 + extra_y, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            height = text_draw_multiline(translation_for(bonus_desc),
                c->x_offset + 22, c->y_offset + 56 + extra_y, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            if (b->monument.upgrades) {
                const translation_key module_desc = temple_module_options[data.god_id * 2 + (b->monument.upgrades - 1)].option.desc;
                height += text_draw_multiline(translation_for(module_desc),
                    c->x_offset + 22, c->y_offset + 66 + height + extra_y, 15 * c->width_blocks,
                    0, FONT_NORMAL_GREEN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_GREEN)->line_height), 0);
            }
        }
        if (Building(b).type().is_grand_temple_mars()) {
            draw_grand_temple_mars_military(c);
        } else if (Building(b).type().is_grand_temple_venus() &&
            (building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE))) {
            draw_grand_temple_venus_wine(c);
        }
        inner_panel_draw(c->x_offset + 16, c->y_offset + 86 + height + extra_y, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 90 + height + extra_y);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76,
            c->y_offset + 94 + height + extra_y);
        if (c->height_blocks > 26) {
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 166 + height + extra_y);
            Image::from_id(banner_id).draw(c->x_offset + 37, c->y_offset + 171 + height + extra_y);
            text_draw_multiline(translation_for(quote),
                c->x_offset + 10, c->y_offset + 386 + height + extra_y, BLOCK_SIZE * c->width_blocks - 16,
                1, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
    }
}

void window_building_draw_grand_temple_foreground(building_info_context *c)
{
    building *b = building_get(c->building_id);
    if (b->monument.phase != MONUMENT_FINISHED) {
        return;
    }
    if (!b->monument.upgrades) {
        button_border_draw(c->x_offset + 80, c->y_offset + 16 * c->height_blocks - 37,
            16 * (c->width_blocks - 10), 20, data.focus_button_id == 1 ? 1 : 0);
        text_draw_centered(translation_for_key("TR_BUILDING_GRAND_TEMPLE_ADD_MODULE"),
            c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 33,
            16 * (c->width_blocks - 10), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
    if (Building(b).type().is_grand_temple_mars()) {
        window_building_draw_priority_buttons(c->x_offset + 50, c->y_offset + 135, b->id);
        window_building_draw_delivery_buttons(c->x_offset + 392, c->y_offset + 40, b->id);
    }
}

int window_building_handle_mouse_grand_temple(const mouse *m, building_info_context *c)
{
    building *b = building_get(c->building_id);
    data.building_id = c->building_id;
    if (b->monument.phase != MONUMENT_FINISHED || b->monument.upgrades) {
        return 0;
    }
    if (GenericButtonList(add_module_button, 1).handle_mouse(
        *m,
        c->x_offset + 80,
        c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        &data.focus_button_id
    )) {
        return 1;
    }
    return 0;
}

void window_building_draw_grand_temple_ceres(building_info_context *c)
{
    draw_grand_temple(c, "wavs/temple_farm.wav", "TR_BUILDING_GRAND_TEMPLE_CERES_DESC",
        "TR_BUILDING_GRAND_TEMPLE_CERES_BONUS_DESC",
        assets_get_image_id("UI", "Ceres L Banner"),
        "TR_BUILDING_CERES_TEMPLE_QUOTE", GOD_CERES, 0);
}

void window_building_draw_grand_temple_neptune(building_info_context *c)
{
    draw_grand_temple(c, "wavs/temple_ship.wav", "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_DESC",
        "TR_BUILDING_GRAND_TEMPLE_NEPTUNE_BONUS_DESC",
        assets_get_image_id("UI", "Nept L Banner"),
        "TR_BUILDING_NEPTUNE_TEMPLE_QUOTE", GOD_NEPTUNE, 0);
}

void window_building_draw_grand_temple_mercury(building_info_context *c)
{
    draw_grand_temple(c, "wavs/temple_comm.wav", "TR_BUILDING_GRAND_TEMPLE_MERCURY_DESC",
        "TR_BUILDING_GRAND_TEMPLE_MERCURY_BONUS_DESC",
        assets_get_image_id("UI", "Merc L Banner"),
        "TR_BUILDING_MERCURY_TEMPLE_QUOTE", GOD_MERCURY, 0);
}

void window_building_draw_grand_temple_mars(building_info_context *c)
{
    draw_grand_temple(c, "wavs/temple_war.wav", "TR_BUILDING_GRAND_TEMPLE_MARS_DESC",
        "TR_BUILDING_GRAND_TEMPLE_MARS_BONUS_DESC",
        assets_get_image_id("UI", "Mars L Banner"),
        "TR_BUILDING_MARS_TEMPLE_QUOTE", GOD_MARS, 150);
}

void window_building_draw_grand_temple_venus(building_info_context *c)
{
    draw_grand_temple(c, "wavs/temple_love.wav", "TR_BUILDING_GRAND_TEMPLE_VENUS_DESC",
        "TR_BUILDING_GRAND_TEMPLE_VENUS_BONUS_DESC",
        assets_get_image_id("UI", "Venus L Banner"),
        "TR_BUILDING_VENUS_TEMPLE_QUOTE", GOD_VENUS, 20);
}

void window_building_draw_pantheon(building_info_context *c)
{
    draw_grand_temple(c, "wavs/oracle.wav", "TR_BUILDING_PANTHEON_DESC", "TR_BUILDING_PANTHEON_BONUS_DESC",
        assets_get_image_id("UI", "Panth L Banner"),
        "TR_BUILDING_PANTHEON_QUOTE", GOD_PANTHEON, 0);
}

void window_building_draw_work_camp(building_info_context *c)
{
    window_building_play_sound(c, "wavs/tower4.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    text_draw_centered(translation_for_key("TR_BUILDING_WORK_CAMP"),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    if (!c->has_road_access) {
        window_building_draw_description_at(c, 76, 69, 25);
    } else
        text_draw_multiline(translation_for_key("TR_BUILDING_WORK_CAMP_DESC"),
            c->x_offset + 32, c->y_offset + 76, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
}

void window_building_draw_architect_guild(building_info_context *c)
{
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Engineer.ogg");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    text_draw_centered(translation_for_key("TR_BUILDING_ARCHITECT_GUILD"),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    if (!c->has_road_access) {
        window_building_draw_description_at(c, 76, 69, 25);
    } else
        text_draw_multiline(translation_for_key("TR_BUILDING_ARCHITECT_GUILD_DESC"),
            c->x_offset + 32, c->y_offset + 76, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
}

void window_building_draw_tavern(building_info_context *c)
{
    c->advisor_button = ADVISOR_ENTERTAINMENT;
    building *b = building_get(c->building_id);
    const building_type_registry_impl::Distribution *distribution = distribution_for(b);

    window_building_play_sound(c, "wavs/market3.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);

    text_draw_centered(translation_for_key("TR_BUILDING_TAVERN"), c->x_offset, c->y_offset + 12,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);

    int x_offset = 32;
    int has_primary_resource = 0;
    int has_secondary_resource = 0;
    int saw_primary_resource = 0;
    if (distribution) {
        for (const building_type_registry_impl::DistributionResourceRule &rule : distribution->resources()) {
            resource_type resource = rule.resource;
            if (!resource_is_inventory(resource)) {
                continue;
            }
            resource_graphics(resource).panel_icon().draw(c->x_offset + x_offset, c->y_offset + 60);
            font_t font = building_accepts_resource(b, resource) ? FONT_NORMAL_BLACK : FONT_NORMAL_RED;
            text_draw_number(b->resources[resource], '@', " ", c->x_offset + x_offset + 25, c->y_offset + 66, font, screen_ui_to_pixel(font_definition_for(font)->line_height), 0);

            if (!saw_primary_resource) {
                has_primary_resource = b->resources[resource] > 0;
                saw_primary_resource = 1;
            } else if (b->resources[resource] > 0) {
                has_secondary_resource = 1;
            }
            x_offset += 85;
        }
    }

    if (!c->has_road_access) {
        window_building_draw_description_at(c, 96, 69, 25);
    } else if (b->num_workers <= 0) {
        text_draw_multiline(translation_for_key("TR_BUILDING_TAVERN_DESC_1"),
            c->x_offset + 32, c->y_offset + 96, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else if (!has_primary_resource) {
        text_draw_multiline(translation_for_key("TR_BUILDING_TAVERN_DESC_2"),
            c->x_offset + 32, c->y_offset + 96, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else if (!has_secondary_resource) {
        translation_key string_key = "TR_BUILDING_TAVERN_DESC_3";
        if (b->upgrade_level) {
            string_key = "TR_BUILDING_TAVERN_DESC_UPGRADED_WINE_NO_FOOD";
        }
        text_draw_multiline(translation_for(string_key),
            c->x_offset + 32, c->y_offset + 96, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        translation_key string_key = "TR_BUILDING_TAVERN_DESC_4";
        if (b->upgrade_level) {
            string_key = "TR_BUILDING_TAVERN_DESC_UPGRADED_WINE_AND_FOOD";
        }
        text_draw_multiline(translation_for(string_key),
            c->x_offset + 32, c->y_offset + 96, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 188, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 192);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 196);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 110, "TR_BUILDING_TAVERN_DESC_5");
    return;
}

static void draw_games_info(building_info_context *c, int y_offset)
{
    inner_panel_draw(c->x_offset + 16, c->y_offset + y_offset + 214, c->width_blocks - 2, 6);
    window_entertainment_draw_games_text(c->x_offset + 32, c->y_offset + y_offset + 224, 1);
}

int window_building_handle_mouse_colosseum(const mouse *m, building_info_context *c)
{
    building *b = building_get(c->building_id);
    data.building_id = c->building_id;
    if (b->monument.phase != MONUMENT_FINISHED) {
        return 0;
    }
    if (GenericButtonList(hold_games_button, 1).handle_mouse(
        *m,
        c->x_offset + 88,
        c->y_offset + (c->height_blocks > 27 ? 535 : 335),
        &data.focus_button_id
    )) {
        return 1;
    }
    return 0;
}

void window_building_draw_colosseum_background(building_info_context *c)
{
    c->help_id = 73;
    building *b = building_get(c->building_id);
    int active_games = city_festival_games_active();
    int is_arena = building_is_xml_type(b, "arena");
    int is_colosseum = building_is_xml_type(b, "colosseum");

    if (is_arena || b->monument.phase == MONUMENT_FINISHED) {
        window_building_play_sound(c, "wavs/colloseum.wav");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        inner_panel_draw(c->x_offset + 16, c->y_offset + 130, c->width_blocks - 2, 7);
        window_building_draw_employment(c, 134);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 138);
        if (b->data.entertainment.days1 > 0) {
            int width = lang_text_draw(74, 8, c->x_offset + 32, c->y_offset + 184, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            lang_text_draw_amount(8, 44, b->data.entertainment.days1,
                c->x_offset + width + 32, c->y_offset + 184, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        } else {
            lang_text_draw(74, 7, c->x_offset + 32, c->y_offset + 184, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }
        if (b->data.entertainment.days2 > 0) {
            int width = lang_text_draw(74, 10, c->x_offset + 32, c->y_offset + 203, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            lang_text_draw_amount(8, 44, b->data.entertainment.days2,
                c->x_offset + width + 32, c->y_offset + 203, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        } else {
            lang_text_draw(74, 9, c->x_offset + 32, c->y_offset + 203, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }

        if (is_arena) {
            text_draw_centered(translation_for_key("TR_BUILDING_ARENA"),
                c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        } else {
            lang_text_draw_centered(74, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        }

        // todo: better link of venue to game
        if (is_colosseum && building_monument_has_labour_problems(b)) {
            text_draw_multiline(translation_for_key("TR_BUILDING_COLOSSEUM_NEEDS_WORKERS"),
                c->x_offset + 22, c->y_offset + 56, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            if (active_games && active_games <= 3 && is_colosseum) {
                static constexpr translation_key underway_games_text[] = {
                    {},
                    "TR_WINDOW_ADVISOR_ENTERTAINMENT_UNDERWAY_NG",
                    "TR_WINDOW_ADVISOR_ENTERTAINMENT_UNDERWAY_IG",
                    "TR_WINDOW_ADVISOR_ENTERTAINMENT_UNDERWAY_AG",
                };
                window_building_draw_description(c, underway_games_text[active_games]);
                int width = text_draw(translation_for_key("TR_WINDOW_BUILDING_GAMES_REMAINING_DURATION"),
                    c->x_offset + 32, c->y_offset + 221, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
                lang_text_draw_amount(8, 44, 2 * city_festival_games_remaining_duration(),
                    c->x_offset + width + 32, c->y_offset + 221, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));

            } else {
                if (!c->has_road_access) {
                    window_building_draw_description(c, 69, 25);
                } else if (b->num_workers <= 0) {
                    window_building_draw_description(c, 74, 6);
                } else if (!b->data.entertainment.num_shows) {
                    window_building_draw_description(c, 74, 2);
                } else if (b->data.entertainment.num_shows == 2) {
                    window_building_draw_description(c, 74, 3);
                } else if (b->data.entertainment.days1) {
                    window_building_draw_description(c, 74, 5);
                } else if (b->data.entertainment.days2) {
                    window_building_draw_description(c, 74, 4);
                }
            }
        }
        if (is_colosseum) {
            c->advisor_button = ADVISOR_ENTERTAINMENT;
            int extra_y_offset = 56;
            if (c->height_blocks > 27) {
                extra_y_offset += 200;
                ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + extra_y_offset);
                Image::from_id(assets_get_image_id("UI", "Colosseum Banner")).draw(c->x_offset + 37, c->y_offset + extra_y_offset + 5);
            }
            draw_games_info(c, extra_y_offset);
        }
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        lang_text_draw_centered(74, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
        window_building_draw_monument_colosseum_construction_process(c);
    }

}

void window_building_draw_colosseum_foreground(building_info_context *c)
{
    building *b = building_get(c->building_id);
    data.building_id = c->building_id;
    if (b->monument.phase != MONUMENT_FINISHED) {
        return;
    }

    if (!city_festival_games_active() && !city_festival_games_planning_time() && !city_festival_games_cooldown()) {
        button_border_draw(c->x_offset + 88, c->y_offset + (c->height_blocks > 27 ? 535 : 335),
            300, 20, data.focus_button_id == 1);
        text_draw_centered(translation_for_key("TR_WINDOW_ADVISOR_ENTERTAINMENT_GAMES_BUTTON"),
            c->x_offset + 88, c->y_offset + (c->height_blocks > 27 ? 540 : 340), 300, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
}

void window_building_draw_arena(building_info_context *c)
{
    c->advisor_button = ADVISOR_ENTERTAINMENT;
    c->help_id = 73;
    building *b = building_get(c->building_id);

    window_building_play_sound(c, "wavs/colloseum.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 168, c->width_blocks - 2, 7);
    window_building_draw_employment(c, 172);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 176);
    text_draw_centered(translation_for_key("TR_BUILDING_ARENA"),
        c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    if (b->data.entertainment.days1 > 0) {
        int width = lang_text_draw(74, 8, c->x_offset + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw_amount(8, 44, b->data.entertainment.days1,
            c->x_offset + width + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw(74, 7, c->x_offset + 32, c->y_offset + 214, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }

    if (b->data.entertainment.days2 > 0) {
        int width = lang_text_draw(74, 10, c->x_offset + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        lang_text_draw_amount(8, 44, b->data.entertainment.days2,
            c->x_offset + width + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    } else {
        lang_text_draw(74, 9, c->x_offset + 32, c->y_offset + 234, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_ARENA_CLOSED");
    } else if (!b->data.entertainment.num_shows && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_ARENA_DESC_UPGRADED_NO_SHOWS");
    } else if (!b->data.entertainment.num_shows) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_ARENA_NO_SHOWS");
    } else if (b->data.entertainment.num_shows == 2 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_ARENA_DESC_UPGRADED_BOTH_SHOWS");
    } else if (b->data.entertainment.num_shows == 2) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_ARENA_BOTH_SHOWS");
    } else if (b->data.entertainment.days1 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_ARENA_DESC_UPGRADED_NO_GLADIATORS");
    } else if (b->data.entertainment.days1) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_ARENA_NEEDS_GLADIATORS");
    } else if (b->data.entertainment.days2 && b->upgrade_level) {
        window_building_draw_description(c, "TR_BUILDING_ARENA_DESC_UPGRADED_NO_LIONS");
    } else if (b->data.entertainment.days2) {
        window_building_draw_description(c, "TR_WINDOW_BUILDING_ARENA_NEEDS_LIONS");
    }
    if (building_is_xml_type(b, "arena")) {
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 80, 74, 1);
    }
}


int window_building_handle_mouse_hippodrome(const mouse *m, building_info_context *c)
{
    building *b = building_get(c->building_id);
    data.building_id = c->building_id;
    if (b->monument.phase != MONUMENT_FINISHED) {
        return 0;
    }
    if (!city_data.games.chosen_horse && GenericButtonList(race_bet_button, 1).handle_mouse(
        *m,
        c->x_offset + 88,
        c->y_offset + (c->height_blocks > 27 ? 603 : 380),
        &data.focus_button_id
    )) {
        return 1;
    }
    return 0;
}

void window_building_draw_hippodrome_foreground(building_info_context *c)
{
    building *b = building_get(c->building_id);
    data.building_id = c->building_id;
    if (b->monument.phase != MONUMENT_FINISHED) {
        return;
    }

    if (!city_data.games.chosen_horse) {
        button_border_draw(c->x_offset + 88, c->y_offset + (c->height_blocks > 27 ? 603 : 380),
                           300, 20, data.focus_button_id == 1);
        text_draw_centered(translation_for_key("TR_WINDOW_RACE_BET_TITLE"),
            c->x_offset + 88, c->y_offset + (c->height_blocks > 27 ? 608 : 385), 300, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
}

void window_building_draw_hippodrome_background(building_info_context *c)
{
    c->help_id = 74;
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(73, 0, c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    building *b = building_get(c->building_id);
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_ENTERTAINMENT;
        window_building_play_sound(c, "wavs/hippodrome.wav");
        if (!c->has_road_access) {
            window_building_draw_description(c, 69, 25);
        } else if (building_monument_has_labour_problems(b)) {
            text_draw_multiline(translation_for_key("TR_BUILDING_HIPPODROME_NEEDS_WORKERS"),
                c->x_offset + 22, c->y_offset + 56, 15 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else if (!b->data.entertainment.num_shows) {
            window_building_draw_description(c, 73, 2);
        } else if (b->data.entertainment.days1) {
            window_building_draw_description(c, 73, 3);
        }

        inner_panel_draw(c->x_offset + 16, c->y_offset + 130, c->width_blocks - 2, 7);
        window_building_draw_employment(c, 134);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 138);
        if (b->data.entertainment.days1 > 0) {
            int width = lang_text_draw(73, 6, c->x_offset + 32, c->y_offset + 202, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            lang_text_draw_amount(8, 44, b->data.entertainment.days1,
                c->x_offset + width + 32, c->y_offset + 202, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        } else {
            lang_text_draw(73, 5, c->x_offset + 32, c->y_offset + 202, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }
        int y_offset = 33;
        if (c->height_blocks > 27) {
            y_offset += 223;
            int banner_id = assets_get_image_id("UI", "Circus Banner");
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + y_offset);
            Image::from_id(banner_id).draw(c->x_offset + 37, c->y_offset + y_offset + 5);
        }

        if (city_data.games.chosen_horse) {
            static const translation_key horse_description_keys[] = {
                "TR_WINDOW_RACE_BLUE_HORSE_DESCRIPTION",
                "TR_WINDOW_RACE_RED_HORSE_DESCRIPTION",
                "TR_WINDOW_RACE_WHITE_HORSE_DESCRIPTION",
                "TR_WINDOW_RACE_GREEN_HORSE_DESCRIPTION"
            };
            const int horse_index = city_data.games.chosen_horse - 1;
            text_draw_with_money(translation_for_key("TR_WINDOW_RACE_YOUR_BET"), city_data.games.bet_amount, " - ", "",
                c->x_offset + 32, c->y_offset + y_offset + 215, 438, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            HORSE_TEAM_IMAGES[horse_index].draw(c->x_offset + 37, c->y_offset + y_offset + 245);
            ImageBorder::image_small().draw(c->x_offset + 32, c->y_offset + y_offset + 240, COLOR_BORDER_RED);

            text_draw_multiline(translation_for(horse_description_keys[horse_index]),
                c->x_offset + 125, c->y_offset + y_offset + 240, 320,
                0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        text_draw_centered(translation_for(city_data.games.chosen_horse ? "TR_WINDOW_IN_PROGRESS_BET_BUTTON" :
            "TR_WINDOW_RACE_BET_TITLE"), c->x_offset + 88, c->y_offset + y_offset + 351, 300, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        window_building_draw_monument_hippodrome_construction_process(c);
    }
}

void window_building_draw_grand_garden(building_info_context *c)
{
    window_building_play_sound(c, "wavs/gardens3.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    text_draw_centered(translation_for_key("TR_BUILDING_GRAND_GARDEN"),
        c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
}

void window_building_draw_nymphaeum(building_info_context *c)
{
    c->help_id = 67;
    building *b = building_get(c->building_id);
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_RELIGION;
        window_building_play_sound(c, "wavs/oracle.wav");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_NYMPHAEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        text_draw_multiline(translation_for_key("TR_BUILDING_NYMPHAEUM_DESC"),
            c->x_offset + 32, c->y_offset + 56, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 150);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 148, 110, 1);
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_NYMPHAEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        window_building_draw_monument_nymphaeum_construction_process(c);
    }
}

void window_building_draw_small_mausoleum(building_info_context *c)
{
    c->help_id = 67;
    building *b = building_get(c->building_id);
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_RELIGION;
        window_building_play_sound(c, "wavs/oracle.wav");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_SMALL_MAUSOLEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        text_draw_multiline(translation_for_key("TR_BUILDING_SMALL_MAUSOLEUM_DESC"),
            c->x_offset + 32, c->y_offset + 56, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 150);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
        text_draw_multiline(translation_for_key("TR_BUILDING_BOTH_MAUSOLEUM_DESC"),
            c->x_offset + 32, c->y_offset + 236, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_SMALL_MAUSOLEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        window_building_draw_monument_small_mausoleum_construction_process(c);
    }
}

void window_building_draw_large_mausoleum(building_info_context *c)
{
    c->help_id = 67;
    building *b = building_get(c->building_id);
    if (b->monument.phase == MONUMENT_FINISHED) {
        c->advisor_button = ADVISOR_RELIGION;
        window_building_play_sound(c, "wavs/oracle.wav");
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_LARGE_MAUSOLEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        text_draw_multiline(translation_for_key("TR_BUILDING_LARGE_MAUSOLEUM_DESC"),
            c->x_offset + 32, c->y_offset + 56, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        inner_panel_draw(c->x_offset + 16, c->y_offset + 146, c->width_blocks - 2, 4);
        window_building_draw_employment(c, 150);
        window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 154);
        text_draw_multiline(translation_for_key("TR_BUILDING_BOTH_MAUSOLEUM_DESC"),
            c->x_offset + 32, c->y_offset + 236, 14 * c->width_blocks, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
        text_draw_centered(translation_for_key("TR_BUILDING_LARGE_MAUSOLEUM"),
            c->x_offset, c->y_offset + 12, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
        window_building_draw_monument_large_mausoleum_construction_process(c);
    }
}

static void add_module(int selection)
{
    if (!selection) {
        return;
    }
    sound_speech_play_file("wavs/oracle.wav");
    building *b = building_get(data.building_id);
    city_finance_process_construction(MODULE_COST);
    building_monument_add_module(b, data.module_choices[selection - 1]);
}

static void generate_module_image_id(int index)
{
    option_menu_item *option = &temple_module_options[index].option;
    if (option->image_id) {
        return;
    }
    option->image_id = assets_get_image_id("UI",
        temple_module_options[index].image_id);
}

static void button_add_module_prompt(const generic_button *button)
{
    int num_options = 0;
    int option_id = data.god_id * 2;

    static option_menu_item options[2];

    if (temple_module_option_is_allowed(option_id)) {
        generate_module_image_id(option_id);
        data.module_choices[num_options] = 1;
        options[num_options++] = temple_module_options[option_id].option;
    }
    if (temple_module_option_is_allowed(option_id + 1)) {
        generate_module_image_id(option_id + 1);
        data.module_choices[num_options] = 2;
        options[num_options++] = temple_module_options[option_id + 1].option;
    }

    if (num_options) {
        window_option_popup_show("TR_SELECT_EPITHET_PROMPT_HEADER", "TR_SELECT_EPITHET_PROMPT_TEXT",
            options, num_options, add_module, 0, 1000, OPTION_MENU_LARGE_ROW);
    }
}

static void button_hold_games(const generic_button *button)
{
    if (!city_festival_games_active() && !city_festival_games_planning_time() && !city_festival_games_cooldown()) {
        window_hold_games_show(1);
    }
}

static void button_race_bet(const generic_button *button)
{
    window_race_bet_show();
}
