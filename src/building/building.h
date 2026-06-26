#pragma once

#include "building/building_fwd.h"
#include "building/building_order.h"
#include "building/storage_type.h"
#include "building/building_type.h"
#include "game/resource.h"

#include "graphics/color.h"
#include "map/point.h"

#include <cstdint>
#include <functional>
#include <source_location>

namespace building_type_registry_impl {
class BuildingAnimation;
}

class building_runtime;

class Building {
    friend class building_runtime;
    friend class building_type_registry_impl::BuildingAnimation;

public:
    class TypeRange {
    public:
        class iterator {
        public:
            explicit iterator(::building *record);
            Building operator*() const;
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

    explicit Building(::building *record, const std::source_location &location = std::source_location::current());
    explicit Building(::building &record, const std::source_location &location = std::source_location::current());
    Building(::building *record, const building_type_registry_impl::BuildingType *type_definition,
        const std::source_location &location = std::source_location::current());
    Building(::building &record, const building_type_registry_impl::BuildingType *type_definition,
        const std::source_location &location = std::source_location::current());

    static TypeRange of_type(building_type type);
    static Building first_of_type(building_type type);
    static Building create(building_type type, int x, int y);
    static int count();

    unsigned int id() const;
    const ::building *record() const;
    Building main() const;
    Building composition_owner() const;
    Building next() const;
    void for_each_part(const std::function<void(Building)> &visitor) const;
    Building next_of_type() const;
    const building_type_registry_impl::BuildingType *type = nullptr;
    int matches(const char *text_id) const;
    int grid_offset() const;
    int x() const;
    int y() const;
    int size() const;
    int previous_part_id() const;
    int next_part_id() const;
    int is_main_part() const;
    int road_network_id() const;
    int distance_from_entry() const;
    void set_distance_from_entry(int value);
    int road_access_x() const;
    int road_access_y() const;
    int state_id() const;
    int formation_id() const;
    void set_formation_id(int formation_id);
    int is_deleted() const;
    int is_in_use() const;
    int is_mothballed() const;
    int has_plague() const;
    int has_cached_road_access() const;
    int has_house_size() const;
    int house_population() const;
    void set_house_population(int value);
    int house_population_room() const;
    void set_house_population_room(int value);
    unsigned int immigrant_figure_id() const;
    void set_immigrant_figure_id(unsigned int id);
    int house_figure_generation_delay() const;
    building_runtime *runtime_instance() const;
    building_type_registry_impl::BuildingAnimation animate();
    int draw_footprint(const BuildingDrawContext &ctx);
    int draw_top(const BuildingDrawContext &ctx);
    int draw_animation(const BuildingDrawContext &ctx);
    void refresh_graphic();
    int refresh_graphic_if_native();
    void assign_graphic_variant(int force_reseed);
    void spawn_figure();
    int worker_count() const;
    int employment_worker_count() const;
    int employment_required_workers() const;
    float labor_access_score() const;
    int has_required_workers() const;
    int has_road_access(map_point *road) const;
    int has_water_access() const;
    int is_working() const;
    int is_merged_house() const;
    int has_primary_figure() const;
    int has_secondary_figure() const;
    int has_quaternary_figure() const;
    unsigned int distribution_cartpusher_id(int index) const;
    int resource_amount(resource_type resource) const;
    void add_resource(resource_type resource, int amount);
    void set_resource_amount(resource_type resource, int amount);
    int add_storage_resource(
        resource_type resource, int amount, building_type_registry_impl::StorageRole role);
    int storage_resource_amount(resource_type resource, building_type_registry_impl::StorageRole role) const;
    int input_storage_available_space(resource_type resource) const;
    int reserve_input_storage_load(resource_type resource, unsigned int figure_id);
    void release_input_storage_reservation(unsigned int figure_id);
    int receive_input_storage_loads(resource_type resource, int loads, unsigned int figure_id);
    int reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id = 0) const;
    int reserve_legacy_storage_loads(resource_type resource, int loads, unsigned int figure_id);
    void release_legacy_storage_reservation(unsigned int figure_id);
    int house_happiness() const;
    void set_house_happiness(int value);
    void set_fetch_inventory_id(resource_type resource);
    int accepts_good(resource_type resource) const;
    void set_accepted_good(resource_type resource, int value);
    void toggle_accepted_good(resource_type resource);
    void copy_accepted_goods(unsigned char *dst, int count) const;
    void set_accepted_goods(const unsigned char *src, int count);
    void set_primary_figure_id(unsigned int id);
    void copy_house_figure_slot_from(const Building &source, unsigned int figure_id);
    int max_distance_to(int x, int y) const;
    int max_distance_to(const Building &other) const;
    int orientation() const;
    void set_orientation(int orientation);
    int variant() const;
    void set_variant(int variant);
    int image_id() const;
    void add_map_tiles(int image_id) const;
    int storage_id() const;
    void set_storage_id(int storage_id);
    int blocked_storage_permission_mask() const;
    int warehouse_flag_frame() const;
    resource_type warehouse_resource_id() const;
    void set_warehouse_resource_id(resource_type resource);
    int loads_stored() const;
    int industry_has_raw_materials() const;
    int dock_has_accepted_route_ids() const;
    int dock_accepted_route_ids() const;
    int dock_trade_ship_id() const;
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
    int reserve_output_storage_loads(resource_type *out_resource, int *out_loads);
    int start_native_production();
    void advance_native_production_stats();
    void bless_native_farm();
    void curse_native_farm(int big_curse);
    void bless_native_industry();
    void set_industry_stockpiling(int value);
    void set_mothballed(int value);
    void change_type(building_type type, const std::source_location &location = std::source_location::current());
    int configure_house_replacement(building_type type, int x, int y, int size, int merged);
    void copy_house_data_from(const Building &source);
    void retire_replaced_house();
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
    std::uint64_t graphics_state_signature(int selected_graphics_option) const;

private:
    ::building *record_ = nullptr;
};


#include "core/buffer.h"
#include "translation/translation.h"

building *building_get(unsigned int id);

int building_dist(int x, int y, int w, int h, building *b);

void building_get_from_buffer(buffer *buf, int id, building *b, int includes_building_size, int save_version,
    int buffer_offset);

int building_count(void);

int building_find(building_type type);

int building_find_with_mothballed(building_type type);

int building_can_repair_type(building_type type);

building *building_first_of_type(building_type type);

void building_change_type(building *b, building_type type);

building *building_main(const building *b);

building *building_next(building *b);

building *building_create(building_type type, int x, int y);

int building_was_tent(const building *b);

int building_is_storage(building_type b_type);
/**
 * @brief Repairs a building using it's entry in the buildings array. In cases of warehouses and burning ruins,
 * some information is removed or reset, so data from b->data.rubble is used to help restore the building.
 * in the future, we should implement a more general system for saving and restoring building state.
 * Keeping a building in the array is helpful because it holds the building's ID, and allows keeping the storage structure.
 */

int building_repair(building *b);

int building_is_still_burning(building *b);

int building_can_repair(building *b);

int building_repair_cost(building *b);

void building_clear_related_data(building *b);

building *building_restore_from_undo(building *to_restore);

void building_trim(void);

void building_update_state(void);

void building_update_desirability(void);

int building_is_house(building_type type);

int building_get_house_group(building_type type);

int building_is_ceres_temple(building_type type);

int building_is_neptune_temple(building_type type);

int building_is_mercury_temple(building_type type);

int building_is_mars_temple(building_type type);

int building_is_venus_temple(building_type type);

int building_has_supplier_inventory(building_type type);

int building_is_house_group(house_groups group, building_type type);

int building_is_statue_garden_temple(building_type type);

int building_is_fort(building_type type);

int building_is_active(const building *b);

int building_is_primary_product_producer(building_type type);

int building_mothball_toggle(building *b);

int building_mothball_set(building *b, int value);

int building_get_tourism(const building *b);

int building_get_laborers(building_type type);

unsigned char building_stockpiling_toggle(building *b);

int building_get_tourism(const building *b);

int building_get_levy(const building *b);

void building_totals_add_corrupted_house(int unfixable);

void building_clear_all(void);

void building_make_immune_cheat(void);

int building_is_close_to_water(const building *b);

void building_save_state(buffer *buf, buffer *highest_id, buffer *highest_id_ever,
                         buffer *sequence, buffer *corrupt_houses);

void building_load_state(buffer *buf, buffer *sequence, buffer *corrupt_houses, int save_version);

void building_repair_loaded_compositions(void);

void building_resource_state_save(buffer *buf);

void building_resource_state_load(buffer *buf);
