#pragma once

#include "building/building_fwd.h"
#include "building/BuildingRuntimeList.h"
#include "building/BuildingGraphics.h"
#include "building/BuildingComposition.h"
#include "building/BuildingFoundation.h"
#include "building/HousingModule.h"
#include "building/RubbleModule.h"
#include "building/building_order.h"
#include "building/storage_type.h"
#include "building/building_type.h"
#include "game/resource.h"

#include "game/Animation.h"
#include "graphics/color.h"
#include "map/point.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <source_location>

class building_runtime;
class BuildingGraphicsState;
struct formation;
class Figure;

class Building {
    friend class building_runtime;
    friend class building_type_registry_impl::BuildingAnimation;

public:
    struct MaintenanceRiskTick {
        int random_selector = 0;
        int damage_risk_bonus = 0;
        int fire_risk_bonus = 0;
        bool suppress_fire = false;
    };

    enum class MaintenanceRiskOutcome {
        None,
        Collapse,
        Fire,
    };

    template <typename Stored, typename Public = Stored>
    class RecordField {
    public:
        RecordField() = default;
        explicit RecordField(Stored *value)
            : value_(value),
              fallback_(value ? static_cast<Public>(*value) : Public{})
        {}

        operator Public() const
        {
            return value_ ? static_cast<Public>(*value_) : fallback_;
        }

        RecordField &operator=(Public value)
        {
            fallback_ = value;
            if (value_) {
                *value_ = static_cast<Stored>(value);
            }
            return *this;
        }

    private:
        Stored *value_ = nullptr;
        Public fallback_ = {};
    };

    class TypeRange {
    public:
        class iterator {
        public:
            explicit iterator(::building *record);
            Building &operator*() const;
            iterator &operator++();
            bool operator!=(const iterator &other) const;

        private:
            ::building *record_ = nullptr;
        };

        iterator begin() const;
        iterator end() const;

    private:
        friend class Building;
        explicit TypeRange(building_type type);

        building_type type_ = BUILDING_NONE;
    };

    Building(
        ::building *record,
        BuildingGraphicsState *graphics_state,
        const std::source_location &location = std::source_location::current());
    Building(std::nullptr_t) = delete;
    Building(
        std::nullptr_t,
        BuildingGraphicsState *graphics_state,
        const std::source_location &location = std::source_location::current()) = delete;
    Building(
        ::building &record,
        BuildingGraphicsState &graphics_state,
        const std::source_location &location = std::source_location::current());
    Building(
        ::building *record,
        const building_type_registry_impl::BuildingType *type_definition,
        BuildingGraphicsState *graphics_state,
        const std::source_location &location = std::source_location::current());
    Building(
        ::building *record,
        const building_type_registry_impl::BuildingType *type_definition,
        BuildingGraphicsState *graphics_state,
        const RubbleDef *rubble_definition,
        RubbleState *rubble_state,
        const std::source_location &location = std::source_location::current());
    Building(
        std::nullptr_t,
        const building_type_registry_impl::BuildingType *type_definition,
        BuildingGraphicsState *graphics_state,
        const std::source_location &location = std::source_location::current()) = delete;
    Building(
        ::building &record,
        const building_type_registry_impl::BuildingType *type_definition,
        BuildingGraphicsState &graphics_state,
        const std::source_location &location = std::source_location::current());
    Building(const Building &other);
    Building &operator=(const Building &other);

    static TypeRange of_type(building_type type);
    static Building *first_of_type(building_type type);
    static Building *get(unsigned int id);
    // Iterates live runtime-owned Building objects; filters belong here so callers stop open-coding id scans.
    static void for_each(const std::function<void(Building *)> &visitor);
    static void for_each(BuildingRuntimeList list, const std::function<void(Building *)> &visitor);
    static int count();

    const ::building *record() const;
    // Dynamic bridge record-chain API. Fixed composition must use
    // BuildingComposition owner/child relationships instead.
    Building &dynamic_bridge_owner() const;
    Building *dynamic_bridge_next() const;
    Building *next_of_type() const;
    const building_type_registry_impl::BuildingType *type = nullptr;
    RubbleModule *Rubble = nullptr;
    building_type_registry_impl::BuildingFoundation *Foundation = nullptr;
    HousingModule *Housing = nullptr;
    BuildingComposition *Composition = nullptr;
    formation *Formation = nullptr;
    int matches(const char *text_id) const;
    int grid_offset() const;
    int x() const;
    int y() const;
    int is_dynamic_bridge_owner() const;
    int is_dynamic_bridge_segment() const;
    int has_dynamic_bridge_predecessor() const;
    int has_dynamic_bridge_next() const;
    int road_network_id() const;
    int distance_from_entry() const;
    void set_distance_from_entry(int value);
    int road_access_x() const;
    int road_access_y() const;
    int state_id() const;
    int formation_id() const;
    void set_formation_id(int formation_id);
    formation *formation_object() const;
    int is_deleted() const;
    int is_created() const;
    int is_in_use() const;
    int is_mothballed() const;
    int is_fire_proof() const;
    MaintenanceRiskOutcome apply_maintenance_risk(const MaintenanceRiskTick &tick);
    void destroy_by_collapse();
    void destroy_by_fire();
    void destroy_by_plague();
    void destroy_without_rubble();
    void initialize_destruction_rubble(const RubbleState &origin, bool burning, bool plagued);
    int rubble_is_still_burning() const;
    int repair_cost() const;
    int repair();
    int yield_rubble_to_repair(const RubbleState &origin);
    void restore_rubble_after_failed_repair();
    void retire_rubble_after_repair();
    int has_plague() const;
    void advance_plague_day();
    void apply_plague_treatment();
    int has_cached_road_access() const;
    int cached_road_access_point(map_point *road) const;
    int access_area_touches_same_road_network(
        const map_point &source_road,
        int radius,
        bool allow_highways = false) const;
    building_runtime *runtime_instance() const;
    BuildingGraphics &Graphics(const std::source_location &location = std::source_location::current()) const;
    building_type_registry_impl::BuildingAnimation animate();
    int draw_footprint(const BuildingDrawContext &ctx);
    int draw_top(const BuildingDrawContext &ctx);
    int draw_animation(const BuildingDrawContext &ctx);
    int mothball_status_icon_offset(int icon_width, int icon_height, int *x, int *y) const;
    void invalidate_graphic();
    void refresh_graphic();
    void spawn_figure();
    int worker_count() const;
    int employment_worker_count() const;
    int employment_required_workers() const;
    float labor_access_score() const;
    int has_required_workers() const;
    int has_road_access(map_point *road) const;
    int query_road_access_point(map_point *road) const;
    int storage_destination_road_access_point(map_point *road) const;
    int has_water_access() const;
    int is_working() const;
    int has_primary_figure() const;
    int has_secondary_figure() const;
    int has_quaternary_figure() const;
    int clear_figure_slot_if_matches(unsigned int figure_id);
    int clear_distribution_cartpusher_slot_if_matches(unsigned int figure_id);
    unsigned int distribution_cartpusher_id(int index) const;
    void set_figure_spawn_delay(int ticks);
    int resource_amount(resource_type resource) const;
    void add_resource(resource_type resource, int amount);
    void set_resource_amount(resource_type resource, int amount);
    int add_storage_resource(
        resource_type resource, int amount, building_type_registry_impl::StorageRole role);
    int storage_resource_amount(resource_type resource, building_type_registry_impl::StorageRole role) const;
    int input_storage_available_space(resource_type resource) const;
    int reserve_input_storage_load(resource_type resource, Figure &figure);
    void release_input_storage_reservation(const Figure &figure);
    int receive_input_storage_loads(resource_type resource, int loads, Figure &figure);
    int reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id = 0) const;
    int reserve_legacy_storage_loads(resource_type resource, int loads, Figure &figure);
    void release_legacy_storage_reservation(const Figure &figure);
    void release_storage_reservations();
    void set_fetch_inventory_id(resource_type resource);
    bool accepts_good(resource_type resource) const;
    void set_accepted_good(resource_type resource, bool accepted);
    void toggle_accepted_good(resource_type resource);
    unsigned char distribution_demand(resource_type resource) const;
    void set_distribution_demand(resource_type resource, unsigned char demand);
    void copy_accepted_goods(unsigned char *dst, int count) const;
    void set_accepted_goods(const unsigned char *src, int count);
    void set_primary_figure_id(unsigned int id);
    void set_quaternary_figure_id(unsigned int id);
    void copy_house_figure_slot_from(const Building &source, unsigned int figure_id);
    int max_distance_to(int x, int y) const;
    int max_distance_to(const Building &other) const;
    int orientation() const;
    void set_orientation(int orientation);
    void add_map_tiles();
    void remove_map_tiles();
    int is_surface_terrain_tile() const;
    void set_storage_id(int new_storage_id);
    int blocked_storage_permission_mask() const;
    int warehouse_flag_frame() const;
    resource_type warehouse_resource_id() const;
    void set_warehouse_resource_id(resource_type resource);
    int loads_stored() const;
    int industry_has_raw_materials() const;
    int dock_accepted_route_ids() const;
    int dock_trade_ship_id() const;
    void set_dock_trade_ship_id(int figure_id);
    int dock_num_ships() const;
    void set_dock_num_ships(int ticks);
    void decrement_dock_num_ships();
    int dock_queued_docker_id() const;
    void set_dock_queued_docker_id(int figure_id);
    int dock_orientation() const;
    int dock_idle_worker_count() const;
    void set_has_water_access(int value);
    void set_dock_accepted_route_ids(int has_route_ids, int route_ids);
    const order &depot_order() const;
    void set_depot_order(const order &value);
    int industry_is_stockpiling() const;
    int has_required_raw_amount_for_production(resource_type resource) const;
    int has_native_production() const;
    int native_production_has_raw_materials() const;
    int native_production_max_progress() const;
    int native_production_efficiency() const;
    int update_native_production(int new_day, int *out_is_striking);
    int native_production_has_completed_effect() const;
    int output_cart_capacity(resource_type resource) const;
    int industry_has_fish() const;
    void add_industry_fish(int amount);
    void add_industry_production_current_month(int amount);
    int reserve_output_storage_loads(resource_type *out_resource, int *out_loads);
    int start_native_production();
    void advance_native_production_stats();
    void bless_native_farm();
    void curse_native_farm(int big_curse);
    void bless_native_industry();
    void set_industry_stockpiling(int value);
    void set_mothballed(int value);
    void change_type(building_type type, const std::source_location &location = std::source_location::current());
    int configure_house_replacement(building_type type, int x, int y);
    void copy_house_data_from(const Building &source);
    void retire_replaced_house();
    void cleanup_figure_references_for_removal();
    int is_being_fumigated() const;
    int fumigation_frame() const;
    void set_fumigation_direction(int direction);
    int fort_figure_type() const;
    int is_unfinished_monument() const;
    int monument_upgrade_level() const;
    int monument_phase() const;
    int monument_secondary_frame() const;
    int entertainment_days1() const;
    int entertainment_days2() const;
    int desirability() const;
    int building_mothball_toggle();

    RecordField<unsigned int> id;
    RecordField<unsigned char, unsigned int> storage_id;
    RecordField<unsigned char, int> dock_has_accepted_route_ids;

private:
    void retire_for_destruction();
    void bind_record_fields();
    void bind_graphics(BuildingGraphicsState *graphics_state);
    void bind_rubble(const RubbleDef *rubble_definition, RubbleState *rubble_state);

    ::building *record_ = nullptr;
    std::source_location construction_location_;
    mutable BuildingGraphics graphics_;
    mutable RubbleModule rubble_;
};


#include "core/buffer.h"
#include "translation/translation.h"

int building_dist(int x, int y, int w, int h, building *b);

void building_get_from_buffer(buffer *buf, int id, building *b, int includes_building_size, int save_version,
    int buffer_offset);

int building_count(void);

// Load/startup bridge: walk full save records before runtime Building instances are materialized.
void building_for_each_loaded_record(const std::function<void(building *)> &visitor);

int building_change_type(building *b, building_type type);
unsigned char building_distribution_demand(const building *b, resource_type resource);
void building_set_distribution_demand(building *b, resource_type resource, unsigned char demand);
unsigned char building_accepted_good_save_value(const building *b, resource_type resource);
void building_load_accepted_good(building *b, resource_type resource, unsigned char value);

void building_clear_related_data(building *b);

building *building_restore_from_undo(building *to_restore);

void building_trim(void);

void building_update_state(void);

void building_update_desirability(void);

int building_is_fort(building_type type);

int building_is_active(const building *b);

int building_mothball_toggle(building *b);

int building_mothball_set(building *b, int value);

int building_get_tourism(const building *b);

int building_get_laborers(building_type type);

unsigned char building_stockpiling_toggle(building *b);

int building_get_tourism(const building *b);

int building_get_levy(const building *b);

void building_clear_all(void);

void building_make_immune_cheat(void);

int building_is_close_to_water(const building *b);

void building_save_state(buffer *buf, buffer *highest_id, buffer *highest_id_ever,
                         buffer *sequence, buffer *corrupt_houses);

int building_load_state(buffer *buf, buffer *sequence, buffer *corrupt_houses, int save_version);

// Load/new-scenario boundary: hydrate fixed composition object graphs from
// legacy record chains before normal runtime initialization.
int building_hydrate_loaded_compositions(int save_version);

void building_resource_state_save(buffer *buf);

void building_resource_state_load(buffer *buf);
