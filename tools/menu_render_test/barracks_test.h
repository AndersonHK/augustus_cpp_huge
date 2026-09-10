#pragma once

#include "building/barracks.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_type_registry.h"
#include "building/figure.h"
#include "building/menu.h"
#include "building/monument.h"
#include "city/monument_gifts.h"
#include "city/data_private.h"
#include "figure/formation.h"
#include "game/tutorial.h"
#include "game/time.h"
#include "scenario/allowed_building.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <map>

// Runs against the loaded test city. The caller reloads it before the soak.
inline bool run_barracks_test()
{
    using namespace building_type_registry_impl;
    const auto type = type_from_attr("barracks");
    const auto *definition = definition_for_type(type);
    if (!definition) return true;
    const int load = resource_units_per_load();
    int capacity = 0;
    for (const auto *storage : definition->storage_types()) if (storage->is_output() && storage->handles_resource(resource_troops())) capacity = storage->capacity();
    if (!definition->has_native_production() || definition->output_resource() != resource_troops() || (capacity != load && capacity != 4 * load)) {
        fprintf(stderr, "Barracks production/storage definition failed.\n");
        return false;
    }
    building_menu_update();
    const bool allowed = tutorial_get_build_buttons() == TUT_BUILD_NORMAL && scenario_allowed_building(definition) && building_monument_has_required_resources_to_build(type) && city_monument_gift_available(type);
    if (allowed && !building_menu_is_enabled(definition)) {
        fprintf(stderr, "Allowed barracks is missing from the construction menu.\n");
        return false;
    }
    Building *recruiter = nullptr;
    Building::for_each([&](Building *b) {
        map_point road;
        if (!recruiter && b->type == definition && b->is_in_use() && b->employment_worker_count() >= b->employment_required_workers() && b->has_road_access(&road)) recruiter = b;
    });
    if (!recruiter) {
        fprintf(stdout, "Barracks definitions/menu passed: troop_capacity=%d loads; no staffed barracks for live test.\n", capacity / load);
        return true;
    }
    const int saved_stress = city_data.mess_hall.food_stress_cumulative;
    city_data.mess_hall.food_stress_cumulative = 20;
    const int unstressed_work = recruiter->native_production_max_progress();
    city_data.mess_hall.food_stress_cumulative = 28;
    const int stressed_work = recruiter->native_production_max_progress();
    city_data.mess_hall.food_stress_cumulative = saved_stress;
    if (stressed_work != unstressed_work * 2) {
        fprintf(stderr, "Declared food-stress modifier did not double training work above its threshold.\n");
        return false;
    }
    auto *record = const_cast<building *>(recruiter->record());
    record->data.industry.progress = 0;
    recruiter->set_resource_amount(resource_troops(), 0);
    const int updates_per_load = (recruiter->native_production_max_progress() + recruiter->employment_worker_count() - 1) / recruiter->employment_worker_count();
    for (int i = 0; i < updates_per_load * (capacity / load + 2); ++i) recruiter->update_native_production(0, nullptr);
    if (recruiter->resource_amount(resource_troops()) != capacity) {
        fprintf(stderr, "Barracks production did not fill and respect its troop buffer.\n");
        return false;
    }
    // Every equipped formation is temporarily starved of its declared inputs.
    // Unarmed formations are held home but unavailable so the test has one cause.
    std::vector<std::pair<formation *, int>> curses;
    std::map<resource_type, int> equipment;
    for (int id = 1; id < formation_count(); ++id) {
        auto *f = formation_get(id);
        if (!f || !f->in_use || !f->is_legion) continue;
        curses.emplace_back(f, f->cursed_by_mars);
        if (f->recruitment_costs().empty()) f->cursed_by_mars = 1;
        for (const auto &cost : f->recruitment_costs()) equipment.emplace(cost.resource, recruiter->resource_amount(cost.resource));
    }
    for (const auto &[resource, amount] : equipment) recruiter->set_resource_amount(resource, 0);
    for (const auto &[resource, amount] : equipment) {
        const bool accepted = recruiter->accepts_good(resource);
        recruiter->set_accepted_good(resource, false);
        const int refused_space = recruiter->input_storage_available_space(resource);
        recruiter->set_accepted_good(resource, true);
        const int accepted_space = recruiter->input_storage_available_space(resource);
        recruiter->set_accepted_good(resource, accepted);
        if (refused_space || accepted_space < load) {
            fprintf(stderr, "Recruitment supply storage ignored acceptance orders.\n");
            return false;
        }
    }
    map_point road;
    recruiter->has_road_access(&road);
    const int without_equipment = Barracks(*recruiter).create_soldier(road.x, road.y);
    for (const auto &[resource, amount] : equipment) recruiter->set_resource_amount(resource, amount);
    for (const auto &[f, curse] : curses) f->cursed_by_mars = curse;
    if (without_equipment || recruiter->resource_amount(resource_troops()) != capacity) {
        fprintf(stderr, "A recruit ignored its declared equipment requirements.\n");
        return false;
    }
    resource_type cargo = RESOURCE_NONE;
    int loads = 0;
    if (recruiter->reserve_output_storage_loads(&cargo, &loads) || recruiter->resource_amount(resource_troops()) != capacity) {
        fprintf(stderr, "An ordinary goods cart took stored troops.\n");
        return false;
    }
    buffer saved{};
    building_resource_state_save(&saved);
    recruiter->set_resource_amount(resource_troops(), 0);
    building_resource_state_load(&saved);
    std::free(saved.data);
    if (recruiter->resource_amount(resource_troops()) != capacity) {
        fprintf(stderr, "Stored troops did not survive the keyed save bridge.\n");
        return false;
    }
    auto soldiers = []() {
        int count = 0;
        for (int id = 1; id < formation_count(); ++id) {
            const auto *f = formation_get(id);
            if (f && f->in_use && f->is_legion) count += f->num_figures;
        }
        return count;
    };
    int before = soldiers();
    recruiter->set_resource_amount(resource_troops(), 0);
    building_figure_generate();
    if (soldiers() != before) {
        fprintf(stderr, "Barracks spawned a recruit without a troop load.\n");
        return false;
    }
    recruiter->set_resource_amount(resource_troops(), capacity);
    bool eligible = false;
    for (int id = 1; id < formation_count(); ++id) {
        const auto *f = formation_get(id);
        if (f && f->in_use && f->is_legion && f->can_receive_recruit() && Barracks(*recruiter).has_recruitment_resources(*f)) eligible = true;
    }
    if (eligible) {
        Barracks(*recruiter).set_priority(PRIORITY_FORT);
        building_figure_generate();
        if (soldiers() != before + 1 || recruiter->resource_amount(resource_troops()) != capacity - load) {
            fprintf(stderr, "Barracks spawn policy did not turn exactly one troop load into a recruit.\n");
            return false;
        }
    }
    // Keep the grand temple's independent religion policy live through the
    // shared input-storage and soldier-cost refactor. This fixture is discarded
    // by the caller's reload and never published onto the city's terrain.
    const auto *mars = definition_for_type(type_from_attr("grand_temple_mars"));
    if (mars && eligible) {
        Building &temple = city_building_runtime().create(*mars, recruiter->x(), recruiter->y());
        auto *temple_record = const_cast<building *>(temple.record());
        temple_record->state = BUILDING_STATE_IN_USE;
        temple_record->monument.phase = MONUMENT_FINISHED;
        temple_record->num_workers = static_cast<unsigned char>(temple.employment_required_workers());
        temple_record->houses_covered = 100;
        for (const auto &cost : equipment) temple.set_resource_amount(cost.first, 4 * load);
        Barracks(temple).set_priority(PRIORITY_FORT);
        const int temple_before = soldiers();
        const int attempts = game_time_scale_legacy_day_ticks(8 + std::max(0, saved_stress - 20)) + 1;
        for (int i = 0; i < attempts && soldiers() == temple_before; ++i) temple.spawn_figure();
        if (soldiers() != temple_before + 1) {
            fprintf(stderr, "Mars Grand Temple did not recruit through its existing policy with declared supplies.\n");
            return false;
        }
        fprintf(stdout, "Mars Grand Temple policy passed: soldiers=%d->%d; shared declared recruitment supplies.\n", temple_before, soldiers());
    }
    fprintf(stdout, "Barracks troop buffer passed: capacity=%d loads updates_per_load=%d soldiers=%d->%d menu_enabled=%d; bounded production, food stress, equipment, cart exclusion, and persistence passed.\n", capacity / load, updates_per_load, before, soldiers(), building_menu_is_enabled(definition));
    return true;
}
