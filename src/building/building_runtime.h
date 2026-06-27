#pragma once

#include "assets/image_group_entry.h"
#include "building/building_fwd.h"
#include "building/building_type.h"
#include "figure/figure.h"
#include "game/resource.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class ImageGroupPayload;

void building_runtime_reset(void);
void building_runtime_initialize_city_graphics_cache(void);

class building_runtime {
public:
    building_runtime(::building *building, const building_type_registry_impl::BuildingType *definition);
    explicit building_runtime(const Building &building);

    // Transitional public access to the legacy saved struct while behavior is still migrating into methods.
    ::building &data;

    void set_building_graphic();
    void assign_graphic_variant(int force_reseed);
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
    void advance_cached_graphic_animation(int animation_cursor);
    void draw_cached_graphic_layers(
        building_type_registry_impl::GraphicsLayerStage stage,
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
    int reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id = 0);
    int reserve_legacy_storage_loads(resource_type resource, int loads, unsigned int figure_id);
    void release_legacy_storage_reservation(unsigned int figure_id);

    Building building() const;

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
            int owns_animation = 0;
            RuntimeDrawSlice animation_slice;
        };
        std::vector<Layer> layers;
        int owns_graphics = 0;
        int owns_graphic_animation = 0;
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
    void check_labor_problem();
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
    void run_labor_phase_if_defined(const map_point &road);
    int has_figure_of_type(figure_type type);
    int has_figure_of_any(const std::vector<figure_type> &types);
    unsigned int *figure_slot_storage(building_type_registry_impl::FigureSlot slot);
    unsigned int find_live_owned_figure(figure_type primary_type, figure_type secondary_type = FIGURE_NONE) const;
    int slot_has_live_figure(
        building_type_registry_impl::FigureSlot slot,
        figure_type primary_type,
        figure_type secondary_type = FIGURE_NONE);
    int spawn_caravanserai_supplier(const map_point &road);
    int spawn_lighthouse_supplier(const map_point &road);
    int spawn_temple_supplier(const map_point &road);
    int spawn_temple_destination_priest(const map_point &road);
    int spawn_temple_mars_mess_hall_priest(const map_point &road);
    int spawn_temple_neptune_chariot(const map_point &road);
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
    int evaluate_delay(const std::vector<building_type_registry_impl::DelayBand> &delay_bands) const;
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
    std::vector<unsigned char> spawn_delay_counters_;
    std::vector<LegacyStorageReservation> legacy_storage_reservations_;
    CachedGraphicsBindings graphics_cache_;
};
