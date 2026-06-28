#include "depot.h"

#include "game/ResourceGraphics.h"
#include "graphics/generic_button.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/ui_runtime_api.h"
#include "translation/translation.h"
#include "window/building/common.h"
#include "window/building/figures.h"

int figuretype::DepotCartPusher::is_recalled() const
{
    return action_state == FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING;
}

void figuretype::DepotCartPusher::recall()
{
    action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
}

void figuretype::DepotCartPusher::draw(building_info_context *c)
{
    draw_big_people_image(c->x_offset + 28, c->y_offset + 112);

    if (!building) {
        return;
    }
    resource_type resource = building->depot_order().resource_type;

    lang_text_draw(current_string_key(65, name), c->x_offset + 90, c->y_offset + 108,
        FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));
    if (loads_sold_or_carrying > 0 && resource_id != RESOURCE_NONE) {
        resource_graphics(resource).panel_icon().draw(c->x_offset + 92, c->y_offset + 135);
        text_draw_number(loads_sold_or_carrying, 'x', "", c->x_offset + 118, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), COLOR_MASK_NONE);
    }

    button_border_draw(c->x_offset + 90, c->y_offset + 160, 100, 22,
        window_building_figure_action_focus_button_id() == 1 || is_recalled());
    translation_key button_text = is_recalled() ?
        "TR_FIGURE_INFO_DEPOT_RETURNING" : "TR_FIGURE_INFO_DEPOT_RECALL";
    text_draw_centered(translation_for(button_text), c->x_offset + 90, c->y_offset + 166,
        100, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);

    if (is_recalled()) {
        return;
    }

    int width = text_draw(translation_for_key("TR_FIGURE_INFO_DEPOT_DELIVER"), c->x_offset + 40, c->y_offset + 200,
        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    resource_graphics(resource).panel_icon().draw(c->x_offset + 40 + width, c->y_offset + 194);
    int y_offset = 0;

    if (!destination_building) {
        return;
    }
    if (building->storage_id) {
        y_offset = 16;
        width = text_draw(translation_for_key("TR_FIGURE_INFO_DEPOT_FROM"),
            c->x_offset + 40, c->y_offset + 200 + y_offset,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        if (building->type) {
            width += text_draw_label_and_number(lang_get_building_type_string(building->type->type()),
                building->storage_id, "",
                c->x_offset + 40 + width, c->y_offset + 200 + y_offset,
                FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        }
    } else {
        width += resource_graphics(resource).panel_icon().width();
    }
    width += text_draw(translation_for_key("TR_FIGURE_INFO_DEPOT_TO"),
        c->x_offset + 40 + width, c->y_offset + 200 + y_offset,
        FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    if (destination_building->type) {
        text_draw_label_and_number(lang_get_building_type_string(destination_building->type->type()),
            destination_building->storage_id, "",
            c->x_offset + 40 + width, c->y_offset + 200 + y_offset,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
}
