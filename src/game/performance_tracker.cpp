#include "game/performance_tracker.h"

#include "core/file.h"
#include "game/settings.h"
#include "platform/platform.h"

#include "SDL.h"

#include <stdio.h>
#include <string.h>
#include <string>

static const char *BUCKET_NAMES[PERFORMANCE_TRACKER_BUCKET_MAX] = {
    "frame",
    "run",
    "draw",
    "present",
    "tick",
    "advance",
    "figure",
    "formation",
    "road_network",
    "resource",
    "water",
    "production",
    "labor",
    "desirability",
    "building_state",
    "house_evolution",
    "figure_generation",
    "culture",
    "trade",
    "maintenance",
    "city_draw",
    "city_draw_footprint",
    "city_draw_main_row",
    "city_draw_main_top",
    "city_draw_main_figures",
    "city_draw_main_animation",
    "city_draw_deletion",
    "city_draw_elevated",
    "city_draw_ghost",
    "city_draw_overlay",
    "city_draw_weather",
    "city_draw_clouds"
};

static const char *ROUTE_METRIC_NAMES[PERFORMANCE_TRACKER_ROUTE_METRIC_MAX] = {
    "requests",
    "plans",
    "cost_maps",
    "cache_hits",
    "cache_misses",
    "pruned_by_network",
    "pruned_by_current_path",
    "failed",
    "async_jobs"
};

static const char *ROUTE_PURPOSE_NAMES[PERFORMANCE_TRACKER_ROUTE_PURPOSE_MAX] = {
    "movement",
    "distance_query",
    "local_workforce",
    "venue",
    "storage",
    "construction",
    "water",
    "wall",
    "debug"
};

static const char *RENDER_METRIC_NAMES[PERFORMANCE_TRACKER_RENDER_METRIC_MAX] = {
    "submissions",
    "managed_submissions",
    "atlas_fallback_submissions",
    "silhouette_submissions",
    "texture_switches",
    "grid_overlays"
};

static thread_local performance_tracker_route_purpose current_route_purpose =
    PERFORMANCE_TRACKER_ROUTE_PURPOSE_DEBUG;

static struct {
    int enabled;
    FILE *log_file;
    uint64_t frequency;
    uint64_t sample_start;
    uint64_t frame_start;
    uint64_t sample_index;
    uint64_t counters[PERFORMANCE_TRACKER_BUCKET_MAX];
    uint64_t frames;
    uint64_t actual_ticks;
    uint64_t target_ticks_x1000;
    uint64_t speed_wait_ms;
    uint64_t zero_tick_frames;
    uint64_t route_counters[PERFORMANCE_TRACKER_ROUTE_METRIC_MAX][PERFORMANCE_TRACKER_ROUTE_PURPOSE_MAX];
    uint64_t render_counters[PERFORMANCE_TRACKER_RENDER_METRIC_MAX];
} data;

static double counter_to_ms(uint64_t counter)
{
    if (!data.frequency) {
        return 0.0;
    }
    return (static_cast<double>(counter) * 1000.0) / static_cast<double>(data.frequency);
}

static double target_ticks(void)
{
    return static_cast<double>(data.target_ticks_x1000) / 1000.0;
}

static void reset_sample(uint64_t now)
{
    memset(data.counters, 0, sizeof(data.counters));
    memset(data.route_counters, 0, sizeof(data.route_counters));
    memset(data.render_counters, 0, sizeof(data.render_counters));
    data.sample_start = now;
    data.frames = 0;
    data.actual_ticks = 0;
    data.target_ticks_x1000 = 0;
    data.speed_wait_ms = 0;
    data.zero_tick_frames = 0;
}

static void flush_sample(uint64_t now)
{
    if (!data.log_file || !data.sample_start) {
        reset_sample(now);
        return;
    }

    const uint64_t elapsed_counter = now - data.sample_start;
    if (elapsed_counter < data.frequency) {
        return;
    }

    data.sample_index++;
    fprintf(data.log_file,
        "perf sample=%llu elapsed_ms=%.3f speed=%d target_ticks=%.3f actual_ticks=%llu frames=%llu "
        "zero_tick_frames=%llu speed_wait_ms=%.3f frame_ms=%.3f run_ms=%.3f tick_ms=%.3f draw_ms=%.3f "
        "present_ms=%.3f buckets=",
        static_cast<unsigned long long>(data.sample_index),
        counter_to_ms(elapsed_counter),
        setting_game_speed(),
        target_ticks(),
        static_cast<unsigned long long>(data.actual_ticks),
        static_cast<unsigned long long>(data.frames),
        static_cast<unsigned long long>(data.zero_tick_frames),
        static_cast<double>(data.speed_wait_ms),
        counter_to_ms(data.counters[PERFORMANCE_TRACKER_BUCKET_FRAME]),
        counter_to_ms(data.counters[PERFORMANCE_TRACKER_BUCKET_RUN]),
        counter_to_ms(data.counters[PERFORMANCE_TRACKER_BUCKET_TICK]),
        counter_to_ms(data.counters[PERFORMANCE_TRACKER_BUCKET_DRAW]),
        counter_to_ms(data.counters[PERFORMANCE_TRACKER_BUCKET_PRESENT]));

    int wrote_bucket = 0;
    for (int i = PERFORMANCE_TRACKER_BUCKET_ADVANCE; i < PERFORMANCE_TRACKER_BUCKET_MAX; i++) {
        if (!data.counters[i]) {
            continue;
        }
        fprintf(data.log_file, "%s%s:%.3f", wrote_bucket ? "," : "", BUCKET_NAMES[i], counter_to_ms(data.counters[i]));
        wrote_bucket = 1;
    }
    if (!wrote_bucket) {
        fprintf(data.log_file, "none");
    }
    int wrote_render_metric = 0;
    for (int metric = 0; metric < PERFORMANCE_TRACKER_RENDER_METRIC_MAX; metric++) {
        if (!data.render_counters[metric]) {
            continue;
        }
        if (!wrote_render_metric) {
            fprintf(data.log_file, " render_metrics=");
        }
        fprintf(data.log_file, "%s%s=%llu",
            wrote_render_metric ? "," : "",
            RENDER_METRIC_NAMES[metric],
            static_cast<unsigned long long>(data.render_counters[metric]));
        wrote_render_metric = 1;
    }
    int wrote_route_metric = 0;
    for (int metric = 0; metric < PERFORMANCE_TRACKER_ROUTE_METRIC_MAX; metric++) {
        int wrote_purpose = 0;
        for (int purpose = 0; purpose < PERFORMANCE_TRACKER_ROUTE_PURPOSE_MAX; purpose++) {
            if (!data.route_counters[metric][purpose]) {
                continue;
            }
            if (!wrote_route_metric) {
                fprintf(data.log_file, " route_metrics=");
            }
            if (!wrote_purpose) {
                fprintf(data.log_file, "%s%s:", wrote_route_metric ? ";" : "", ROUTE_METRIC_NAMES[metric]);
            }
            fprintf(data.log_file, "%s%s=%llu",
                wrote_purpose ? "," : "",
                ROUTE_PURPOSE_NAMES[purpose],
                static_cast<unsigned long long>(data.route_counters[metric][purpose]));
            wrote_purpose = 1;
        }
        if (wrote_purpose) {
            wrote_route_metric = 1;
        }
    }
    fprintf(data.log_file, "\n");
    fflush(data.log_file);

    reset_sample(now);
}

void performance_tracker_init(int enabled)
{
    memset(&data, 0, sizeof(data));
    if (!enabled) {
        return;
    }

    data.frequency = SDL_GetPerformanceFrequency();
    if (!data.frequency) {
        return;
    }

    const std::string log_file = platform::logging_path() + "vespasian-performance.log";

    file_remove(log_file);
    data.log_file = file_open(log_file, "wt");
    if (!data.log_file) {
        memset(&data, 0, sizeof(data));
        return;
    }

    data.enabled = 1;
    reset_sample(SDL_GetPerformanceCounter());
}

void performance_tracker_shutdown(void)
{
    if (data.log_file) {
        fflush(data.log_file);
        file_close(data.log_file);
    }
    memset(&data, 0, sizeof(data));
}

int performance_tracker_enabled(void)
{
    return data.enabled;
}

void performance_tracker_begin_frame(void)
{
    if (!data.enabled) {
        return;
    }
    data.frame_start = SDL_GetPerformanceCounter();
    data.frames++;
}

void performance_tracker_end_frame(void)
{
    if (!data.enabled || !data.frame_start) {
        return;
    }
    uint64_t now = SDL_GetPerformanceCounter();
    data.counters[PERFORMANCE_TRACKER_BUCKET_FRAME] += now - data.frame_start;
    data.frame_start = 0;
    flush_sample(now);
}

void performance_tracker_record_ticks_processed(int ticks)
{
    if (!data.enabled || ticks <= 0) {
        return;
    }
    data.actual_ticks += static_cast<uint64_t>(ticks);
}

void performance_tracker_record_speed_goal(uint64_t elapsed_ms, uint64_t millis_per_tick_x1000)
{
    if (!data.enabled || !millis_per_tick_x1000) {
        return;
    }
    data.target_ticks_x1000 += (elapsed_ms * 1000000ULL) / millis_per_tick_x1000;
}

void performance_tracker_record_speed_wait(uint64_t elapsed_ms)
{
    if (!data.enabled) {
        return;
    }
    data.zero_tick_frames++;
    data.speed_wait_ms += elapsed_ms;
}

void performance_tracker_record_route_metric(
    performance_tracker_route_metric metric,
    performance_tracker_route_purpose purpose,
    uint64_t amount)
{
    if (!data.enabled ||
        metric < 0 ||
        metric >= PERFORMANCE_TRACKER_ROUTE_METRIC_MAX ||
        purpose < 0 ||
        purpose >= PERFORMANCE_TRACKER_ROUTE_PURPOSE_MAX ||
        amount == 0) {
        return;
    }
    data.route_counters[metric][purpose] += amount;
}

void performance_tracker_record_route_plan(performance_tracker_route_purpose purpose)
{
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_REQUESTS,
        purpose,
        1);
    performance_tracker_record_route_metric(
        PERFORMANCE_TRACKER_ROUTE_METRIC_PLANS,
        purpose,
        1);
}

performance_tracker_route_purpose performance_tracker_current_route_purpose(void)
{
    return current_route_purpose;
}

void performance_tracker_record_render_metric(performance_tracker_render_metric metric, uint64_t amount)
{
    if (!data.enabled ||
        metric < 0 ||
        metric >= PERFORMANCE_TRACKER_RENDER_METRIC_MAX ||
        amount == 0) {
        return;
    }
    data.render_counters[metric] += amount;
}

PerformanceTrackerScope::PerformanceTrackerScope(performance_tracker_bucket bucket)
    : bucket_(bucket), start_(0), active_(0)
{
    if (!data.enabled || bucket < 0 || bucket >= PERFORMANCE_TRACKER_BUCKET_MAX) {
        return;
    }
    start_ = SDL_GetPerformanceCounter();
    active_ = 1;
}

PerformanceTrackerScope::~PerformanceTrackerScope()
{
    if (!active_) {
        return;
    }
    data.counters[bucket_] += SDL_GetPerformanceCounter() - start_;
}

PerformanceTrackerRouteScope::PerformanceTrackerRouteScope(performance_tracker_route_purpose purpose)
    : previous_(current_route_purpose)
{
    if (purpose >= 0 && purpose < PERFORMANCE_TRACKER_ROUTE_PURPOSE_MAX) {
        current_route_purpose = purpose;
    }
}

PerformanceTrackerRouteScope::~PerformanceTrackerRouteScope()
{
    current_route_purpose = previous_;
}
