#pragma once

#include "building/building_fwd.h"
#include "building/building_order.h"
#include "core/time.h"
#include "game/resource.h"

// Transitional runtime building record. Do not add peeled module state here:
// save/load bridges must read and write legacy payload fields through the owning runtime modules.
using workforce_count = short;

typedef struct building {
    unsigned int id;

    struct building *prev_of_type;
    struct building *next_of_type;

    time_millis last_update;

    unsigned char state;
    unsigned char faction_id;
    unsigned char unknown_value;
    unsigned char x; //these are not grid coordinates but image coordinates
    unsigned char y;
    short grid_offset;
    building_type type;
    union {
        short warehouse_resource_id;
        short orientation; // rotation of the building, in number of turns. Used for statues, warehouses, etc.
        short fort_figure_type;
        short native_meeting_center_id;
        short barracks_priority;
    } subtype;
    unsigned char road_network_id;
    unsigned short created_sequence;
    short houses_covered;
    float labor_access_score;
    short percentage_houses_covered;
    workforce_count local_workforce_assigned;
    workforce_count local_workforce_unemployed;
    short distance_from_entry;
    unsigned char road_access_x;
    unsigned char road_access_y;
    unsigned int figure_id;
    unsigned int figure_id2; // labor seeker or market supplier
    unsigned int figure_id4; // tower ballista, burning ruin prefect, doctor healing plague
    unsigned char figure_spawn_delay;
    unsigned char local_workforce_validation_delay;
    unsigned char days_since_offering;
    unsigned char figure_roam_direction;
    unsigned char has_water_access;
    short prev_part_building_id;
    short next_part_building_id;
    unsigned char has_well_access;
    short num_workers;
    unsigned char labor_category;
    unsigned char output_resource_id;
    unsigned char has_road_access;
    short damage_risk;
    short fire_risk;
    short fire_duration;
    unsigned char fire_proof; // cannot catch fire or collapse
    short formation_id;
    signed char levy_amount;
    struct {
        struct {
            short queued_docker_id;
            unsigned char num_ships;
            signed char orientation;
            short trade_ship_id;
            unsigned char has_accepted_route_ids;
            int accepted_route_ids;
        } dock;
        struct {
            unsigned int cartpusher_ids[3]; //changed from short to match f->id
        } distribution;
        struct {
            unsigned char fetch_inventory_id;
            unsigned char is_mess_hall;
            unsigned char pottery_demand;
            unsigned char furniture_demand;
            unsigned char oil_demand;
            unsigned char wine_demand;
        } market;
        struct {
            short progress;
            unsigned char blessing_days_left;
            unsigned char curse_days_left;
            unsigned char has_raw_materials;
            unsigned char has_fish;
            unsigned char is_stockpiling;
            unsigned char orientation;
            unsigned int fishing_boat_id; // in line with f->id
            unsigned int second_fishing_boat_id;
            unsigned char age_months;
            unsigned char average_production_per_month;
            short production_current_month;
        } industry;
        struct {
            unsigned char num_shows;
            unsigned char days1;
            unsigned char days2;
            unsigned char play;
        } entertainment;
        struct {
            unsigned short exceptions;
        } roadblock;
        struct {
            short flag_frame;
        } warehouse;
        struct {
            order current_order;
        } depot;
    } data;
    struct {
        int upgrades;
        short progress;
        short phase;
        short secondary_frame;
    } monument;
    int collected_tax_income;
    unsigned char has_plague;
    signed char desirability;
    unsigned char is_deleted;
    unsigned char is_close_to_water;
    unsigned char storage_id; //player-visible ID of the storage building, e.g. Granary 7, Warehouse 33
    signed char native_anger;
    unsigned char show_on_problem_overlay;
    unsigned char is_tourism_venue;
    unsigned char tourism_disabled;
    unsigned char tourism_income;
    unsigned char tourism_income_this_year;
    unsigned char upgrade_level;
    unsigned char strike_duration_days;
    unsigned char sickness_level;
    unsigned char sickness_duration;
    unsigned char sickness_doctor_cure;
    unsigned char fumigation_frame;
    unsigned char fumigation_direction;
    unsigned char has_latrines_access;
    short resources[RESOURCE_SLOT_COUNT];
    bool accepted_goods[RESOURCE_SLOT_COUNT];
} building;
