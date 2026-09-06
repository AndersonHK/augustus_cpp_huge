#include "config.h"
#include "core/config_options.h"

#include "core/dir.h"
#include "core/file.h"
#include "core/log.h"

#include <stdio.h>
#include <string.h>

#define MAX_LINE 100

static const char *PRIMARY_INI_FILENAME = "Vespasian.ini";
static const char *LEGACY_INI_FILENAME = "augustus.ini";
static int needs_user_directory_setup;


static const char *ini_string_keys[] = {
    "ui_language_dir",
    "ui_locale",
    "ui_auto_cycle_groups",
};

static int values[CONFIG_MAX_ENTRIES];
static char string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX];


static const char default_string_values[CONFIG_STRING_MAX_ENTRIES][CONFIG_STRING_VALUE_MAX] = { 0 };

static int parse_scale_filter_value(const char *value)
{
    if (strcmp(value, "nearest") == 0) {
        return CONFIG_SCALE_FILTER_NEAREST;
    }
    if (strcmp(value, "linear") == 0) {
        return CONFIG_SCALE_FILTER_LINEAR;
    }
    if (strcmp(value, "best") == 0) {
        return CONFIG_SCALE_FILTER_BEST;
    }
    if (strcmp(value, "auto") == 0) {
        return CONFIG_SCALE_FILTER_AUTO;
    }
    return atoi(value);
}

static int parse_config_value(config_key key, const char *value)
{
    if (key == CONFIG_SCALE_FILTER) {
        return parse_scale_filter_value(value);
    }
    return atoi(value);
}

static const char *scale_filter_value_name(int value)
{
    switch (value) {
        case CONFIG_SCALE_FILTER_NEAREST:
            return "nearest";
        case CONFIG_SCALE_FILTER_LINEAR:
            return "linear";
        case CONFIG_SCALE_FILTER_BEST:
            return "best";
        case CONFIG_SCALE_FILTER_AUTO:
        default:
            return "auto";
    }
}

static const char *config_get_load_file_name(void)
{
    const char *file_name = dir_get_file_at_location(PRIMARY_INI_FILENAME, PATH_LOCATION_CONFIG);
    if (file_name) {
        return file_name;
    }
    return dir_get_file_at_location(LEGACY_INI_FILENAME, PATH_LOCATION_CONFIG);
}

int config_get(config_key key)
{
    return values[key];
}

void config_set(config_key key, int value)
{
    values[key] = value;
}

const char *config_get_string(config_string_key key)
{
    return string_values[key];
}

void config_set_string(config_string_key key, const char *value)
{
    if (!value) {
        string_values[key][0] = 0;
    } else {
        snprintf(string_values[key], CONFIG_STRING_VALUE_MAX, "%s", value);
    }
}

int config_get_default_value(config_key key)
{
    return config_options[key].fallback;
}

const char *config_get_default_string_value(config_string_key key)
{
    return default_string_values[key];
}

static void set_defaults(void)
{
    memset(string_values, 0, sizeof(string_values));
    for (int i = 0; i < CONFIG_MAX_ENTRIES; ++i) {
        values[i] = config_options[i].fallback;
    }
    snprintf(string_values[CONFIG_STRING_UI_LANGUAGE_DIR], CONFIG_STRING_VALUE_MAX, "%s",
        default_string_values[CONFIG_STRING_UI_LANGUAGE_DIR]);
}

void config_load(void)
{
    set_defaults();
    needs_user_directory_setup = 1;
    const char *file_name = config_get_load_file_name();
    if (!file_name) {
        return;
    }
    FILE *fp = file_open(file_name, "rt");
    if (!fp) {
        return;
    }
    // Override default, if value is the same at end, then we never setup the directories
    needs_user_directory_setup = 0;
    values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = -1;

    char line_buffer[MAX_LINE];
    char *line;
    while ((line = fgets(line_buffer, MAX_LINE, fp)) != 0) {
        // Remove newline from string
        size_t size = strlen(line);
        while (size > 0 && (line[size - 1] == '\n' || line[size - 1] == '\r')) {
            line[--size] = 0;
        }
        char *equals = strchr(line, '=');
        if (equals) {
            *equals = 0;
            for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
                if (strcmp(config_options[i].key, line) == 0) {
                    int value = parse_config_value(static_cast<config_key>(i), &equals[1]);
                    if (log_is_debug_enabled()) {
                        log_info("Config key", config_options[i].key, value);
                    }
                    values[i] = value;
                    break;
                }
            }
            for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
                if (strcmp(ini_string_keys[i], line) == 0) {
                    const char *value = &equals[1];
                    if (log_is_debug_enabled()) {
                        log_info("Config key", ini_string_keys[i], 0);
                        log_info("Config value", value, 0);
                    }
                    snprintf(string_values[i], CONFIG_STRING_VALUE_MAX, "%s", value);
                    break;
                }
            }
        }
    }
    file_close(fp);
    if (values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] == -1) {
        values[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES] = config_options[CONFIG_GENERAL_HAS_SET_USER_DIRECTORIES].fallback;
        needs_user_directory_setup = 1;
    }
}

int config_must_configure_user_directory(void)
{
    return needs_user_directory_setup;
}

void config_save(void)
{
    const char *file_name = dir_append_location(PRIMARY_INI_FILENAME, PATH_LOCATION_CONFIG);
    if (!file_name) {
        return;
    }
    FILE *fp = file_open(file_name, "wt");
    if (!fp) {
        log_error("Unable to write configuration file", PRIMARY_INI_FILENAME, 0);
        return;
    }
    for (int i = 0; i < CONFIG_MAX_ENTRIES; i++) {
        if (i == CONFIG_SCALE_FILTER) {
            fprintf(fp, "%s=%s\n", config_options[i].key, scale_filter_value_name(values[i]));
        } else {
            fprintf(fp, "%s=%d\n", config_options[i].key, values[i]);
        }
    }
    for (int i = 0; i < CONFIG_STRING_MAX_ENTRIES; i++) {
        fprintf(fp, "%s=%s\n", ini_string_keys[i], string_values[i]);
    }
    file_close(fp);
    needs_user_directory_setup = 0;
}

config_key config_key_from_name(const char *name)
{
    for (int i = 0; i < CONFIG_MAX_ENTRIES; ++i) if (strcmp(name, config_options[i].key) == 0) return static_cast<config_key>(i);
    return CONFIG_MAX_ENTRIES;
}
