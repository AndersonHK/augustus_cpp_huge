#pragma once

#include "building/building_fwd.h"
#include "building/building_type.h"
#include "figure/type.h"
#include "map/point.h"

class Figure;

class BuildingFigureGenerator {
public:
    void generate();

private:
    Building working_pantheon();
    int type_is_basic_temple(building_type type);
    int type_uses_native_spawn(building_type type);
    int figure_belongs_to_building(const Figure *figure, const building *owner);
    void attach_figure_to_building(Figure *figure, building *owner);
    int worker_percentage(const Building &building);
    int worker_slot_capacity(const Building &building, int pct75, int pct50, int pct1);
    int has_figure_of_types(building *record, figure_type type1, figure_type type2);
    int has_figure_of_type(building *record, figure_type type);
    int worker_spawn_delay_days(
        const Building &building,
        int pct100,
        int pct75,
        int pct50,
        int pct25,
        int pct1);
    int worker_scaled_spawn_delay(
        const Building &building,
        int pct100,
        int pct75,
        int pct50,
        int pct25,
        int pct1);
    int default_spawn_delay(const Building &building);
    void create_roaming_figure(building *record, int x, int y, figure_type type);
    void spawn_figure_warehouse(building *record);
    void spawn_figure_granary(building *record);
    void spawn_figure_tower(building *record);
    void spawn_figure_market(building *record);
    void spawn_figure_grand_temple_mars(building *record);
    void spawn_figure_tavern(building *record);
    void spawn_figure_temple(building *record);
    void spawn_figure_senate_forum(building *record);
    void spawn_figure_mission_post(building *record);
    void spawn_figure_industry(building *record);
    void spawn_figure_dock(building *record);
    void spawn_figure_native_hut(building *record);
    void spawn_figure_native_meeting(building *record);
    void spawn_figure_barracks(building *record);
    void spawn_figure_military_academy(building *record);
    void spawn_figure_fort_supplier(building *fort);
    void spawn_figure_mess_hall(building *record);
    void spawn_figure_depot(building *record);
    void advance_native_crop_graphics(building *record);
    int building_uses_runtime_spawn(const building *record);
};

void building_figure_generate(void);
