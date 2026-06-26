#include "custom_variable.h"

#include "core/log.h"
#include "core/string.h"
#include "game/save_version.h"
#include "graphics/color.h"
#include "scenario/message_media_text_blob.h"

#include <cctype>
#include <cstdio>
#include <vector>

struct custom_variable_t {
    unsigned int id;
    int in_use;
    int value;
    uint8_t name[CUSTOM_VARIABLE_NAME_LENGTH];
    uint8_t text_display[CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH];
    unsigned char allow_display;
    unsigned char color_group;
};

static std::vector<custom_variable_t> custom_variables;
static custom_variable_t *get_variable(unsigned int id);

static custom_variable_t new_variable(unsigned int index)
{
    custom_variable_t variable = {};
    variable.id = index;
    return variable;
}

static int variable_in_use(const custom_variable_t &variable)
{
    return variable.in_use;
}

static custom_variable_t *append_variable()
{
    custom_variables.push_back(new_variable(static_cast<unsigned int>(custom_variables.size())));
    return &custom_variables.back();
}

static void resize_variables(size_t size)
{
    custom_variables.clear();
    custom_variables.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_variable();
    }
}

static custom_variable_t *create_variable_slot()
{
    for (size_t i = 1; i < custom_variables.size(); ++i) {
        if (!variable_in_use(custom_variables[i])) {
            custom_variables[i] = new_variable(static_cast<unsigned int>(i));
            return &custom_variables[i];
        }
    }
    return append_variable();
}

static void trim_variables()
{
    while (custom_variables.size() > 1 && !variable_in_use(custom_variables.back())) {
        custom_variables.pop_back();
    }
}

unsigned int scenario_custom_variable_create(const uint8_t *name, int initial_value)
{
    custom_variable_t *variable = create_variable_slot();
    if (!variable) {
        return 0;
    }
    variable->in_use = 1;
    variable->value = initial_value;
    variable->text_display[0] = 0; // Initialize to empty string
    variable->allow_display = 0; // Initialize to not visible
    if (name) {
        string_copy(name, variable->name, CUSTOM_VARIABLE_NAME_LENGTH);
    }

    return variable->id;
}

void scenario_custom_variable_delete_all(void)
{
    resize_variables(1);
}

void scenario_custom_variable_set_color_group(unsigned int id, int color_group)
{
    custom_variable_t *var = get_variable(id);
    var->color_group = color_group;
}

int scenario_custom_variable_get_color_group(unsigned int id)
{
    custom_variable_t *var = get_variable(id);
    int color_id = var ? var->color_group : 0;
    return color_id;
}

color_t scenario_custom_variable_get_color(unsigned int id)
{
    custom_variable_t *var = get_variable(id);
    unsigned char color_id = var->color_group;
    switch (color_id) {
        case 1: return COLOR_MASK_PASTEL_GREEN;
        case 2: return COLOR_MASK_PASTEL_PURPLE;
        case 3: return COLOR_MASK_PASTEL_ORANGE;
        case 4: return COLOR_MASK_PASTEL_OLIVE;
        case 5: return COLOR_MASK_PASTEL_TURQUOISE;
        case 6: return COLOR_MASK_PASTEL_CORAL;
        case 7: return COLOR_MASK_PASTEL_GRAY;
        case 8: return COLOR_MASK_PASTEL_BLUE;
        case 9: return COLOR_MASK_PASTEL_DARK_BLUE;
        case 10: return COLOR_MASK_PASTEL_BLACK;

        default:
            return COLOR_MASK_NONE;
    }
}

unsigned int scenario_custom_variable_get_id_by_name(const uint8_t *name)
{
    const custom_variable_t *variable;
    for (const custom_variable_t &current : custom_variables) {
        variable = &current;
        if (name && string_equals(variable->name, name)) {
            return variable->id;
        }
    }
    return 0;
}

static custom_variable_t *get_variable(unsigned int id)
{
    if (id < 1 || id >= custom_variables.size()) {
        return 0;
    }
    return &custom_variables[id];
}

unsigned int scenario_custom_variable_count(void)
{
    return static_cast<unsigned int>(custom_variables.size());
}

int scenario_custom_variable_exists(unsigned int id)
{
    const custom_variable_t *variable = get_variable(id);
    return variable && variable->in_use;
}

void scenario_custom_variable_delete(unsigned int id)
{
    custom_variable_t *variable = get_variable(id);
    if (variable) {
        variable->in_use = 0;
        trim_variables();
    }
}

const uint8_t *scenario_custom_variable_get_name(unsigned int id)
{
    const custom_variable_t *variable = get_variable(id);
    return variable ? variable->name : 0;
}

int scenario_custom_variable_is_visible(unsigned int id)
{
    const custom_variable_t *variable = get_variable(id);
    return variable ? variable->allow_display : 0;
}

void scenario_custom_variable_set_visibility(unsigned int id, int visible)
{
    custom_variable_t *variable = get_variable(id);
    if (variable) {
        variable->allow_display = visible ? 1 : 0;
    }
}

const uint8_t *scenario_custom_variable_get_text_display(unsigned int id)
{
    const custom_variable_t *variable = get_variable(id);
    return variable ? variable->text_display : 0;
}

void scenario_custom_variable_set_text_display(unsigned int id, const uint8_t *text)
{
    custom_variable_t *variable = get_variable(id);
    if (!variable) {
        return;
    }
    if (!text || !*text) {
        variable->text_display[0] = 0;
    }
    string_copy(text, variable->text_display, CUSTOM_VARIABLE_NAME_LENGTH);
}

void scenario_custom_variable_rename(unsigned int id, const uint8_t *name)
{
    custom_variable_t *variable = get_variable(id);
    if (!variable) {
        return;
    }
    if (!name || !*name) {
        variable->name[0] = 0;
    }
    string_copy(name, variable->name, CUSTOM_VARIABLE_NAME_LENGTH);
}

int scenario_custom_variable_get_value(unsigned int id)
{
    const custom_variable_t *variable = get_variable(id);
    return variable ? variable->value : 0;
}

void scenario_custom_variable_set_value(unsigned int id, int new_value)
{
    custom_variable_t *variable = get_variable(id);
    if (!variable) {
        return;
    }
    variable->value = new_value;
}

void scenario_custom_variable_save_state(buffer *buf)
{
    uint32_t struct_size =
        sizeof(uint8_t) + //variable in use
        sizeof(int32_t) + // value
        sizeof(uint8_t) * CUSTOM_VARIABLE_NAME_LENGTH + //name
        sizeof(uint8_t) * CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH + //display name
        sizeof(uint8_t) + // allow display
        sizeof(uint8_t); // color group
    buffer_init_dynamic_array(buf, custom_variables.size(), struct_size);

    const custom_variable_t *variable;
    for (const custom_variable_t &current : custom_variables) {
        variable = &current;
        buffer_write_u8(buf, variable->in_use);
        buffer_write_i32(buf, variable->value);
        buffer_write_raw(buf, variable->name, CUSTOM_VARIABLE_NAME_LENGTH);
        buffer_write_raw(buf, variable->text_display, CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH);
        buffer_write_u8(buf, variable->allow_display);
        buffer_write_u8(buf, variable->color_group);
    }
}

void scenario_custom_variable_load_state(buffer *buf, int version)
{
    size_t total_variables = buffer_load_dynamic_array(buf);

    resize_variables(total_variables);

    for (size_t i = 0; i < total_variables; i++) {
        custom_variable_t *variable = get_variable(static_cast<unsigned int>(i));
        if (!variable && i == 0) {
            variable = &custom_variables[0];
        }
        variable->in_use = buffer_read_u8(buf);
        variable->value = buffer_read_i32(buf);
        buffer_read_raw(buf, variable->name, CUSTOM_VARIABLE_NAME_LENGTH);

        if (version > SCENARIO_LAST_NO_VISIBLE_CUSTOM_VARIABLES) {
            buffer_read_raw(buf, variable->text_display, CUSTOM_VARIABLE_TEXT_DISPLAY_LENGTH);
            variable->allow_display = buffer_read_u8(buf);
        } else {
            variable->text_display[0] = 0; //initialize to empty string
            variable->allow_display = 0; //initialize to not visible
        }

        if (version > SCENARIO_LAST_NO_FORMULAS_AND_MODEL_DATA) {
            variable->color_group = buffer_read_u8(buf);
        } else {
            variable->color_group = static_cast<unsigned char>(-1);
        }
    }
}

void scenario_custom_variable_load_state_old_version(buffer *buf)
{
    resize_variables(MAX_ORIGINAL_CUSTOM_VARIABLES);

    for (unsigned int i = 0; i < MAX_ORIGINAL_CUSTOM_VARIABLES; i++) {
        custom_variable_t *variable = &custom_variables[i];
        variable->in_use = buffer_read_u8(buf);
        variable->value = buffer_read_i32(buf);
        int name_link = buffer_read_i32(buf);
        variable->text_display[0] = 0; // Initialize to empty string
        variable->allow_display = 0; // Initialize to not visible
        if (!variable->in_use) {
            name_link = 0;
            variable->value = 0;
        }
        if (!name_link) {
            continue;
        }
        const text_blob_string_t *text = message_media_text_blob_get_entry(name_link);
        if (!text) {
            continue;
        }
        string_copy(text->text, variable->name, CUSTOM_VARIABLE_NAME_LENGTH);
        message_media_text_blob_mark_entry_as_unused(text);
    }
    trim_variables();
    message_media_text_blob_remove_unused();
}

void scenario_custom_variable_resolve_name(const uint8_t *input, uint8_t *output, int max_length)
{
    if (!input || !output || max_length <= 0) {
        return;
    }

    const uint8_t *src = input;
    uint8_t *dst = output;
    int remaining = max_length - 1; // reserve space for null terminator

    while (*src && remaining > 0) {
        if (*src == '[') {
            src++; // move past '['
            // Try to parse a number inside [ ]
            unsigned int id = 0;
            const uint8_t *start = src;
            while (*src && std::isdigit(*src)) {
                id = id * 10 + (*src - '0');
                src++;
            }

            if (*src == ']') {
                src++; // skip closing bracket
                // Replace with variable value
                int value = scenario_custom_variable_get_value(id);
                int written = std::snprintf(reinterpret_cast<char *>(dst), remaining, "%d", value);
                if (written < 0) written = 0;
                if (written > remaining) written = remaining;
                dst += written;
                remaining -= written;
            } else {
                // malformed [something -> copy it as-is
                const uint8_t *p = start - 1; // include '['
                while (p < src && remaining > 0) {
                    *dst++ = *p++;
                    remaining--;
                }
            }
        } else {
            *dst++ = *src++;
            remaining--;
        }
    }

    *dst = '\0';
}
