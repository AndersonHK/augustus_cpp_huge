#pragma once

#include <cstdint>

// One reporting vocabulary for the runtime and command-line tools. Context is
// neutral diagnostic information, not an error in itself.
// Info: expected diagnostic/compatibility note. Warning: unintended but probably
// safe. Error: unintended, survivable without corrupting state. Fatal: cannot
// continue safely; fatal() reports, optionally shows the host dialog, and exits.
class Logger final {
public:
    enum class Severity { Info, Warning, Error, Fatal };
    using Output = void (*)(void *userdata, Severity severity, const char *message);
    using Dialog = void (*)(const char *title, const char *message);
    using ContextCallback = void (*)(void *userdata);
    struct Destination { Output output; void *userdata; };

    class Scope final {
    public:
        Scope(const char *stage, const char *context = nullptr, ContextCallback callback = nullptr, void *userdata = nullptr);
        ~Scope();
        Scope(const Scope &) = delete;
        Scope &operator=(const Scope &) = delete;
        void set_context(const char *context);
        void set_callback(ContextCallback callback, void *userdata = nullptr);
    private:
        uint64_t token_;
    };

    static void info(const char *message, const char *detail = nullptr, int value = 0);
    static void warning(const char *message, const char *detail = nullptr, int value = 0);
    static void error(const char *message, const char *detail = nullptr, int value = 0);
    static void infof(const char *format, ...);
    static void warningf(const char *format, ...);
    static void errorf(const char *format, ...);
    [[noreturn]] static void fatal(const char *title, const char *message, const char *detail = nullptr);
    static void report(Severity severity, const char *message, const char *detail = nullptr);
    static const char *label(Severity severity);

    // Adapters own files/dialogs. The logger owns formatting, context and repeat
    // accounting. A null output restores the console sink (warnings to stderr).
    static Destination set_output(Output output, void *userdata = nullptr);
    static void set_dialog(Dialog dialog);
    static void flush();
    static bool debug_enabled();
    static void set_debug_enabled(bool enabled);
    static void clear_context();
    static void set_stage(const char *stage, const char *context);
    static void log_context();

private:
    Logger() = delete;
    struct State;
    static State &state();
};
