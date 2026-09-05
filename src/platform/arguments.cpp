#include "arguments.h"

#include "SDL.h"

#include <stdio.h>

#define CURSOR_SCALE_ERROR_MESSAGE "Option --cursor-scale must be followed by a scale value of 1, 1.5 or 2"
#define DISPLAY_SCALE_ERROR_MESSAGE "Option --display-scale must be followed by a scale value between 0.5 and 5"
#define WINDOWED_AND_FULLSCREEN_ERROR_MESSAGE "Option --windowed and --fullscreen cannot both be specified"
#define DISPLAY_ID_ERROR_MESSAGE "Option --display must be followed by a number indicating the display, starting from 0"
#define MOD_NAME_ERROR_MESSAGE "Option --mod must be followed by a mod name"
#define LOAD_SAVE_TEST_ERROR_MESSAGE "Option --load-save-test must be followed by a save file path"
#define LOAD_SAVE_TEST_LIMIT_ERROR_MESSAGE "Too many --load-save-test options"
#define SAVE_ROUNDTRIP_TEST_ERROR_MESSAGE "Option --save-roundtrip-test must be followed by an output save file path"
#define SAVE_ROUNDTRIP_TEST_LIMIT_ERROR_MESSAGE "Too many --save-roundtrip-test options"
#define SAVE_SOAK_TICKS_ERROR_MESSAGE "Option --save-soak-ticks must be followed by a positive tick count"
#define UNKNOWN_OPTION_ERROR_MESSAGE "Option %s not recognized"

static void print_log(const char *message)
{
    printf("%s\n", message);
}

static void print_log_str(const char *format, const char *value)
{
    printf(format, value);
    printf("\n");
}

static int parse_decimal_as_percentage(const char *str)
{
    const char *start = str;
    char *end;
    long whole = SDL_strtol(start, &end, 10);
    int percentage = 100 * (int) whole;
    if (*end == ',' || *end == '.') {
        end++;
        start = end;
        long fraction = SDL_strtol(start, &end, 10);
        switch (end - start) {
            case 0:
                break;
            case 1:
                percentage += fraction * 10;
                break;
            case 2:
                percentage += fraction;
                break;
            default: {
                int fraction_digits = (int) (end - start);
                while (fraction_digits > 2) {
                    fraction = fraction / 10;
                    fraction_digits--;
                }
                percentage += fraction;
                break;
            }
        }
    }
    if (*end) {
        // still some characters left, print out warning
        print_log_str("Invalid decimal: %s", str);
        return -1;
    }
    return percentage;
}

int platform_parse_arguments(int argc, char **argv, augustus_args *output_args)
{
    int ok = 1;
    int add_blank_line = 1;

    // Set sensible defaults
    output_args->data_directory = 0;
    output_args->mod_name = "Vespasian";
    output_args->display_scale_percentage = 0;
    output_args->cursor_scale_percentage = 0;
    output_args->force_windowed = 0;
    output_args->launch_asset_previewer = 0;
    output_args->enable_joysticks = 0;
    output_args->use_software_cursor = 0;
    output_args->force_fullscreen = 0;
    output_args->display_id = 0;
    output_args->debug = 0;
    output_args->disable_audio = 0;
    output_args->startup_test = 0;
    output_args->load_save_test_count = 0;
    output_args->save_roundtrip_test_count = 0;
    output_args->save_soak_ticks = 0;
    output_args->formation_test = 0;

    for (int i = 1; i < argc; i++) {
        // we ignore "-psn" arguments, this is needed to launch the app
        // from the Finder on macOS.
        // https://hg.libsdl.org/SDL/file/c005c49beaa9/test/testdropfile.c#l47
        if (SDL_strncmp(argv[i], "-psn", 4) == 0) {
            continue;
        }
        if (SDL_strcmp(argv[i], "--display-scale") == 0) {
            if (i + 1 < argc) {
                int percentage = parse_decimal_as_percentage(argv[i + 1]);
                i++;
                if (percentage < 50 || percentage > 500) {
                    print_log(DISPLAY_SCALE_ERROR_MESSAGE);
                    ok = 0;
                } else {
                    output_args->display_scale_percentage = percentage;
                }
            } else {
                print_log(DISPLAY_SCALE_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--cursor-scale") == 0) {
            if (i + 1 < argc) {
                int percentage = parse_decimal_as_percentage(argv[i + 1]);
                i++;
                if (percentage == 100 || percentage == 150 || percentage == 200) {
                    output_args->cursor_scale_percentage = percentage;
                } else {
                    print_log(CURSOR_SCALE_ERROR_MESSAGE);
                    ok = 0;
                }
            } else {
                print_log(CURSOR_SCALE_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--display") == 0) {
            if (i + 1 < argc) {
                output_args->display_id = SDL_strtol(argv[i + 1], 0, 10);
                i++;
            } else {
                print_log(DISPLAY_ID_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--mod") == 0) {
            if (i + 1 < argc) {
                output_args->mod_name = argv[i + 1];
                i++;
            } else {
                print_log(MOD_NAME_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--windowed") == 0) {
            output_args->force_windowed = 1;
        } else if (SDL_strcmp(argv[i], "--asset-previewer") == 0) {
            output_args->launch_asset_previewer = 1;
        } else if (SDL_strcmp(argv[i], "--enable-joysticks") == 0) {
            output_args->enable_joysticks = 1;
        } else if (SDL_strcmp(argv[i], "--software-cursor") == 0) {
            output_args->use_software_cursor = 1;
        } else if (SDL_strcmp(argv[i], "--fullscreen") == 0) {
            output_args->force_fullscreen = 1;
        } else if (SDL_strcmp(argv[i], "--debug") == 0) {
            output_args->debug = 1;
        } else if (SDL_strcmp(argv[i], "--no-audio") == 0) {
            output_args->disable_audio = 1;
        } else if (SDL_strcmp(argv[i], "--startup-test") == 0) {
            output_args->startup_test = 1;
        } else if (SDL_strcmp(argv[i], "--formation-test") == 0) {
            output_args->formation_test = 1;
        } else if (SDL_strcmp(argv[i], "--load-save-test") == 0) {
            if (i + 1 < argc) {
                if (output_args->load_save_test_count < MAX_LOAD_SAVE_TESTS) {
                    output_args->load_save_tests[output_args->load_save_test_count++] = argv[++i];
                } else {
                    print_log(LOAD_SAVE_TEST_LIMIT_ERROR_MESSAGE);
                    i++;
                    ok = 0;
                }
            } else {
                print_log(LOAD_SAVE_TEST_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--save-roundtrip-test") == 0) {
            if (i + 1 < argc) {
                if (output_args->save_roundtrip_test_count < MAX_LOAD_SAVE_TESTS) {
                    output_args->save_roundtrip_tests[output_args->save_roundtrip_test_count++] = argv[++i];
                } else {
                    print_log(SAVE_ROUNDTRIP_TEST_LIMIT_ERROR_MESSAGE);
                    i++;
                    ok = 0;
                }
            } else {
                print_log(SAVE_ROUNDTRIP_TEST_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--save-soak-ticks") == 0 || SDL_strcmp(argv[i], "--save-soak-frames") == 0) {
            if (i + 1 < argc) {
                output_args->save_soak_ticks = (int) SDL_strtol(argv[++i], 0, 10);
                if (output_args->save_soak_ticks <= 0) {
                    print_log(SAVE_SOAK_TICKS_ERROR_MESSAGE);
                    ok = 0;
                }
            } else {
                print_log(SAVE_SOAK_TICKS_ERROR_MESSAGE);
                ok = 0;
            }
        } else if (SDL_strcmp(argv[i], "--help") == 0) {
            add_blank_line = 0;
            ok = 0;
        } else if (SDL_strncmp(argv[i], "--", 2) == 0) {
            print_log_str(UNKNOWN_OPTION_ERROR_MESSAGE, argv[i]);
            ok = 0;
        } else {
            output_args->data_directory = argv[i];
        }
    }
    if (output_args->force_fullscreen && output_args->force_windowed) {
        print_log(WINDOWED_AND_FULLSCREEN_ERROR_MESSAGE);
        ok = 0;
    }
    if (output_args->save_soak_ticks && !output_args->load_save_test_count) {
        print_log("Option --save-soak-ticks requires --load-save-test");
        ok = 0;
    }
    if (output_args->formation_test && !output_args->load_save_test_count) {
        print_log("Option --formation-test requires --load-save-test");
        ok = 0;
    }
    if (output_args->save_roundtrip_test_count && !output_args->load_save_test_count) {
        print_log("Option --save-roundtrip-test requires --load-save-test");
        ok = 0;
    }
    if (output_args->save_roundtrip_test_count && output_args->save_roundtrip_test_count != output_args->load_save_test_count) {
        print_log("Each --load-save-test requires one --save-roundtrip-test when roundtrip validation is enabled");
        ok = 0;
    }
    if (output_args->startup_test || output_args->load_save_test_count) output_args->disable_audio = 1;

    if (!ok) {
        if (add_blank_line) {
            print_log("");
        }
        print_log("Usage: Vespasian [ARGS] [DATA_DIR]");
        print_log("ARGS may be:");
        print_log("--display-scale NUMBER");
        print_log("          Scales the display by a factor of NUMBER. Number can be between 0.5 and 5");
        print_log("--cursor-scale NUMBER");
        print_log("          Scales the mouse cursor by a factor of NUMBER. Number can be 1, 1.5 or 2");
        print_log("--windowed");
        print_log("          Forces the game to start in windowed mode");
        print_log("--fullscreen");
        print_log("          Forces the game to start fullscreen");
        print_log("--display ID");
        print_log("          Forces the game to start on the specified display, numbered from 0");
        print_log("--debug");
        print_log("          Enables verbose startup/debug logging");
        print_log("--no-audio");
        print_log("          Disables audio initialization and playback");
        print_log("--startup-test");
        print_log("          Runs the real executable startup path with a hidden window and exits before the game loop");
        print_log("--load-save-test FILE");
        print_log("          Loads FILE through the real save loader; repeat to validate saves sequentially in one process");
        print_log("--save-roundtrip-test FILE");
        print_log("          Writes and strictly reloads the corresponding loaded save; repeat once per input save");
        print_log("--save-soak-ticks NUMBER");
        print_log("          Advances and renders a loaded save for NUMBER headless frames; warnings and errors fail the test");
        print_log("--mod NAME");
        print_log("          Loads data from Mods/NAME, relative to the active Caesar 3 directory");
        print_log("--asset-previewer");
        print_log("          Runs the extra asset previewer instead of the game");
        print_log("--enable-joysticks");
        print_log("          Enables joystick support");
        print_log("--software-cursor");
        print_log("          Uses a software cursor instead of the default hardware cursor");
        print_log("The last argument, if present, is interpreted as data directory for the Caesar 3 installation");
    }
    return ok;
}
