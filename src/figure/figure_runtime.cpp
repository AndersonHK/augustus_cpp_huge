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
#include <cstring>
#include <exception>
#include <limits>
#include <memory>
#include <vector>

namespace {

struct RuntimeEntry {
    Figure *data = nullptr;
    unsigned short created_sequence = 0;
    figure_type type = FIGURE_NONE;
    const figure_type_registry_impl::FigureTypeDefinition *definition = nullptr;
    const figure_type_registry_impl::FigureGraphics *graphics = nullptr;
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

const char *profile_id_for_priest_owner(const Building *owner)
{
    if (!owner) {
        return nullptr;
    }

    const building_type_registry_impl::BuildingType *type = owner->type;
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

const char *translate_legacy_profile_id(const Figure *f, const Building *owner)
{
    if (!f) {
        return nullptr;
    }

    // This is used only while translating explicitly versioned pre-profile-identity saves.
    switch (f->type) {
        case FIGURE_LABOR_SEEKER:
            return f->collecting_item_id ? "validation" : "acquisition";
        case FIGURE_PRIEST:
            return profile_id_for_priest_owner(owner);
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
                    if (!owner) {
                        return nullptr;
                    }
                    const building_type_registry_impl::BuildingType *building_type = owner->type;
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
    if (!f || !f->id()) {
        return nullptr;
    }

    RuntimeEntry *entry = entry_for_id(f->id());
    if (!entry) {
        return nullptr;
    }

    const figure_type type = static_cast<figure_type>(f->type);
    const char *profile_id = f->runtime_profile_id();
    if (entry_matches_figure(*entry, f) && entry->type == type && entry->graphics &&
        ((!entry->profile && (!profile_id || !*profile_id)) ||
            (entry->profile && profile_id && std::strcmp(entry->profile->id(), profile_id) == 0))) {
        return entry;
    }
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
    entry->type = type;
    entry->graphics = graphics;
    entry->graphics_state.bind(*f, graphics);

    if (!definition) {
        entry->definition = nullptr;
        entry->profile = nullptr;
        entry->controller.reset();
        return graphics ? entry : nullptr;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = entry->profile;
    if (figure_changed || entry->definition != definition || !profile) {
        profile = profile_id && *profile_id ? definition->profile(profile_id) : definition->default_profile();
        if (profile && (!profile_id || !*profile_id) && !f->set_runtime_profile_id(profile->id())) {
            profile = nullptr;
        }
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

void log_loaded_owner_error(const char *message, const Figure &figure, const figure_type_registry_impl::FigureTypeProfile *profile, unsigned int saved_owner_id)
{
    const Building *owner = saved_owner_id ? Building::get(saved_owner_id) : nullptr;
    const building *record = owner ? owner->record() : nullptr;
    char detail[384];
    snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u profile=%s saved_owner_id=%u owner_type=%d owner_state=%d owner_primary=%u owner_secondary=%u owner_quaternary=%u", figure.id(), static_cast<unsigned int>(figure.type), profile ? profile->id() : "<none>", saved_owner_id, record ? record->type : 0, record ? record->state : 0, record ? record->figure_id : 0, record ? record->figure_id2 : 0, record ? record->figure_id4 : 0);
    log_error(message, detail, 0);
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

bool figure_runtime_resolve_loaded_owner(Figure *f, unsigned int saved_owner_id, bool allow_legacy_profile_translation, bool allow_legacy_owner_reference_repair, bool allow_delayed_owner_binding_bridge, bool allow_land_trade_profile_bridge, Building **resolved_owner)
{
    if (resolved_owner) {
        *resolved_owner = nullptr;
    }
    if (!f) {
        return false;
    }

    if (!f->state) return true;

    const figure_type_registry_impl::FigureTypeDefinition *definition = figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    // Original figures can still have legacy controllers/graphics without a
    // FigureType declaration. Only later mod figures require that declaration.
    if (f->type >= FIGURE_WORK_CAMP_WORKER && !definition && !figure_type_registry_impl::graphics_for(static_cast<figure_type>(f->type))) {
        char detail[256];
        snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u saved_owner_id=%u", f->id(), static_cast<unsigned int>(f->type), saved_owner_id);
        log_warning("Discarding imported transient figure absent from the active mod definitions", detail, 0);
        f->remove();
        return true;
    }

    Building *owner = saved_owner_id ? Building::get(saved_owner_id) : nullptr;
    if (saved_owner_id && (!owner || !owner->id)) {
        if (!allow_legacy_owner_reference_repair && !allow_delayed_owner_binding_bridge) {
            log_loaded_owner_error("Figure has an invalid serialized owner reference", *f, nullptr, saved_owner_id);
            return false;
        }
        char detail[256];
        snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u saved_owner_id=%u", f->id(), static_cast<unsigned int>(f->type), saved_owner_id);
        log_warning("Repairing invalid serialized figure owner reference", detail, 0);
        owner = nullptr;
        saved_owner_id = 0;
    }

    const figure_type_registry_impl::FigureTypeProfile *profile = nullptr;
    const char *profile_id = f->runtime_profile_id();
    if (definition && !definition->profiles().empty()) {
        const auto *default_profile = definition->default_profile();
        const bool bridge_trade = allow_land_trade_profile_bridge && default_profile && (default_profile->native_class() == figure_type_registry_impl::NativeClassId::LandTrade || default_profile->native_class() == figure_type_registry_impl::NativeClassId::TradeFollower);
        if ((!profile_id || !*profile_id) && (allow_legacy_profile_translation || bridge_trade)) {
            if (bridge_trade) log_warning("Migrating pre-native land trade figure to its explicit XML profile", definition->attr(), f->id());
            const char *translated_id = translate_legacy_profile_id(f, owner);
            profile = translated_id ? definition->profile(translated_id) : definition->default_profile();
            if (!profile || !f->set_runtime_profile_id(profile->id())) {
                log_loaded_owner_error("Legacy figure profile identity could not be translated", *f, profile, saved_owner_id);
                return false;
            }
        } else if (profile_id && *profile_id) {
            profile = definition->profile(profile_id);
        }
        if (!profile) {
            log_loaded_owner_error("Figure has a missing or invalid serialized profile identity", *f, nullptr, saved_owner_id);
            return false;
        }
    } else if (profile_id && *profile_id) {
        log_loaded_owner_error("Figure serialized a profile identity without a runtime profile definition", *f, nullptr, saved_owner_id);
        return false;
    }

    if (profile && profile->requires_owner() && !owner && allow_delayed_owner_binding_bridge) {
        Building *slot_owner = nullptr;
        bool ambiguous = false;
        Building::for_each([&](Building *candidate) {
            if (!candidate || !candidate->record() ||
                !figure_runtime_native_impl::owner_binding_matches(f, candidate->record(), profile->owner_binding())) {
                return;
            }
            if (slot_owner) {
                ambiguous = true;
                return;
            }
            slot_owner = candidate;
        });
        if (slot_owner && !ambiguous) {
            owner = slot_owner;
            saved_owner_id = static_cast<unsigned int>(slot_owner->id);
            char detail[256];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u profile=%s owner_id=%u", f->id(), static_cast<unsigned int>(f->type), profile->id(), saved_owner_id);
            log_warning("Migrating delayed figure owner binding from its exact building slot", detail, 0);
        }
    }

    if (profile && !profile->requires_owner()) {
        if (!saved_owner_id) {
            return true;
        }
        if (owner && figure_runtime_native_impl::owner_binding_matches(f, owner->record(), profile->owner_binding())) {
            if (resolved_owner) {
                *resolved_owner = owner;
            }
            return true;
        }
        if (allow_legacy_owner_reference_repair) {
            char detail[256];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u profile=%s saved_owner_id=%u", f->id(), static_cast<unsigned int>(f->type), profile->id(), saved_owner_id);
            log_warning("Discarding invalid optional owner reference from legacy figure profile", detail, 0);
            return true;
        }
        log_loaded_owner_error("Optional figure owner reference failed save validation", *f, profile, saved_owner_id);
        return false;
    }

    if (!profile) {
        if (resolved_owner) {
            *resolved_owner = owner;
        }
        return true;
    }

    const building *owner_record = owner ? owner->record() : nullptr;
    if (!owner_record ||
        !figure_runtime_native_impl::owner_binding_matches(f, owner_record, profile->owner_binding())) {
        if (allow_legacy_owner_reference_repair || allow_delayed_owner_binding_bridge) {
            char detail[320];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u profile=%s saved_owner_id=%u", f->id(), static_cast<unsigned int>(f->type), profile->id(), saved_owner_id);
            log_warning("Discarding legacy transient figure whose required owner cannot be recovered", detail, 0);
            f->remove();
            return true;
        }
        log_loaded_owner_error("Figure required-owner invariant failed during save validation", *f, profile, saved_owner_id);
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
        figure_action_refresh_graphics(f);
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
    if (!f->set_runtime_profile_id(profile->id())) {
        return 0;
    }

    RuntimeEntry *entry = entry_for_id(f->id());
    if (!entry) {
        return 0;
    }

    entry->data = f;
    entry->created_sequence = f->created_sequence;
    entry->type = static_cast<figure_type>(f->type);
    entry->definition = definition;
    entry->profile = profile;
    entry->graphics = figure_type_registry_impl::FigureGraphics::for_type(entry->type);
    entry->graphics_state.bind(*f, entry->graphics);
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
    const int terrain_usage = entry->profile->pathing_policy().activeTerrain().legacy_usage;
    if (f->terrain_usage != terrain_usage && entry->profile->pathing_policy().terrain_setting != CONFIG_MAX_ENTRIES) Route::remove(f);
    f->terrain_usage = static_cast<unsigned char>(terrain_usage);
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

void figure_runtime_graphics_begin_update(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.begin_update();
    }
}

void figure_runtime_graphics_select_default_entry(Figure *f, const char *image_id)
{
    figure_runtime_graphics_select_default_entry_frame(f, image_id, 0);
}

void figure_runtime_graphics_select_default_entry_frame(Figure *f, const char *image_id, int one_based_frame)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry && image_id && *image_id && one_based_frame >= 0) {
        entry->graphics_state.select_default_entry(image_id, one_based_frame);
    }
}

void figure_runtime_graphics_set_default_offset(Figure *f, int x, int y)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) entry->graphics_state.set_default_offset({ x, y });
}

void figure_runtime_graphics_add_required_layer(Figure *f, const char *role, const char *image_id, int one_based_frame, int x, int y, int draw_before_base)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry && role && *role && image_id && *image_id && one_based_frame >= 0) {
        entry->graphics_state.add_required_layer(role, image_id, one_based_frame, { x, y }, draw_before_base);
    }
}

void figure_runtime_graphics_select_directional_entry_frame(Figure *f, const char *state_id, int direction, int one_based_frame)
{
    if (!state_id || !*state_id) return;
    std::string image_id = state_id;
    image_id += '_';
    image_id += graphics_direction8_suffix(figure_image_normalize_direction(direction));
    figure_runtime_graphics_select_default_entry_frame(f, image_id.c_str(), one_based_frame);
}

void figure_runtime_graphics_select_corpse_entry(Figure *f, const char *image_id)
{
    if (!f) return;
    figure_runtime_graphics_select_default_entry_frame(f, image_id, figure_type_registry_impl::FigureGraphics::corpse_frame_for_wait_ticks(f->wait_ticks) + 1);
}

void figure_runtime_graphics_hide_default_entry(Figure *f)
{
    RuntimeEntry *entry = bind_entry(f);
    if (entry) {
        entry->graphics_state.hide_default_entry();
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
