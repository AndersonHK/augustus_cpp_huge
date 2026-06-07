#pragma once

#include "core/locale.h"

#include <stdint.h>

enum lang_type {
    TYPE_MANUAL = 0,
    TYPE_ABOUT = 1,
    TYPE_MESSAGE = 2,
    TYPE_MISSION = 3
};

enum lang_message_type {
    MESSAGE_TYPE_GENERAL = 0,
    MESSAGE_TYPE_DISASTER = 1,
    MESSAGE_TYPE_IMPERIAL = 2,
    MESSAGE_TYPE_EMIGRATION = 3,
    MESSAGE_TYPE_TUTORIAL = 4,
    MESSAGE_TYPE_TRADE_CHANGE = 5,
    MESSAGE_TYPE_PRICE_CHANGE = 6,
    MESSAGE_TYPE_INVASION = 7,
    MESSAGE_TYPE_BUILDING_COMPLETION = 8,
    MESSAGE_TYPE_CUSTOM = 9,
    MESSAGE_TYPE_ROUTE_PRICE_CHANGE = 10,
    MESSAGE_TYPE_RANK_CHANGE = 11,
};

struct lang_message_image {
    int id;
    int x;
    int y;
};

struct lang_message_string {
    const uint8_t *text;
    int x;
    int y;
};

struct lang_message {
    lang_type type;
    lang_message_type message_type;
    int x;
    int y;
    int width_blocks;
    int height_blocks;
    int urgent;
    lang_message_image image;
    lang_message_string title;
    lang_message_string subtitle;
    lang_message_string video;
    lang_message_string content;
};

struct translation_key {
    const char *id = nullptr;

    constexpr translation_key() = default;
    constexpr translation_key(const char *key) : id(key) {}
    constexpr operator bool() const { return id && *id; }
    static constexpr bool same_id(const char *left, const char *right)
    {
        if (left == right) {
            return true;
        }
        if (!left || !right) {
            return false;
        }
        while (*left && *right) {
            if (*left++ != *right++) {
                return false;
            }
        }
        return *left == *right;
    }
    constexpr bool operator==(translation_key other) const { return same_id(id, other.id); }
    constexpr bool operator!=(translation_key other) const { return !(*this == other); }
};


typedef struct {
    translation_key key;
    const char *string;
} translation_string;

void translation_load(language_type language);

const uint8_t *translation_for_key(const char *key);
inline const uint8_t *translation_for(translation_key key) { return translation_for_key(key.id); }

int lang_dir_is_valid(const char *dir);
int lang_load(int is_editor);
void load_augustus_messages(void);
void lang_refresh_message_cache(void);

const uint8_t *lang_get_string_by_key(const char *key);
const uint8_t *lang_get_building_type_string(int type);
const lang_message *lang_get_message(int id);

const uint8_t *lang_get_string(int group, int index);
const uint8_t *lang_get_string(translation_key key);
