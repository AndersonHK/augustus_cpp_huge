#include "core/Logger.h"

#include <atomic>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {
struct Context {
    uint64_t token;
    std::string stage;
    std::string detail;
    Logger::ContextCallback callback;
    void *userdata;
};
thread_local std::vector<Context> contexts;
thread_local uint64_t next_token = 0;
thread_local bool reporting_context = false;

Context *find_context(uint64_t token)
{
    for (auto &context : contexts) if (context.token == token) return &context;
    return nullptr;
}

std::string context_text()
{
    std::string result;
    for (const auto &context : contexts) {
        result += "\nContext: " + context.stage;
        if (!context.detail.empty()) result += "\n  " + context.detail;
    }
    return result;
}

void console_output(void *, Logger::Severity severity, const char *message)
{
    FILE *stream = severity >= Logger::Severity::Warning ? stderr : stdout;
    std::fprintf(stream, "%s: %s\n", Logger::label(severity), message);
    std::fflush(stream);
}

std::string format_message(const char *format, va_list args)
{
    va_list copy;
    va_copy(copy, args);
    const int length = std::vsnprintf(nullptr, 0, format, copy);
    va_end(copy);
    if (length < 0) return format;
    std::string text(static_cast<size_t>(length) + 1, '\0');
    std::vsnprintf(text.data(), text.size(), format, args);
    text.resize(static_cast<size_t>(length));
    return text;
}

std::string with_value(const char *detail, int value)
{
    std::string text = detail ? detail : "";
    if (value) { if (!text.empty()) text += "  "; text += std::to_string(value); }
    return text;
}

std::string wrap_dialog(const std::string &text)
{
    std::string result;
    size_t column = 0;
    for (char ch : text) {
        if (ch == '\n') column = 0;
        else if (column >= 96 && ch == ' ') { ch = '\n'; column = 0; }
        else ++column;
        result += ch;
    }
    return result;
}
}

struct Logger::State {
    std::recursive_mutex mutex;
    Destination destination{console_output, nullptr};
    Dialog dialog = nullptr;
    std::atomic<bool> debug{false};
    struct Repeat { std::string message; uint64_t count = 1; };
    std::vector<Repeat> info;

    void flush()
    {
        auto pending = std::move(info);
        info.clear();
        for (const auto &entry : pending) if (entry.count > 1) {
            const auto text = entry.message + " (occurred " + std::to_string(entry.count) + " times total)";
            destination.output(destination.userdata, Severity::Info, text.c_str());
        }
    }
};

Logger::State &Logger::state()
{
    static State instance;
    // Registered after instance construction, so flushing precedes its destruction.
    static const bool registered = [] { std::atexit(Logger::flush); return true; }();
    (void) registered;
    return instance;
}

Logger::Scope::Scope(const char *stage, const char *context, ContextCallback callback, void *userdata) : token_(++next_token)
{
    contexts.push_back({token_, stage ? stage : "", context ? context : "", callback, userdata});
}

Logger::Scope::~Scope()
{
    for (auto it = contexts.begin(); it != contexts.end(); ++it) if (it->token == token_) { contexts.erase(it); break; }
}

void Logger::Scope::set_context(const char *text) { if (auto *context = find_context(token_)) context->detail = text ? text : ""; }
void Logger::Scope::set_callback(ContextCallback callback, void *userdata)
{
    if (auto *context = find_context(token_)) { context->callback = callback; context->userdata = userdata; }
}

const char *Logger::label(Severity severity)
{
    switch (severity) {
        case Severity::Info: return "INFO";
        case Severity::Warning: return "WARNING";
        case Severity::Error: return "ERROR";
        case Severity::Fatal: return "FATAL";
    }
    return "ERROR";
}

void Logger::report(Severity severity, const char *message, const char *detail)
{
    std::string text = message ? message : label(severity);
    if (detail && *detail) { text += "  "; text += detail; }
    if (severity != Severity::Info) text += context_text();
    auto &logger = state();
    std::lock_guard lock(logger.mutex);
    // Never hide a repeated warning/error from save gates or diagnostic counters.
    // Only informational messages are summarized, with bounded memory use.
    if (severity == Severity::Info) {
        for (auto &entry : logger.info) if (entry.message == text) { ++entry.count; return; }
        if (logger.info.size() == 64) logger.flush();
        logger.info.push_back({text, 1});
    }
    logger.destination.output(logger.destination.userdata, severity, text.c_str());
}

void Logger::info(const char *message, const char *detail, int value) { report(Severity::Info, message, with_value(detail, value).c_str()); }
void Logger::warning(const char *message, const char *detail, int value) { report(Severity::Warning, message, with_value(detail, value).c_str()); }
void Logger::error(const char *message, const char *detail, int value) { report(Severity::Error, message, with_value(detail, value).c_str()); }

void Logger::infof(const char *format, ...) { va_list args; va_start(args, format); auto text = format_message(format, args); va_end(args); info(text.c_str()); }
void Logger::warningf(const char *format, ...) { va_list args; va_start(args, format); auto text = format_message(format, args); va_end(args); warning(text.c_str()); }
void Logger::errorf(const char *format, ...) { va_list args; va_start(args, format); auto text = format_message(format, args); va_end(args); error(text.c_str()); }

[[noreturn]] void Logger::fatal(const char *title, const char *message, const char *detail)
{
    report(Severity::Fatal, message, detail);
    log_context();
    flush();
    Dialog dialog;
    { auto &logger = state(); std::lock_guard lock(logger.mutex); dialog = logger.dialog; }
    if (dialog) {
        const auto text = wrap_dialog(std::string(message ? message : "Fatal error") + "\n\n" + (detail ? detail : "") + "\n\nThe game will now close. More details were written to vespasian-log.txt.");
        dialog(title ? title : "Vespasian Fatal Error", text.c_str());
    }
    std::exit(EXIT_FAILURE);
}

Logger::Destination Logger::set_output(Output output, void *userdata)
{
    auto &logger = state(); std::lock_guard lock(logger.mutex);
    logger.flush();
    const auto previous = logger.destination;
    logger.destination = {output ? output : console_output, userdata};
    return previous;
}

void Logger::set_dialog(Dialog dialog) { auto &logger = state(); std::lock_guard lock(logger.mutex); logger.dialog = dialog; }
void Logger::flush() { auto &logger = state(); std::lock_guard lock(logger.mutex); logger.flush(); }
bool Logger::debug_enabled() { return state().debug; }
void Logger::set_debug_enabled(bool enabled) { state().debug = enabled; }
void Logger::clear_context() { contexts.clear(); }
void Logger::set_stage(const char *stage, const char *context)
{
    if (contexts.empty()) contexts.push_back({++next_token, "", "", nullptr, nullptr});
    contexts.back().stage = stage ? stage : "";
    contexts.back().detail = context ? context : "";
}

void Logger::log_context()
{
    if (reporting_context) return;
    struct Guard { Guard() { reporting_context = true; } ~Guard() { reporting_context = false; } } guard;
    const auto snapshot = contexts;
    const auto text = context_text();
    if (!text.empty()) info(text.c_str());
    for (const auto &context : snapshot) if (context.callback) context.callback(context.userdata);
}
