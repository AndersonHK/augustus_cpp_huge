#pragma once

#include "building/building.h"
#include "figure/type.h"
#include "map/point.h"

#include <vector>

class Figure;

class BuildingEntertainment {
public:
    explicit BuildingEntertainment(const Building &venue);

    void spawn_race_participants();
    void spawn_hippodrome_service();
    void spawn_colosseum_service();
    void run_show_countdown();

private:
    building *record() const;
    static int is_show_venue(const Building &building);
    int has_figure_of_types(figure_type primary_type, figure_type secondary_type = FIGURE_NONE) const;
    int has_figure_of_type(figure_type type) const;
    void attach_figure_to_venue(Figure *figure) const;
    void copy_problem_overlay_to_hippodrome_parts() const;
    int find_hippodrome_road(map_point &road) const;
    int find_venue_road(map_point &road) const;
    void run_labor_phase(const map_point &road) const;
    int delay_has_elapsed(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const;
    Figure *create_charioteer(const map_point &road, const char *profile_id, int fallback_action) const;
    Figure *create_roaming_figure(const map_point &road, figure_type type) const;
    bool create_race_participants() const;
    void spawn_execution_lion_tamers(const map_point &road) const;
    void post_hippodrome_message_if_active() const;
    void post_colosseum_message_if_active() const;

    Building venue_;
};

void building_entertainment_run_shows(void);
