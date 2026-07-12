#include "building/building.h"
#include "building/house.h"
#include "building/local_workforce.h"
#include "building/production_method.h"
#include "building/building_type_registry_internal.h"
#include "city/labor.h"
#include "game/ResourceGraphics.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/image_border.h"
#include "graphics/lang_text.h"

#include "translation/translation.h"
#include "common.h"

#include "building/building_record.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/population.h"
#include "city/resource.h"
#include "city/view.h"
#include "core/calc.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "sound/speech.h"


#include <stdlib.h>
#include <math.h>

void window_building_set_possible_position(int *x_offset, int *y_offset, int width_blocks, int height_blocks)
{
    int dialog_width = BLOCK_SIZE * width_blocks;
    int dialog_height = BLOCK_SIZE * height_blocks;
    int stub;
    int width;
    city_view_get_viewport(&stub, &stub, &width, &stub);
    width = screen_pixel_to_ui(width) - MARGIN_POSITION;

    if (*y_offset + dialog_height > screen_height() - MARGIN_POSITION) {
        *y_offset -= dialog_height;
    }

    *y_offset = (*y_offset < MIN_Y_POSITION) ? MIN_Y_POSITION : *y_offset;

    if (*x_offset + dialog_width > width) {
        *x_offset = width - dialog_width;
    }
}

int window_building_get_vertical_offset(building_info_context *c, int new_window_height)
{
    new_window_height = new_window_height * BLOCK_SIZE;
    int old_window_height = c->height_blocks * BLOCK_SIZE;
    int y_offset = c->y_offset;

    int center = (old_window_height / 2) + y_offset;
    int new_window_y = center - (new_window_height / 2);

    if (new_window_y < MIN_Y_POSITION) {
        new_window_y = MIN_Y_POSITION;
    } else {
        int height = screen_height() - MARGIN_POSITION;

        if (new_window_y + new_window_height > height) {
            new_window_y = height - new_window_height;
        }
    }

    return new_window_y;
}

static int get_employment_info_text(const Building &current, const building *b, int consider_house_covering)
{
    int text_id;
    int local_workforce = building_local_workforce::is_workforce_building(current);
    int labor_access = building_local_workforce::access_score(current);
    int required_workers = current.employment_required_workers();
    int current_workers = current.employment_worker_count();

    if (current_workers >= required_workers) {
        text_id = 0;
    } else if (city_population() <= 0) {
        text_id = 16; // no people in city
    } else if (!consider_house_covering) {
        text_id = 19;
    } else if (labor_access <= 0) {
        text_id = 17; // no employees nearby
    } else if ((local_workforce && labor_access < required_workers) ||
        (!local_workforce && labor_access < 40)) {
        text_id = 20; // poor access to employees
    } else if (city_labor_category(b->labor_category)->workers_allocated <= 0) {
        text_id = 18; // no people allocated
    } else {
        text_id = 19; // too few people allocated
    }
    if (!text_id && consider_house_covering &&
        ((local_workforce && labor_access < required_workers) ||
        (!local_workforce && labor_access < 40))) {
        text_id = 20; // poor access to employees
    }
    return text_id;
}

void window_building_draw_levy(int amount, int x_offset, int y_offset)
{
    resource_graphics(resource_denarii()).panel_icon().draw(x_offset, y_offset + 5);
    int width = text_draw_money(abs(amount), x_offset + 20, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    if (amount > 0) {
        text_draw(translation_for_key("TR_BUILDING_INFO_MONTHLY_LEVY"),
            x_offset + 20 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    }
}

static void draw_employment_details(building_info_context *c, const Building &current, building *b, int y_offset, int text_id)
{
    y_offset += c->y_offset;
    Image::from_id(Image::group(GROUP_CONTEXT_ICONS) + 14).draw(c->x_offset + 40, y_offset + 6);

    int levy = building_get_levy(b);
    if (levy) {
        y_offset -= 10;
    }

    int laborers_needed = current.employment_required_workers();
    int worker_count = current.employment_worker_count();
    if (laborers_needed) {
        if (b->state == BUILDING_STATE_MOTHBALLED) {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, worker_count), worker_count, c->x_offset + 60, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            text_draw(translation_for_key("TR_BUILDING_INFO_MOTHBALL_WARNING"),
                c->x_offset + 70, y_offset + 26, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        } else if (text_id) {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, worker_count), worker_count, c->x_offset + 60, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 10, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            lang_text_draw(current_string_key(69, text_id), c->x_offset + 70, y_offset + 26, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            y_offset += 6;
        } else {
            int width = lang_text_draw_amount(current_string_amount_key(8, 12, worker_count), worker_count, c->x_offset + 60, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
            width += text_draw_number(laborers_needed, '(', "",
                c->x_offset + 70 + width, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
            lang_text_draw("main_strings.69.0", c->x_offset + 70 + width, y_offset + 16, FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
        }
    }
    if (levy) {
        window_building_draw_levy(levy, c->x_offset + 64, y_offset + 26);
    }
}

void window_building_draw_employment(building_info_context *c, int y_offset)
{
    if (!c->building) {
        return;
    }
    Building current = c->building->main();
    building *b = const_cast<building *>(current.record());
    if (!b) {
        return;
    }
    int text_id = get_employment_info_text(current, b, 1);
    draw_employment_details(c, current, b, y_offset, text_id);
}

void window_building_draw_employment_without_house_cover(building_info_context *c, int y_offset)
{
    if (!c->building) {
        return;
    }
    Building current = c->building->main();
    building *b = const_cast<building *>(current.record());
    if (!b) {
        return;
    }
    int text_id = get_employment_info_text(current, b, 0);
    draw_employment_details(c, current, b, y_offset, text_id);
}

static int description_y_offset(void)
{
    return font_uses_vector_runtime() ? -4 : 0;
}

void window_building_draw_description(building_info_context *c, int text_group, int text_id)
{
    lang_text_draw_multiline(current_string_key(text_group, text_id), c->x_offset + 32,
        c->y_offset + 56 + description_y_offset(), BLOCK_SIZE * (c->width_blocks - 3),
        FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

void window_building_draw_description(building_info_context *c, translation_key key)
{
    lang_text_draw_multiline(key, c->x_offset + 32, c->y_offset + 56 + description_y_offset(),
       BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

int window_building_draw_description_at(building_info_context *c, int y_offset, int text_group, int text_id)
{
    return lang_text_draw_multiline(current_string_key(text_group, text_id), c->x_offset + 32,
        c->y_offset + y_offset + description_y_offset(), BLOCK_SIZE * (c->width_blocks - 3),
        FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

int window_building_draw_description_at(building_info_context *c, int y_offset, translation_key key)
{
    return lang_text_draw_multiline(key, c->x_offset + 32,
        c->y_offset + y_offset + description_y_offset(),
        BLOCK_SIZE * (c->width_blocks - 3), FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
}

void window_building_play_sound(building_info_context *c, const char *sound_file)
{
    if (c->can_play_sound) {
        sound_speech_play_file(sound_file);
        c->can_play_sound = 0;
    }
}

static int output_storage_capacity(building_info_context *c, resource_type resource)
{
    if (!c->building || !c->building->type) {
        return 0;
    }

    int capacity = 0;
    for (const building_type_registry_impl::StorageType *storage : c->building->type->storage_types()) {
        if (storage && storage->is_output() && storage->handles_resource(resource)) {
            capacity += storage->capacity();
        }
    }
    return capacity;
}

static int draw_output_storage_amount(
    building_info_context *c,
    const building_type_registry_impl::ProductionMethod *method,
    resource_type resource,
    int x,
    int y,
    int pixel_size)
{
    const int capacity = output_storage_capacity(c, resource);
    if (capacity <= 0) {
        return 0;
    }

    const Building &current_building = *c->building;
    const int amount = current_building.storage_resource_amount(
        resource, building_type_registry_impl::StorageRole::Output);
    const ::building *record = current_building.record();
    const int max_progress = method ? method->max_progress_for(current_building) : 0;
    const int is_full_and_complete = amount >= capacity && record && max_progress > 0 &&
        record->data.industry.progress >= max_progress;
    const font_t font = is_full_and_complete ? FONT_NORMAL_RED : FONT_NORMAL_BLACK;
    lang_text_draw_amount_colored(current_string_amount_key(8, 10, amount), amount,
        x, y, font, pixel_size, is_full_and_complete ? COLOR_MASK_RED : COLOR_MASK_NONE);
    const int reserved_amount = amount > capacity ? amount : capacity;
    return lang_text_get_amount_width(
        current_string_amount_key(8, 10, reserved_amount), reserved_amount, font, pixel_size);
}

static int draw_production_resource_row(
    building_info_context *c,
    const building_type_registry_impl::ProductionMethod *method,
    resource_type resource,
    int value,
    int needed,
    int y_offset,
    int show_output_storage)
{
    const resource_data *data = resource_get_data(resource);
    if (!data || !data->text) {
        return 0;
    }

    const int y = c->y_offset + y_offset;
    const int pixel_size = screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height);
    const font_t amount_font = needed > 0 && value < needed ? FONT_NORMAL_RED : FONT_NORMAL_BLACK;

    resource_graphics(resource).panel_icon().draw(c->x_offset + 32, y);
    int width = text_draw(data->text, c->x_offset + 60, y + 4, FONT_NORMAL_BLACK, pixel_size, COLOR_MASK_NONE);
    if (show_output_storage) {
        width += draw_output_storage_amount(c, method, resource, c->x_offset + 64 + width, y + 4,
            screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    } else if (value > 0 || needed > 0) {
        width += lang_text_draw_amount(current_string_amount_key(8, 10, value), value,
            c->x_offset + 64 + width, y + 4, amount_font,
            screen_ui_to_pixel(font_definition_for(amount_font)->line_height));
    }
    if (needed > 0) {
        text_draw_number(needed, '(',
            reinterpret_cast<const char *>(translation_for_key("TR_BUILDING_WINDOW_INDUSTRY_NEEDED")),
            c->x_offset + 64 + width, y + 4, FONT_NORMAL_BLACK, pixel_size, COLOR_MASK_NONE);
    }
    return 20;
}

static int draw_production_resource_label(resource_type resource, int x, int y)
{
    const resource_data *data = resource_get_data(resource);
    if (!data || !data->text) {
        return 0;
    }

    const int pixel_size = screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height);
    resource_graphics(resource).panel_icon().draw(x, y);
    return 28 + text_draw(data->text, x + 28, y + 4, FONT_NORMAL_BLACK, pixel_size, COLOR_MASK_NONE);
}

static int draw_production_output_inline(
    building_info_context *c, const building_type_registry_impl::ProductionMethod *method, resource_type resource, int x, int y)
{
    int width = draw_production_resource_label(resource, x, y);
    if (width) {
        width += draw_output_storage_amount(c, method, resource, x + width + 4, y + 4,
            screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height));
    }
    return width;
}

static int resource_slot(resource_type resource)
{
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return -1;
    }
    return static_cast<int>(resource);
}

static int production_method_matches_current_output(
    building_info_context *c, const building_type_registry_impl::ProductionMethod *method)
{
    if (!method || !method->has_resource_output()) {
        return 1;
    }

    const building *record = c->building ? c->building->record() : nullptr;
    const resource_type output = record ? static_cast<resource_type>(record->output_resource_id) : RESOURCE_NONE;
    return resource_slot(output) < 0 || method->output_resource() == output;
}

static int draw_production_rows_for_type(
    building_info_context *c,
    const building_type_registry_impl::BuildingType &type,
    int y_offset,
    int flags,
    unsigned char *seen_outputs,
    unsigned char *seen_inputs)
{
    int consumed_height = 0;
    for (const building_type_registry_impl::ProductionMethod *method : type.production_methods()) {
        if (!production_method_matches_current_output(c, method)) {
            continue;
        }
        if ((flags & WINDOW_BUILDING_PRODUCTION_OUTPUTS) && method->has_resource_output()) {
            const int output_slot = resource_slot(method->output_resource());
            if (output_slot >= 0 && !seen_outputs[output_slot]) {
                seen_outputs[output_slot] = 1;
                consumed_height += draw_production_resource_row(c, method, method->output_resource(),
                    0, 0, y_offset + consumed_height, 1);
            }
        }
        if (flags & WINDOW_BUILDING_PRODUCTION_INPUTS) {
            for (const building_type_registry_impl::ProductionResourceAmount &input : method->inputs()) {
                const int input_slot = resource_slot(input.resource);
                if (input_slot >= 0 && !seen_inputs[input_slot]) {
                    seen_inputs[input_slot] = 1;
                    consumed_height += draw_production_resource_row(c, nullptr, input.resource,
                        c->building->storage_resource_amount(input.resource,
                            building_type_registry_impl::StorageRole::Input),
                        method->scaled_input_amount(input),
                        y_offset + consumed_height, 0);
                }
            }
        }
    }
    return consumed_height;
}

static int draw_production_outputs_inline_for_type(
    building_info_context *c,
    const building_type_registry_impl::BuildingType &type,
    int x,
    int y,
    unsigned char *seen_outputs)
{
    int consumed_width = 0;
    for (const building_type_registry_impl::ProductionMethod *method : type.production_methods()) {
        if (!production_method_matches_current_output(c, method) || !method->has_resource_output()) {
            continue;
        }

        const int output_slot = resource_slot(method->output_resource());
        if (output_slot < 0 || seen_outputs[output_slot]) {
            continue;
        }

        seen_outputs[output_slot] = 1;
        if (consumed_width) {
            consumed_width += 12;
        }
        consumed_width += draw_production_output_inline(c, method, method->output_resource(), x + consumed_width, y);
    }
    return consumed_width;
}

static int has_figure_delivery_output_for_type(
    building_info_context *c,
    const building_type_registry_impl::BuildingType &type)
{
    for (const building_type_registry_impl::ProductionMethod *method : type.production_methods()) {
        if (production_method_matches_current_output(c, method) &&
            method->has_resource_output() && method->is_figure_delivery_output()) {
            return 1;
        }
    }
    return 0;
}

int window_building_draw_production_rows(building_info_context *c, int y_offset, int flags)
{
    if (!c->building || !c->building->type) {
        return 0;
    }

    unsigned char seen_outputs[RESOURCE_SLOT_COUNT] = {};
    unsigned char seen_inputs[RESOURCE_SLOT_COUNT] = {};
    int consumed_height = draw_production_rows_for_type(
        c, *c->building->type, y_offset, flags, seen_outputs, seen_inputs);

    if (c->building->type->has_composition()) {
        for (const building_type_registry_impl::ComposedPartDefinition &part :
            c->building->type->composition().parts()) {
            const building_type_registry_impl::BuildingType *part_type =
                building_type_registry_impl::definition_for_type(part.type);
            if (part_type) {
                consumed_height += draw_production_rows_for_type(
                    c, *part_type, y_offset + consumed_height, flags, seen_outputs, seen_inputs);
            }
        }
    }
    return consumed_height;
}

int window_building_draw_production_outputs_inline(building_info_context *c, int x_offset, int y_offset)
{
    if (!c->building || !c->building->type) {
        return 0;
    }

    unsigned char seen_outputs[RESOURCE_SLOT_COUNT] = {};
    const int x = c->x_offset + x_offset;
    const int y = c->y_offset + y_offset;
    int consumed_width = draw_production_outputs_inline_for_type(c, *c->building->type, x, y, seen_outputs);

    if (c->building->type->has_composition()) {
        for (const building_type_registry_impl::ComposedPartDefinition &part :
            c->building->type->composition().parts()) {
            const building_type_registry_impl::BuildingType *part_type =
                building_type_registry_impl::definition_for_type(part.type);
            if (part_type) {
                const int part_x = x + consumed_width + (consumed_width ? 12 : 0);
                int part_width = draw_production_outputs_inline_for_type(
                    c, *part_type, part_x, y, seen_outputs);
                if (part_width) {
                    if (consumed_width) {
                        consumed_width += 12;
                    }
                    consumed_width += part_width;
                }
            }
        }
    }
    return consumed_width;
}

int window_building_has_figure_delivery_output(building_info_context *c)
{
    if (!c->building || !c->building->type) {
        return 0;
    }

    if (has_figure_delivery_output_for_type(c, *c->building->type)) {
        return 1;
    }

    if (c->building->type->has_composition()) {
        for (const building_type_registry_impl::ComposedPartDefinition &part :
            c->building->type->composition().parts()) {
            const building_type_registry_impl::BuildingType *part_type =
                building_type_registry_impl::definition_for_type(part.type);
            if (part_type && has_figure_delivery_output_for_type(c, *part_type)) {
                return 1;
            }
        }
    }
    return 0;
}

static void window_building_draw_monument_resources_needed(building_info_context *c)
{
    building *b = c->building ? const_cast<building *>(c->building->record()) : nullptr;
    if (!b) {
        return;
    }
    int y_offset = 95;
    inner_panel_draw(c->x_offset + 16, c->y_offset + y_offset, c->width_blocks - 2, 5);
    if (building_monument_needs_resources(b)) {
        for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
            const resource_type r = static_cast<resource_type>(resource);
            int resource_needed_amount = building_monument_resources_needed_for_monument_type(b->type, r,
                b->monument.phase);
            if (!resource_needed_amount) {
                continue;
            }
            int resource_delivered_amount = resource_needed_amount - b->resources[r];
            resource_graphics(r).panel_icon().draw(c->x_offset + 32, c->y_offset + y_offset + 10);
            int width = text_draw_number(resource_delivered_amount, '@', "/",
                c->x_offset + 64, c->y_offset + y_offset + 15, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            width += text_draw_number(resource_needed_amount, '@', "",
                c->x_offset + 64 + width, c->y_offset + y_offset + 15, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            text_draw(resource_get_data(r)->text, c->x_offset + 68 + width, c->y_offset + y_offset + 15,
                FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
            y_offset += 20;
        }
    } else {
        text_draw_multiline(translation_for_key("TR_BUILDING_MONUMENT_CONSTRUCTION_ARCHITECT_NEEDED"),
            c->x_offset + 32, c->y_offset + y_offset + 10, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_WHITE, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_WHITE)->line_height), 0);
    }
}

static translation_key phase_key_from(const translation_key *keys, int count, int phase)
{
    int index = phase - 1;
    if (index < 0 || index >= count) {
        return {};
    }
    return keys[index];
}

static translation_key monument_phase_name_key(translation_key first_phase, int phase)
{
    static const translation_key large_temple[] = { "TR_BUILDING_LARGE_TEMPLE_PHASE_1", "TR_BUILDING_LARGE_TEMPLE_PHASE_2" };
    static const translation_key oracle[] = { "TR_BUILDING_ORACLE_PHASE_1", "TR_BUILDING_ORACLE_PHASE_2" };
    static const translation_key nymphaeum[] = { "TR_BUILDING_NYMPHAEUM_PHASE_1", "TR_BUILDING_NYMPHAEUM_PHASE_2" };
    static const translation_key small_mausoleum[] = { "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1", "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_2" };
    static const translation_key large_mausoleum[] = { "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1", "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_2" };
    static const translation_key grand_temple[] = {
        "TR_BUILDING_GRAND_TEMPLE_PHASE_1", "TR_BUILDING_GRAND_TEMPLE_PHASE_2", "TR_BUILDING_GRAND_TEMPLE_PHASE_3",
        "TR_BUILDING_GRAND_TEMPLE_PHASE_4", "TR_BUILDING_GRAND_TEMPLE_PHASE_5"
    };
    static const translation_key colosseum[] = {
        "TR_BUILDING_COLOSSEUM_PHASE_1", "TR_BUILDING_COLOSSEUM_PHASE_2", "TR_BUILDING_COLOSSEUM_PHASE_3",
        "TR_BUILDING_COLOSSEUM_PHASE_4"
    };
    static const translation_key hippodrome[] = {
        "TR_BUILDING_HIPPODROME_PHASE_1", "TR_BUILDING_HIPPODROME_PHASE_2", "TR_BUILDING_HIPPODROME_PHASE_3",
        "TR_BUILDING_HIPPODROME_PHASE_4"
    };
    static const translation_key caravanserai[] = { "TR_BUILDING_CARAVANSERAI_PHASE_1", "TR_BUILDING_CARAVANSERAI_PHASE_2" };
    static const translation_key lighthouse[] = {
        "TR_BUILDING_LIGHTHOUSE_PHASE_1", "TR_BUILDING_LIGHTHOUSE_PHASE_2", "TR_BUILDING_LIGHTHOUSE_PHASE_3",
        "TR_BUILDING_LIGHTHOUSE_PHASE_4"
    };
    static const translation_key city_mint[] = { "TR_BUILDING_CITY_MINT_PHASE_1", "TR_BUILDING_CITY_MINT_PHASE_2" };

    if (first_phase == "TR_BUILDING_LARGE_TEMPLE_PHASE_1") return phase_key_from(large_temple, 2, phase);
    if (first_phase == "TR_BUILDING_ORACLE_PHASE_1") return phase_key_from(oracle, 2, phase);
    if (first_phase == "TR_BUILDING_NYMPHAEUM_PHASE_1") return phase_key_from(nymphaeum, 2, phase);
    if (first_phase == "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1") return phase_key_from(small_mausoleum, 2, phase);
    if (first_phase == "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1") return phase_key_from(large_mausoleum, 2, phase);
    if (first_phase == "TR_BUILDING_GRAND_TEMPLE_PHASE_1") return phase_key_from(grand_temple, 5, phase);
    if (first_phase == "TR_BUILDING_COLOSSEUM_PHASE_1") return phase_key_from(colosseum, 4, phase);
    if (first_phase == "TR_BUILDING_HIPPODROME_PHASE_1") return phase_key_from(hippodrome, 4, phase);
    if (first_phase == "TR_BUILDING_CARAVANSERAI_PHASE_1") return phase_key_from(caravanserai, 2, phase);
    if (first_phase == "TR_BUILDING_LIGHTHOUSE_PHASE_1") return phase_key_from(lighthouse, 4, phase);
    if (first_phase == "TR_BUILDING_CITY_MINT_PHASE_1") return phase_key_from(city_mint, 2, phase);
    return first_phase;
}

static translation_key monument_phase_text_key(translation_key first_phase_text, int phase)
{
    static const translation_key large_temple[] = { "TR_BUILDING_LARGE_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_LARGE_TEMPLE_PHASE_2_TEXT" };
    static const translation_key oracle[] = { "TR_BUILDING_ORACLE_PHASE_1_TEXT", "TR_BUILDING_ORACLE_PHASE_2_TEXT" };
    static const translation_key nymphaeum[] = { "TR_BUILDING_NYMPHAEUM_PHASE_1_TEXT", "TR_BUILDING_NYMPHAEUM_PHASE_2_TEXT" };
    static const translation_key small_mausoleum[] = { "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_2_TEXT" };
    static const translation_key large_mausoleum[] = { "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1_TEXT", "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_2_TEXT" };
    static const translation_key grand_temple[] = {
        "TR_BUILDING_GRAND_TEMPLE_PHASE_1_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_2_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_3_TEXT",
        "TR_BUILDING_GRAND_TEMPLE_PHASE_4_TEXT", "TR_BUILDING_GRAND_TEMPLE_PHASE_5_TEXT"
    };
    static const translation_key colosseum[] = {
        "TR_BUILDING_COLOSSEUM_PHASE_1_TEXT", "TR_BUILDING_COLOSSEUM_PHASE_2_TEXT", "TR_BUILDING_COLOSSEUM_PHASE_3_TEXT",
        "TR_BUILDING_COLOSSEUM_PHASE_4_TEXT"
    };
    static const translation_key hippodrome[] = {
        "TR_BUILDING_HIPPODROME_PHASE_1_TEXT", "TR_BUILDING_HIPPODROME_PHASE_2_TEXT", "TR_BUILDING_HIPPODROME_PHASE_3_TEXT",
        "TR_BUILDING_HIPPODROME_PHASE_4_TEXT"
    };
    static const translation_key caravanserai[] = { "TR_BUILDING_CARAVANSERAI_PHASE_1_TEXT", "TR_BUILDING_CARAVANSERAI_PHASE_2_TEXT" };
    static const translation_key lighthouse[] = {
        "TR_BUILDING_LIGHTHOUSE_PHASE_1_TEXT", "TR_BUILDING_LIGHTHOUSE_PHASE_2_TEXT", "TR_BUILDING_LIGHTHOUSE_PHASE_3_TEXT",
        "TR_BUILDING_LIGHTHOUSE_PHASE_4_TEXT"
    };
    static const translation_key city_mint[] = { "TR_BUILDING_CITY_MINT_PHASE_1_TEXT", "TR_BUILDING_CITY_MINT_PHASE_2_TEXT" };

    if (first_phase_text == "TR_BUILDING_LARGE_TEMPLE_PHASE_1_TEXT") return phase_key_from(large_temple, 2, phase);
    if (first_phase_text == "TR_BUILDING_ORACLE_PHASE_1_TEXT") return phase_key_from(oracle, 2, phase);
    if (first_phase_text == "TR_BUILDING_NYMPHAEUM_PHASE_1_TEXT") return phase_key_from(nymphaeum, 2, phase);
    if (first_phase_text == "TR_BUILDING_SMALL_MAUSOLEUM_PHASE_1_TEXT") return phase_key_from(small_mausoleum, 2, phase);
    if (first_phase_text == "TR_BUILDING_LARGE_MAUSOLEUM_PHASE_1_TEXT") return phase_key_from(large_mausoleum, 2, phase);
    if (first_phase_text == "TR_BUILDING_GRAND_TEMPLE_PHASE_1_TEXT") return phase_key_from(grand_temple, 5, phase);
    if (first_phase_text == "TR_BUILDING_COLOSSEUM_PHASE_1_TEXT") return phase_key_from(colosseum, 4, phase);
    if (first_phase_text == "TR_BUILDING_HIPPODROME_PHASE_1_TEXT") return phase_key_from(hippodrome, 4, phase);
    if (first_phase_text == "TR_BUILDING_CARAVANSERAI_PHASE_1_TEXT") return phase_key_from(caravanserai, 2, phase);
    if (first_phase_text == "TR_BUILDING_LIGHTHOUSE_PHASE_1_TEXT") return phase_key_from(lighthouse, 4, phase);
    if (first_phase_text == "TR_BUILDING_CITY_MINT_PHASE_1_TEXT") return phase_key_from(city_mint, 2, phase);
    return first_phase_text;
}

void window_building_draw_monument_construction_process(building_info_context *c,
    translation_key tr_phase_name, translation_key tr_phase_name_text, translation_key tr_construction_desc)
{
    building *b = c->building ? const_cast<building *>(c->building->record()) : nullptr;
    if (!b) {
        return;
    }

    if (b->monument.phase != MONUMENT_FINISHED) {
        if (!c->has_road_access) {
            window_building_draw_description(c, "TR_WINDOW_BUILDING_INFO_WARNING_NO_MONUMENT_ROAD_ACCESS");
            text_draw_multiline(translation_for(tr_construction_desc),
                c->x_offset + 32, c->y_offset + 180, 16 * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
            return;
        }
        int width = text_draw(translation_for_key("TR_CONSTRUCTION_PHASE"),
            c->x_offset + 32, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        width += text_draw_number_pair(b->monument.phase, building_monument_phases(b->type) - 1, '@', "/",
            c->x_offset + 32 + width, c->y_offset + 50, 0, 0, 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw(translation_for(monument_phase_name_key(tr_phase_name, b->monument.phase)),
            c->x_offset + 32 + width, c->y_offset + 50, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        text_draw(translation_for_key("TR_REQUIRED_RESOURCES"), c->x_offset + 32, c->y_offset + 80, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        window_building_draw_monument_resources_needed(c);
        int height = text_draw_multiline(translation_for(monument_phase_text_key(tr_phase_name_text, b->monument.phase)),
            c->x_offset + 32, c->y_offset + 190, BLOCK_SIZE * (c->width_blocks - 4), 0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);

        if (building_monument_is_construction_halted(b)) {
            height += text_draw_multiline(translation_for_key("TR_BUILDING_MONUMENT_CONSTRUCTION_HALTED"),
                c->x_offset + 32, c->y_offset + 200 + height, BLOCK_SIZE * (c->width_blocks - 4),
                0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        } else {
            height += text_draw_multiline(translation_for(tr_construction_desc),
                c->x_offset + 32, c->y_offset + 200 + height, BLOCK_SIZE * (c->width_blocks - 4),
                0, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
        }
        if (c->height_blocks > 28) {
            int phase_offset = b->monument.phase % 2;
            const char *banner_entry = phase_offset ? "Construction_Banner_02" : "Construction_Banner_01";
            ImageBorder::large_banner().draw(c->x_offset + 32, c->y_offset + 216 + height);
            ImageGroupEntryRef::from_group(phase_offset ? "UI\\Construction_Banner_02" : "UI\\Construction_Banner_01", banner_entry).draw(c->x_offset + 37, c->y_offset + 221 + height);
        }
    }
}

static color_t get_color_for_risk(int risk_level)
{
    static color_t risk_colors[4] = { COLOR_RISK_ICON_LOW, COLOR_RISK_ICON_MEDIUM,
        COLOR_RISK_ICON_HIGH, COLOR_RISK_ICON_EXTREME };
    return risk_colors[calc_bound(risk_level, 0, 10) / 3];
}

void window_building_draw_risks(building_info_context *c, int x_offset, int y_offset)
{
    c->risk_icons.active = 1;
    c->risk_icons.x_offset = x_offset;
    c->risk_icons.y_offset = y_offset;

    const building *b = c->building ? c->building->record() : nullptr;
    if (!b) {
        return;
    }
    static const ImageGroupEntryRef collapse_icon =
        ImageGroupEntryRef::from_group("UI\\Risk_Widget_Collapse", "Risk_Widget_Collapse");
    static const ImageGroupEntryRef fire_icon =
        ImageGroupEntryRef::from_group("UI\\Risk_Widget_Fire", "Risk_Widget_Fire");
    static const ImageGroupEntryRef health_icon =
        ImageGroupEntryRef::from_group("UI\\Risk_Widget_Health", "Risk_Widget_Health");
    static const ImageGroupEntryRef cross_icon =
        ImageGroupEntryRef::from_group("UI\\Risk_Widget_Cross", "Risk_Widget_Cross");

    // Health risk
    if (b->house_size && b->house_population) {
        graphics_draw_inset_rect(x_offset - 28, y_offset, 24, 24,
            COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
        health_icon.draw(x_offset - 28, y_offset, get_color_for_risk(b->sickness_level / 10));
    }

    // Fire risk
    graphics_draw_inset_rect(x_offset, y_offset, 24, 24, COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
    if (b->fire_proof) {
        fire_icon.draw(x_offset, y_offset);
        cross_icon.draw(x_offset, y_offset);
    } else {
        fire_icon.draw(x_offset, y_offset, get_color_for_risk(b->fire_risk / 10));
    }

    // Damage risk
    graphics_draw_inset_rect(x_offset + 28, y_offset, 24, 24,
        COLOR_RISK_ICON_BORDER_DARK, COLOR_RISK_ICON_BORDER_LIGHT);
    int house_level = building_house_legacy_level(*c->building);
    if (b->fire_proof || (b->house_size && house_level >= HOUSE_MIN && house_level <= HOUSE_LARGE_TENT)) {
        collapse_icon.draw(x_offset + 28, y_offset);
        cross_icon.draw(x_offset + 28, y_offset);
    } else {
        collapse_icon.draw(x_offset + 28, y_offset, get_color_for_risk(b->damage_risk / 20));
    }
}

void window_building_get_risks_tooltip(
    const building_info_context *c, int *group_id, int *text_id, translation_key *translation)
{
    if (!c->risk_icons.active) {
        return;
    }

    const building *b = c->building ? c->building->record() : nullptr;
    if (!b) {
        return;
    }
    const mouse *m = mouse_get();

    // Health tooltip
    if (b->house_size && b->house_population) {
        if (m->x >= c->risk_icons.x_offset - 28 && m->x < c->risk_icons.x_offset - 4 &&
            m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
            static const translation_key sickness_tooltips[] = {
                "TR_TOOLTIP_OVERLAY_SICKNESS_LOW",
                "TR_TOOLTIP_OVERLAY_SICKNESS_MEDIUM",
                "TR_TOOLTIP_OVERLAY_SICKNESS_HIGH",
                "TR_TOOLTIP_OVERLAY_SICKNESS_PLAGUE",
            };
            *translation = b->sickness_level <= 0 ? "TR_TOOLTIP_OVERLAY_SICKNESS_NONE" :
                sickness_tooltips[calc_bound(b->sickness_level, 0, 100) / 30];
            return;
        }
    }

    // Fire tooltip
    if (m->x >= c->risk_icons.x_offset && m->x < c->risk_icons.x_offset + 24 &&
        m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
        *group_id = 66;
        *text_id = 46 + calc_bound(b->fire_risk + 19, 0, 100) / 20;
        return;
    }

    // Damage tooltip
    if (m->x >= c->risk_icons.x_offset + 28 && m->x < c->risk_icons.x_offset + 52 &&
        m->y >= c->risk_icons.y_offset && m->y < c->risk_icons.y_offset + 24) {
        *group_id = 66;
        *text_id = 52 + calc_bound(b->damage_risk + 39, 0, 200) / 40;
    }
}
