#include "building/building.h"
#include "building/list.h"
#include "building/building_runtime.h"
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
#include "core/log.h"
#include "figure/figure_runtime_native.h"
#include "figure/figure_type_registry_internal.h"
#include "map/road_service_history.h"

#include "building/building_record.h"
#include "core/buffer.h"
#include "building/monument.h"
#include "city/figures.h"
#include "core/calc.h"
#include "core/image.h"
#include "figure/combat.h"
#include "figure/enemy_army.h"
#include "figure/FigureGraphics.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "game/time.h"
#include "map/grid.h"
#include "map/terrain.h"
#include "scenario/gladiator_revolt.h"
#include "sound/effect.h"

#include <cstdint>
#include <exception>
#include <limits>
#include <memory>
#include <vector>

namespace figure_runtime_native_impl {
bool update_legacy_figure_graphics_image_state(
    Figure &figure,
    const figure_type_registry_impl::FigureTypeDefinition *definition);
}

namespace {

struct RuntimeEntry {
    Figure *data = nullptr;
    unsigned short created_sequence = 0;
    const figure_type_registry_impl::FigureTypeDefinition *definition = nullptr;
    const figure_type_registry_impl::FigureTypeProfile *profile = nullptr;
    figure_type_registry_impl::FigureGraphicsState graphics_state;
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

const char *profile_id_for_priest_owner(const Figure *f)
{
    if (!f || !f->building) {
        return nullptr;
    }

    // Legacy saves do not persist XML profile bindings. Priest recovery keeps
    // the old temple-to-god mapping, then the recovered profile owns the effect.
    const building_type_registry_impl::BuildingType *type = f->building->type;
    if (!type) {
        return nullptr;
    }
    if (type->is_temple(GOD_ALL, building_type_registry_impl::ReligionTier::Grand)) {
        return "pantheon_service";
    }
    if (type->is_temple(GOD_CERES)) {
        return "ceres_service";
    }
    if (type->is_temple(GOD_NEPTUNE)) {
        return "neptune_service";
    }
    if (type->is_temple(GOD_MERCURY)) {
        return "mercury_service";
    }
    if (type->is_temple(GOD_MARS)) {
        return "mars_service";
    }
    if (type->is_temple(GOD_VENUS)) {
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
        case FIGURE_FISHING_BOAT:
            return "fish_fetch";
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
                    if (!f->building) {
                        return nullptr;
                    }
                    const building_type_registry_impl::BuildingType *building_type = f->building->type;
                    if (f->type == FIGURE_ACTOR) {
                        return building_type && building_type->attr_is("amphitheater") ?
                            "amphitheater_service" :
                            "theater_service";
                    }
                    if (f->type == FIGURE_GLADIATOR) {
                        if (building_type && building_type->attr_is("arena")) {
                            return "arena_service";
                        }
                        if (building_type && building_type->attr_is("colosseum")) {
                            return "colosseum_service";
                        }
                        return "amphitheater_service";
                    }
                    if (f->type == FIGURE_LION_TAMER) {
                        return building_type && building_type->attr_is("colosseum") ?
                            "colosseum_service" :
                            "arena_service";
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

    const figure_type type = static_cast<figure_type>(f->type);
    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(type);
    const figure_type_registry_impl::FigureGraphics *graphics =
        figure_type_registry_impl::FigureGraphics::for_type(type);
    const bool figure_changed = !entry_matches_figure(*entry, f);
    if (figure_changed) {
        *entry = RuntimeEntry();
        entry->data = f;
        entry->created_sequence = f->created_sequence;
    }
    entry->graphics_state.bind(*f, graphics);

    if (!definition) {
        entry->definition = nullptr;
        entry->profile = nullptr;
        entry->controller.reset();
        return graphics ? entry : nullptr;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = entry->profile;
    if (figure_changed || entry->definition != definition || !profile) {
        profile = definition->profile(infer_profile_id(f));
    }
    if (!profile) {
        profile = definition->default_profile();
    }

    if (figure_changed ||
        entry->definition != definition ||
        entry->profile != profile) {
        entry->data = f;
        entry->created_sequence = f->created_sequence;
        entry->definition = definition;
        entry->profile = profile;
        entry->controller = figure_runtime_native_impl::make_controller(f, definition, profile);
    } else {
        entry->data = f;
        if (entry->controller) {
            entry->controller->set_figure(f);
        }
    }

    if (!entry->profile) {
        entry->controller.reset();
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

const figure_type_registry_impl::FigureTypeProfile *profile_for_figure(const Figure *f)
{
    if (!f) {
        return nullptr;
    }
    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    if (!definition) {
        return nullptr;
    }
    const figure_type_registry_impl::FigureTypeProfile *profile = definition->profile(infer_profile_id(f));
    return profile ? profile : definition->default_profile();
}

void log_loaded_owner_repair(
    const char *message,
    const Figure &figure,
    const figure_type_registry_impl::FigureTypeProfile *profile,
    unsigned int saved_owner_id)
{
    char detail[256];
    snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u profile=%s saved_owner_id=%u",
        figure.id(),
        static_cast<unsigned int>(figure.type),
        profile ? profile->id() : "<none>",
        saved_owner_id);
    log_warning(message, detail, 0);
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

static unsigned int known_figure_id_for_pointer(const Figure *figure)
{
    if (!figure) {
        return 0;
    }
    for (unsigned int id = 1; id < Figure::count(); ++id) {
        if (Figure::get(id) == figure) {
            return id;
        }
    }
    return 0;
}

} // namespace

void figure_runtime_debug_dump(FILE *file)
{
    if (!file) {
        return;
    }

    fprintf(file, "  \"figure_runtime\": [\n");
    int wrote = 0;
    for (size_t slot = 0; slot < g_runtime_entries.size(); ++slot) {
        const RuntimeEntry &entry = g_runtime_entries[slot];
        if (!entry.data && !entry.definition && !entry.profile && !entry.controller) {
            continue;
        }

        if (wrote++) {
            fprintf(file, ",\n");
        }

        fprintf(file, "    {\n");
        fprintf(file, "      \"slot\": %zu,\n", slot);
        fprintf(file, "      \"figure_pointer\": ");
        write_debug_pointer(file, entry.data);
        fprintf(file, ",\n");
        fprintf(file, "      \"known_figure_id\": %u,\n", known_figure_id_for_pointer(entry.data));
        fprintf(file, "      \"created_sequence\": %u,\n", entry.created_sequence);
        fprintf(file, "      \"definition_attr\": ");
        write_debug_json_string(file, entry.definition ? entry.definition->attr() : nullptr);
        fprintf(file, ",\n");
        fprintf(file, "      \"profile_id\": ");
        write_debug_json_string(file, entry.profile ? entry.profile->id() : nullptr);
        fprintf(file, ",\n");
        fprintf(file, "      \"has_controller\": %d\n", entry.controller ? 1 : 0);
        fprintf(file, "    }");
    }
    fprintf(file, "\n  ]");
}

void figure_runtime_reset()
{
    g_runtime_entries.clear();
}

bool figure_runtime_resolve_loaded_owner(Figure *f, unsigned int saved_owner_id, Building **resolved_owner)
{
    if (resolved_owner) {
        *resolved_owner = nullptr;
    }
    if (!f) {
        return false;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = profile_for_figure(f);
    if (profile && !profile->requires_owner()) {
        if (saved_owner_id) {
            log_loaded_owner_repair(
                "Discarding saved owner reference for ownerless figure",
                *f,
                profile,
                saved_owner_id);
        }
        return true;
    }

    Building *owner = saved_owner_id ? Building::get(saved_owner_id) : nullptr;
    if (!profile) {
        if (saved_owner_id && !owner) {
            log_loaded_owner_repair("Discarding invalid saved figure owner reference", *f, nullptr, saved_owner_id);
        }
        if (resolved_owner) {
            *resolved_owner = owner;
        }
        return true;
    }

    const building *owner_record = owner ? owner->record() : nullptr;
    if (!owner_record ||
        !figure_runtime_native_impl::owner_binding_matches(f, owner_record, profile->owner_binding())) {
        log_loaded_owner_repair("Removing loaded figure with invalid required owner", *f, profile, saved_owner_id);
        return false;
    }

    if (resolved_owner) {
        *resolved_owner = owner;
    }
    return true;
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

static int figure_runtime_bind_profile(Figure *f, const char *profile_id)
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
    entry->graphics_state.bind(
        *f,
        figure_type_registry_impl::FigureGraphics::for_type(static_cast<figure_type>(f->type)));
    entry->controller = figure_runtime_native_impl::make_controller(f, definition, profile);
    return profile->native_class() == figure_type_registry_impl::NativeClassId::LegacyAction ||
        entry->controller ? 1 : 0;
}

Figure *figure_runtime_create_profiled(
    figure_type type,
    int x,
    int y,
    direction_type dir,
    const Building &building,
    const char *profile_id)
{
    const figure_type_registry_impl::FigureTypeProfile *profile =
        figure_type_registry_impl::profile_for(type, profile_id);
    if (!profile) {
        // BuildingType spawn profile references are validated during FigureType
        // load; callers still fail closed if a mod removes a profile later.
        return nullptr;
    }

    Building &owner = building.runtime_instance()->building;

    Figure *f = Figure::create(type, x, y, dir);

    f->set_home_building(profile->requires_owner() ? &owner : nullptr);
    const figure_type_registry_impl::ProfileSpawnBehavior spawn_behavior = profile->spawn_behavior();
    if (spawn_behavior.has_action_state) {
        f->action_state = static_cast<unsigned char>(spawn_behavior.action_state);
    }
    if (spawn_behavior.init_roaming) {
        figure_movement_init_roaming(f);
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

int figure_runtime_apply_profile_movement(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->profile) {
        ErrorContextScope scope("FigureType profile movement");
        error_context_report_fatal_error_dialog(
            "Figure runtime error",
            "Figure has no XML movement profile.",
            "A legacy action walker attempted to read profile-owned movement data, but no FigureType profile was bound.");
        std::terminate();
    }

    const figure_type_registry_impl::MovementProfile &movement = entry->profile->movement_profile();
    f->terrain_usage = static_cast<unsigned char>(entry->profile->pathing_policy().terrain.legacy_usage);
    f->use_cross_country = 0;
    f->max_roam_length = static_cast<short>(movement.max_roam_length);
    return 1;
}

const figure_type_registry_impl::PathingPolicy *figure_runtime_pathing_policy(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    return entry && entry->profile ? &entry->profile->pathing_policy() : nullptr;
}

roadblock_permission figure_runtime_roadblock_permission(Figure *f)
{
    const figure_type_registry_impl::PathingPolicy *pathing = figure_runtime_pathing_policy(f);
    if (!f || !pathing) {
        return f ? Roadblock::permission_for(*f) : PERMISSION_NONE;
    }
    return pathing->roadblockPermissionFor(*f);
}

figure_type_registry_impl::PathingMode::RoutePolicySelection figure_runtime_route_policy_selection(
    Figure *f,
    RouteNeighborhood neighborhood)
{
    using figure_type_registry_impl::PathingMode;
    PathingMode::RoutePolicySelection selection;
    const figure_type_registry_impl::PathingPolicy *pathing = figure_runtime_pathing_policy(f);
    if (pathing && f) {
        return pathing->routePolicySelection(pathing->roadblockPermissionFor(*f), neighborhood);
    }
    const roadblock_permission permission = f ? Roadblock::permission_for(*f) : PERMISSION_NONE;
    selection.terrain = PathingMode::terrainFromLegacyUsage(f ? f->terrain_usage : TERRAIN_USAGE_ANY);
    selection.policy = PathingMode::routePolicyForTerrain(selection.terrain, permission, neighborhood);
    return selection;
}

int figure_runtime_update_graphics(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (!entry || !entry->definition) {
        return 0;
    }
    return figure_runtime_native_impl::update_legacy_figure_graphics_image_state(
        *f,
        entry->definition) ? 1 : 0;
}

void figure_runtime_graphics_begin_update(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.begin_cart_update();
    }
}

void figure_runtime_graphics_show_empty_cart(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.show_empty_cart();
    }
}

void figure_runtime_graphics_show_resource_cart(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.show_resource_cart();
    }
}

void figure_runtime_graphics_hide_cart(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.hide_cart();
    }
}

const figure_type_registry_impl::FigureGraphicsState *figure_runtime_graphics_state(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    return entry ? &entry->graphics_state : nullptr;
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
