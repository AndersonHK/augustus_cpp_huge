#include "core/log.h"
#include "SDL.h"

#include <cstdio>
#include <cstring>

namespace {

constexpr int MSG_SIZE = 1000;
constexpr int MAX_OLD_MESSAGES = 5;

char log_buffer[MSG_SIZE];
struct PreviousLogMessage {
    char buffer[MSG_SIZE];
    unsigned int count;
};
PreviousLogMessage previous_log_messages[MAX_OLD_MESSAGES];
int old_message_index;
bool debug_enabled;

const char *build_message(const char *msg, const char *param_str, int param_int)
{
    int index = 0;
    index += std::snprintf(&log_buffer[index], MSG_SIZE - index, "%s", msg);
    if (param_str) {
        index += std::snprintf(&log_buffer[index], MSG_SIZE - index, "  %s", param_str);
    }
    if (param_int) {
        index += std::snprintf(&log_buffer[index], MSG_SIZE - index, "  %d", param_int);
    }
    return log_buffer;
}

bool count_archived_message()
{
    for (int i = 0; i < MAX_OLD_MESSAGES; i++) {
        if (std::strcmp(previous_log_messages[i].buffer, log_buffer) == 0) {
            previous_log_messages[i].count++;
            return true;
        }
    }
    if (old_message_index == MAX_OLD_MESSAGES) {
        log_repeated_messages();
    }
    std::snprintf(previous_log_messages[old_message_index++].buffer, MSG_SIZE, "%s", log_buffer);
    if (old_message_index < MAX_OLD_MESSAGES) {
        previous_log_messages[old_message_index].count = 0;
    }
    return false;
}

} // namespace

void log_repeated_messages()
{
    for (int i = 0; i < MAX_OLD_MESSAGES; i++) {
        if (previous_log_messages[i].count) {
            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s (message repeats %u %s)",
                previous_log_messages[i].buffer, previous_log_messages[i].count,
                previous_log_messages[i].count == 1 ? "time" : "times");
        }
        previous_log_messages[i].buffer[0] = 0;
        previous_log_messages[i].count = 0;
    }
    old_message_index = 0;
}

void log_info(const char *msg, const char *param_str, int param_int)
{
    build_message(msg, param_str, param_int);
    if (!count_archived_message()) {
        SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION, "%s", log_buffer);
    }
}

void log_set_debug_enabled(bool enabled)
{
    debug_enabled = enabled;
}

bool log_is_debug_enabled()
{
    return debug_enabled;
}

void log_warning(const char *msg, const char *param_str, int param_int)
{
    build_message(msg, param_str, param_int);
    if (!count_archived_message()) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "%s", log_buffer);
    }
}

void log_error(const char *msg, const char *param_str, int param_int)
{
    build_message(msg, param_str, param_int);
    if (!count_archived_message()) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", log_buffer);
    }
}
