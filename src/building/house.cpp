#include "building/image.h"
#include "game/undo.h"
#include "map/building.h"
#include "map/image.h"
#include "map/road_access.h"
#include "figure/figure.h"

#include "house.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_runtime.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/HousingTransitionPlanner.h"
#include "building/HousingProfileDef.h"
#include "building/local_workforce.h"

#include "core/config.h"
#include "core/image.h"
#include "game/resource.h"
#include "map/grid.h"
#include "map/random.h"
#include "map/terrain.h"

#include <algorithm>
#include <utility>
#include <vector>

#define OFFSET(x,y) (x + GRID_SIZE * y)

struct HouseFootprintCell {
    int x = 0;
    int y = 0;
};

static int split_blocking_houses(
    Building source,
    int x,
    int y,
    const std::vector<HouseFootprintCell> &target_cells,
    int dry_run);

static Building *runtime_building(building *record)
{
    building_runtime *runtime = building_runtime_impl::get_or_create_instance(record);
    return runtime ? &runtime->building : nullptr;
}

static void remove_house_tiles(Building &house)
{
    house.remove_map_tiles();
}

static void add_house_tiles(Building &house_object)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    Building *runtime_house = runtime_building(house);
    if (!runtime_house) {
        return;
    }
    // House evolution redraws often. Clamp the existing stable option here instead
    // of reseeding so a valid visual choice survives normal evolve/devolve cycles.
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        runtime->assign_graphic_variant(0);
    }
    const int image_id = building_image_get(runtime_house);
    runtime_house->add_map_tiles(image_id);
    runtime_house->refresh_graphic_if_native();
}

static const building_type_registry_impl::HousingDef *housing_definition_for_type(building_type type)
{
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    return definition && definition->housing_def().profile ? &definition->housing_def() : nullptr;
}

static std::vector<HouseFootprintCell> housing_foundation_cells(building_type type, int orientation)
{
    std::vector<HouseFootprintCell> result;
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    const auto *foundation = definition ? definition->foundation_def() : nullptr;
    if (!foundation) {
        return result;
    }
    for (const auto &cell : foundation->rotated_cells(orientation)) {
        result.push_back({ cell.x, cell.y });
    }
    return result;
}

static std::vector<HouseFootprintCell> housing_foundation_cells(const Building &house)
{
    std::vector<HouseFootprintCell> result;
    if (!house.Foundation) {
        return result;
    }
    for (const auto &cell : house.Foundation->cells(house.orientation())) {
        result.push_back({ cell.x, cell.y });
    }
    return result;
}

static building_type vacant_lot_fill_type()
{
    return building_type_registry_impl::vacant_lot_fill_type();
}

static bool is_empty_vacant_lot(const Building &house)
{
    return house.Housing && house.Housing->state().population == 0 &&
        house.type && house.type->type() == vacant_lot_fill_type();
}

void building_house_change_to(Building house_object, building_type type)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    int should_reseed_graphics = is_empty_vacant_lot(house_object);
    if (!housing_definition_for_type(type)) {
        return;
    }
    remove_house_tiles(house_object);
    house_object.change_type(type);
    // Vacant lots do not carry a meaningful house visual variant, so first
    // occupation gets a fresh stable choice. Other transitions preserve it.
    if (building_runtime *runtime = building_runtime_impl::get_or_create_instance(house)) {
        runtime->assign_graphic_variant(should_reseed_graphics);
    }
    add_house_tiles(house_object);
}

static void create_vacant_lot(int x, int y)
{
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE || !housing_definition_for_type(type)) {
        return;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }
    Building &building_obj = city_building_runtime().create(*definition, x, y);
    building *b = const_cast<building *>(building_obj.record());
    building_obj.Housing->state().population = 0;
    b->distance_from_entry = 0;
    add_house_tiles(building_obj);
}

void building_house_change_to_vacant_lot(Building house_object)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return;
    }
    const auto occupied_cells = house_object.Foundation
        ? house_object.Foundation->cells(house_object.orientation())
        : std::vector<building_type_registry_impl::RotatedFoundationCell>{};
    const bool has_multiple_cells = occupied_cells.size() > 1;
    const int origin_x = house->x;
    const int origin_y = house->y;
    remove_house_tiles(house_object);
    house_object.Housing->state().population = 0;
    building_type type = vacant_lot_fill_type();
    if (type == BUILDING_NONE || !housing_definition_for_type(type)) {
        return;
    }
    house_object.change_type(type);
    if (has_multiple_cells) {
        house->is_close_to_water = static_cast<unsigned char>(building_is_close_to_water(house));
        add_house_tiles(house_object);
        for (const auto &cell : occupied_cells) {
            if (cell.x || cell.y) {
                create_vacant_lot(origin_x + cell.x, origin_y + cell.y);
            }
        }
    } else {
        add_house_tiles(house_object);
    }
}

struct HouseMergePlan {
    building_type type = BUILDING_NONE;
    int x = 0;
    int y = 0;
    Building *source = nullptr;
    std::vector<Building *> participants;
    int inventory[RESOURCE_SLOT_COUNT] = {};
    int population = 0;
    int happiness_weight = 0;
};

enum class HouseExpandMode {
    HousesOnly,
    ClearTerrain,
    Gardens,
};

static Building *building_at_tile(int grid_offset)
{
    return map_building_exists_at(grid_offset) ? &map_building_at(grid_offset) : nullptr;
}

static int plan_has_participant(const HouseMergePlan &plan, unsigned int building_id)
{
    for (const Building *participant : plan.participants) {
        if (participant && participant->id == building_id) {
            return 1;
        }
    }
    return 0;
}

static void add_participant(HouseMergePlan &plan, Building *house)
{
    if (!house || !house->id || plan_has_participant(plan, house->id)) {
        return;
    }
    plan.participants.push_back(house);
    const HousingState &state = house->Housing->state();
    plan.population += state.population;
    plan.happiness_weight += state.population * state.happiness;
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        plan.inventory[r] += house->resource_amount(r);
    }
}

static int house_can_share_expand_footprint(Building source, Building *other)
{
    if (!other || !other->id) {
        return 0;
    }
    if (other->id == source.id) {
        return 1;
    }
    const auto *other_profile = other->Housing ? other->Housing->definition().profile : nullptr;
    const auto *source_profile = source.Housing ? source.Housing->definition().profile : nullptr;
    return other->is_in_use() && other_profile && source_profile &&
        other_profile->compatibility_level <= source_profile->compatibility_level;
}

static int tile_can_expand_into(Building source, int tile_offset, HouseExpandMode mode, bool merge_validation)
{
    if (map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
        Building *occupant = building_at_tile(tile_offset);
        if (merge_validation) {
            return occupant && occupant->is_in_use() && occupant->Housing;
        }
        return house_can_share_expand_footprint(source, occupant);
    }
    if (mode == HouseExpandMode::HousesOnly) {
        return 0;
    }
    if (!map_terrain_is(tile_offset, TERRAIN_NOT_CLEAR)) {
        return 1;
    }
    return mode == HouseExpandMode::Gardens &&
        !config_get(CONFIG_GP_CH_HOUSES_DONT_EXPAND_INTO_GARDENS) &&
        map_terrain_is(tile_offset, TERRAIN_GARDEN);
}

static bool target_contains_source(
    const Building &source,
    int target_x,
    int target_y,
    const std::vector<HouseFootprintCell> &target_cells)
{
    const auto source_cells = housing_foundation_cells(source);
    for (const HouseFootprintCell &source_cell : source_cells) {
        const int world_x = source.x() + source_cell.x;
        const int world_y = source.y() + source_cell.y;
        const auto found = std::find_if(target_cells.begin(), target_cells.end(), [&](const HouseFootprintCell &target_cell) {
            return target_x + target_cell.x == world_x && target_y + target_cell.y == world_y;
        });
        if (found == target_cells.end()) {
            return false;
        }
    }
    return !source_cells.empty();
}

static int find_transition_origin(
    Building house,
    building_type target_type,
    bool houses_only,
    int *out_x,
    int *out_y)
{
    const auto target_cells = housing_foundation_cells(target_type, house.orientation());
    const auto source_cells = housing_foundation_cells(house);
    if (target_cells.empty() || source_cells.empty()) {
        return 0;
    }

    const HouseExpandMode modes[] = {
        HouseExpandMode::HousesOnly,
        HouseExpandMode::ClearTerrain,
        HouseExpandMode::Gardens,
    };
    for (HouseExpandMode mode : modes) {
        if (houses_only && mode != HouseExpandMode::HousesOnly) {
            break;
        }
        std::vector<std::pair<int, int>> candidates;
        for (const HouseFootprintCell &source_cell : source_cells) {
            for (const HouseFootprintCell &target_cell : target_cells) {
                const std::pair<int, int> candidate = {
                    house.x() + source_cell.x - target_cell.x,
                    house.y() + source_cell.y - target_cell.y,
                };
                if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
                    candidates.push_back(candidate);
                }
            }
        }

        for (const auto &[candidate_x, candidate_y] : candidates) {
            if (!target_contains_source(house, candidate_x, candidate_y, target_cells)) {
                continue;
            }
            bool valid = true;
            for (const HouseFootprintCell &cell : target_cells) {
                const int world_x = candidate_x + cell.x;
                const int world_y = candidate_y + cell.y;
                if (!map_grid_is_inside(world_x, world_y, 1)) {
                    valid = false;
                    break;
                }
                const int tile_offset = map_grid_offset(world_x, world_y);
                if (!map_grid_is_valid_offset(tile_offset) ||
                    !tile_can_expand_into(house, tile_offset, mode, houses_only)) {
                    valid = false;
                    break;
                }
            }
            if (valid && split_blocking_houses(house, candidate_x, candidate_y, target_cells, 1)) {
                *out_x = candidate_x;
                *out_y = candidate_y;
                return 1;
            }
        }
    }
    return 0;
}

static int collect_house_merge_plan(Building &source, building_type type, int x, int y,
    const std::vector<HouseFootprintCell> &target_cells, bool require_merge_target, HouseMergePlan *plan)
{
    if (!source.id || type == BUILDING_NONE || !plan) {
        return 0;
    }
    *plan = {};
    plan->type = type;
    plan->x = x;
    plan->y = y;
    plan->source = &source;

    for (const HouseFootprintCell &cell : target_cells) {
        int tile_offset = map_grid_offset(x + cell.x, y + cell.y);
        if (!map_terrain_is(tile_offset, TERRAIN_BUILDING)) {
            continue;
        }
        Building *participant = building_at_tile(tile_offset);
        if (!participant || !participant->id || !participant->is_in_use() || !participant->Housing) {
            return 0;
        }
        add_participant(*plan, participant);
    }
    if (!plan_has_participant(*plan, source.id)) {
        add_participant(*plan, &source);
    }

    if (require_merge_target) {
        const auto *target_definition = building_type_registry_impl::definition_for_type(type);
        std::vector<building_type_registry_impl::HousingFootprintCell> target_world_cells;
        for (const HouseFootprintCell &cell : target_cells) {
            target_world_cells.push_back({ x + cell.x, y + cell.y });
        }
        std::vector<building_type_registry_impl::HousingMergeParticipant> participants;
        for (Building *participant : plan->participants) {
            building_type_registry_impl::HousingMergeParticipant candidate;
            candidate.building_id = participant->id;
            const bool vacant = participant->Housing->state().population <= 0 &&
                participant->type->type() == building_type_registry_impl::vacant_lot_fill_type() &&
                config_get(CONFIG_GP_CH_HOUSING_PRE_MERGE_VACANT_LOTS);
            candidate.merge_target = vacant
                ? target_definition
                : participant->Housing->transition_target(building_type_registry_impl::HousingTransitionKind::MergeTo);
            candidate.qualifies = participant->is_in_use() && (vacant || candidate.merge_target == target_definition);
            for (const HouseFootprintCell &cell : housing_foundation_cells(*participant)) {
                candidate.cells.push_back({ participant->x() + cell.x, participant->y() + cell.y });
            }
            participants.push_back(std::move(candidate));
        }
        const auto merge_plan = building_type_registry_impl::plan_housing_merge(
            target_definition, target_world_cells, participants);
        if (!merge_plan) {
            return 0;
        }
        std::sort(plan->participants.begin(), plan->participants.end(), [](const Building *a, const Building *b) {
            return a->id < b->id;
        });
        plan->source = plan->participants.front();
    }
    return !plan->participants.empty();
}

static void retarget_figures_for_house_merge(const HouseMergePlan &plan, Building &replacement)
{
    std::vector<unsigned int> figure_ids;
    for (Building *participant : plan.participants) {
        if (!participant) {
            continue;
        }
        for (unsigned int figure_id : Figure::ids_referencing_building(*participant)) {
            if (std::find(figure_ids.begin(), figure_ids.end(), figure_id) == figure_ids.end()) {
                figure_ids.push_back(figure_id);
            }
        }
    }

    for (unsigned int figure_id : figure_ids) {
        Figure *figure = Figure::get(figure_id);
        if (!figure || figure->id() != figure_id || !figure->state) {
            continue;
        }
        for (Building *participant : plan.participants) {
            if (!participant) {
                continue;
            }
            const int was_immigrant = figure->immigrant_building && figure->immigrant_building->id == participant->id;
            const int retargeted = figure->retarget_building(*participant, replacement);
            if (retargeted) {
                if (was_immigrant && !replacement.Housing->state().immigrant_figure_id) {
                    replacement.Housing->track_immigrant(*figure);
                }
                replacement.copy_house_figure_slot_from(*participant, figure->id());
            }
        }
    }
}

static unsigned int apply_house_merge_plan(const HouseMergePlan &plan)
{
    if (plan.type == BUILDING_NONE || !plan.source || !plan.source->id || plan.participants.empty() ||
        !housing_definition_for_type(plan.type)) {
        return 0;
    }

    Building &source = *plan.source;
    const building_type_registry_impl::BuildingType *replacement_type =
        building_type_registry_impl::definition_for_type(plan.type);
    if (!replacement_type) {
        return 0;
    }
    Building &replacement = city_building_runtime().create(*replacement_type, plan.x, plan.y);
    replacement.copy_house_data_from(source);
    if (!replacement.configure_house_replacement(plan.type, plan.x, plan.y)) {
        replacement.retire_replaced_house();
        return 0;
    }
    HousingState &replacement_state = replacement.Housing->state();
    replacement_state.population = static_cast<int16_t>(plan.population);
    replacement_state.happiness = static_cast<int8_t>(
        plan.population ? plan.happiness_weight / plan.population : source.Housing->state().happiness);
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        replacement.set_resource_amount(r, plan.inventory[r]);
    }
    int capacity = replacement.Housing->effective_capacity();
    replacement_state.population_room =
        static_cast<int16_t>(std::max(0, capacity - replacement_state.population));

    retarget_figures_for_house_merge(plan, replacement);
    for (Building *participant : plan.participants) {
        if (participant) {
            building_local_workforce::replace_house(*participant, replacement);
            remove_house_tiles(*participant);
        }
    }
    add_house_tiles(replacement);
    for (Building *participant : plan.participants) {
        if (participant) {
            participant->retire_replaced_house();
        }
    }
    return replacement.id;
}

unsigned int building_house_merge(Building house_object)
{
    if (!house_object.id || !house_object.Housing) {
        return 0;
    }
    if (!config_get(CONFIG_GP_CH_ALL_HOUSES_MERGE)) {
        if ((map_random_get(house_object.grid_offset()) & 7) >= 5) {
            return 0;
        }
    }
    const auto *merge_definition = house_object.Housing->transition_target(
        building_type_registry_impl::HousingTransitionKind::MergeTo);
    if (!merge_definition) {
        return 0;
    }
    const building_type merge_type = merge_definition->type();
    int x = 0;
    int y = 0;
    if (!find_transition_origin(house_object, merge_type, true, &x, &y)) {
        return 0;
    }
    HouseMergePlan plan;
    const auto target_cells = housing_foundation_cells(merge_type, house_object.orientation());
    if (target_cells.empty() || !housing_definition_for_type(merge_type)) {
        return 0;
    }
    if (!collect_house_merge_plan(
            house_object, merge_type, x, y, target_cells, true, &plan)) {
        return 0;
    }
    game_undo_disable();
    return apply_house_merge_plan(plan);
}

int building_house_can_expand(Building house_object, building_type target_type)
{
    ::building *house = const_cast<::building *>(house_object.record());
    if (!house) {
        return 0;
    }
    int x = 0;
    int y = 0;
    if (find_transition_origin(house_object, target_type, false, &x, &y)) {
        return 1;
    }
    house_object.Housing->state().no_space_to_expand = 1;
    return 0;
}

static void create_splitted_house_tile(Building &source, building_type type,
    int x, int y, int population, const int *inventory)
{
    if (type == BUILDING_NONE || !housing_definition_for_type(type)) {
        return;
    }
    if (!source.id) {
        return;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return;
    }
    Building &house = city_building_runtime().create(*definition, x, y);
    house.copy_house_data_from(source);
    house.Housing->state().population = static_cast<int16_t>(population);
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        house.set_resource_amount(r, inventory[r]);
    }
    house.set_distance_from_entry(0);
    add_house_tiles(house);
}

static bool one_cell_housing_foundation(building_type type, HouseFootprintCell *cell = nullptr)
{
    const auto cells = housing_foundation_cells(type, 0);
    if (!housing_definition_for_type(type) || cells.size() != 1) {
        return false;
    }
    if (cell) {
        *cell = cells.front();
    }
    return true;
}

static void split_house_into_cells(building *house, building_type split_type)
{
    Building *source = runtime_building(house);
    HouseFootprintCell split_cell;
    if (!source || !one_cell_housing_foundation(split_type, &split_cell)) {
        return;
    }

    const auto footprint = housing_foundation_cells(*source);
    if (footprint.empty()) {
        return;
    }
    const int shares = static_cast<int>(footprint.size());
    int inventory_per_cell[RESOURCE_SLOT_COUNT];
    int inventory_remainder[RESOURCE_SLOT_COUNT];
    for (int i = 0; i < RESOURCE_SLOT_COUNT; ++i) {
        inventory_per_cell[i] = house->resources[i] / shares;
        inventory_remainder[i] = house->resources[i] % shares;
    }
    const int population_per_cell = source->Housing->state().population / shares;
    const int population_remainder = source->Housing->state().population % shares;
    const int source_x = house->x;
    const int source_y = house->y;

    remove_house_tiles(*source);
    const int first_x = source_x + footprint.front().x - split_cell.x;
    const int first_y = source_y + footprint.front().y - split_cell.y;
    if (!source->configure_house_replacement(split_type, first_x, first_y)) {
        return;
    }
    source->Housing->state().population =
        static_cast<int16_t>(population_per_cell + population_remainder);
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        source->set_resource_amount(r, inventory_per_cell[r] + inventory_remainder[r]);
    }
    source->set_distance_from_entry(0);
    add_house_tiles(*source);

    for (size_t i = 1; i < footprint.size(); ++i) {
        create_splitted_house_tile(
            *source,
            split_type,
            source_x + footprint[i].x - split_cell.x,
            source_y + footprint[i].y - split_cell.y,
            population_per_cell,
            inventory_per_cell);
    }
}

static int expansion_contains_house_footprint(
    int expansion_x,
    int expansion_y,
    const std::vector<HouseFootprintCell> &target_cells,
    const Building &house)
{
    if (!house.Foundation) {
        return 0;
    }

    const auto cells = house.Foundation->cells(house.orientation());
    if (cells.size() <= 1) {
        return 1;
    }

    for (const auto &cell : cells) {
        const int world_x = house.x() + cell.x;
        const int world_y = house.y() + cell.y;
        const auto found = std::find_if(target_cells.begin(), target_cells.end(), [&](const HouseFootprintCell &target_cell) {
            return expansion_x + target_cell.x == world_x && expansion_y + target_cell.y == world_y;
        });
        if (found == target_cells.end()) {
            return 0;
        }
    }
    return 1;
}

static int split_blocking_houses(
    Building source,
    int x,
    int y,
    const std::vector<HouseFootprintCell> &target_cells,
    int dry_run)
{
    const unsigned int source_id = source.id;
    std::vector<Building *> blockers;
    for (const HouseFootprintCell &cell : target_cells) {
        int tile_offset = map_grid_offset(x + cell.x, y + cell.y);
        if (map_terrain_is(tile_offset, TERRAIN_BUILDING) && map_building_exists_at(tile_offset)) {
            Building &other_object = map_building_at(tile_offset);
            building *other_house = const_cast<::building *>(other_object.record());
            if (other_house && other_house->id != source_id && other_object.Housing) {
                const auto already_seen = std::find_if(blockers.begin(), blockers.end(), [&](const Building *blocker) {
                    return blocker && blocker->id == other_object.id;
                });
                if (already_seen != blockers.end()) {
                    continue;
                }
                if (!expansion_contains_house_footprint(x, y, target_cells, other_object)) {
                    return 0;
                }
                const auto *split_definition = other_object.Housing->transition_target(
                    building_type_registry_impl::HousingTransitionKind::SplitTo);
                const building_type split_type = split_definition ? split_definition->type() : BUILDING_NONE;
                if (!one_cell_housing_foundation(split_type)) {
                    return 0;
                }
                blockers.push_back(&other_object);
            }
        }
    }
    if (!dry_run) {
        for (Building *blocker : blockers) {
            building *record = blocker ? const_cast<building *>(blocker->record()) : nullptr;
            const auto *split_definition = blocker && blocker->Housing
                ? blocker->Housing->transition_target(building_type_registry_impl::HousingTransitionKind::SplitTo)
                : nullptr;
            split_house_into_cells(record, split_definition ? split_definition->type() : BUILDING_NONE);
        }
    }
    return 1;
}

/// Foundation-cell house transitions

int building_house_expand_to_type(Building house_object, building_type type)
{
    if (!house_object.id || !house_object.Housing) {
        return 0;
    }
    const auto source_cells = housing_foundation_cells(house_object);
    const auto target_cells = housing_foundation_cells(type, house_object.orientation());
    if (!housing_definition_for_type(type) || target_cells.size() <= source_cells.size()) {
        return 0;
    }

    int x = 0;
    int y = 0;
    if (!find_transition_origin(house_object, type, false, &x, &y)) {
        ::building *house = const_cast<::building *>(house_object.record());
        if (house) {
            house_object.Housing->state().no_space_to_expand = 1;
        }
        return 0;
    }

    if (!split_blocking_houses(house_object, x, y, target_cells, 1)) {
        return 0;
    }
    if (!split_blocking_houses(house_object, x, y, target_cells, 0)) {
        return 0;
    }
    HouseMergePlan plan;
    if (!collect_house_merge_plan(
            house_object, type, x, y, target_cells, false, &plan)) {
        return 0;
    }
    return apply_house_merge_plan(plan) ? 1 : 0;
}

static bool footprint_contains_world_cell(
    int origin_x,
    int origin_y,
    const std::vector<HouseFootprintCell> &cells,
    int world_x,
    int world_y)
{
    return std::any_of(cells.begin(), cells.end(), [&](const HouseFootprintCell &cell) {
        return origin_x + cell.x == world_x && origin_y + cell.y == world_y;
    });
}

static bool find_shrink_origin(
    const Building &source,
    const std::vector<HouseFootprintCell> &source_cells,
    const std::vector<HouseFootprintCell> &target_cells,
    int *target_x,
    int *target_y)
{
    std::vector<std::pair<int, int>> candidates = { { source.x(), source.y() } };
    for (const HouseFootprintCell &source_cell : source_cells) {
        for (const HouseFootprintCell &target_cell : target_cells) {
            const std::pair<int, int> candidate = {
                source.x() + source_cell.x - target_cell.x,
                source.y() + source_cell.y - target_cell.y,
            };
            if (std::find(candidates.begin(), candidates.end(), candidate) == candidates.end()) {
                candidates.push_back(candidate);
            }
        }
    }
    for (const auto &[candidate_x, candidate_y] : candidates) {
        const bool contained = std::all_of(target_cells.begin(), target_cells.end(), [&](const HouseFootprintCell &cell) {
            return footprint_contains_world_cell(
                source.x(), source.y(), source_cells, candidate_x + cell.x, candidate_y + cell.y);
        });
        if (contained) {
            *target_x = candidate_x;
            *target_y = candidate_y;
            return true;
        }
    }
    return false;
}

static void shrink_house_to_type(Building &source, building_type type)
{
    building *house = const_cast<building *>(source.record());
    const auto source_cells = housing_foundation_cells(source);
    const auto target_cells = housing_foundation_cells(type, source.orientation());
    if (!house || !housing_definition_for_type(type) || source_cells.empty() || target_cells.empty() ||
        target_cells.size() > source_cells.size()) {
        return;
    }

    int target_x = 0;
    int target_y = 0;
    if (!find_shrink_origin(source, source_cells, target_cells, &target_x, &target_y)) {
        return;
    }

    std::vector<HouseFootprintCell> leftover_world_cells;
    for (const HouseFootprintCell &cell : source_cells) {
        const int world_x = source.x() + cell.x;
        const int world_y = source.y() + cell.y;
        if (!footprint_contains_world_cell(target_x, target_y, target_cells, world_x, world_y)) {
            leftover_world_cells.push_back({ world_x, world_y });
        }
    }

    const auto *split_definition = source.Housing->transition_target(
        building_type_registry_impl::HousingTransitionKind::SplitTo);
    const building_type split_type = split_definition ? split_definition->type() : BUILDING_NONE;
    HouseFootprintCell split_cell;
    if (!leftover_world_cells.empty() && !one_cell_housing_foundation(split_type, &split_cell)) {
        return;
    }

    const int shares = 1 + static_cast<int>(leftover_world_cells.size());
    int inventory_per_result[RESOURCE_SLOT_COUNT];
    int inventory_remainder[RESOURCE_SLOT_COUNT];
    for (int i = 0; i < RESOURCE_SLOT_COUNT; ++i) {
        inventory_per_result[i] = house->resources[i] / shares;
        inventory_remainder[i] = house->resources[i] % shares;
    }
    const int population_per_result = source.Housing->state().population / shares;
    const int population_remainder = source.Housing->state().population % shares;

    remove_house_tiles(source);
    if (!source.configure_house_replacement(type, target_x, target_y)) {
        return;
    }
    source.Housing->state().population =
        static_cast<int16_t>(population_per_result + population_remainder);
    for (resource_type r = RESOURCE_NONE; r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        source.set_resource_amount(r, inventory_per_result[r] + inventory_remainder[r]);
    }
    source.set_distance_from_entry(0);
    add_house_tiles(source);

    for (const HouseFootprintCell &cell : leftover_world_cells) {
        create_splitted_house_tile(
            source,
            split_type,
            cell.x - split_cell.x,
            cell.y - split_cell.y,
            population_per_result,
            inventory_per_result);
    }
}

void building_house_devolve_to_type(Building house_object, building_type type)
{
    if (!house_object.id || !house_object.Housing || !housing_definition_for_type(type)) {
        return;
    }
    shrink_house_to_type(house_object, type);
}
