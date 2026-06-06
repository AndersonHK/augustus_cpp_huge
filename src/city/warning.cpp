#include "warning.h"

extern "C" {
#include "core/string.h"
}

#include "core/time.h"
#include "game/settings.h"
#include "graphics/window.h"

#include <cstring>

#define MAX_WARNINGS 5
#define MAX_TEXT 256
#define TIMEOUT_MS 15000
#define TIMEOUT_FLASH 30

typedef struct {
    int in_use;
    time_millis time;
    const char *type;
    int flashing;
    uint8_t text[MAX_TEXT];
} warning;

static warning warnings[MAX_WARNINGS];

static int same_warning_type(const char *left, const char *right)
{
    if (!left || !right) {
        return left == right;
    }
    return strcmp(left, right) == 0;
}

static warning *find_warning_by_type(warning_type type)
{
    if (!type.name) {
        return 0;
    }
    for (int i = 0; i < MAX_WARNINGS; i++) {
        if (warnings[i].in_use && same_warning_type(warnings[i].type, type.name)) {
            return &warnings[i];
        }
    }
    return 0;
}

static warning *find_warning_by_text(const uint8_t *text)
{
    for (int i = 0; i < MAX_WARNINGS; i++) {
        if (warnings[i].in_use && string_equals(warnings[i].text, text)) {
            return &warnings[i];
        }
    }
    return 0;
}

static warning *find_empty_warning_slot(void)
{
    for (int i = 0; i < MAX_WARNINGS; i++) {
        if (!warnings[i].in_use) {
            return &warnings[i];
        }
    }
    return 0;
}

static warning *get_warning_slot(warning_type type, const uint8_t *text)
{
    warning *slot = find_warning_by_type(type);
    if (!slot) {
        slot = find_warning_by_text(text);
    }
    if (!slot) {
        slot = find_empty_warning_slot();
        if (slot) {
            slot->type = type.name;
            slot->flashing = 0;
        }
        return slot;
    }

    if (slot->time != time_get_millis()) {
        slot->flashing = 1;
    }
    return slot;
}

int city_warning_show(warning_type type, const uint8_t *text)
{
    if (!setting_warnings()) {
        return 0;
    }
    warning *w = get_warning_slot(type, text);
    if (!w) {
        return 0;
    }
    w->in_use = 1;
    w->type = type.name;
    w->time = time_get_millis();
    if (!w->flashing) {
        string_copy(text, w->text, MAX_TEXT);
    }
    return 1;
}

int city_has_warnings(void)
{
    for (int i = 0; i < MAX_WARNINGS; i++) {
        if (warnings[i].in_use) {
            return 1;
        }
    }
    return 0;
}

const uint8_t *city_warning_get(int position)
{
    if (warnings[position].in_use) {
        if (warnings[position].flashing) {
            if (time_get_millis() - warnings[position].time > TIMEOUT_FLASH) {
                warnings[position].flashing = 0;
                window_request_refresh();
            }
        } else {
            return warnings[position].text;
        }
    }
    return 0;
}

void city_warning_clear(warning_type type)
{
    warning *slot = find_warning_by_type(type);
    if (slot) {
        slot->in_use = 0;
        slot->type = 0;
    }
}

void city_warning_clear_all(void)
{
    for (int i = 0; i < MAX_WARNINGS; i++) {
        warnings[i].in_use = 0;
        warnings[i].type = 0;
    }
}

void city_warning_clear_outdated(void)
{
    for (int i = 0; i < MAX_WARNINGS; i++) {
        if (warnings[i].in_use && time_get_millis() - warnings[i].time > TIMEOUT_MS) {
            warnings[i].in_use = 0;
            warnings[i].type = 0;
            window_request_refresh();
        }
    }
}
