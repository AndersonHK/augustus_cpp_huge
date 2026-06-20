#include "building/building.h"
#include "building/list.h"
#include "building/local_workforce.h"
#include "building/maintenance.h"
#include "city/festival.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "map/building.h"
#include "map/road_access.h"

#include "figure/figure_runtime_api.h"

#include "building/building_type_registry_internal.h"
#include "core/crash_context.h"
#include "figure/figure_runtime_native.h"
#include "figure/figure_type_registry_internal.h"
#include "map/road_service_history.h"
#include "map/routing_distance.h"

#include "building/building_record.h"
#include "core/buffer.h"
#include "building/monument.h"
#include "city/figures.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/enemy_army.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/grid.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "scenario/gladiator_revolt.h"
#include "sound/effect.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

namespace {

struct RuntimeEntry {
    Figure *data = nullptr;
    unsigned short created_sequence = 0;
    const figure_type_registry_impl::FigureTypeDefinition *definition = nullptr;
    const figure_type_registry_impl::FigureTypeProfile *profile = nullptr;
    std::unique_ptr<figure_runtime_native_impl::NativeFigure> controller;
};

std::vector<RuntimeEntry> g_runtime_entries;

RuntimeEntry *entry_for_id(unsigned int id)
{
    if (!id) {
        return nullptr;
    }
    if (g_runtime_entries.size() <= id) {
        g_runtime_entries.resize(id + 1);
    }
    return &g_runtime_entries[id];
}

void clear_entry(unsigned int id)
{
    if (!id || g_runtime_entries.size() <= id) {
        return;
    }
    g_runtime_entries[id] = RuntimeEntry();
}

int entry_matches_figure(const RuntimeEntry &entry, const Figure *f)
{
    return entry.data == f && entry.created_sequence == f->created_sequence;
}

bool type_matches(const building_type_registry_impl::BuildingType *type, const char *text_id)
{
    return type && std::string_view(type->attr()) == text_id;
}

const char *profile_id_for_priest_owner(const Figure *f)
{
    if (!f || !f->building.id()) {
        return nullptr;
    }

    // Legacy saves do not persist XML profile bindings. Priest recovery keeps
    // the old temple-to-god mapping, then the recovered profile owns the effect.
    const building_type_registry_impl::BuildingType *type = f->building.type;
    if (!type) {
        return nullptr;
    }
    if (type->is_pantheon()) {
        return "pantheon_service";
    }
    if (type->is_ceres_temple()) {
        return "ceres_service";
    }
    if (type->is_neptune_temple()) {
        return "neptune_service";
    }
    if (type->is_mercury_temple()) {
        return "mercury_service";
    }
    if (type->is_mars_temple()) {
        return "mars_service";
    }
    if (type->is_venus_temple()) {
        return "venus_service";
    }
    return nullptr;
}

const char *infer_profile_id(const Figure *f)
{
    if (!f) {
        return nullptr;
    }

    // Loaded saves only have legacy action/owner fields, so this is a compatibility bridge.
    // New XML spawns bind the exact profile at creation and do not rely on this inference.
    switch (f->type) {
        case FIGURE_LABOR_SEEKER:
            return f->collecting_item_id ? "validation" : "acquisition";
        case FIGURE_PRIEST:
            return profile_id_for_priest_owner(f);
        case FIGURE_PATRICIAN:
            return "house_roamer";
        case FIGURE_BEGGAR:
            return "unemployment_wanderer";
        case FIGURE_MARKET_TRADER:
            return "service";
        case FIGURE_MARKET_SUPPLIER:
            return "storage_fetch";
        case FIGURE_DELIVERY_BOY:
            return "follow_leader";
        case FIGURE_DEPOT_CART_PUSHER:
            return "order_runner";
        case FIGURE_ACTOR:
        case FIGURE_GLADIATOR:
        case FIGURE_LION_TAMER:
        case FIGURE_CHARIOTEER:
            switch (f->action_state) {
                case FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED:
                case FIGURE_ACTION_91_ENTERTAINER_EXITING_SCHOOL:
                case FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE:
                    return "venue_seeker";
                case FIGURE_ACTION_94_ENTERTAINER_ROAMING:
                case FIGURE_ACTION_95_ENTERTAINER_RETURNING: {
                    if (!f->building.id()) {
                        return nullptr;
                    }
                    const building_type_registry_impl::BuildingType *building_type = f->building.type;
                    if (f->type == FIGURE_ACTOR) {
                        return type_matches(building_type, "amphitheater") ? "amphitheater_service" : "theater_service";
                    }
                    if (f->type == FIGURE_GLADIATOR) {
                        if (type_matches(building_type, "arena")) {
                            return "arena_service";
                        }
                        if (type_matches(building_type, "colosseum")) {
                            return "colosseum_service";
                        }
                        return "amphitheater_service";
                    }
                    if (f->type == FIGURE_LION_TAMER) {
                        return type_matches(building_type, "colosseum") ? "colosseum_service" : "arena_service";
                    }
                    if (f->type == FIGURE_CHARIOTEER) {
                        return "hippodrome_service";
                    }
                    return nullptr;
                }
                default:
                    return nullptr;
            }
        default:
            return nullptr;
    }
}

RuntimeEntry *bind_entry(Figure *f)
{
    // Save files and legacy spawners only persist figure fields, not the
    // XML profile pointer. Rebinding lazily here lets loaded figures recover
    // their controller the first tick they execute.
    if (!f || !f->id()) {
        return nullptr;
    }

    RuntimeEntry *entry = entry_for_id(f->id());
    if (!entry) {
        return nullptr;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    if (!definition) {
        *entry = RuntimeEntry();
        return nullptr;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = entry->profile;
    if (!entry_matches_figure(*entry, f) || entry->definition != definition || !profile) {
        profile = definition->profile(infer_profile_id(f));
    }
    if (!profile) {
        profile = definition->default_profile();
    }

    if (!entry_matches_figure(*entry, f) ||
        entry->definition != definition ||
        entry->profile != profile ||
        !entry->controller) {
        entry->data = f;
        entry->created_sequence = f->created_sequence;
        entry->definition = definition;
        entry->profile = profile;
        entry->controller = figure_runtime_native_impl::make_controller(f, definition, profile);
    } else {
        entry->data = f;
        entry->controller->set_figure(f);
    }

    if (!entry->controller) {
        *entry = RuntimeEntry();
        return nullptr;
    }
    return entry;
}

void record_service_history(road_service_effect effect, int grid_offset)
{
    if (effect == ROAD_SERVICE_EFFECT_RELIGION_PANTHEON) {
        // Pantheon service historically refreshed all five gods plus Pantheon
        // itself, so the explicit profile effect expands at every history write.
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_CERES, grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_NEPTUNE, grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_MERCURY, grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_MARS, grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_VENUS, grid_offset);
        map_road_service_history_record(ROAD_SERVICE_EFFECT_RELIGION_PANTHEON, grid_offset);
    } else if (effect != ROAD_SERVICE_EFFECT_NONE) {
        map_road_service_history_record(effect, grid_offset);
    }
}

} // namespace

void figure_runtime_reset()
{
    g_runtime_entries.clear();
}

void figure_runtime_initialize_city()
{
    figure_runtime_reset();

    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (!f || !f->state) {
            continue;
        }
        bind_entry(f);
    }
}

void figure_runtime_on_created(Figure *f)
{
    if (!f || !f->id()) {
        return;
    }

    RuntimeEntry *entry = entry_for_id(f->id());
    if (!entry) {
        return;
    }
    *entry = RuntimeEntry();
    bind_entry(f);
}

int figure_runtime_bind_profile(Figure *f, const char *profile_id)
{
    if (!f || !f->id()) {
        return 0;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    if (!definition) {
        return 0;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = definition->profile(profile_id);
    if (!profile) {
        return 0;
    }

    RuntimeEntry *entry = entry_for_id(f->id());
    if (!entry) {
        return 0;
    }

    entry->data = f;
    entry->created_sequence = f->created_sequence;
    entry->definition = definition;
    entry->profile = profile;
    entry->controller = figure_runtime_native_impl::make_controller(f, definition, profile);
    return entry->controller ? 1 : 0;
}

Figure *figure_runtime_create_profiled(
    figure_type type,
    int x,
    int y,
    direction_type dir,
    Building &building,
    const char *profile_id)
{
    const figure_type_registry_impl::FigureTypeProfile *profile =
        figure_type_registry_impl::profile_for(type, profile_id);
    if (!profile) {
        // BuildingType spawn profile references are validated during FigureType
        // load; callers still fail closed if a mod removes a profile later.
        return nullptr;
    }

    Figure *f = Figure::create(type, x, y, dir);
    if (!f) {
        return nullptr;
    }

    f->building = building;
    // The profile owns the lifecycle; these legacy action states remain the
    // save-compatible storage used to rebind the same profile after load.
    switch (profile->native_class()) {
        case figure_type_registry_impl::NativeClassId::EngineerService:
            f->action_state = FIGURE_ACTION_60_ENGINEER_CREATED;
            break;
        case figure_type_registry_impl::NativeClassId::PrefectService:
            f->action_state = FIGURE_ACTION_70_PREFECT_CREATED;
            break;
        case figure_type_registry_impl::NativeClassId::EntertainmentService:
            f->action_state = FIGURE_ACTION_94_ENTERTAINER_ROAMING;
            figure_movement_init_roaming(f);
            break;
        case figure_type_registry_impl::NativeClassId::EntertainmentVenueSeeker:
            f->action_state = FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED;
            break;
        case figure_type_registry_impl::NativeClassId::MarketSupplier:
            f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
            break;
        case figure_type_registry_impl::NativeClassId::DepotCartPusher:
            f->action_state = FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL;
            break;
        case figure_type_registry_impl::NativeClassId::LegacyAction:
            break;
        case figure_type_registry_impl::NativeClassId::DeliveryFollower:
            break;
        case figure_type_registry_impl::NativeClassId::TransientWanderer:
            if (profile->pathing_policy().mode == &figure_type_registry_impl::StandStill) {
                f->action_state = 0;
            } else {
                f->action_state = FIGURE_ACTION_125_ROAMING;
                figure_movement_init_roaming(f);
            }
            break;
        case figure_type_registry_impl::NativeClassId::RoamingService:
        default:
            f->action_state = FIGURE_ACTION_125_ROAMING;
            figure_movement_init_roaming(f);
            break;
    }

    if (!figure_runtime_bind_profile(f, profile_id)) {
        f->state = FIGURE_STATE_DEAD;
        return nullptr;
    }
    return f;
}

void figure_runtime_on_deleted(Figure *f)
{
    if (!f) {
        return;
    }
    clear_entry(f->id());
}

int figure_runtime_execute(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->controller) {
        return 0;
    }
    return entry->controller->execute();
}

int figure_runtime_has_native_graphics(const Figure *f)
{
    if (!f) {
        return 0;
    }
    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    return figure_runtime_native_impl::graphics_policy_has_native_payload(definition);
}

const RuntimeDrawSlice *figure_runtime_graphic_slice(const Figure *f)
{
    if (!f) {
        return nullptr;
    }
    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    return figure_runtime_native_impl::graphics_policy_slice_for_figure(f, definition);
}

int figure_runtime_update_graphics(Figure *f)
{
    if (!figure_runtime_has_native_graphics(f)) {
        return 0;
    }
    f->is_enemy_image = 0;
    return figure_runtime_graphic_slice(f) ? 1 : 0;
}

int figure_runtime_choose_roaming_direction(
    Figure *f,
    const int *road_tiles,
    int came_from_direction,
    int vanilla_direction)
{
    // Keep vanilla ordering as the tie-breaker. Smart service pathing only
    // changes real intersections where more than one forward road is available.
    if (!f || !road_tiles || vanilla_direction < 0 || vanilla_direction >= 8) {
        return vanilla_direction;
    }

    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->profile) {
        return vanilla_direction;
    }

    const figure_type_registry_impl::PathingPolicy &pathing = entry->profile->pathing_policy();
    const road_service_effect effect = figure_runtime_native_impl::primary_service_effect_for_profile(entry->profile);
    if (pathing.mode != &figure_type_registry_impl::SmartService ||
        effect == ROAD_SERVICE_EFFECT_NONE) {
        return vanilla_direction;
    }

    int candidate_count = 0;
    uint32_t best_stamp = std::numeric_limits<uint32_t>::max();
    int best_direction = vanilla_direction;
    for (int direction = 0; direction < 8; direction += 2) {
        if (!road_tiles[direction] || direction == came_from_direction) {
            continue;
        }

        candidate_count++;
        const int target_grid_offset = f->grid_offset + map_grid_direction_delta(direction);
        const uint32_t stamp = map_road_service_history_get(effect, target_grid_offset);
        if (stamp < best_stamp) {
            best_stamp = stamp;
            best_direction = direction;
        } else if (stamp == best_stamp && direction == vanilla_direction) {
            best_direction = direction;
        }
    }

    if (candidate_count <= 1) {
        return vanilla_direction;
    }

    // Reserve the chosen target immediately so another same-service walker
    // choosing later in the same tick does not lockstep onto the same road.
    const int reserved_grid_offset = f->grid_offset + map_grid_direction_delta(best_direction);
    record_service_history(effect, reserved_grid_offset);
    return best_direction;
}

void figure_runtime_record_road_service_visit(Figure *f)
{
    // Recording is pathing telemetry only. Coverage/risk systems still use the
    // existing service callbacks; this history only informs future road choices.
    if (!f || !figure_runtime_native_impl::is_road_history_tile(f->grid_offset)) {
        return;
    }

    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->profile) {
        return;
    }

    const road_service_effect effect = figure_runtime_native_impl::primary_service_effect_for_profile(entry->profile);
    if (effect == ROAD_SERVICE_EFFECT_NONE) {
        return;
    }

    record_service_history(effect, f->grid_offset);
}
