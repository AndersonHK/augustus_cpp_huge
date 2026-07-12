#include "core/runtime_diagnostics.h"

#if defined(STARTUP_PARSER_TEST)

void runtime_diagnostics_write_crash_dump(const char *)
{
}

#else

#include "building/building_runtime.h"
#include "city/population.h"
#include "core/dir.h"
#include "core/log.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "game/time.h"
#include "platform/file_manager.h"
#include "platform/platform.h"

#include "SDL.h"

#include <cstdio>
#include <ctime>
#include <string>

namespace {

static std::string crash_dump_path()
{
    char *logging_path = platform_get_logging_path();
    std::string directory = logging_path ? logging_path : "";
    SDL_free(logging_path);
    if (!directory.empty() && directory.back() != '/' && directory.back() != '\\') {
        directory.push_back('/');
    }
    return directory + "vespasian-runtime-crash-dump.json";
}

static void write_json_string(FILE *file, const char *text)
{
    fputc('"', file);
    if (text) {
        for (const char *cursor = text; *cursor; ++cursor) {
            switch (*cursor) {
                case '\\':
                    fputs("\\\\", file);
                    break;
                case '"':
                    fputs("\\\"", file);
                    break;
                case '\n':
                    fputs("\\n", file);
                    break;
                case '\r':
                    fputs("\\r", file);
                    break;
                case '\t':
                    fputs("\\t", file);
                    break;
                default:
                    fputc(*cursor, file);
                    break;
            }
        }
    }
    fputc('"', file);
}

static void write_metadata(FILE *file, const char *reason)
{
    const std::time_t now = std::time(nullptr);
    fprintf(file, "  \"metadata\": {\n");
    fprintf(file, "    \"reason\": ");
    write_json_string(file, reason ? reason : "crash");
    fprintf(file, ",\n");
    fprintf(file, "    \"unix_time\": %lld,\n", static_cast<long long>(now));
    fprintf(file, "    \"game_year\": %d,\n", game_time_year());
    fprintf(file, "    \"game_month\": %d,\n", game_time_month());
    fprintf(file, "    \"game_day\": %d,\n", game_time_day());
    fprintf(file, "    \"game_tick\": %d\n", game_time_tick());
    fprintf(file, "  }");
}

}

void runtime_diagnostics_write_crash_dump(const char *reason)
{
    static int writing_dump = 0;
    if (writing_dump) {
        return;
    }
    writing_dump = 1;

    const std::string path = crash_dump_path();
    FILE *file = platform_file_manager_open_file(path.c_str(), "wb");
    if (!file) {
        log_error("Unable to write runtime crash dump", path.c_str(), 0);
        writing_dump = 0;
        return;
    }

    fprintf(file, "{\n");
    write_metadata(file, reason);
    fprintf(file, ",\n");
    building_runtime_debug_dump(file);
    fprintf(file, ",\n");
    figure_debug_dump(file);
    fprintf(file, ",\n");
    figure_runtime_debug_dump(file);
    fprintf(file, ",\n");
    city_population_debug_dump(file);
    fprintf(file, "\n}\n");

    platform_file_manager_close_file(file);
    log_info("Runtime crash dump written", path.c_str(), 0);
    writing_dump = 0;
}

#endif
