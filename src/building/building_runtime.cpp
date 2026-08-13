#include "building/count.h"
#include "building/distribution.h"
#include "building/image.h"
#include "building/industry.h"
#include "figure/action.h"
#include "map/building.h"
#include "map/building_tiles.h"
#include "map/road_access.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/HousingStateBridge.h"
#include "building/FoundationStateSaveBridge.h"
#include "building/water_access_runtime.h"

#include "assets/image_group_payload.h"
#include "building/armoury.h"
#include "building/barracks.h"
#include "building/building_runtime_graphics.h"
#include "building/caravanserai.h"
#include "building/lighthouse.h"
#include "building/production_runtime.h"
#include "building/storage_runtime.h"
#include "building/temple.h"
#include "city/culture.h"
#include "core/crash_context.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "figure/formation.h"

#include "building/granary.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "city/buildings.h"
#include "city/data_private.h"
#include "city/population.h"
#include "core/calc.h"
#include "core/config.h"
#include "figure/movement.h"
#include "game/Animation.h"
#include "game/resource.h"
#include "game/time.h"
#include "map/sprite.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "core/log.h"

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <exception>
#include <string>
#include <vector>

namespace {
void invalidate_runtime_building_index();
}

static const RubbleDef *runtime_rubble_definition_for_record(
    const ::building *building_data,
    const building_type_registry_impl::BuildingType *definition)
{
    if (definition && definition->has_rubble()) {
        return &definition->rubble();
    }
    if (building_data && building_data->state == BUILDING_STATE_RUBBLE) {
        const building_type rubble_type = building_type_registry_impl::type_from_attr("rubble");
        const building_type_registry_impl::BuildingType *rubble_definition =
            building_type_registry_impl::definition_for_type(rubble_type);
        return rubble_definition && rubble_definition->has_rubble() ? &rubble_definition->rubble() : nullptr;
    }
    return nullptr;
}

building_runtime::building_runtime(::building *building_data, const building_type_registry_impl::BuildingType *definition)
    : building_runtime(building_data, definition, building_data ? building_data->id : 0, 0)
{
}

building_runtime::building_runtime(
    ::building *building_data,
    const building_type_registry_impl::BuildingType *definition,
    unsigned int runtime_id,
    int ephemeral)
    : record_(building_data)
    , definition_(definition)
    , runtime_id_(runtime_id)
    , ephemeral_(ephemeral)
    , graphics_state_()
    , rubble_state_(runtime_rubble_definition_for_record(building_data, definition) ? std::make_unique<RubbleState>() : nullptr)
    , foundation_state_()
    , housing_state_()
    , building_(
        building_data,
        definition,
        &graphics_state_,
        runtime_rubble_definition_for_record(building_data, definition),
        rubble_state_.get())
    , data(*building_data)
    , building(building_)
{
    bind_native_modules();
}

void building_runtime::bind_native_modules()
{
    foundation_module_.reset();
    housing_module_.reset();
    building_.Foundation = nullptr;
    building_.Housing = nullptr;
    building_.Formation = nullptr;

    if (definition_ && definition_->foundation_def()) {
        foundation_module_ = std::make_unique<building_type_registry_impl::BuildingFoundation>(
            building_, *definition_->foundation_def(), foundation_state_);
        building_.Foundation = foundation_module_.get();
    }
    if (definition_ && definition_->has_housing()) {
        housing_module_ = std::make_unique<HousingModule>(building_, &definition_->housing_def(), housing_state_);
        building_.Housing = housing_module_.get();
    }
    if (definition_ && definition_->has_composition()) {
        composition_module_.bind_owner(&building_, &definition_->composition());
    } else {
        composition_module_.bind_standalone(&building_);
    }
    building_.Composition = &composition_module_;

    if (!ephemeral_ && definition_ && definition_->has_military() && record_ && record_->formation_id > 0) {
        const char *failure = nullptr;
        if (!formation_bind_runtime_fort(building_, &failure)) {
            log_error("Unable to bind fort runtime to saved formation", failure, static_cast<int>(record_->id));
        }
    }
}

void building_runtime::rebind_definition(const building_type_registry_impl::BuildingType *definition)
{
    definition_ = definition;
    building_.type = definition;
    const RubbleDef *rubble_definition = runtime_rubble_definition_for_record(record_, definition_);
    if (rubble_definition && rubble_definition->has_any()) {
        if (!rubble_state_) {
            rubble_state_ = std::make_unique<RubbleState>();
        }
    } else {
        rubble_state_.reset();
    }
    building_.bind_graphics(&graphics_state_);
    building_.bind_rubble(rubble_definition, rubble_state_.get());
    bind_native_modules();
    invalidate_graphics_cache();
    invalidate_runtime_building_index();
}

unsigned int building_runtime::runtime_id() const
{
    return runtime_id_;
}

int building_runtime::is_ephemeral() const
{
    return ephemeral_;
}

BuildingGraphicsState &building_runtime::graphics_state()
{
    return graphics_state_;
}

const BuildingGraphicsState &building_runtime::graphics_state() const
{
    return graphics_state_;
}

BuildingGraphicsState building_runtime::graphics_state_snapshot() const
{
    return graphics_state_;
}

void building_runtime::restore_graphics_state(const BuildingGraphicsState &state)
{
    graphics_state_ = state;
    invalidate_graphics_cache();
}

[[noreturn]] static void report_building_runtime_create_failure(
    const building_type_registry_impl::BuildingType &type,
    const ::building *record)
{
    char detail[256];
    snprintf(detail, sizeof(detail), "type=%s record_id=%u",
        type.attr(),
        record ? record->id : 0);
    log_error("BuildingRuntime failed to create a runtime-owned building", detail, 0);
    std::terminate();
}

BuildingRuntime &city_building_runtime()
{
    static BuildingRuntime runtime;
    return runtime;
}

Building &BuildingRuntime::create(const building_type_registry_impl::BuildingType &type, int x, int y)
{
    ::building *record = building_create(type.type(), x, y);
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    if (!runtime || !runtime->building.id || runtime->definition() != &type) {
        report_building_runtime_create_failure(type, record);
    }
    return runtime->building;
}

namespace building_runtime_impl {

std::vector<std::unique_ptr<building_runtime>> g_runtime_instances;
static ScopedEphemeralBuildingRuntime *g_ephemeral_runtime_context = nullptr;

constexpr BuildingRuntimeList INDEXED_BUILDING_LISTS[] = {
    BuildingRuntimeList::Housing,
    BuildingRuntimeList::Labor,
    BuildingRuntimeList::Production,
    BuildingRuntimeList::Granaries,
    BuildingRuntimeList::Warehouses,
    BuildingRuntimeList::Storage,
    BuildingRuntimeList::PlagueTargets,
};

struct RuntimeBuildingIndex {
    std::vector<Building *> housing;
    std::vector<Building *> labor;
    std::vector<Building *> production;
    std::vector<Building *> granaries;
    std::vector<Building *> warehouses;
    std::vector<Building *> storage;
    std::vector<Building *> plague_targets;
    uint64_t revision = 1;
    uint64_t built_revision = 0;

    static bool contains(
        BuildingRuntimeList list,
        const building_type_registry_impl::BuildingType *definition)
    {
        if (!definition) {
            return false;
        }
        switch (list) {
            case BuildingRuntimeList::Housing: return definition->has_housing();
            case BuildingRuntimeList::Labor: return definition->has_labor();
            case BuildingRuntimeList::Production: return definition->has_native_production();
            case BuildingRuntimeList::Granaries: return definition->is_granary();
            case BuildingRuntimeList::Warehouses: return definition->is_warehouse();
            case BuildingRuntimeList::Storage: return definition->is_storage();
            case BuildingRuntimeList::PlagueTargets: return definition->is_plague_treatment_target();
        }
        std::terminate();
    }

    std::vector<Building *> &entries(BuildingRuntimeList list)
    {
        switch (list) {
            case BuildingRuntimeList::Housing: return housing;
            case BuildingRuntimeList::Labor: return labor;
            case BuildingRuntimeList::Production: return production;
            case BuildingRuntimeList::Granaries: return granaries;
            case BuildingRuntimeList::Warehouses: return warehouses;
            case BuildingRuntimeList::Storage: return storage;
            case BuildingRuntimeList::PlagueTargets: return plague_targets;
        }
        std::terminate();
    }

    void invalidate()
    {
        ++revision;
    }

    void clear()
    {
        for (BuildingRuntimeList list : INDEXED_BUILDING_LISTS) {
            entries(list).clear();
        }
        revision = 1;
        built_revision = 0;
    }

    void ensure_current()
    {
        if (built_revision == revision) {
            return;
        }
        for (BuildingRuntimeList list : INDEXED_BUILDING_LISTS) {
            entries(list).clear();
        }
        for (const std::unique_ptr<building_runtime> &runtime : g_runtime_instances) {
            if (!runtime) {
                continue;
            }
            Building *building = &runtime->building;
            const building_type_registry_impl::BuildingType *definition = building->type;
            for (BuildingRuntimeList list : INDEXED_BUILDING_LISTS) {
                if (contains(list, definition)) {
                    entries(list).push_back(building);
                }
            }
        }
        const auto type_then_id = [](const Building *left, const Building *right) {
            const building_type left_type = left && left->type ? left->type->type() : BUILDING_NONE;
            const building_type right_type = right && right->type ? right->type->type() : BUILDING_NONE;
            return left_type != right_type ? left_type < right_type : left->id < right->id;
        };
        std::sort(granaries.begin(), granaries.end(), type_then_id);
        std::sort(warehouses.begin(), warehouses.end(), type_then_id);
        std::sort(storage.begin(), storage.end(), type_then_id);
        std::sort(plague_targets.begin(), plague_targets.end(), [](const Building *left, const Building *right) {
            return left->id < right->id;
        });
        built_revision = revision;
    }
};

static RuntimeBuildingIndex g_runtime_building_index;

ScopedEphemeralBuildingRuntime::ScopedEphemeralBuildingRuntime(
    const std::vector<EphemeralBuildingRuntimeBinding> &bindings)
    : previous_(g_ephemeral_runtime_context)
{
    for (const EphemeralBuildingRuntimeBinding &binding : bindings) {
        if (!binding.record || !binding.definition || !binding.runtime_id) {
            continue;
        }

        binding.record->id = binding.runtime_id;
        std::unique_ptr<building_runtime> runtime =
            std::make_unique<building_runtime>(binding.record, binding.definition, binding.runtime_id, 1);
        runtime->restore_graphics_state(binding.graphics_state);

        building_runtime *runtime_ptr = runtime.get();
        runtime_id_by_record_[binding.record] = binding.runtime_id;
        runtime_by_id_[binding.runtime_id] = runtime_ptr;
        main_id_by_runtime_id_[binding.runtime_id] =
            binding.main_runtime_id ? binding.main_runtime_id : binding.runtime_id;
        runtimes_.push_back(std::move(runtime));
    }

    // Placement ghosts use the same object-owned composition graph as live
    // buildings. main_runtime_id is scoped preview metadata, not a building
    // record chain.
    for (const std::unique_ptr<building_runtime> &owner_runtime : runtimes_) {
        BuildingComposition *composition = owner_runtime ? owner_runtime->building.Composition : nullptr;
        if (!composition || !composition->is_owner()) {
            continue;
        }
        std::vector<BuildingComposition *> children;
        for (const std::unique_ptr<building_runtime> &candidate : runtimes_) {
            if (!candidate || candidate.get() == owner_runtime.get()) {
                continue;
            }
            const auto main = main_id_by_runtime_id_.find(candidate->runtime_id());
            if (main != main_id_by_runtime_id_.end() && main->second == owner_runtime->runtime_id()) {
                children.push_back(candidate->building.Composition);
            }
        }
        std::string error;
        if (!composition->attach_children(children, &error)) {
            log_error("Unable to bind ephemeral building composition", error.c_str(), owner_runtime->runtime_id());
        }
    }

    g_ephemeral_runtime_context = this;
}

ScopedEphemeralBuildingRuntime::~ScopedEphemeralBuildingRuntime()
{
    g_ephemeral_runtime_context = previous_;
}

building_runtime *ScopedEphemeralBuildingRuntime::runtime_for_record(::building *record) const
{
    if (record) {
        const auto id_it = runtime_id_by_record_.find(record);
        if (id_it != runtime_id_by_record_.end()) {
            return runtime_for_id(id_it->second);
        }
    }
    return previous_ ? previous_->runtime_for_record(record) : nullptr;
}

building_runtime *ScopedEphemeralBuildingRuntime::runtime_for_id(unsigned int runtime_id) const
{
    if (runtime_id) {
        const auto runtime_it = runtime_by_id_.find(runtime_id);
        if (runtime_it != runtime_by_id_.end()) {
            return runtime_it->second;
        }
    }
    return previous_ ? previous_->runtime_for_id(runtime_id) : nullptr;
}

building_runtime *ScopedEphemeralBuildingRuntime::main_runtime_for_record(::building *record) const
{
    if (!record) {
        return nullptr;
    }

    const auto id_it = runtime_id_by_record_.find(record);
    if (id_it == runtime_id_by_record_.end()) {
        return previous_ ? previous_->main_runtime_for_record(record) : nullptr;
    }

    building_runtime *runtime = runtime_for_id(id_it->second);
    if (!runtime) {
        return nullptr;
    }

    const auto main_it = main_id_by_runtime_id_.find(id_it->second);
    if (main_it == main_id_by_runtime_id_.end()) {
        return runtime;
    }
    building_runtime *main_runtime = runtime_for_id(main_it->second);
    return main_runtime ? main_runtime : runtime;
}

building_runtime *ScopedEphemeralBuildingRuntime::next_runtime_for_record(::building *record) const
{
    // Ephemeral record chains are authored only by the dynamic-bridge ghost.
    // Native composition previews attach BuildingComposition directly.
    if (!record) {
        return nullptr;
    }

    const auto id_it = runtime_id_by_record_.find(record);
    if (id_it == runtime_id_by_record_.end()) {
        return previous_ ? previous_->next_runtime_for_record(record) : nullptr;
    }

    if (record->next_part_building_id <= 0) {
        return nullptr;
    }
    building_runtime *runtime = runtime_for_id(static_cast<unsigned int>(record->next_part_building_id));
    if (runtime) {
        return runtime;
    }
    return previous_ ? previous_->next_runtime_for_record(record) : nullptr;
}

struct GraphicsStateBackup {
    int valid = 0;
    BuildingGraphicsState state;
};

struct LoadedBuildingRuntimeState {
    int valid = 0;
    int graphics_state_valid = 0;
    BuildingGraphicsState graphics_state;
    int rubble_state_valid = 0;
    RubbleState rubble_state;
    int housing_state_valid = 0;
    HousingState housing_state;
    int foundation_state_valid = 0;
    building_type_registry_impl::FoundationTerrainSaveState foundation_state;
};

std::vector<GraphicsStateBackup> g_graphics_state_backup;
std::vector<LoadedBuildingRuntimeState> g_loaded_building_runtime_state;

void reset_live_runtime_modules()
{
    g_runtime_building_index.clear();
    g_runtime_instances.clear();
    g_graphics_state_backup.clear();
    production_runtime_impl::reset();
    storage_runtime_impl::reset();
}

const LoadedBuildingRuntimeState *loaded_runtime_state_for(unsigned int building_id)
{
    return building_id < g_loaded_building_runtime_state.size() && g_loaded_building_runtime_state[building_id].valid ?
        &g_loaded_building_runtime_state[building_id] :
        nullptr;
}

void clear_loaded_runtime_state()
{
    g_loaded_building_runtime_state.clear();
}

static int building_has_required_workers_for_runtime_water(const Building &building)
{
    if (!building.type) {
        return 0;
    }
    const int required_workers = building.employment_required_workers();
    return required_workers <= 0 || building.employment_worker_count() > 0;
}

building_runtime *get_city_building(::building *building_data)
{
    return get_or_create_instance(building_data);
}

building_runtime *get_ephemeral_instance(::building *building_data)
{
    return g_ephemeral_runtime_context ?
        g_ephemeral_runtime_context->runtime_for_record(building_data) :
        nullptr;
}

building_runtime *get_ephemeral_instance(unsigned int runtime_id)
{
    return g_ephemeral_runtime_context ?
        g_ephemeral_runtime_context->runtime_for_id(runtime_id) :
        nullptr;
}

building_runtime *get_ephemeral_main_instance(::building *building_data)
{
    return g_ephemeral_runtime_context ?
        g_ephemeral_runtime_context->main_runtime_for_record(building_data) :
        nullptr;
}

building_runtime *get_ephemeral_next_instance(::building *building_data)
{
    return g_ephemeral_runtime_context ?
        g_ephemeral_runtime_context->next_runtime_for_record(building_data) :
        nullptr;
}

building_runtime *get_or_create_instance(::building *building_data)
{
    if (building_runtime *runtime = get_ephemeral_instance(building_data)) {
        return runtime;
    }
    if (!building_data || !building_data->id) {
        return nullptr;
    }

    // For now runtime wrappers are materialized lazily, but they are owned here rather than by the type registry.
    if (g_runtime_instances.size() <= building_data->id) {
        g_runtime_instances.resize(building_data->id + 1);
    }

    // Every live building gets a runtime object, even before that type has migrated to XML-driven behavior.
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(building_data->type);

    const RubbleDef *rubble_definition = runtime_rubble_definition_for_record(building_data, definition);
    std::unique_ptr<building_runtime> &slot = g_runtime_instances[building_data->id];
    const int has_expected_rubble_module =
        (!rubble_definition && (!slot || !slot->building.Rubble)) ||
        (rubble_definition && slot && slot->building.Rubble &&
            slot->building.Rubble->definition() == rubble_definition);
    if (!slot) {
        slot = std::make_unique<building_runtime>(building_data, definition);
        g_runtime_building_index.invalidate();
    } else if (&slot->data != building_data) {
        // Runtime indexes and owner modules publish stable pointers. Loading a
        // different record array must reset the runtime first, never replace a
        // live wrapper behind those pointers.
        log_error("Building runtime id is already bound to a different record", 0, building_data->id);
        std::terminate();
    } else if (slot->definition() != definition || !has_expected_rubble_module) {
        slot->rebind_definition(definition);
    }
    return slot.get();
}

}

namespace {
void invalidate_runtime_building_index()
{
    building_runtime_impl::g_runtime_building_index.invalidate();
}
}

void building_runtime_for_each(const std::function<void(Building *)> &visitor)
{
    if (!visitor) {
        return;
    }
    const size_t initial_count = building_runtime_impl::g_runtime_instances.size();
    for (size_t id = 1; id < initial_count; ++id) {
        building_runtime *instance = building_runtime_impl::g_runtime_instances[id].get();
        if (instance && instance->building.id &&
            instance->building.state_id() != BUILDING_STATE_UNUSED) {
            visitor(&instance->building);
        }
    }
}

void building_runtime_for_each(
    BuildingRuntimeList list,
    const std::function<void(Building *)> &visitor)
{
    if (!visitor) {
        return;
    }

    const auto visit_if_matching = [&](Building *building) {
        if (!building->id) {
            return;
        }
        if (building->state_id() == BUILDING_STATE_UNUSED) {
            return;
        }

        if (!building_runtime_impl::RuntimeBuildingIndex::contains(list, building->type)) {
            return;
        }

        visitor(building);
    };

    building_runtime_impl::g_runtime_building_index.ensure_current();
    std::vector<Building *> &indexed = building_runtime_impl::g_runtime_building_index.entries(list);
    const size_t initial_count = indexed.size();
    for (size_t i = 0; i < initial_count; ++i) {
        visit_if_matching(indexed[i]);
    }
}

static void write_debug_json_string(FILE *file, const char *text)
{
    fputc('"', file);
    if (text) {
        for (const char *cursor = text; *cursor; ++cursor) {
            switch (*cursor) {
                case '\\':
                    fputs("\\\\", file);
                    break;
                case '"':
                    fputs("\\\"", file);
                    break;
                case '\n':
                    fputs("\\n", file);
                    break;
                case '\r':
                    fputs("\\r", file);
                    break;
                case '\t':
                    fputs("\\t", file);
                    break;
                default:
                    fputc(*cursor, file);
                    break;
            }
        }
    }
    fputc('"', file);
}

static void write_debug_pointer(FILE *file, const void *pointer)
{
    fputc('"', file);
    fprintf(file, "%p", pointer);
    fputc('"', file);
}

unsigned int building_runtime_debug_known_building_id(const Building *building)
{
    if (!building) {
        return 0;
    }

    for (const std::unique_ptr<building_runtime> &instance : building_runtime_impl::g_runtime_instances) {
        if (instance && &instance->building == building) {
            return instance->building.id;
        }
    }
    return 0;
}

void building_runtime_debug_dump(FILE *file)
{
    if (!file) {
        return;
    }

    fprintf(file, "  \"buildings\": [\n");
    int wrote = 0;
    for (size_t slot = 0; slot < building_runtime_impl::g_runtime_instances.size(); ++slot) {
        const std::unique_ptr<building_runtime> &instance = building_runtime_impl::g_runtime_instances[slot];
        if (!instance) {
            continue;
        }

        Building &building = instance->building;
        const ::building *record = building.record();
        const building_type_registry_impl::BuildingType *definition = instance->definition();
        const RubbleState *rubble_state = building.Rubble ? building.Rubble->state() : nullptr;
        const int foundation_rotation = building.Foundation && building.Foundation->state().is_published()
            ? building.Foundation->state().rotation()
            : building.orientation();
        const int foundation_width = building.Foundation
            ? building.Foundation->width(foundation_rotation)
            : 0;
        const int foundation_height = building.Foundation
            ? building.Foundation->height(foundation_rotation)
            : 0;
        const std::size_t foundation_cell_count = building.Foundation
            ? building.Foundation->cells(foundation_rotation).size()
            : 0;

        if (wrote++) {
            fprintf(file, ",\n");
        }

        fprintf(file, "    {\n");
        fprintf(file, "      \"slot\": %zu,\n", slot);
        fprintf(file, "      \"runtime_id\": %u,\n", instance->runtime_id());
        fprintf(file, "      \"ephemeral\": %d,\n", instance->is_ephemeral());
        fprintf(file, "      \"building_pointer\": ");
        write_debug_pointer(file, &building);
        fprintf(file, ",\n");
        fprintf(file, "      \"record_pointer\": ");
        write_debug_pointer(file, record);
        fprintf(file, ",\n");
        fprintf(file, "      \"building_id\": %u,\n", static_cast<unsigned int>(building.id));
        fprintf(file, "      \"record_id\": %u,\n", record ? record->id : 0);
        fprintf(file, "      \"type_id\": %d,\n", record ? static_cast<int>(record->type) : 0);
        fprintf(file, "      \"type_attr\": ");
        write_debug_json_string(file, definition ? definition->attr() : nullptr);
        fprintf(file, ",\n");
        fprintf(file, "      \"state\": %d,\n", record ? record->state : 0);
        fprintf(file, "      \"is_deleted\": %d,\n", record ? record->is_deleted : 0);
        fprintf(file, "      \"x\": %d,\n", record ? record->x : 0);
        fprintf(file, "      \"y\": %d,\n", record ? record->y : 0);
        fprintf(file, "      \"grid_offset\": %d,\n", record ? record->grid_offset : 0);
        fprintf(file, "      \"foundation_width\": %d,\n", foundation_width);
        fprintf(file, "      \"foundation_height\": %d,\n", foundation_height);
        fprintf(file, "      \"foundation_cells\": %zu,\n", foundation_cell_count);
        fprintf(file, "      \"prev_part_id\": %d,\n", record ? record->prev_part_building_id : 0);
        fprintf(file, "      \"next_part_id\": %d,\n", record ? record->next_part_building_id : 0);
        fprintf(file, "      \"road_network_id\": %d,\n", record ? record->road_network_id : 0);
        fprintf(file, "      \"distance_from_entry\": %d,\n", record ? record->distance_from_entry : 0);
        fprintf(file, "      \"has_road_access\": %d,\n", record ? record->has_road_access : 0);
        fprintf(file, "      \"road_access_x\": %d,\n", record ? record->road_access_x : 0);
        fprintf(file, "      \"road_access_y\": %d,\n", record ? record->road_access_y : 0);
        fprintf(file, "      \"unknown_value\": %d,\n", record ? record->unknown_value : 0);
        fprintf(file, "      \"graphics_variant\": %u,\n", instance->graphics_variant());
        fprintf(file, "      \"formation_id\": %d,\n", record ? record->formation_id : 0);
        fprintf(file, "      \"figure_id\": %u,\n", record ? record->figure_id : 0);
        fprintf(file, "      \"figure_id2\": %u,\n", record ? record->figure_id2 : 0);
        fprintf(file, "      \"immigrant_figure_id\": %u,\n",
            building.Housing ? building.Housing->state().immigrant_figure_id : 0);
        fprintf(file, "      \"figure_id4\": %u,\n", record ? record->figure_id4 : 0);
        fprintf(file, "      \"house\": {\n");
        fprintf(file, "        \"has_module\": %d,\n", building.Housing ? 1 : 0);
        fprintf(file, "        \"population\": %d,\n",
            building.Housing ? building.Housing->state().population : 0);
        fprintf(file, "        \"population_room\": %d,\n",
            building.Housing ? building.Housing->state().population_room : 0);
        fprintf(file, "        \"unreachable_ticks\": %d,\n",
            building.Housing ? building.Housing->state().unreachable_ticks : 0);
        fprintf(file, "        \"local_workforce_assigned\": %d,\n", record ? record->local_workforce_assigned : 0);
        fprintf(file, "        \"local_workforce_unemployed\": %d\n", record ? record->local_workforce_unemployed : 0);
        fprintf(file, "      },\n");
        fprintf(file, "      \"rubble\": {\n");
        fprintf(file, "        \"has_module\": %d,\n", building.Rubble ? 1 : 0);
        fprintf(file, "        \"is_rubble\": %d,\n", building.Rubble ? building.Rubble->is_rubble() : 0);
        fprintf(file, "        \"is_burning\": %d,\n", building.Rubble ? building.Rubble->is_burning() : 0);
        fprintf(file, "        \"original_grid_offset\": %u,\n", rubble_state ? rubble_state->original_grid_offset : 0);
        fprintf(file, "        \"original_orientation\": %u,\n", rubble_state ? rubble_state->original_orientation : 0);
        fprintf(file, "        \"original_type_attr\": ");
        write_debug_json_string(
            file,
            rubble_state && rubble_state->original_type ? rubble_state->original_type->attr() : nullptr);
        fprintf(file, "\n");
        fprintf(file, "      }\n");
        fprintf(file, "    }");
    }
    fprintf(file, "\n  ]");
}

void building_runtime::refresh_runtime_state()
{
    if (!record_ || !definition()) {
        return;
    }

    int graphics_state_changed = 0;
    if (type().water_access().has_requirements()) {
        const unsigned char has_water_access = static_cast<unsigned char>(
            building_runtime_impl::building_has_required_workers_for_runtime_water(building) &&
            water_access_runtime_building_has_required_access(&building) ? 1 : 0);
        if (record().has_water_access != has_water_access) {
            record().has_water_access = has_water_access;
            graphics_state_changed = 1;
        }
    }

    if (type().has_graphic()) {
        const unsigned char upgrade_level = static_cast<unsigned char>(type().upgrade_level_for(building));
        if (record().upgrade_level != upgrade_level) {
            city_culture_remove_building_module_capacity(&record());
            record().upgrade_level = upgrade_level;
            city_culture_add_building_module_capacity(&record());
            graphics_state_changed = 1;
        }
    }
    if (graphics_state_changed) {
        invalidate_graphics_cache();
    }
}

int building_runtime::owns_native_storage() const
{
    return definition() && type().has_native_storage();
}

int building_runtime::owns_native_production() const
{
    return definition() && type().has_native_production();
}

unsigned char building_runtime::graphics_variant() const
{
    return graphics_state_.variant();
}

void building_runtime::set_graphics_variant(int variant)
{
    graphics_state_.set_variant(variant);
}

building_runtime::LegacyStorageReservation *building_runtime::legacy_storage_reservation_for(const Figure &figure)
{
    for (LegacyStorageReservation &reservation : legacy_storage_reservations_) {
        if (reservation.figure == &figure) {
            return &reservation;
        }
    }
    return nullptr;
}

int building_runtime::reserved_legacy_storage_loads(resource_type resource, unsigned int ignore_figure_id)
{
    int total = 0;
    for (const LegacyStorageReservation &reservation : legacy_storage_reservations_) {
        if (reservation.figure && reservation.figure->id() != ignore_figure_id &&
            (resource == RESOURCE_NONE || reservation.resource == resource)) {
            total += reservation.loads;
        }
    }
    return total;
}

int building_runtime::reserve_legacy_storage_loads(resource_type resource, int loads, Figure &figure)
{
    if (!record_ || !figure.id() || resource == RESOURCE_NONE || loads <= 0) {
        return 0;
    }

    if (LegacyStorageReservation *existing = legacy_storage_reservation_for(figure)) {
        existing->resource = resource;
        existing->loads = loads;
        return 1;
    }

    legacy_storage_reservations_.push_back({ &figure, resource, loads });
    return 1;
}

void building_runtime::release_legacy_storage_reservation(const Figure &figure)
{
    legacy_storage_reservations_.erase(
        std::remove_if(legacy_storage_reservations_.begin(), legacy_storage_reservations_.end(),
            [&figure](const LegacyStorageReservation &reservation) {
                return reservation.figure == &figure;
            }),
        legacy_storage_reservations_.end());
}

void building_runtime::release_all_legacy_storage_reservations()
{
    legacy_storage_reservations_.clear();
}

void building_runtime_reset(void)
{
    building_runtime_impl::reset_live_runtime_modules();
    building_runtime_impl::clear_loaded_runtime_state();
}

void building_runtime_begin_load_bridge(int building_count)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    g_loaded_building_runtime_state.clear();
    g_loaded_building_runtime_state.resize(building_count > 0 ? static_cast<size_t>(building_count) : 0);
}

void building_runtime_stage_loaded_graphics_state(
    unsigned int building_id,
    const BuildingGraphicsState &state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].graphics_state_valid = 1;
    g_loaded_building_runtime_state[building_id].graphics_state = state;
}

void building_runtime_stage_loaded_rubble_state(unsigned int building_id, const RubbleState &state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;

    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].rubble_state_valid = 1;
    g_loaded_building_runtime_state[building_id].rubble_state = state;
}

void building_runtime_stage_loaded_housing_state(unsigned int building_id, const HousingState &state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;
    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].housing_state_valid = 1;
    g_loaded_building_runtime_state[building_id].housing_state = state;
}

void building_runtime_stage_loaded_foundation_state(
    unsigned int building_id,
    const building_type_registry_impl::FoundationTerrainSaveState &state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;
    if (!building_id) {
        return;
    }
    if (g_loaded_building_runtime_state.size() <= building_id) {
        g_loaded_building_runtime_state.resize(static_cast<size_t>(building_id) + 1);
    }
    g_loaded_building_runtime_state[building_id].valid = 1;
    g_loaded_building_runtime_state[building_id].foundation_state_valid = 1;
    g_loaded_building_runtime_state[building_id].foundation_state = state;
}

int building_runtime_loaded_foundation_state(
    unsigned int building_id,
    building_type_registry_impl::FoundationTerrainSaveState *state)
{
    using building_runtime_impl::g_loaded_building_runtime_state;
    if (!state || !building_id || building_id >= g_loaded_building_runtime_state.size()) {
        return 0;
    }
    const building_runtime_impl::LoadedBuildingRuntimeState &loaded =
        g_loaded_building_runtime_state[building_id];
    if (!loaded.valid || !loaded.foundation_state_valid) {
        return 0;
    }
    *state = loaded.foundation_state;
    return 1;
}

static int restore_loaded_foundation_state(
    Building &building_object,
    const building_type_registry_impl::FoundationTerrainSaveState &saved)
{
    using namespace building_type_registry_impl;
    if (!building_object.Foundation || !building_object.record()) {
        return 0;
    }
    std::vector<FoundationTerrainDelta> deltas;
    const FoundationDef &definition = building_object.Foundation->definition();
    if (!foundation_terrain_deltas_from_save(definition, saved, &deltas)) {
        return 0;
    }

    const building *record = building_object.record();
    const int rotation = definition.rotates() ? building_object.orientation() : 0;
    FoundationState &state = building_object.Foundation->state();
    state.begin_publication(record->x, record->y, rotation);
    const std::vector<FoundationCellDefinition> &canonical = definition.cells();
    for (const RotatedFoundationCell &cell : definition.rotated_cells(rotation)) {
        if (!cell.definition || !map_grid_is_inside(record->x + cell.x, record->y + cell.y, 1)) {
            state.clear();
            return 0;
        }
        const int cell_index = static_cast<int>(cell.definition - canonical.data());
        if (cell_index < 0 || cell_index >= static_cast<int>(deltas.size())) {
            state.clear();
            return 0;
        }
        FoundationTerrainDelta delta = deltas[cell_index];
        delta.grid_offset = map_grid_offset(record->x + cell.x, record->y + cell.y);
        state.record_delta(delta);
    }
    return 1;
}

int building_runtime_loaded_rubble_state(unsigned int building_id, RubbleState *state)
{
    if (!state) {
        return 0;
    }
    const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
        building_runtime_impl::loaded_runtime_state_for(building_id);
    if (!loaded || !loaded->rubble_state_valid) {
        return 0;
    }
    *state = loaded->rubble_state;
    return 1;
}

int building_runtime_loaded_graphics_state(unsigned int building_id, BuildingGraphicsState *state)
{
    if (!state) {
        return 0;
    }
    const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
        building_runtime_impl::loaded_runtime_state_for(building_id);
    if (!loaded || !loaded->graphics_state_valid) {
        return 0;
    }
    *state = loaded->graphics_state;
    return 1;
}

int building_runtime_loaded_housing_state(unsigned int building_id, HousingState *state)
{
    if (!state) {
        return 0;
    }
    const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
        building_runtime_impl::loaded_runtime_state_for(building_id);
    if (!loaded || !loaded->housing_state_valid) {
        return 0;
    }
    *state = loaded->housing_state;
    return 1;
}

void building_runtime_backup_graphics_state(void)
{
    using building_runtime_impl::g_graphics_state_backup;
    using building_runtime_impl::g_runtime_instances;

    g_graphics_state_backup.clear();
    g_graphics_state_backup.resize(g_runtime_instances.size());
    for (size_t id = 1; id < g_runtime_instances.size(); id++) {
        building_runtime *instance = g_runtime_instances[id].get();
        if (!instance) {
            continue;
        }
        g_graphics_state_backup[id].valid = 1;
        g_graphics_state_backup[id].state = instance->graphics_state_snapshot();
    }
}

void building_runtime_restore_graphics_state(void)
{
    using building_runtime_impl::g_graphics_state_backup;
    using building_runtime_impl::g_runtime_instances;

    const size_t count = std::min(g_runtime_instances.size(), g_graphics_state_backup.size());
    for (size_t id = 1; id < count; id++) {
        building_runtime *instance = g_runtime_instances[id].get();
        if (!instance || !g_graphics_state_backup[id].valid) {
            continue;
        }
        instance->restore_graphics_state(g_graphics_state_backup[id].state);
    }
}

struct NativeCompositionRuntimeSnapshot {
    unsigned int owner_id = 0;
    std::vector<unsigned int> child_ids;
};

static std::vector<NativeCompositionRuntimeSnapshot> snapshot_native_compositions()
{
    std::vector<NativeCompositionRuntimeSnapshot> snapshots;
    for (const std::unique_ptr<building_runtime> &runtime : building_runtime_impl::g_runtime_instances) {
        BuildingComposition *composition = runtime ? runtime->building.Composition : nullptr;
        if (!composition || !composition->is_owner() || !composition->complete()) {
            continue;
        }
        NativeCompositionRuntimeSnapshot snapshot;
        snapshot.owner_id = runtime->building.id;
        for (BuildingComposition *child : composition->children()) {
            if (child && child->building()) {
                snapshot.child_ids.push_back(child->building()->id);
            }
        }
        snapshots.push_back(std::move(snapshot));
    }
    return snapshots;
}

static int restore_native_composition_snapshots(
    const std::vector<NativeCompositionRuntimeSnapshot> &snapshots)
{
    for (const NativeCompositionRuntimeSnapshot &snapshot : snapshots) {
        Building *owner = Building::get(snapshot.owner_id);
        BuildingComposition *composition = owner ? owner->Composition : nullptr;
        if (!composition || !composition->is_owner() ||
            composition->children().size() != snapshot.child_ids.size()) {
            return 0;
        }
        std::vector<BuildingComposition *> children;
        children.reserve(snapshot.child_ids.size());
        for (unsigned int child_id : snapshot.child_ids) {
            Building *child = Building::get(child_id);
            if (!child || !child->Composition) {
                return 0;
            }
            children.push_back(child->Composition);
        }
        std::string error;
        if (!composition->attach_children(children, &error)) {
            log_error("Unable to restore native composition snapshot", error.c_str(), snapshot.owner_id);
            return 0;
        }
    }
    for (const NativeCompositionRuntimeSnapshot &snapshot : snapshots) {
        if (Building *owner = Building::get(snapshot.owner_id)) {
            if (building *record = const_cast<building *>(owner->record())) {
                record->prev_part_building_id = 0;
                record->next_part_building_id = 0;
            }
        }
        for (unsigned int child_id : snapshot.child_ids) {
            if (Building *child = Building::get(child_id)) {
                if (building *record = const_cast<building *>(child->record())) {
                    record->prev_part_building_id = 0;
                    record->next_part_building_id = 0;
                }
            }
        }
    }
    return 1;
}

// After save load/new city init, bind each live building instance to its runtime wrapper, rebuild native storage/production instances,
// and precompute cached image-group bindings.
void building_runtime_initialize_city_graphics_cache(void)
{
    const std::vector<NativeCompositionRuntimeSnapshot> composition_snapshots =
        snapshot_native_compositions();
    building_runtime_impl::reset_live_runtime_modules();
    map_building_rebind_runtime_references();
    building_local_workforce_initialize_city();

    building_for_each_loaded_record([](building *b) {
        if (building_runtime *instance = building_runtime_impl::get_or_create_instance(b)) {
            if (const building_runtime_impl::LoadedBuildingRuntimeState *loaded =
                    building_runtime_impl::loaded_runtime_state_for(b->id)) {
                if (loaded->graphics_state_valid) {
                    instance->restore_graphics_state(loaded->graphics_state);
                }
                if (loaded->rubble_state_valid) {
                    if (instance->building.Rubble && instance->building.Rubble->state()) {
                        *instance->building.Rubble->state() = loaded->rubble_state;
                    }
                }
                if (loaded->housing_state_valid && instance->building.Housing) {
                    instance->building.Housing->state() = loaded->housing_state;
                }
                if (loaded->foundation_state_valid && instance->building.Foundation &&
                    !restore_loaded_foundation_state(instance->building, loaded->foundation_state)) {
                    log_error("Unable to restore building FoundationState", 0, b->id);
                }
            }
            if (b->state == BUILDING_STATE_IN_USE || b->state == BUILDING_STATE_MOTHBALLED ||
                b->state == BUILDING_STATE_CREATED) {
                if (instance->building.Foundation) {
                    const int rotation = instance->building.Foundation->definition().rotates()
                        ? instance->building.orientation()
                        : 0;
                    instance->building.Foundation->rebind(b->x, b->y, rotation);
                    if (instance->building.Foundation->has_owner_controlled_passage() &&
                        !(instance->building.type && instance->building.type->bridge().is_bridge())) {
                        if (instance->building.type && instance->building.type->is_warehouse()) {
                            // Warehouse tower roads are freight access, not a
                            // shortcut for ordinary walkers. Freight is handled
                            // explicitly by Roadblock::allows_at().
                            if (config_get(CONFIG_GP_CH_WAREHOUSE_DEFAULT_TO_PASS_ALL_WALKERS)) {
                                instance->building.Foundation->roadblock_state().accept_all();
                            } else {
                                instance->building.Foundation->roadblock_state().accept_none();
                            }
                        } else {
                            instance->building.Foundation->roadblock_state().hydrate(
                                b->data.roadblock.exceptions);
                        }
                    }
                }
                instance->set_building_graphic();
            }
        }
    });

    // Load initialization has already consumed legacy fixed-composition chains
    // through building_hydrate_loaded_compositions(). Preserve that owner-bound
    // graph across the runtime-module reset instead of interpreting record ids
    // a second time. Dynamic bridge chains remain on their legacy path.
    if (!composition_snapshots.empty() &&
        !restore_native_composition_snapshots(composition_snapshots)) {
        log_error("Unable to restore native composition runtime snapshots", 0, 0);
    }

    storage_runtime_impl::initialize_city();
    production_runtime_impl::initialize_city();

    // Culture modules (including religion) depend on the runtime composition
    // graph, so loading cannot rebuild their capacity cache any earlier.
    city_culture_rebuild_module_capacity_cache();

    building_runtime_impl::clear_loaded_runtime_state();
}
