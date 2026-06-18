#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PERFORMANCE_TRACKER_BUCKET_FRAME,
    PERFORMANCE_TRACKER_BUCKET_RUN,
    PERFORMANCE_TRACKER_BUCKET_DRAW,
    PERFORMANCE_TRACKER_BUCKET_PRESENT,
    PERFORMANCE_TRACKER_BUCKET_TICK,
    PERFORMANCE_TRACKER_BUCKET_ADVANCE,
    PERFORMANCE_TRACKER_BUCKET_FIGURE,
    PERFORMANCE_TRACKER_BUCKET_FORMATION,
    PERFORMANCE_TRACKER_BUCKET_ROAD_NETWORK,
    PERFORMANCE_TRACKER_BUCKET_RESOURCE,
    PERFORMANCE_TRACKER_BUCKET_WATER,
    PERFORMANCE_TRACKER_BUCKET_PRODUCTION,
    PERFORMANCE_TRACKER_BUCKET_LABOR,
    PERFORMANCE_TRACKER_BUCKET_DESIRABILITY,
    PERFORMANCE_TRACKER_BUCKET_BUILDING_STATE,
    PERFORMANCE_TRACKER_BUCKET_HOUSE_EVOLUTION,
    PERFORMANCE_TRACKER_BUCKET_FIGURE_GENERATION,
    PERFORMANCE_TRACKER_BUCKET_CULTURE,
    PERFORMANCE_TRACKER_BUCKET_TRADE,
    PERFORMANCE_TRACKER_BUCKET_MAINTENANCE,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_FOOTPRINT,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_MAIN_ROW,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_DELETION,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_ELEVATED,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_GHOST,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_OVERLAY,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_WEATHER,
    PERFORMANCE_TRACKER_BUCKET_CITY_DRAW_CLOUDS,
    PERFORMANCE_TRACKER_BUCKET_MAX
} performance_tracker_bucket;

void performance_tracker_init(int enabled);
void performance_tracker_shutdown(void);
int performance_tracker_enabled(void);
void performance_tracker_begin_frame(void);
void performance_tracker_end_frame(void);
void performance_tracker_record_ticks_processed(int ticks);
void performance_tracker_record_speed_goal(uint64_t elapsed_ms, uint64_t millis_per_tick_x1000);
void performance_tracker_record_speed_wait(uint64_t elapsed_ms);

#ifdef __cplusplus
}

class PerformanceTrackerScope {
public:
    explicit PerformanceTrackerScope(performance_tracker_bucket bucket);
    ~PerformanceTrackerScope();

    PerformanceTrackerScope(const PerformanceTrackerScope &) = delete;
    PerformanceTrackerScope &operator=(const PerformanceTrackerScope &) = delete;

private:
    performance_tracker_bucket bucket_;
    uint64_t start_;
    int active_;
};
#endif
