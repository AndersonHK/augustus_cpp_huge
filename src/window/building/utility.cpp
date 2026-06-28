#include "graphics/generic_button.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "map/building.h"
#include "map/water_supply.h"
#include "window/building_info.h"

#include "graphics/complex_button.h"
#include "translation/translation.h"
#include "utility.h"
#include "window/building/figures.h"
#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/roadblock.h"

#include "assets/assets.h"
#include "building/building_record.h"
#include "city/constants.h"
#include "city/finance.h"
#include "core/dir.h"
#include "core/image.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "graphics/window.h"

#include <stdlib.h>

static void button_go_to_orders(const generic_button *button);
static void button_toggle_figure_state(const generic_button *button);
static void button_roadblock_orders(const generic_button *button);

static struct {
    unsigned int focus_button_id;
    unsigned int orders_focus_button_id;
    unsigned int figure_focus_button_id;
    Building building = Building(nullptr);
    int tooltip_id = 0;
} data;

static generic_button go_to_orders_button[] = {
    {0, 0, 304, 20, button_go_to_orders},
};
complex_button repair_building_button[] = { 0 };

static generic_button orders_permission_buttons[] = {
    {0, 4, 210, 22, button_toggle_figure_state, 0, PERMISSION_MAINTENANCE},
    {0, 36, 210, 22, button_toggle_figure_state, 0, PERMISSION_PRIEST},
    {0, 68, 210, 22, button_toggle_figure_state, 0, PERMISSION_MARKET},
    {0, 100, 210, 22, button_toggle_figure_state, 0, PERMISSION_ENTERTAINER},
    {0, 132, 210, 22, button_toggle_figure_state, 0, PERMISSION_EDUCATION},
    {0, 164, 210, 22, button_toggle_figure_state, 0, PERMISSION_MEDICINE},
    {0, 192, 210, 22, button_toggle_figure_state, 0, PERMISSION_TAX_COLLECTOR},
    {0, 224, 210, 22, button_toggle_figure_state, 0, PERMISSION_LABOR_SEEKER},
    {0, 256, 210, 22, button_toggle_figure_state, 0, PERMISSION_MISSIONARY},
    {0, 288, 210, 22, button_toggle_figure_state, 0, PERMISSION_WATCHMAN},
};

static translation_key permission_tooltip_translations[] = { {},
"TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_MAINTENANCE","TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_PRIEST",
"TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_MARKET", "TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_ENTERTAINER",
"TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_EDUCATION", "TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_MEDICINE",
"TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_TAX_COLLECTOR", "TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_LABOR_SEEKER",
"TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_MISSIONARY", "TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION_WATCHMAN",
};

static translation_key permission_orders_tooltip_translations[] = {
    "TR_TOOLTIP_BUTTON_ROADBLOCK_ORDER_REJECT_ALL", "TR_TOOLTIP_BUTTON_ROADBLOCK_ORDER_ACCEPT_ALL" };

static generic_button roadblock_orders_buttons[] = {
    {309, 0, 20, 20, button_roadblock_orders},
};

static unsigned int size_of_orders_permission_buttons = sizeof(orders_permission_buttons) / sizeof(*orders_permission_buttons);

typedef enum {
    REJECT_ALL = 0,
    ACCEPT_ALL = 1,
} affect_all_button_current_state;

void window_building_draw_engineers_post(building_info_context *c)
{
    c->advisor_button = ADVISOR_CHIEF;
    c->help_id = 81;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Engineer.ogg");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.104.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building.id);

    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (!b->num_workers) {
        window_building_draw_description(c, 104, 9);
    } else {
        if (b->figure_id) {
            window_building_draw_description(c, 104, 2);
        } else {
            window_building_draw_description(c, 104, 3);
        }
        if (c->worker_percentage >= 100) {
            window_building_draw_description_at(c, 72, 104, 4);
        } else if (c->worker_percentage >= 75) {
            window_building_draw_description_at(c, 72, 104, 5);
        } else if (c->worker_percentage >= 50) {
            window_building_draw_description_at(c, 72, 104, 6);
        } else if (c->worker_percentage >= 25) {
            window_building_draw_description_at(c, 72, 104, 7);
        } else {
            window_building_draw_description_at(c, 72, 104, 8);
        }
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 106, 104, 1);
}

void window_building_draw_prefect(building_info_context *c)
{
    c->advisor_button = ADVISOR_CHIEF;
    c->help_id = 86;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Prefect.ogg");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.88.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    building *b = building_get(c->building.id);
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else if (b->num_workers <= 0) {
        window_building_draw_description(c, 88, 9);
    } else {
        if (b->figure_id) {
            window_building_draw_description(c, 88, 2);
        } else {
            window_building_draw_description(c, 88, 3);
        }
        if (c->worker_percentage >= 100) {
            window_building_draw_description_at(c, 72, 88, 4);
        } else if (c->worker_percentage >= 75) {
            window_building_draw_description_at(c, 72, 88, 5);
        } else if (c->worker_percentage >= 50) {
            window_building_draw_description_at(c, 72, 88, 6);
        } else if (c->worker_percentage >= 25) {
            window_building_draw_description_at(c, 72, 88, 7);
        } else {
            window_building_draw_description_at(c, 72, 88, 8);
        }
    }

    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 106, 88, 1);
}

static int affect_all_button_state(void)
{
    building *b = building_get(data.building.id);
    if (b->data.roadblock.exceptions) {
        return REJECT_ALL;
    } else {
        return ACCEPT_ALL;
    }
}

static void draw_roadblock_orders_buttons(int x, int y, int focused)
{
    if (affect_all_button_state() == ACCEPT_ALL) {
        ImageGroupEntryRef::from_group("UI\\Selection_Checkmark", "Selection_Checkmark").draw(x + 29, y + 4);
    } else {
        ImageGroupEntryRef::from_group("UI\\Denied_Walker_Checkmark", "Denied_Walker_Checkmark").draw(x + 29, y + 4);
    }
    button_border_draw(x + 25, y, 20, 20, data.orders_focus_button_id == 1);
}


void window_building_draw_roadblock(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.28.115", c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, 96, "TR_BUILDING_ROADBLOCK_DESC");
}

void window_building_draw_roadblock_button(building_info_context *c)
{
    button_border_draw(c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        BLOCK_SIZE * (c->width_blocks - 10), 20, data.focus_button_id == 1 ? 1 : 0);
    text_draw_centered(translation_for_key("TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION"), c->x_offset + 80, c->y_offset + BLOCK_SIZE * c->height_blocks - 30,
        BLOCK_SIZE * (c->width_blocks - 10), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
}

void window_building_draw_roadblock_orders(building_info_context *c)
{
    c->help_id = 3;
    int y_offset = window_building_get_vertical_offset(c, 28);
    outer_panel_draw(c->x_offset, y_offset, 29, 28);
    text_draw_centered(translation_for_key("TR_TOOLTIP_BUTTON_ROADBLOCK_PERMISSION"), c->x_offset, y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    inner_panel_draw(c->x_offset + 16, y_offset + 42, c->width_blocks - 2, 21);
}

void window_building_draw_roadblock_orders_foreground(building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);
    int ids[] = { GROUP_FIGURE_ENGINEER,GROUP_FIGURE_PREFECT,GROUP_FIGURE_PRIEST,GROUP_FIGURE_PRIEST,
        GROUP_FIGURE_MARKET_LADY,GROUP_FIGURE_MARKET_LADY,GROUP_FIGURE_ACTOR,GROUP_FIGURE_LION_TAMER,
        GROUP_FIGURE_TEACHER_LIBRARIAN, GROUP_FIGURE_SCHOOL_CHILD, GROUP_FIGURE_DOCTOR_SURGEON, GROUP_FIGURE_BATHHOUSE_WORKER,
        GROUP_FIGURE_TAX_COLLECTOR, GROUP_FIGURE_TAX_COLLECTOR, GROUP_FIGURE_LABOR_SEEKER, GROUP_FIGURE_LABOR_SEEKER,
        GROUP_FIGURE_MISSIONARY, GROUP_FIGURE_MISSIONARY, GROUP_FIGURE_TOWER_SENTRY, GROUP_FIGURE_TOWER_SENTRY,
    };
    building *b = building_get(c->building.id);
    data.building = c->building;
    draw_roadblock_orders_buttons(c->x_offset + 365, y_offset + 404, data.orders_focus_button_id == 1);

    for (unsigned int i = 0; i < size_of_orders_permission_buttons; i++) {
        Image::from_id(Image::group(ids[i * 2]) + 4).draw(c->x_offset + 32, y_offset + 46 + 32 * i);
        Image::from_id(Image::group(ids[i * 2 + 1]) + 4).draw(c->x_offset + 64, y_offset + 46 + 32 * i);
        button_border_draw(c->x_offset + 180, y_offset + 50 + 32 * i, 210, 22, data.figure_focus_button_id == i + 1);
        int state = Roadblock(b).has_permission(static_cast<roadblock_permission>(i + 1));
        if (state) {
            lang_text_draw_centered("main_strings.99.7", c->x_offset + 180, y_offset + 55 + 32 * i, 210, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height));
        } else {
            lang_text_draw_centered("main_strings.99.8", c->x_offset + 180, y_offset + 55 + 32 * i, 210, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height));
        }
    }
}

void window_building_roadblock_get_tooltip_walker_permissions(translation_key *translation)
{
    if (data.figure_focus_button_id) {
        *translation = permission_tooltip_translations[data.figure_focus_button_id];
    } else if (data.orders_focus_button_id) {
        *translation = permission_orders_tooltip_translations[affect_all_button_state()];
    } else {
        *translation = {};
    }
}

void window_building_draw_garden_gate(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    //window_building_play_sound(c, "wavs/garden.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_GARDEN_WALL_GATE"), c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    window_building_draw_description_at(c, 96, "TR_BUILDING_GARDEN_WALL_GATE_DESC");
}

void window_building_draw_palisade_gate(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    //window_building_play_sound(c, "wavs/gatehouse.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    text_draw_centered(translation_for_key("TR_BUILDING_PALISADE_GATE"), c->x_offset, c->y_offset + 10, 16 * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height), 0);
    window_building_draw_description_at(c, 96, "TR_BUILDING_PALISADE_GATE_DESC");
}

void window_building_draw_burning_ruin(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/ruin.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.111.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    text_draw(lang_get_building_type_string(building_get(c->rubble_building_id)->type), c->x_offset + 32, c->y_offset + BLOCK_SIZE * c->height_blocks - 173, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    lang_text_draw_multiline("main_strings.111.1", c->x_offset + 32, c->y_offset + BLOCK_SIZE * c->height_blocks - 143, BLOCK_SIZE * (c->width_blocks - 4), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

static void trigger_building_repair(const complex_button *button)
{
    building *b = building_get(button->parameters[0]);
    building_repair(b);
    window_invalidate();
    window_go_back();
}

static void init_repair_building_button(building_info_context *c)
{
    int context_width = BLOCK_SIZE * c->width_blocks;
    int button_width = context_width / 2;  // 50% of context width
    repair_building_button->x = c->x_offset + (context_width - button_width) / 2;  // Center horizontally
    repair_building_button->y = c->y_offset + BLOCK_SIZE * c->height_blocks - 30;
    repair_building_button->width = button_width;
    repair_building_button->height = 20;
    building *b = building_repair_target(building_get(c->rubble_building_id));
    repair_building_button->parameters[0] = b ? b->id : c->rubble_building_id;
    static lang_fragment frag;
    frag = {};
    frag.type = LANG_FRAG_LABEL;
    if (!building_can_repair(b)) {
        frag.text_key = "TR_WARNING_REPAIR_IMPOSSIBLE";
        repair_building_button->is_disabled = 1;
    } else if (building_is_still_burning(b)) {
        frag.text_key = "TR_BUILDING_INFO_BUILDING_BURNING";
        repair_building_button->is_disabled = 1;
    } else {
        frag.text_key = "TR_BUILDING_INFO_REPAIR_BUILDING";
        repair_building_button->is_disabled = 0;
    }
    repair_building_button->user_data = c;
    repair_building_button->left_click_handler = trigger_building_repair;
    repair_building_button->sequence = &frag;
    repair_building_button->sequence_size = 1;
}

void window_building_draw_rubble(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/ruin.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.140.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    building *b = building_repair_target(building_get(c->rubble_building_id));
    const building_type_registry_impl::BuildingType *og_type = Building(b).og_type;
    const building_type original_type = og_type ? og_type->type() : BUILDING_NONE;
    building_type type = original_type == BUILDING_NONE ? static_cast<building_type>(b->type) : original_type;
    int is_burning_ruins = building_type_registry_impl::type_attr_is(static_cast<building_type>(b->type), "burning_ruin");

    if (building_can_repair(b)) {
        init_repair_building_button(c);
        complex_button_draw(repair_building_button);
    } else if (type != BUILDING_NONE) {
        // cant repair but can clone - aqueducts or limited monuments. Show disabled button
        init_repair_building_button(c);
        complex_button_draw(repair_building_button);
    }
    int cursor = text_draw(lang_get_building_type_string(type), c->x_offset + 32, c->y_offset + BLOCK_SIZE * c->height_blocks - 173, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    if (is_burning_ruins && type) { // show original building type if it's burning ruins
        cursor += text_draw(lang_get_building_type_string(type), c->x_offset + 32 + cursor, c->y_offset + BLOCK_SIZE * c->height_blocks - 173, FONT_NORMAL_RED, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_RED)->line_height), 0);
    }
    lang_text_draw_multiline("main_strings.140.1", c->x_offset + 32, c->y_offset + BLOCK_SIZE * c->height_blocks - 143, BLOCK_SIZE * (c->width_blocks - 4), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

void window_building_draw_reservoir(building_info_context *c)
{
    c->help_id = 59;
    window_building_play_sound(c, "wavs/resevoir.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.107.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    int text_id = c->building.has_water_access() ? 1 : 3;
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 173, 107, text_id);
}

void window_building_draw_aqueduct(building_info_context *c)
{
    c->help_id = 60;
    window_building_play_sound(c, "wavs/aquaduct.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.141.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 144, 141, c->aqueduct_has_water ? 1 : 2);
}

void window_building_draw_fountain(building_info_context *c)
{
    c->help_id = 61;
    window_building_play_sound(c, "wavs/fountain.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.108.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    int text_id;
    building *b = building_get(c->building.id);
    if (b->has_water_access) {
        if (b->num_workers > 0) {
            text_id = 1;
        } else {
            text_id = 2;
        }
    } else if (c->has_reservoir_pipes) {
        text_id = 2;
    } else {
        text_id = 3;
    }
    window_building_draw_description(c, 108, text_id);
    inner_panel_draw(c->x_offset + 16, c->y_offset + 166, c->width_blocks - 2, 4);
    window_building_draw_employment_without_house_cover(c, 170);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 174);
}

void window_building_draw_well(building_info_context *c)
{
    c->help_id = 62;
    window_building_play_sound(c, "wavs/well.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.109.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    int well_necessity = map_water_supply_is_building_unnecessary(c->building.id, 2);
    int text_id = 0;
    if (well_necessity == BUILDING_NECESSARY) { // well is OK
        text_id = 1;
    } else if (well_necessity == BUILDING_UNNECESSARY_FOUNTAIN) { // all houses have fountain
        text_id = 2;
    } else if (well_necessity == BUILDING_UNNECESSARY_NO_HOUSES) { // no houses around
        text_id = 3;
    }
    if (text_id) {
        window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 160, 109, text_id);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 116, c->width_blocks - 2, 4);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 124);
}

void window_building_draw_latrines(building_info_context *c)
{
    if (rand() % 10 == 0) {
        window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Latrines.ogg");
    } else {
        window_building_play_sound(c, "wavs/well.wav");
    }
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("TR_BUILDING_LATRINES", c->x_offset, c->y_offset + 10,
        BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));

    int latrines_necessity = map_water_supply_is_building_unnecessary(c->building.id, 3);
    building *b = building_get(c->building.id);
    if (b->num_workers <= 0) {
        window_building_draw_description(c, "TR_BUILDING_LATRINES_NO_WORKERS");
    } else if (latrines_necessity == BUILDING_NECESSARY) { // latrines cover at least one house with well access
        window_building_draw_description(c, "TR_BUILDING_LATRINES_DESC_1");
    } else if (latrines_necessity == BUILDING_UNNECESSARY_FOUNTAIN) { // latrines cover houses having all fountain access
        window_building_draw_description(c, "TR_BUILDING_LATRINES_UNNECESSARY");
    } else if (latrines_necessity == BUILDING_UNNECESSARY_NO_HOUSES) { // no houses around
        window_building_draw_description(c, "TR_BUILDING_LATRINES_NO_HOUSES");
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 116, c->width_blocks - 2, 4);
    window_building_draw_employment_without_house_cover(c, 120);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 124);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 126, "TR_BUILDING_LATRINES_DESC_2");
}

void window_building_draw_mission_post(building_info_context *c)
{
    c->advisor_button = ADVISOR_EDUCATION;
    c->help_id = 8;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/MissionPost.ogg");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("main_strings.134.0", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    if (!c->has_road_access) {
        window_building_draw_description(c, 69, 25);
    } else {
        window_building_draw_description(c, 134, 1);
    }
    inner_panel_draw(c->x_offset + 16, c->y_offset + 136, c->width_blocks - 2, 4);
    window_building_draw_employment_without_house_cover(c, 140);
    window_building_draw_risks(c, c->x_offset + c->width_blocks * BLOCK_SIZE - 76, c->y_offset + 144);
}

static void draw_native(building_info_context *c, int group_id)
{
    c->help_id = 0;
    //window_building_play_sound(c, "wavs/empty_land.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered(current_string_key(group_id, 0), c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, 106, group_id, 1);
}

void window_building_draw_native_hut(building_info_context *c)
{
    draw_native(c, 131);
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/NativeHut.ogg");
}

void window_building_draw_native_meeting(building_info_context *c)
{
    draw_native(c, 132);
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/NativeHut.ogg");
}

void window_building_draw_native_crops(building_info_context *c)
{
    draw_native(c, 133);
    window_building_play_sound(c, "wavs/wheat_farm.wav");
}

void window_building_draw_native_decoration(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/park.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("TR_BUILDING_NATIVE_DECORATION", c->x_offset,
        c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, 96, "TR_BUILDING_NATIVE_DECORATION_DESC");
}
void window_building_draw_native_monument(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/park.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("TR_BUILDING_NATIVE_MONUMENT", c->x_offset,
        c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, 96, "TR_BUILDING_NATIVE_MONUMENT_DESC");
}
void window_building_draw_native_watchtower(building_info_context *c)
{
    c->help_id = 0;
    window_building_play_sound(c, "wavs/tower2.wav");
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("TR_BUILDING_NATIVE_WATCHTOWER", c->x_offset,
        c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_description_at(c, 96, "TR_BUILDING_NATIVE_WATCHTOWER_DESC");
}

void window_building_draw_highway(building_info_context *c)
{
    //c->help_id = 0;
    window_building_play_sound(c, ASSETS_DIRECTORY "/Sounds/Road.ogg");
    window_building_prepare_figure_list(c);
    outer_panel_draw(c->x_offset, c->y_offset, c->width_blocks, c->height_blocks);
    lang_text_draw_centered("TR_BUILDING_HIGHWAY", c->x_offset, c->y_offset + 10, BLOCK_SIZE * c->width_blocks, FONT_LARGE_BLACK, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BLACK)->line_height));
    window_building_draw_figure_list(c);
    window_building_draw_levy(HIGHWAY_LEVY_MONTHLY, c->x_offset + 30, c->y_offset + BLOCK_SIZE * c->height_blocks - 130);
    window_building_draw_description_at(c, BLOCK_SIZE * c->height_blocks - 80, "TR_BUILDING_HIGHWAY_DESC");
}

static void button_toggle_figure_state(const generic_button *button)
{
    int index = button->parameter1;
    building *b = building_get(data.building.id);
    Roadblock roadblock(b);
    if (roadblock.kind() != ROADBLOCK_NONE) {
        roadblock.toggle_permission(static_cast<roadblock_permission>(index));
    }
    window_invalidate();
}

static void button_roadblock_orders(const generic_button *button)
{
    building *b = building_get(data.building.id);
    Roadblock roadblock(b);
    if (affect_all_button_state() == REJECT_ALL) {
        roadblock.accept_none();
    } else {
        roadblock.accept_all();
    }
    window_invalidate();

}

static void button_go_to_orders(const generic_button *button)
{
    window_building_info_show_storage_orders();
}

int window_building_handle_rubble_button(const mouse *m, building_info_context *c)
{
    return complex_button_handle_mouse(m, repair_building_button);
}

int window_building_handle_mouse_roadblock_button(const mouse *m, building_info_context *c)
{
    return GenericButtonList(go_to_orders_button, 1).handle_mouse(
        *m,
        c->x_offset + 80,
        c->y_offset + BLOCK_SIZE * c->height_blocks - 34,
        &data.focus_button_id
    );
}

int window_building_handle_mouse_roadblock_orders(const mouse *m, building_info_context *c)
{
    int y_offset = window_building_get_vertical_offset(c, 28);

    data.building = c->building;
    if (GenericButtonList(orders_permission_buttons, size_of_orders_permission_buttons).handle_mouse(
        *m,
        c->x_offset + 180,
        y_offset + 46,
        &data.figure_focus_button_id
    )) {
        return 1;
    }

    return GenericButtonList(roadblock_orders_buttons, 1).handle_mouse(
        *m,
        c->x_offset + 80,
        y_offset + 404,
        &data.orders_focus_button_id
    );
}
