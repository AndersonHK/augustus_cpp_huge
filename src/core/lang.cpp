#include "building/building.h"
#include "lang.h"

#include "translation/translation.h"
extern "C" {
#include "building/building_record.h"
#include "building/building_type_api.h"
#include "city/message.h"
#include "core/log.h"
#include "core/string.h"
#include "core/file.h"
}

#include "translation/localization.h"

#include <stdlib.h>
#include <string.h>

#define MAX_MESSAGE_ENTRIES 400

static struct {
    lang_message message_entries[MAX_MESSAGE_ENTRIES];
    int editor_mode;
} data;

static building_type runtime_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

static int type_matches(building_type type, const char *text_id)
{
    return type == runtime_type(text_id);
}

static int native_building_uses_editor_name_group(building_type type)
{
    static const char *native_buildings[] = {"native_meeting", "native_hut", "native_hut_alt", "native_crops", nullptr};
    for (int i = 0; native_buildings[i]; i++) {
        if (type_matches(type, native_buildings[i])) {
            return 1;
        }
    }
    return 0;
}

extern "C" int lang_dir_is_valid(const char *dir)
{
    auto has_files = [](const char *base, const char *strings, const char *messages) -> bool {
        char strings_path[FILE_NAME_MAX];
        char messages_path[FILE_NAME_MAX];
        if (base && *base && strcmp(base, ".") != 0) {
            const size_t base_length = strlen(base);
            const int has_separator = base_length > 0 && (base[base_length - 1] == '/' || base[base_length - 1] == '\\');
            if (snprintf(strings_path, sizeof(strings_path), has_separator ? "%s%s" : "%s/%s", base, strings) >= static_cast<int>(sizeof(strings_path)) ||
                snprintf(messages_path, sizeof(messages_path), has_separator ? "%s%s" : "%s/%s", base, messages) >= static_cast<int>(sizeof(messages_path))) {
                return false;
            }
        } else {
            snprintf(strings_path, sizeof(strings_path), "%s", strings);
            snprintf(messages_path, sizeof(messages_path), "%s", messages);
        }
        return file_exists(strings_path, NOT_LOCALIZED) && file_exists(messages_path, NOT_LOCALIZED);
    };
    return has_files(dir, "c3.eng", "c3_mm.eng") || has_files(dir, "c3.rus", "c3_mm.rus");
}


static void set_message_parameters(
    lang_message *m,
    const char *title,
    const char *text,
    int urgent,
    lang_message_type message_type)
{
    m->type = TYPE_MESSAGE;
    m->message_type = message_type;
    m->x = 0;
    m->y = 0;
    m->width_blocks = 30;
    m->height_blocks = 20;
    m->title.x = 0;
    m->title.y = 0;
    m->urgent = urgent;

    m->title.text = title ? translation_for_key(title) : reinterpret_cast<const uint8_t *>("");
    m->content.text = text ? translation_for_key(text) : reinterpret_cast<const uint8_t *>("");
}

static int augustus_message_text_id(city_message_type message_type)
{
    return message_type > 50 ? message_type + 199 : message_type + 99;
}

static const char *consume_key_prefix(const char *key, const char *prefix)
{
    if (!key || !prefix) {
        return 0;
    }
    const size_t prefix_length = strlen(prefix);
    return strncmp(key, prefix, prefix_length) == 0 ? key + prefix_length : 0;
}

static int parse_key_int(const char *&cursor, int &value)
{
    if (!cursor || *cursor < '0' || *cursor > '9') {
        return 0;
    }
    char *end = 0;
    long parsed = strtol(cursor, &end, 10);
    if (end == cursor || parsed < 0 || parsed > 100000) {
        return 0;
    }
    value = static_cast<int>(parsed);
    cursor = end;
    return 1;
}

static const uint8_t *building_type_legacy_string_key_text(const char *display_key)
{
    const char *cursor = consume_key_prefix(display_key, "main_strings.");
    const int is_editor = cursor ? 0 : 1;
    if (!cursor) {
        cursor = consume_key_prefix(display_key, "editor_strings.");
    }
    if (!cursor) {
        return 0;
    }

    int group = 0;
    int index = 0;
    if (!parse_key_int(cursor, group) || *cursor != '.') {
        return 0;
    }
    cursor++;
    if (!parse_key_int(cursor, index) || *cursor) {
        return 0;
    }

    const uint8_t *legacy_text = localization::legacy_legacy_string(is_editor, group, index);
    return legacy_text && legacy_text[0] ? legacy_text : 0;
}

static const uint8_t *building_type_display_key_text(const char *display_key)
{
    const uint8_t *legacy_text = building_type_legacy_string_key_text(display_key);
    if (legacy_text && legacy_text[0]) {
        return legacy_text;
    }

    const uint8_t *named_translation = localization::legacy_named_project_string(display_key);
    if (named_translation && named_translation[0]) {
        return named_translation;
    }

    return reinterpret_cast<const uint8_t *>(display_key);
}

static int building_type_display_key_is_localized(const char *display_key)
{
    const uint8_t *legacy_text = building_type_legacy_string_key_text(display_key);
    if (legacy_text && legacy_text[0]) {
        return 1;
    }

    const uint8_t *named_translation = localization::legacy_named_project_string(display_key);
    return named_translation && named_translation[0] ? 1 : 0;
}

extern "C" const uint8_t *lang_get_string_by_key(const char *key)
{
    return building_type_display_key_is_localized(key) ? building_type_display_key_text(key) : 0;
}

static lang_message *set_augustus_message_parameters_by_key(
    city_message_type message_type,
    const char *title,
    const char *text,
    int urgent,
    lang_message_type message_kind)
{
    const int text_id = augustus_message_text_id(message_type);
    if (text_id < 0 || text_id >= MAX_MESSAGE_ENTRIES) {
        log_error("Augustus message entry out of range", "", text_id);
        return nullptr;
    }
    lang_message *m = &data.message_entries[text_id];
    set_message_parameters(m, title, text, urgent, message_kind);
    return m;
}

static const char *optional_project_key(const char *key)
{
    return key && strcmp(key, "0") == 0 ? nullptr : key;
}

#define set_augustus_message_parameters(message_type, title, text, urgent, message_kind) \
    set_augustus_message_parameters_by_key(message_type, optional_project_key(#title), optional_project_key(#text), urgent, message_kind)

void load_augustus_messages(void)
{
    // soldiers starving
    lang_message *m = set_augustus_message_parameters(MESSAGE_SOLDIERS_STARVING,
        TR_CITY_MESSAGE_TITLE_MESS_HALL_NEEDS_FOOD, TR_CITY_MESSAGE_TEXT_MESS_HALL_NEEDS_FOOD, 1,
        MESSAGE_TYPE_GENERAL);
    if (m) {
        m->video.text = (uint8_t *) "smk/god_mars.smk";
    }

    // soldiers starving, no mess hall
    set_augustus_message_parameters(MESSAGE_SOLDIERS_STARVING_NO_MESS_HALL,
        TR_CITY_MESSAGE_TITLE_MESS_HALL_NEEDS_FOOD, TR_CITY_MESSAGE_TEXT_MESS_HALL_MISSING, 1,
        MESSAGE_TYPE_GENERAL);

    // monument completed
    set_augustus_message_parameters(MESSAGE_GRAND_TEMPLE_COMPLETE,
        TR_CITY_MESSAGE_TITLE_GRAND_TEMPLE_COMPLETE, TR_CITY_MESSAGE_TEXT_GRAND_TEMPLE_COMPLETE, 0,
        MESSAGE_TYPE_BUILDING_COMPLETION);

    // replacement Mercury blessing
    set_augustus_message_parameters(MESSAGE_BLESSING_FROM_MERCURY_ALTERNATE,
        TR_CITY_MESSAGE_TITLE_MERCURY_BLESSING, TR_CITY_MESSAGE_TEXT_MERCURY_BLESSING, 0,
        MESSAGE_TYPE_GENERAL);

    // auto festivals
    set_augustus_message_parameters(MESSAGE_AUTO_FESTIVAL_CERES,
        TR_CITY_MESSAGE_TITLE_PANTHEON_FESTIVAL, TR_CITY_MESSAGE_TEXT_PANTHEON_FESTIVAL_CERES, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_AUTO_FESTIVAL_NEPTUNE,
        TR_CITY_MESSAGE_TITLE_PANTHEON_FESTIVAL, TR_CITY_MESSAGE_TEXT_PANTHEON_FESTIVAL_NEPTUNE, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_AUTO_FESTIVAL_MERCURY,
        TR_CITY_MESSAGE_TITLE_PANTHEON_FESTIVAL, TR_CITY_MESSAGE_TEXT_PANTHEON_FESTIVAL_MERCURY, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_AUTO_FESTIVAL_MARS,
        TR_CITY_MESSAGE_TITLE_PANTHEON_FESTIVAL, TR_CITY_MESSAGE_TEXT_PANTHEON_FESTIVAL_MARS, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_AUTO_FESTIVAL_VENUS,
        TR_CITY_MESSAGE_TITLE_PANTHEON_FESTIVAL, TR_CITY_MESSAGE_TEXT_PANTHEON_FESTIVAL_VENUS, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_PANTHEON_COMPLETE,
        TR_CITY_MESSAGE_TITLE_MONUMENT_COMPLETE, TR_CITY_MESSAGE_TEXT_PANTHEON_COMPLETE, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_LIGHTHOUSE_COMPLETE,
        TR_CITY_MESSAGE_TITLE_MONUMENT_COMPLETE, TR_CITY_MESSAGE_TEXT_LIGHTHOUSE_COMPLETE, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_BLESSING_FROM_NEPTUNE_ALTERNATE,
        TR_CITY_MESSAGE_TITLE_NEPTUNE_BLESSING, TR_CITY_MESSAGE_TEXT_NEPTUNE_BLESSING, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_BLESSING_FROM_VENUS_ALTERNATE,
        TR_CITY_MESSAGE_TITLE_VENUS_BLESSING, TR_CITY_MESSAGE_TEXT_VENUS_BLESSING, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_COLOSSEUM_COMPLETE,
        TR_CITY_MESSAGE_TITLE_MONUMENT_COMPLETE, TR_CITY_MESSAGE_TEXT_COLOSSEUM_COMPLETE, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_HIPPODROME_COMPLETE,
        TR_CITY_MESSAGE_TITLE_MONUMENT_COMPLETE, TR_CITY_MESSAGE_TEXT_HIPPODROME_COMPLETE, 0,
        MESSAGE_TYPE_GENERAL);

    m = set_augustus_message_parameters(MESSAGE_COLOSSEUM_WORKING_NEW,
        TR_CITY_MESSAGE_TITLE_COLOSSEUM_WORKING, TR_CITY_MESSAGE_TEXT_COLOSSEUM_WORKING, 1,
        MESSAGE_TYPE_GENERAL);
    if (m) {
        m->video.text = (uint8_t *) "smk/1ST_GLAD.smk";
    }

    m = set_augustus_message_parameters(MESSAGE_HIPPODROME_WORKING_NEW,
        TR_CITY_MESSAGE_TITLE_HIPPODROME_WORKING, TR_CITY_MESSAGE_TEXT_HIPPODROME_WORKING, 1,
        MESSAGE_TYPE_GENERAL);
    if (m) {
        m->video.text = (uint8_t *) "smk/1st_Chariot.smk";
    }

    static const char *const new_games_messages[] = {
        "TR_CITY_MESSAGE_TEXT_NAVAL_GAMES_PLANNING",
        "TR_CITY_MESSAGE_TEXT_NAVAL_GAMES_STARTING",
        "TR_CITY_MESSAGE_TEXT_NAVAL_GAMES_ENDING",
        "TR_CITY_MESSAGE_TEXT_ANIMAL_GAMES_PLANNING",
        "TR_CITY_MESSAGE_TEXT_ANIMAL_GAMES_STARTING",
        "TR_CITY_MESSAGE_TEXT_ANIMAL_GAMES_ENDING",
        "TR_CITY_MESSAGE_TEXT_KALENDS_GAMES_PLANNING",
        "TR_CITY_MESSAGE_TEXT_KALENDS_GAMES_STARTING",
        "TR_CITY_MESSAGE_TEXT_KALENDS_GAMES_ENDING",
        "TR_CITY_MESSAGE_TEXT_OLYMPIC_GAMES_PLANNING",
        "TR_CITY_MESSAGE_TEXT_OLYMPIC_GAMES_STARTING",
        "TR_CITY_MESSAGE_TEXT_OLYMPIC_GAMES_ENDING",
    };
    for (int j = 0; j < 12; ++j) {
        set_augustus_message_parameters_by_key(static_cast<city_message_type>(MESSAGE_NG_GAMES_PLANNED + j),
            "TR_CITY_MESSAGE_TITLE_GREAT_GAMES", new_games_messages[j], 1,
            MESSAGE_TYPE_GENERAL);
    }

    set_augustus_message_parameters(MESSAGE_LOOTING, TR_CITY_MESSAGE_TITLE_LOOTING, TR_CITY_MESSAGE_TEXT_LOOTING, 1,
        MESSAGE_TYPE_DISASTER);

    static const char *const imperial_games_messages[] = {
        "TR_CITY_MESSAGE_TEXT_IMPERIAL_GAMES_PLANNING",
        "TR_CITY_MESSAGE_TEXT_IMPERIAL_GAMES_STARTING",
        "TR_CITY_MESSAGE_TEXT_IMPERIAL_GAMES_ENDING",
    };
    for (int j = 0; j < 3; ++j) {
        set_augustus_message_parameters_by_key(static_cast<city_message_type>(MESSAGE_IG_GAMES_PLANNED + j),
            "TR_CITY_MESSAGE_TITLE_GREAT_GAMES", imperial_games_messages[j], 1,
            MESSAGE_TYPE_GENERAL);
    }

    set_augustus_message_parameters(MESSAGE_SICKNESS, TR_CITY_MESSAGE_TITLE_SICKNESS, TR_CITY_MESSAGE_TEXT_SICKNESS, 1,
        MESSAGE_TYPE_DISASTER);

    m = set_augustus_message_parameters(MESSAGE_CAESAR_ANGER,
        TR_CITY_MESSAGE_TITLE_EMPERORS_WRATH, TR_CITY_MESSAGE_TEXT_EMPERORS_WRATH, 1, MESSAGE_TYPE_GENERAL);
    if (m) {
        m->video.text = (uint8_t *) "smk/Emp_send_army.smk";
        m->urgent = 1;
    }

    set_augustus_message_parameters(MESSAGE_WRATH_OF_MARS_NO_NATIVES,
        TR_CITY_MESSAGE_TITLE_MARS_MINOR_CURSE_PREVENTED, TR_CITY_MESSAGE_TEXT_MARS_MINOR_CURSE_PREVENTED, 1,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_ENEMIES_LEAVING,
        TR_CITY_MESSAGE_TITLE_ENEMIES_LEAVING, TR_CITY_MESSAGE_TEXT_ENEMIES_LEAVING, 1, MESSAGE_TYPE_GENERAL);

    m = set_augustus_message_parameters(MESSAGE_ROAD_TO_ROME_WARNING,
        TR_CITY_MESSAGE_TITLE_ROAD_TO_ROME_WARNING, TR_CITY_MESSAGE_TEXT_ROAD_TO_ROME_WARNING, 1, MESSAGE_TYPE_GENERAL);
    if (m) {
        m->urgent = 1;
    }

    // Custom message placeholder (MESSAGE_CUSTOM_MESSAGE = 160). Actual displayed text is determined by the contents of the custom message being displayed.
    set_augustus_message_parameters(MESSAGE_CUSTOM_MESSAGE,
        TR_EDITOR_CUSTOM_MESSAGES_TITLE, TR_EDITOR_CUSTOM_MESSAGES_TITLE, 0, MESSAGE_TYPE_CUSTOM);

    set_augustus_message_parameters(MESSAGE_ROUTE_PRICE_CHANGE,
        TR_CITY_MESSAGE_TITLE_TRADE_ROUTE_PRICE_CHANGE, TR_CITY_MESSAGE_TEXT_TRADE_ROUTE_PRICE_CHANGE, 0,
        MESSAGE_TYPE_ROUTE_PRICE_CHANGE);

    set_augustus_message_parameters(MESSAGE_CARAVANSERAI_COMPLETE,
        TR_CITY_MESSAGE_TITLE_MONUMENT_COMPLETE, TR_CITY_MESSAGE_TEXT_CARAVANSERAI_COMPLETE, 0,
        MESSAGE_TYPE_GENERAL);

    set_augustus_message_parameters(MESSAGE_GOVERNOR_RANK_CHANGE,
        TR_CITY_MESSAGE_TITLE_GOVERNOR_RANK_CHANGE, 0, 0, MESSAGE_TYPE_RANK_CHANGE);
}

extern "C" int lang_load(int is_editor)
{
    data.editor_mode = is_editor;
    if (!localization::load_active_locale(is_editor)) {
        return 0;
    }
    localization::copy_legacy_messages(is_editor, data.message_entries, MAX_MESSAGE_ENTRIES);
    return 1;
}

extern "C" void lang_refresh_message_cache(void)
{
    localization::copy_legacy_messages(data.editor_mode, data.message_entries, MAX_MESSAGE_ENTRIES);
}

extern "C" const uint8_t *lang_get_string(int group, int index)
{
    //locale-dependent fixes
    language_type l_type = locale_last_determined_language();
    if (l_type == LANGUAGE_KOREAN && group == 28 && index == 46) {
        const uint8_t *try_translation = translation_for_key("TR_FIX_KOREAN_BUILDING_DOCTORS_CLINIC");
        if (try_translation) {
            return try_translation;
        }
    }
    //Custom translations
    if (group == CUSTOM_TRANSLATION) {
        return reinterpret_cast<const uint8_t *>("");
    }
    // XML overrides of original strings
    if ((group == 28 || group == 41) && index > BUILDING_NONE && index < BUILDING_TYPE_MAX &&
        building_type_registry_has_definition(static_cast<building_type>(index))) {
        building_type type = static_cast<building_type>(index);
        const char *display_key = building_type_registry_get_name_key(type);
        if (display_key && *display_key && building_type_display_key_is_localized(display_key)) {
            return building_type_display_key_text(display_key);
        }
    }

    return localization::legacy_legacy_string(data.editor_mode, group, index);
}

const uint8_t *lang_get_building_type_string(int type)
{
    building_type building_type_id = static_cast<building_type>(type);
    if (building_is_house(building_type_id) || native_building_uses_editor_name_group(building_type_id)) {
        return lang_get_string(41, type);
    } else {
        return lang_get_string(28, type);
    }
}

extern "C" const lang_message *lang_get_message(int id)
{
    return &data.message_entries[id];
}
