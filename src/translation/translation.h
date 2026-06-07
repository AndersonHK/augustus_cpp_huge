#pragma once

#include "core/locale.h"

#include <stdint.h>
#include <string>
#include <utility>

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
    std::string storage;
    const char *id = nullptr;

    translation_key() = default;
    translation_key(const char *key) : id(key) {}
    translation_key(std::string key) : storage(std::move(key)), id(storage.c_str()) {}
    translation_key(const translation_key &other) : storage(other.storage), id(storage.empty() ? other.id : storage.c_str()) {}
    translation_key(translation_key &&other) noexcept : storage(std::move(other.storage)), id(storage.empty() ? other.id : storage.c_str()) {}
    translation_key &operator=(const translation_key &other)
    {
        if (this != &other) {
            storage = other.storage;
            id = storage.empty() ? other.id : storage.c_str();
        }
        return *this;
    }
    translation_key &operator=(translation_key &&other) noexcept
    {
        if (this != &other) {
            storage = std::move(other.storage);
            id = storage.empty() ? other.id : storage.c_str();
        }
        return *this;
    }
    const char *c_str() const { return storage.empty() ? id : storage.c_str(); }
    operator bool() const
    {
        const char *key = c_str();
        return key && *key;
    }
    static bool same_id(const char *left, const char *right)
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
    bool operator==(translation_key other) const { return same_id(c_str(), other.c_str()); }
    bool operator!=(translation_key other) const { return !(*this == other); }
};


typedef struct {
    translation_key key;
    const char *string;
} translation_string;

void translation_load(language_type language);

translation_key main_string_key(int group, int index);
translation_key editor_string_key(int group, int index);
translation_key localized_string_key(int is_editor, int group, int index);
translation_key current_string_key(int group, int index);
translation_key main_string_amount_key(int group, int first_index, int amount);
translation_key current_string_amount_key(int group, int first_index, int amount);

const uint8_t *translation_for_key(const char *key);
inline const uint8_t *translation_for(translation_key key) { return translation_for_key(key.c_str()); }

int lang_dir_is_valid(const char *dir);
int lang_load(int is_editor);
void load_augustus_messages(void);
void lang_refresh_message_cache(void);

const uint8_t *lang_get_string_by_key(const char *key);
const uint8_t *lang_get_building_type_string(int type);
const lang_message *lang_get_message(int id);

const uint8_t *lang_get_string(translation_key key);
