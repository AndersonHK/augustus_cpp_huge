#include "core/Logger.h"
#include <cstdio>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

struct Entry { Logger::Severity severity; std::string text; };
static void require(bool result, const char *message) { if (!result) throw std::runtime_error(message); }

int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "--fatal") {
        Logger::Scope scope("fatal-test", "child process context");
        Logger::fatal("Fatal test", "Deliberate fatal fixture");
    }
    try {
        std::vector<Entry> entries;
        const auto previous = Logger::set_output([](void *data, Logger::Severity severity, const char *text) {
            static_cast<std::vector<Entry> *>(data)->push_back({severity, text});
        }, &entries);
        struct Restore { Logger::Destination previous; ~Restore() { Logger::set_output(previous.output, previous.userdata); } } restore{previous};
        {
            Logger::Scope scope("outer", "city.svv");
            Logger::info("same"); Logger::info("same");
            require(entries.size() == 1 && entries[0].text == "same", "Info should omit scopes and summarize repeats");
            Logger::warning("same"); Logger::warning("same");
            require(entries.size() == 3 && entries[1].severity == Logger::Severity::Warning && entries[2].text.find("city.svv") != std::string::npos, "Repeated warnings or severity distinctions were lost");
            Logger::Scope inner("inner", "old"); inner.set_context("new");
            const std::string long_detail(12000, 'x');
            Logger::error("long", long_detail.c_str());
            require(entries.back().text.find(long_detail) != std::string::npos && entries.back().text.find("new") != std::string::npos, "Long message/context was truncated");
            std::thread worker([&] { Logger::Scope context("worker", "isolated"); Logger::error("thread"); });
            worker.join();
            require(entries.back().text.find("worker") != std::string::npos && entries.back().text.find("city.svv") == std::string::npos, "Context leaked across threads");
            int calls = 0;
            inner.set_callback([](void *value) { ++*static_cast<int *>(value); Logger::log_context(); }, &calls);
            Logger::log_context();
            require(calls == 1, "Context callback recursion was not contained");
        }
        Logger::error("outside");
        require(entries.back().text == "outside", "Destroyed scopes leaked into later diagnostics");
        {
            Logger::Scope old("old");
            Logger::clear_context();
            { Logger::Scope fresh("fresh"); Logger::warning("fresh message"); }
        }
        Logger::flush();
        const auto count = entries.size();
        Logger::flush();
        require(entries.size() == count, "Flush duplicated a repeat summary");
        Logger::info("after flush"); Logger::info("after flush"); Logger::flush();
        require(entries.back().text.find("occurred 2 times") != std::string::npos, "Logging stopped counting after a flush");
        Logger::errorf("formatted %d %s", 0, "zero");
        require(entries.back().text == "formatted 0 zero", "Formatted logger lost zero values");
        Logger::set_output(previous.output, previous.userdata);
        std::puts("Logger contracts passed: severity, repeats, full context, thread isolation, callbacks, reset, flush and formatting.");
        return 0;
    } catch (const std::exception &error) {
        Logger::set_output(nullptr);
        std::fprintf(stderr, "Logger test failed: %s\n", error.what());
        return 1;
    }
}
