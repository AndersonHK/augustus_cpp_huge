#pragma once

#include "building/BuildingGraphicsState.h"
#include "building/BuildingForEachArgs.h"
#include "building/RubbleState.h"
#include "assets/image_group_entry.h"
#include "building/building.h"
#include "building/building_type.h"
#include "figure/figure.h"
#include "game/resource.h"

#include <cstddef>
#include <cstdio>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

class ImageGroupPayload;

void building_runtime_reset(void);
void building_runtime_begin_load_bridge(int building_count);
void building_runtime_stage_loaded_graphics_state(
    unsigned int building_id,
    const BuildingGraphicsState &state);
void building_runtime_stage_loaded_rubble_state(unsigned int building_id, const RubbleState &state);
int building_runtime_loaded_graphics_state(unsigned int building_id, BuildingGraphicsState *state);
void building_runtime_backup_graphics_state(void);
void building_runtime_restore_graphics_state(void);
void building_runtime_initialize_city_graphics_cache(void);
void building_runtime_for_each(const std::function<void(Building *)> &visitor);
void building_runtime_for_each(
    const BuildingForEachArgs &args,
    const std::function<void(Building *)> &visitor);
unsigned int building_runtime_debug_known_building_id(const Building *building);
void building_runtime_debug_dump(FILE *file);

class BuildingRuntime {
public:
    Building &create(const building_type_registry_impl::BuildingType &type, int x, int y);
};

BuildingRuntime &city_building_runtime();

class building_runtime {
    friend class Building;

public:
    building_runtime(::building *building_data, const building_type_registry_impl::BuildingType *definition);
    building_runtime(
        ::building *building_data,
        const building_type_registry_impl::BuildingType *definition,
        unsigned int runtime_id,
        int ephemeral);

    void set_building_graphic();
    void assign_graphic_variant(int force_reseed);
    unsigned char graphics_variant() const;
    void set_graphics_variant(int variant);
    BuildingGraphicsState &graphics_state();
    const BuildingGraphicsState &graphics_state() const;
    BuildingGraphicsState graphics_state_snapshot() const;
    void restore_graphics_state(const BuildingGraphicsState &state);
    void spawn_figure();
    int uses_new_graphics() const;
    const RuntimeDrawSlice *graphic_footprint();
    const RuntimeDrawSlice *graphic_top();
    const RuntimeDrawSlice *graphic_animation(int animation_cursor);
    void advance_graphic_animation(int animation_cursor);
    int resolve_graphics_cache();
    const RuntimeDrawSlice *cached_graphic_footprint() const;
    const RuntimeDrawSlice *cached_graphic_top() const;
    const RuntimeDrawSlice *cached_graphic_animation(int animation_cursor);
    int cached_graphics_no_draw() const;
    int cached_graphics_uses_terrain_foundation() const;
    void advance_cached_graphic_animation(int animation_cursor);
    void draw_cached_graphic_layers(
        building_type_registry_impl::GraphicsLayerStage stage,
        int animation_cursor,
        int x,
        int y,
        color_t color,
        float scale);
    int draw_cached_graphic_layer_role(
        const char *role,
        int animation_cursor,
        int x,
        int y,
        color_t color,
        float scale);
    void draw_cached_graphic_layer_animations(int animation_cursor, int x, int y, color_t color, float scale);
    int cached_owns_graphic_animation() const;
    int owns_graphics();
    int owns_graphic_animation();
    int owns_native_storage() const;
    int owns_native_production() const;
    unsigned int runtime_id() const;
    int is_ephemeral() const;
    int reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id = 0);
    int reserve_legacy_storage_loads(resource_type resource, int loads, unsigned int figure_id);
    void release_legacy_storage_reservation(unsigned int figure_id);

    void check_labor_problem();
    void run_labor_phase_if_defined(const map_point &road);
    int spawn_temple_distribution_supplier(const map_point &road);
    int spawn_market_supplier(const map_point &road);
    int spawn_tavern_supplier(const map_point &road);
    int spawn_primary_mess_hall_supplier(const map_point &road);
    int spawn_secondary_mess_hall_supplier(const map_point &road);
    int spawn_temple_destination_priest(const map_point &road);
    int spawn_temple_mars_mess_hall_priest(const map_point &road);
    int spawn_temple_neptune_chariot(const map_point &road);
    int evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const;

    const building_type_registry_impl::BuildingType *definition() const
    {
        return definition_;
    }

private:
    // Per-building-instance cached native graphics bindings.
    // Input: one live building instance plus its shared BuildingType definition.
    // Output: stable references to materialized payload entries for the current resolved target/option.
    struct CachedGraphicsBindings {
        const ImageGroupPayload *base_payload = nullptr;
        const ImageGroupEntry *base_entry = nullptr;
        const ImageGroupPayload *animation_payload = nullptr;
        const ImageGroupEntry *animation_entry = nullptr;
        RuntimeDrawSlice animation_slice;
        struct Layer {
            const ImageGroupPayload *payload = nullptr;
            const ImageGroupEntry *entry = nullptr;
            building_type_registry_impl::GraphicsLayerStage stage = building_type_registry_impl::GraphicsLayerStage::Auto;
            int x_offset = 0;
            int y_offset = 0;
            std::string role;
            int owns_animation = 0;
            RuntimeDrawSlice animation_slice;
        };
        std::vector<Layer> layers;
        int owns_graphics = 0;
        int owns_graphic_animation = 0;
        int no_draw = 0;
        int terrain_foundation = 0;
        int dirty = 1;
        int resolved = 0;
        std::uint64_t signature = 0;
    };

    struct LegacyStorageReservation {
        unsigned int figure_id = 0;
        resource_type resource = RESOURCE_NONE;
        int loads = 0;
    };

    void refresh_runtime_state();
    void clear_cached_graphics_bindings();
    void invalidate_graphics_cache();
    void rebuild_cached_graphics_bindings();
    void ensure_cached_graphics_bindings();
    void rebuild_cached_animation_slice(int animation_cursor);
    std::uint64_t graphics_state_signature() const;
    int building_state_supports_native_graphics() const;
    const building_type_registry_impl::GraphicsTarget *resolve_graphic_target() const;
    int resolve_graphic_binding(
        const building_type_registry_impl::GraphicsTarget &target,
        const ImageGroupPayload *&payload,
        const ImageGroupEntry *&entry) const;
    const Animation *cached_animation() const;
    int worker_percentage() const;
    int default_spawn_delay() const;
    void apply_global_labor_house_coverage(int amount);
    void generate_labor_seeker(int x, int y);
    void run_house_spawn_labor_phase(
        const building_type_registry_impl::LaborSeekerPolicy &policy,
        const map_point &road);
    void run_house_generate_labor_phase(
        const building_type_registry_impl::LaborSeekerPolicy &policy,
        const map_point &road);
    void run_workforce_labor_phase(
        const building_type_registry_impl::LaborSeekerPolicy &policy,
        const map_point &road);
    void run_labor_phase(const building_type_registry_impl::LaborDefinition &labor, const map_point &road);
    int has_figure_of_type(figure_type type);
    int has_figure_of_any(const std::vector<figure_type> &types);
    unsigned int *figure_slot_storage(building_type_registry_impl::FigureSlot slot);
    int slot_has_live_figure(
        building_type_registry_impl::FigureSlot slot,
        figure_type primary_type,
        figure_type secondary_type = FIGURE_NONE);
    int spawn_caravanserai_supplier(const map_point &road);
    int spawn_lighthouse_supplier(const map_point &road);
    int spawn_temple_supplier(const map_point &road);
    int supplier_slot_waits_this_tick(
        building_type_registry_impl::FigureSlot slot,
        figure_type supplier_type,
        figure_type secondary_type = FIGURE_NONE);
    int spawn_supplier_to_destination(
        building_type_registry_impl::FigureSlot slot,
        figure_type supplier_type,
        const map_point &road,
        Building *destination,
        unsigned char collecting_item_id);
    int spawn_grand_temple_mars_recruit(const map_point &road);
    void spawn_architect_guild();
    void spawn_caravanserai();
    void spawn_lighthouse();
    void spawn_watchtower();
    void spawn_armoury();
    void run_native_production_phase(const map_point &road, int run_labor);
    resource_type figure_delivery_output_resource() const;
    void spawn_figure_delivery_cart(const map_point &road);
    int resolve_road_access(building_type_registry_impl::RoadAccessMode mode, map_point *road) const;
    int evaluate_condition(building_type_registry_impl::SpawnCondition condition) const;
    int evaluate_spawn_chance(const building_type_registry_impl::SpawnPolicy &policy);
    LegacyStorageReservation *legacy_storage_reservation_for(unsigned int figure_id);
    int legacy_storage_reservation_is_current(const LegacyStorageReservation &reservation) const;
    void prune_legacy_storage_reservations();
    int should_apply_graphic_for_timing(
        const building_type_registry_impl::SpawnDelayGroup &group,
        building_type_registry_impl::GraphicTiming timing) const;
    unsigned char get_spawn_delay_counter(size_t policy_index) const;
    void set_spawn_delay_counter(size_t policy_index, unsigned char value);
    void assign_figure_slot(building_type_registry_impl::FigureSlot slot, unsigned int figure_id);
    int create_spawned_figure(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road);
    int try_spawn_policy(const building_type_registry_impl::SpawnPolicy &policy, const map_point &road);
    void run_spawn_group(
        const building_type_registry_impl::SpawnDelayGroup &group,
        size_t group_index,
        int run_labor);

    ::building &record()
    {
        return *record_;
    }

    const ::building &record() const
    {
        return *record_;
    }

    const building_type_registry_impl::BuildingType &type() const
    {
        return *definition_;
    }

    ::building *record_ = nullptr;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    unsigned int runtime_id_ = 0;
    int ephemeral_ = 0;
    BuildingGraphicsState graphics_state_;
    std::unique_ptr<RubbleState> rubble_state_;
    Building building_;
    std::vector<unsigned char> spawn_delay_counters_;
    std::vector<LegacyStorageReservation> legacy_storage_reservations_;
    CachedGraphicsBindings graphics_cache_;

public:
    // Transitional public access to the legacy saved struct while behavior is still migrating into methods.
    ::building &data;
    Building &building;
};
