#pragma once

enum { MAX_LOAD_SAVE_TESTS = 256 };

typedef struct {
    const char *data_directory;
    const char *mod_name;
    int display_scale_percentage;
    int cursor_scale_percentage;
    int force_windowed;
    int launch_asset_previewer;
    int enable_joysticks;
    int use_software_cursor;
    int force_fullscreen;
    int display_id;
    int debug;
    int disable_audio;
    int startup_test;
    const char *load_save_tests[MAX_LOAD_SAVE_TESTS];
    const char *save_roundtrip_tests[MAX_LOAD_SAVE_TESTS];
    int load_save_test_count;
    int save_roundtrip_test_count;
    int save_soak_ticks;
} augustus_args;

int platform_parse_arguments(int argc, char **argv, augustus_args *output_args);
