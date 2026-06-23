#include "city_figure.h"

#include "assets/image_group_payload.h"
#include "game/resource_graphics.h"

#include "city/view.h"
#include "figure/formation.h"
#include "figure/figure_runtime_api.h"
#include "figure/image.h"
#include "figuretype/editor.h"
#include "graphics/runtime_texture.h"
#include "graphics/text.h"

#include <cstdio>


static color_t get_highlight_mask(int highlight_mask)
{
    switch (highlight_mask) {
        case FIGURE_HIGHLIGHT_NONE:
            return COLOR_MASK_NONE;
        case FIGURE_HIGHLIGHT_RED:
            return COLOR_MASK_LEGION_HIGHLIGHT;
        case FIGURE_HIGHLIGHT_GREEN:
            return COLOR_MASK_GREEN;
        default:
            return COLOR_MASK_NONE;
    }
}

static int clamp_frame(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

static const char *warrior_direction_suffix(int direction)
{
    static constexpr const char *suffixes[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    direction = clamp_frame(direction, 0, 7);
    return suffixes[direction];
}

static int soldier_native_direction(const Figure *f)
{
    const formation *m = formation_get(f->formation_id);
    int direction = 0;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        direction = f->attack_direction;
    } else if (m && m->missile_fired) {
        direction = f->direction;
    } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD && m) {
        direction = m->direction;
    } else if (f->direction < 8) {
        direction = f->direction;
    } else {
        direction = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(direction);
}

static int missile_native_direction(const Figure *f)
{
    int direction = f->direction;
    if (direction >= 8) {
        direction /= 2;
    }
    return figure_image_normalize_direction(direction);
}

static int enemy_missile_native_direction(const Figure *f)
{
    const formation *m = formation_get(f->formation_id);
    int direction = 0;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        direction = f->attack_direction;
    } else if ((m && m->missile_fired) || f->direction < 8) {
        direction = f->direction;
    } else {
        direction = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(direction);
}

static const ImageGroupEntry *native_entry(const char *group, const char *image)
{
    if (!group || !image || !image_group_payload_load(group)) {
        return nullptr;
    }
    const ImageGroupPayload *payload = image_group_payload_get(group);
    return payload ? payload->entry_for(image) : nullptr;
}

static const ImageGroupEntry *native_warrior_entry(const Figure *f)
{
    char name[64];
    switch (f->type) {
        case FIGURE_FORT_INFANTRY: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "auxinf_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(soldier_native_direction(f));
                if (f->action_state == FIGURE_ACTION_150_ATTACK) {
                    const int frame = f->attack_image_offset < 14 ?
                        1 :
                        clamp_frame(((f->attack_image_offset - 14) / 2) + 1, 1, 5);
                    std::snprintf(name, sizeof(name), "auxinf_f_%s_%02d", direction, frame);
                } else {
                    std::snprintf(name, sizeof(name), "auxinf_%s_%02d", direction,
                        clamp_frame(f->image_offset + 1, 1, 12));
                }
            }
            break;
        }
        case FIGURE_FORT_ARCHER: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "auxarch_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(soldier_native_direction(f));
                if (f->action_state == FIGURE_ACTION_150_ATTACK) {
                    const int frame = f->attack_image_offset < 14 ?
                        1 :
                        clamp_frame(((f->attack_image_offset - 14) / 2) + 1, 1, 5);
                    std::snprintf(name, sizeof(name), "auxarch_fm_%s_%02d", direction, frame);
                } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
                    const int frame = clamp_frame(figure_image_missile_launcher_offset(const_cast<Figure *>(f)), 0, 4) + 1;
                    std::snprintf(name, sizeof(name), "auxarch_fr_%s_%02d", direction, frame);
                } else {
                    std::snprintf(name, sizeof(name), "auxarch_%s_%02d", direction,
                        clamp_frame(f->image_offset + 1, 1, 12));
                }
            }
            break;
        }
        case FIGURE_ENEMY_CATAPULT: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "catapult_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(enemy_missile_native_direction(f));
                if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL) {
                    const int frame = clamp_frame(figure_image_missile_launcher_offset(const_cast<Figure *>(f)) + 1, 1, 8);
                    if (frame == 1) {
                        std::snprintf(name, sizeof(name), "catapult_fe_%s_01", direction);
                    } else {
                        std::snprintf(name, sizeof(name), "catapult_fr_%s_%02d", direction, frame);
                    }
                } else {
                    std::snprintf(name, sizeof(name), "catapult_%s_01", direction);
                }
            }
            break;
        }
        case FIGURE_CATAPULT_MISSILE:
            std::snprintf(name, sizeof(name), "catapult_rock_%s_01",
                warrior_direction_suffix(missile_native_direction(f)));
            break;
        default:
            return nullptr;
    }

    char group[96];
    std::snprintf(group, sizeof(group), "Warriors\\%s", name);
    return native_entry(group, name);
}

static const ImageGroupEntry *native_auxiliary_standard_flag(const Figure *f)
{
    if (f->type != FIGURE_FORT_STANDARD) {
        return nullptr;
    }
    const formation *m = formation_get(f->formation_id);
    if (!m) {
        return nullptr;
    }
    const char *prefix = nullptr;
    if (m->figure_type == FIGURE_FORT_INFANTRY) {
        prefix = "auxinf";
    } else if (m->figure_type == FIGURE_FORT_ARCHER) {
        prefix = "auxarch";
    } else {
        return nullptr;
    }

    char name[32];
    if (m->is_halted) {
        std::snprintf(name, sizeof(name), "%s_banner_0", prefix);
    } else {
        std::snprintf(name, sizeof(name), "%s_banner_%02d", prefix, clamp_frame(f->image_offset / 2 + 1, 1, 8));
    }
    char group[64];
    std::snprintf(group, sizeof(group), "UI\\%s", name);
    return native_entry(group, name);
}

static resource_type cart_resource_for_figure(const Figure *f)
{
    if (f->type == FIGURE_LIGHTHOUSE_SUPPLIER && f->action_state == FIGURE_ACTION_146_SUPPLIER_RETURNING) {
        return static_cast<resource_type>(f->collecting_item_id);
    }
    return static_cast<resource_type>(f->resource_id);
}

static void draw_cart_image(const Figure *f, int x, int y, color_t color_mask, float scale)
{
    if (!resource_graphics_cart_marker_is(f->cart_image_id)) {
        Image::from_id(f->cart_image_id).draw(x, y, color_mask, scale);
        return;
    }

    resource_type resource = cart_resource_for_figure(f);
    int loads = f->loads_sold_or_carrying > 0 ? f->loads_sold_or_carrying : 1;
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        resource = RESOURCE_NONE;
        loads = 0;
    }
    const ImageGroupEntryRef ref = resource_graphics(resource).cart_image_for_direction(
        loads,
        resource_is_food(resource),
        resource_graphics_cart_marker_direction(f->cart_image_id));
    ref.draw(x, y, color_mask, scale);
}

static void draw_figure_with_cart(const Figure *f, int x, int y, color_t color_mask, float scale)
{
    if (f->y_offset_cart >= 0) {
        Image::from_id(f->image_id).draw(x, y, color_mask, scale);
        draw_cart_image(f, x + f->x_offset_cart, y + f->y_offset_cart, color_mask, scale);
    } else {
        draw_cart_image(f, x + f->x_offset_cart, y + f->y_offset_cart, color_mask, scale);
        Image::from_id(f->image_id).draw(x, y, color_mask, scale);
    }
}

static void draw_hippodrome_horse(const Figure *f, int x, int y, color_t color_mask, float scale)
{
    int val = f->wait_ticks_missile;
    switch (city_view_orientation()) {
        case DIR_0_TOP:
            x += 10;
            if (val <= 10) {
                y -= 2;
            } else if (val <= 11) {
                y -= 10;
            } else if (val <= 12) {
                y -= 18;
            } else if (val <= 13) {
                y -= 16;
            } else if (val <= 20) {
                y -= 14;
            } else if (val <= 21) {
                y -= 10;
            } else {
                y -= 2;
            }
            break;
        case DIR_2_RIGHT:
            x -= 10;
            if (val <= 9) {
                y -= 12;
            } else if (val <= 10) {
                y += 4;
            } else if (val <= 11) {
                x -= 5;
                y += 2;
            } else if (val <= 13) {
                x -= 5;
            } else if (val <= 20) {
                y -= 2;
            } else if (val <= 21) {
                y -= 6;
            } else {
                y -= 12;
            }
            break;
        case DIR_4_BOTTOM:
            x += 20;
            if (val <= 9) {
                y += 4;
            } else if (val <= 10) {
                x += 10;
                y += 4;
            } else if (val <= 11) {
                x += 10;
                y -= 4;
            } else if (val <= 13) {
                y -= 6;
            } else if (val <= 20) {
                y -= 12;
            } else if (val <= 21) {
                y -= 10;
            } else {
                y -= 2;
            }
            break;
        case DIR_6_LEFT:
            x -= 10;
            if (val <= 9) {
                y -= 12;
            } else if (val <= 10) {
                y += 4;
            } else if (val <= 11) {
                y += 2;
            } else if (val <= 13) {
                // no change
            } else if (val <= 20) {
                y -= 2;
            } else if (val <= 21) {
                y -= 6;
            } else {
                y -= 12;
            }
            break;
    }
    draw_figure_with_cart(f, x, y, color_mask, scale);
}

static void draw_fort_standard(const Figure *f, int x, int y, float scale)
{
    const formation *m = formation_get(f->formation_id);
    if (m && !m->in_distant_battle) {
        // base
        Image::from_id(f->image_id).draw(x, y, COLOR_MASK_NONE, scale);
        if (const ImageGroupEntry *native_flag = native_auxiliary_standard_flag(f)) {
            const RuntimeDrawSlice *slice = native_flag->footprint();
            if (slice && slice->is_valid()) {
                runtime_texture_draw(*slice, x, y - slice->height, COLOR_MASK_NONE, scale);
                int icon_image_id = m->legion_flag_id;
                const Image &icon_image = Image::from_id(icon_image_id);
                icon_image.draw(x, y - icon_image.height() - slice->height, COLOR_MASK_NONE, scale);
            }
            return;
        }
        // flag
        const Image &flag_image = Image::from_id(f->cart_image_id);
        int flag_height = flag_image.height();
        flag_image.draw(x, y - flag_height, COLOR_MASK_NONE, scale);
        // top icon
        int icon_image_id = m->legion_flag_id;
        const Image &icon_image = Image::from_id(icon_image_id);
        icon_image.draw(x, y - icon_image.height() - flag_height, COLOR_MASK_NONE, scale);
    }
}

static void draw_map_flag(const Figure *f, int x, int y, float scale)
{
    // base
    Image::from_id(f->image_id).draw(x, y, COLOR_MASK_NONE, scale);
    // flag
    const Image &flag_image = Image::from_id(f->cart_image_id);
    flag_image.draw(x, y - flag_image.height(), COLOR_MASK_NONE, scale);
    // flag number
    int number = 0;
    int id = f->resource_id;
    if (id >= MAP_FLAG_INVASION_MIN && id < MAP_FLAG_INVASION_MAX) {
        number = id - MAP_FLAG_INVASION_MIN + 1;
    } else if (id >= MAP_FLAG_FISHING_MIN && id < MAP_FLAG_FISHING_MAX) {
        number = id - MAP_FLAG_FISHING_MIN + 1;
    } else if (id >= MAP_FLAG_HERD_MIN && id < MAP_FLAG_HERD_MAX) {
        number = id - MAP_FLAG_HERD_MIN + 1;
    }
    if (number > 0) {
        int pixel_size = (int) (font_definition_for(FONT_NORMAL_PLAIN)->line_height * scale);
        text_draw_number(number, '@', 0, x + 6, y + 7, FONT_NORMAL_PLAIN, pixel_size, COLOR_WHITE);
    }
}

static void tile_cross_country_offset_to_pixel_offset(int cross_country_x, int cross_country_y,
    int *pixel_x, int *pixel_y)
{
    int dir = city_view_orientation();
    if (dir == DIR_0_TOP || dir == DIR_4_BOTTOM) {
        int base_pixel_x = 2 * cross_country_x - 2 * cross_country_y;
        int base_pixel_y = cross_country_x + cross_country_y;
        *pixel_x = dir == DIR_0_TOP ? base_pixel_x : -base_pixel_x;
        *pixel_y = dir == DIR_0_TOP ? base_pixel_y : -base_pixel_y;
    } else {
        int base_pixel_x = 2 * cross_country_x + 2 * cross_country_y;
        int base_pixel_y = cross_country_x - cross_country_y;
        *pixel_x = dir == DIR_2_RIGHT ? base_pixel_x : -base_pixel_x;
        *pixel_y = dir == DIR_6_LEFT ? base_pixel_y : -base_pixel_y;
    }
}

static int tile_progress_to_pixel_offset_x(int direction, int progress)
{
    if (progress >= 15) {
        return 0;
    }
    switch (direction) {
        case DIR_0_TOP:
        case DIR_2_RIGHT:
            return 2 * progress - 28;
        case DIR_1_TOP_RIGHT:
            return 4 * progress - 56;
        case DIR_4_BOTTOM:
        case DIR_6_LEFT:
            return 28 - 2 * progress;
        case DIR_5_BOTTOM_LEFT:
            return 56 - 4 * progress;
        default:
            return 0;
    }
}

static int tile_progress_to_pixel_offset_y(int direction, int progress)
{
    if (progress >= 15) {
        return 0;
    }
    switch (direction) {
        case DIR_0_TOP:
        case DIR_6_LEFT:
            return 14 - progress;
        case DIR_2_RIGHT:
        case DIR_4_BOTTOM:
            return progress - 14;
        case DIR_3_BOTTOM_RIGHT:
            return 2 * progress - 28;
        case DIR_7_TOP_LEFT:
            return 28 - 2 * progress;
        default:
            return 0;
    }
}

static void tile_progress_to_pixel_offset(int direction, int progress, int *pixel_x, int *pixel_y)
{
    *pixel_x = tile_progress_to_pixel_offset_x(direction, progress);
    *pixel_y = tile_progress_to_pixel_offset_y(direction, progress);
}

static void adjust_pixel_offset(const Figure *f, int *pixel_x, int *pixel_y)
{
    // determining x/y offset on tile
    int x_offset = 0;
    int y_offset = 0;
    if (f->use_cross_country) {
        tile_cross_country_offset_to_pixel_offset(
            f->cross_country_x % 15, f->cross_country_y % 15, &x_offset, &y_offset);
        y_offset -= f->missile_height;
    } else {
        int direction = figure_image_normalize_direction(f->direction);
        tile_progress_to_pixel_offset(direction, f->progress_on_tile, &x_offset, &y_offset);
        y_offset -= f->current_height;
        if (f->figures_on_same_tile_index && f->type != FIGURE_BALLISTA) {
            // an attempt to not let people walk through each other
            static const int BUSY_ROAD_X_OFFSETS[] = {
                0, 8, 8, -8, -8, 0, 16, 0, -16, 8, -8, 16, -16, 16, -16, 8, -8, 0, 24, 0, -24
            };
            static const int BUSY_ROAD_Y_OFFSETS[] = {
                0, 0, 8, 8, -8, -16, 0, 16, 0, -16, 16, 8, -8, -8, 8, 16, -16, -24, 0, 24, 0
            };
            static const int BUSY_ROAD_OFFSET_LEN = 21;
            x_offset += BUSY_ROAD_X_OFFSETS[f->figures_on_same_tile_index % BUSY_ROAD_OFFSET_LEN];
            y_offset += BUSY_ROAD_Y_OFFSETS[f->figures_on_same_tile_index % BUSY_ROAD_OFFSET_LEN];
        }
    }

    x_offset += 29;
    y_offset += 15;

    const int has_native_graphics = figure_runtime_has_native_graphics(f) || native_warrior_entry(f);
    if (!has_native_graphics && f->image_id >= 10000) {
        // TODO
        // Ugly hack, remove
        // Draws new walkers at their proper spots
        x_offset -= 26;
        y_offset -= 29;
    }


    if (has_native_graphics) {
        int sprite_offset_x = 0;
        int sprite_offset_y = 0;
        if (const ImageGroupEntry *native_entry = native_warrior_entry(f)) {
            if (native_entry->has_sprite_offset()) {
                sprite_offset_x = native_entry->sprite_offset_x();
                sprite_offset_y = native_entry->sprite_offset_y();
            }
        } else {
            figure_runtime_graphic_sprite_offset(f, &sprite_offset_x, &sprite_offset_y);
        }
        *pixel_x += x_offset - sprite_offset_x;
        *pixel_y += y_offset - sprite_offset_y;
    } else {
        const Image &img = f->is_enemy_image ? Image::enemy(f->image_id) : Image::from_id(f->image_id);
        const image_animation *animation = img.animation();
        *pixel_x += x_offset - (animation ? animation->sprite_offset_x : 0);
        *pixel_y += y_offset - (animation ? animation->sprite_offset_y : 0);
    }
}

static void draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    color_t color_mask = get_highlight_mask(highlight);
    if (const ImageGroupEntry *native_entry = native_warrior_entry(f)) {
        if (const RuntimeDrawSlice *slice = native_entry->footprint()) {
            runtime_texture_draw(*slice, x, y, color_mask, scale);
        }
        return;
    }
    if (figure_runtime_has_native_graphics(f)) {
        if (const RuntimeDrawSlice *slice = figure_runtime_graphic_slice(f)) {
            runtime_texture_draw(*slice, x, y, color_mask, scale);
        }
        return;
    }
    if (f->cart_image_id) {
        switch (f->type) {
            case FIGURE_CART_PUSHER:
            case FIGURE_DEPOT_CART_PUSHER:
            case FIGURE_WAREHOUSEMAN:
            case FIGURE_LION_TAMER:
            case FIGURE_DOCKER:
            case FIGURE_NATIVE_TRADER:
            case FIGURE_IMMIGRANT:
            case FIGURE_EMIGRANT:
            case FIGURE_LIGHTHOUSE_SUPPLIER:
                draw_figure_with_cart(f, x, y, color_mask, scale);
                break;
            case FIGURE_HIPPODROME_HORSES:
                draw_hippodrome_horse(f, x, y, color_mask, scale);
                break;
            case FIGURE_FORT_STANDARD:
                draw_fort_standard(f, x, y, scale);
                break;
            case FIGURE_MAP_FLAG:
                draw_map_flag(f, x, y, scale);
                break;
            default:
                Image::from_id(f->image_id).draw(x, y, color_mask, scale);
                break;
        }
    } else {
        if (f->is_enemy_image) {
            Image::enemy(f->image_id).draw(x, y, COLOR_MASK_NONE, scale);
        } else {

            Image::from_id(f->image_id).draw(x, y, color_mask, scale);
        }
    }
}

void city_draw_figure(const Figure *f, int x, int y, float scale, int highlight)
{
    adjust_pixel_offset(f, &x, &y);
    draw_figure(f, x, y, scale, highlight);
}

void city_draw_selected_figure(const Figure *f, int x, int y, float scale, pixel_coordinate *coord)
{
    adjust_pixel_offset(f, &x, &y);
    draw_figure(f, x, y, scale, 0);
    coord->x = x;
    coord->y = y;
}
