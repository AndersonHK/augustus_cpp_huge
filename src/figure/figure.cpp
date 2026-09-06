#include "city/trade_ledger.h"
#include "figure/figure.h"

#include "assets/assets.h"
#include "building/building_runtime.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/emperor.h"
#include "city/race_bet.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "figure/combat.h"
#include "figure/image.h"
#include "figure/figure_runtime_api.h"
#include "figure/figure_runtime_native.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/formation.h"
#include "figure/movement.h"
#include "figure/name.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "figuretype/animal.h"
#include "figuretype/cartpusher.h"
#include "figuretype/depot.h"
#include "figuretype/enemy.h"
#include "figuretype/fishing_boat.h"
#include "figuretype/supplier.h"
#include "figuretype/trader.h"
#include "figuretype/water.h"
#include "figuretype/workcamp.h"
#include "game/resource.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/ui_constants.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_service_history.h"
#include "scenario/property.h"
#include "window/building/common.h"

#include <cstdlib>
#include <algorithm>
#include <exception>
#include <cstring>
#include <memory>
#include <vector>

#define CAMEL_PORTRAIT 59

static const int FIGURE_TYPE_TO_BIG_FIGURE_IMAGE[] = {
    8, 4, 4, 9, 51, 13, 8, 16, 7, 4, // 0-9
    18, 42, 26, 41, 8, 1, 33, 10, 11, 25, //10-19
    61, 25, 15, 15, 15, 60, 12, 14, 5, 52, //20-29
    52, 2, 3, 6, 6, 13, 8, 8, 17, 12, //30-39
    58, 21, 50, 8, 8, 8, 28, 30, 23, 8, //40-49
    8, 8, 34, 39, 33, 43, 27, 48, 63, 8, //50-59
    8, 8, 8, 8, 53, 8, 38, 62, 54, 55, //60-69
    56, 8, 8, 58, 0, 7, 50, 0, 14, 3, //70-79
    3, 58, 50, 0, 0, 3, 15, 15, 0, 51, //80-89
    0, 0, 7, 17, 7, 0, 0, 0, 0, 0, 0, //90-99
};

static const translation_key NEW_FIGURE_TYPES[] = {
    "TR_FIGURE_TYPE_WORK_CAMP_WORKER", "TR_FIGURE_TYPE_WORK_CAMP_SLAVE", "TR_FIGURE_TYPE_WORK_CAMP_ARCHITECT",
    "TR_FIGURE_TYPE_MESS_HALL_SUPPLIER", "TR_FIGURE_TYPE_MESS_HALL_COLLECTOR", "TR_FIGURE_TYPE_PRIEST_SUPPLIER",
    "TR_FIGURE_TYPE_BARKEEP", "TR_FIGURE_TYPE_BARKEEP_SUPPLIER", "TR_FIGURE_TYPE_TOURIST",
    "TR_FIGURE_TYPE_WATCHMAN", {}, {}, "TR_FIGURE_TYPE_CARAVANSERAI_SUPPLIER", "TR_FIGURE_TYPE_ROBBER",
    "TR_FIGURE_TYPE_LOOTER", "TR_FIGURE_TYPE_CARAVANSERAI_COLLECTOR", "TR_FIGURE_TYPE_LIGHTHOUSE_SUPPLIER",
    "TR_FIGURE_TYPE_MESS_HALL_COLLECTOR", {}, "TR_BUILDING_FORT_AUXILIA_INFANTRY", "TR_FIGURE_TYPE_BEGGAR",
    "TR_BUILDING_FORT_ARCHERS", {}, {}
};

static ImageGroupEntryRef big_people_image_ref(figure_type type)
{
    if (const auto *definition = figure_type_registry_impl::definition_for(type); definition && !definition->portrait().group_path().empty()) return definition->portrait();
    type = figure_type_base(type);
    switch (type) {
        case FIGURE_WORK_CAMP_SLAVE:
            return ImageGroupEntryRef::from_group("Walkers\\Slave_Portrait", "Slave Portrait");
        case FIGURE_CARAVANSERAI_SUPPLIER:
            return ImageGroupEntryRef::from_group("Walkers\\caravanserai_overseer_portrait", "caravanserai_overseer_portrait");
        case FIGURE_CARAVANSERAI_COLLECTOR:
            return ImageGroupEntryRef::from_group("Walkers\\caravanserai_walker_portrait", "caravanserai_walker_portrait");
        case FIGURE_MESS_HALL_COLLECTOR:
        case FIGURE_MESS_HALL_FORT_SUPPLIER:
            return ImageGroupEntryRef::from_group("Walkers\\M_Hall_Portrait", "M Hall Portrait");
        case FIGURE_BARKEEP:
        case FIGURE_BARKEEP_SUPPLIER:
            return ImageGroupEntryRef::from_group("Walkers\\Barkeep_Portrait", "Barkeep Portrait");
        case FIGURE_WORK_CAMP_ARCHITECT:
            return ImageGroupEntryRef::from_group("Walkers\\architect_portrait", "architect_portrait");
        case FIGURE_WORK_CAMP_WORKER:
            return ImageGroupEntryRef::from_group("Walkers\\overseer_portrait", "overseer_portrait");
        case FIGURE_MESS_HALL_SUPPLIER:
            return ImageGroupEntryRef::from_group("Walkers\\quartermaster_portrait", "quartermaster_portrait");
        default:
            return ImageGroupEntryRef();
    }
}

namespace {

constexpr int kFigureArraySizeStep = 1000;
constexpr int kFigureOriginalBufferSize = 128;
constexpr int kFigureLastNoExactProfileIdentityBufferSize = 184;
constexpr int kFigureExactProfileBufferSize = kFigureLastNoExactProfileIdentityBufferSize + static_cast<int>(FIGURE_RUNTIME_PROFILE_ID_CAPACITY);
constexpr int kFigureCurrentBufferSize = kFigureExactProfileBufferSize + FIGURE_TYPE_IDENTITY_CAPACITY;

static int32_t migrate_legacy_cross_country_unit(int value)
{
    const int64_t scaled = static_cast<int64_t>(value) * FIGURE_CROSS_COUNTRY_TILE_UNITS;
    return static_cast<int32_t>(scaled >= 0 ? (scaled + FIGURE_LEGACY_TILE_PROGRESS_MAX / 2) / FIGURE_LEGACY_TILE_PROGRESS_MAX : (scaled - FIGURE_LEGACY_TILE_PROGRESS_MAX / 2) / FIGURE_LEGACY_TILE_PROGRESS_MAX);
}

static bool figure_progress_is_action_timer(const Figure &f)
{
    if (f.action_state == FIGURE_ACTION_150_ATTACK) {
        return true;
    }
    switch (f.type) {
        case FIGURE_EXPLOSION:
        case FIGURE_ARROW:
        case FIGURE_JAVELIN:
        case FIGURE_BOLT:
        case FIGURE_FISH_GULLS:
        case FIGURE_SPEAR:
        case FIGURE_FRIENDLY_ARROW:
        case FIGURE_CATAPULT_MISSILE:
            return true;
        default:
            return false;
    }
}
// around 12 bytes left free in the current buffer size - save version 0xa7, August 2025

struct FigureStore {
    int created_sequence = 0;
    std::vector<std::unique_ptr<Figure>> figures;
    struct PendingBuildingRefs {
        unsigned int building_id = 0;
        unsigned int immigrant_building_id = 0;
        unsigned int destination_building_id = 0;
    };
    struct PendingFigureRefs {
        unsigned int target_figure_id = 0;
        unsigned int targeted_by_figure_id = 0;
        unsigned int attacker1_id = 0;
        unsigned int attacker2_id = 0;
        unsigned int opponent_id = 0;
    };
    std::vector<PendingBuildingRefs> pending_building_refs;
    std::vector<PendingFigureRefs> pending_figure_refs;
};

FigureStore data;

Figure *invalid_figure()
{
    if (data.figures.empty()) {
        data.figures.push_back(std::make_unique<Figure>(0));
    }
    return data.figures.front().get();
}

Building *loaded_building_ref(unsigned int id)
{
    return Building::get(id);
}

bool loaded_optional_building_ref(
    const Figure &figure,
    unsigned int &id,
    const char *relation,
    int save_version,
    Building **out)
{
    *out = nullptr;
    Building *building = loaded_building_ref(id);
    if (id && !building) {
        char detail[256];
        snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u relation=%s saved_building_id=%u",
            figure.id(),
            static_cast<unsigned int>(figure.type),
            relation ? relation : "<unknown>",
            id);
        if (save_version < SAVE_GAME_CURRENT_VERSION) {
            log_warning("Clearing invalid legacy figure building reference", detail, 0);
            id = 0;
            return true;
        }
        log_error("Loaded save contains an invalid figure building reference", detail, 0);
        return false;
    }
    *out = building;
    return true;
}

void store_pending_building_refs(unsigned int figure_id, unsigned int building_id, unsigned int immigrant_building_id, unsigned int destination_building_id)
{
    if (data.pending_building_refs.size() <= figure_id) {
        data.pending_building_refs.resize(figure_id + 1);
    }
    data.pending_building_refs[figure_id] = { building_id, immigrant_building_id, destination_building_id };
}

bool resolve_loaded_figure_relation(Figure &owner, FigureRelation &relation, unsigned int id, const char *role)
{
    if (!id) {
        relation.clear();
        return true;
    }
    Figure *target = id < Figure::count() ? Figure::get(id) : nullptr;
    if (!target || target->id() != id || !target->state) {
        char detail[256];
        snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u relation=%s saved_figure_id=%u",
            owner.id(), static_cast<unsigned int>(owner.type), role, id);
        log_warning("Clearing stale saved figure relationship", detail, 0);
        relation.clear();
        return false;
    }
    relation.retarget(*target);
    return true;
}

int get_resource_id(figure_type type, int resource)
{
    switch (type) {
        case FIGURE_PRIEST_SUPPLIER:
        case FIGURE_BARKEEP_SUPPLIER:
        case FIGURE_MESS_HALL_SUPPLIER:
        case FIGURE_MARKET_SUPPLIER:
        case FIGURE_CARAVANSERAI_SUPPLIER:
        case FIGURE_PRIEST:
        case FIGURE_DELIVERY_BOY:
        case FIGURE_MESS_HALL_COLLECTOR:
        case FIGURE_CARAVANSERAI_COLLECTOR:
            return resource_map_legacy_inventory(resource);
        default:
            return resource_remap(resource);
    }
}

void clear_known_building_refs_for_figure(Figure &figure)
{
    if (figure.building) {
        figure.building->clear_figure_slot_if_matches(figure.id());
    }
    if (figure.destination_building) {
        figure.destination_building->clear_figure_slot_if_matches(figure.id());
    }
    if (figure.immigrant_building && figure.immigrant_building->Housing) {
        figure.immigrant_building->Housing->clear_immigrant_if_matches(figure);
    }
}

bool last_destination_is_figure_id(figure_type type)
{
    switch (type) {
        case FIGURE_ARROW:
        case FIGURE_JAVELIN:
        case FIGURE_FRIENDLY_ARROW:
        case FIGURE_BOLT:
        case FIGURE_CATAPULT_MISSILE:
        case FIGURE_SPEAR:
            return true;
        default:
            return false;
    }
}

Figure *loaded_live_figure(unsigned int figure_id)
{
    if (!figure_id || figure_id >= Figure::count()) {
        return nullptr;
    }
    Figure *figure = Figure::get(figure_id);
    return figure && figure->id() == figure_id && figure->state == FIGURE_STATE_ALIVE ? figure : nullptr;
}

int loaded_figure_is_owned_by(unsigned int figure_id, const Building &owner)
{
    Figure *figure = loaded_live_figure(figure_id);
    return figure && figure->home_building_id() == static_cast<unsigned int>(owner.id);
}

void repair_loaded_building_figure_slots()
{
    Building::for_each([](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (!b) return;

        const auto clear_invalid_relation = [building](unsigned int &figure_id, const char *relation) {
            Figure *figure = loaded_live_figure(figure_id);
            const unsigned int building_id = building->id;
            if (!figure_id || (figure && (figure->home_building_id() == building_id || figure->immigrant_building_id() == building_id || figure->destination_building_id() == building_id))) return;
            char detail[256];
            snprintf(detail, sizeof(detail), "building_id=%u relation=%s saved_figure_id=%u", building_id, relation, figure_id);
            log_warning("Repairing stale building figure slot from saved runtime state", detail, 0);
            figure_id = 0;
        };
        clear_invalid_relation(b->figure_id, "figure_id");
        clear_invalid_relation(b->figure_id2, "figure_id2");
        clear_invalid_relation(b->figure_id4, "figure_id4");

        const bool is_dock = building->matches("dock");
        const bool is_depot = building->matches("depot");
        if (is_dock || is_depot) {
            for (unsigned int &cartpusher_id : b->data.distribution.cartpusher_ids) {
                Figure *cart = loaded_live_figure(cartpusher_id);
                const figure_type expected_type = is_dock ? FIGURE_DOCKER : FIGURE_DEPOT_CART_PUSHER;
                if (!cartpusher_id || (cart && cart->type == expected_type && loaded_figure_is_owned_by(cartpusher_id, *building))) continue;
                char detail[256];
                snprintf(detail, sizeof(detail), "building_id=%u relation=distribution_cart saved_figure_id=%u", static_cast<unsigned int>(building->id), cartpusher_id);
                log_warning("Repairing stale distribution cart slot from saved runtime state", detail, 0);
                cartpusher_id = 0;
            }
        }
        if (building->matches("wharf")) {
            const auto clear_invalid_boat = [building](unsigned int &figure_id, const char *relation) {
                Figure *boat = loaded_live_figure(figure_id);
                if (!figure_id || (boat && boat->type == FIGURE_FISHING_BOAT && loaded_figure_is_owned_by(figure_id, *building))) return;
                char detail[256];
                snprintf(detail, sizeof(detail), "building_id=%u relation=%s saved_figure_id=%u", static_cast<unsigned int>(building->id), relation, figure_id);
                log_warning("Repairing stale fishing boat slot from saved runtime state", detail, 0);
                figure_id = 0;
            };
            clear_invalid_boat(b->data.industry.fishing_boat_id, "primary_fishing_boat");
            clear_invalid_boat(b->data.industry.second_fishing_boat_id, "secondary_fishing_boat");
        }
    });
}

bool validate_loaded_building_figure_slots()
{
    bool valid = true;
    Building::for_each([&valid](Building *building) {
        if (!valid) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (!b) {
            return;
        }
        const auto validate_relation = [building](unsigned int figure_id, const char *relation) {
            Figure *figure = loaded_live_figure(figure_id);
            const unsigned int building_id = building->id;
            if (figure_id && (!figure ||
                (figure->home_building_id() != building_id &&
                    figure->immigrant_building_id() != building_id &&
                    figure->destination_building_id() != building_id))) {
                char detail[256];
                snprintf(detail, sizeof(detail), "building_id=%u relation=%s saved_figure_id=%u",
                    building_id, relation, figure_id);
                log_error("Loaded save contains an invalid building figure slot", detail, 0);
                return false;
            }
            return true;
        };
        if (!validate_relation(b->figure_id, "figure_id") ||
            !validate_relation(b->figure_id2, "figure_id2") ||
            !validate_relation(b->figure_id4, "figure_id4")) {
            valid = false;
            return;
        }

        const bool is_dock = building->matches("dock");
        const bool is_depot = building->matches("depot");
        if (is_dock || is_depot) {
            for (unsigned int cartpusher_id : b->data.distribution.cartpusher_ids) {
                Figure *cart = loaded_live_figure(cartpusher_id);
                const figure_type expected_type = is_dock ? FIGURE_DOCKER : FIGURE_DEPOT_CART_PUSHER;
                if (cartpusher_id && (!cart || cart->type != expected_type ||
                    !loaded_figure_is_owned_by(cartpusher_id, *building))) {
                    char detail[256];
                    snprintf(detail, sizeof(detail), "building_id=%u relation=distribution_cart saved_figure_id=%u",
                        static_cast<unsigned int>(building->id), cartpusher_id);
                    log_error("Loaded save contains an invalid distribution cart figure slot", detail, 0);
                    valid = false;
                    return;
                }
            }
        }
        if (building->matches("wharf")) {
            Figure *primary_boat = loaded_live_figure(b->data.industry.fishing_boat_id);
            if (b->data.industry.fishing_boat_id && (!primary_boat ||
                primary_boat->type != FIGURE_FISHING_BOAT ||
                !loaded_figure_is_owned_by(b->data.industry.fishing_boat_id, *building))) {
                log_error("Loaded save contains an invalid primary fishing boat slot", 0, building->id);
                valid = false;
                return;
            }
            Figure *secondary_boat = loaded_live_figure(b->data.industry.second_fishing_boat_id);
            if (b->data.industry.second_fishing_boat_id && (!secondary_boat ||
                secondary_boat->type != FIGURE_FISHING_BOAT ||
                !loaded_figure_is_owned_by(b->data.industry.second_fishing_boat_id, *building))) {
                log_error("Loaded save contains an invalid secondary fishing boat slot", 0, building->id);
                valid = false;
            }
        }
    });
    return valid;
}

bool validate_loaded_migrant_house_refs()
{
    bool valid = true;
    Building::for_each(BuildingRuntimeList::Housing, [&valid](Building *house) {
        if (!valid || !house || !house->Housing) {
            return;
        }
        const unsigned int migrant_id = house->Housing->state().immigrant_figure_id;
        if (!migrant_id) {
            return;
        }
        Figure *figure = Figure::get(migrant_id);
        if (!figure || figure->id() != migrant_id || figure->state != FIGURE_STATE_ALIVE ||
            (figure->type != FIGURE_IMMIGRANT && figure->type != FIGURE_HOMELESS)) {
            log_error("Loaded save contains an invalid housing immigrant figure reference", 0, house->id);
            valid = false;
            return;
        }
        if (figure->immigrant_building != house || figure->destination_building != house ||
            figure->last_destination_id != static_cast<int>(house->id)) {
            char detail[256];
            snprintf(detail, sizeof(detail), "house_id=%u saved_figure_id=%u immigrant_id=%u destination_id=%u last_destination_id=%u",
                static_cast<unsigned int>(house->id), migrant_id, figure->immigrant_building_id(),
                figure->destination_building_id(), static_cast<unsigned int>(figure->last_destination_id));
            log_error("Loaded save contains inconsistent migrant and housing references", detail, 0);
            valid = false;
            return;
        }
    });
    return valid;
}

bool repair_loaded_migrant_house_refs(int save_version)
{
    if (save_version >= SAVE_GAME_CURRENT_VERSION) {
        return true;
    }

    bool repaired = true;
    Building::for_each(BuildingRuntimeList::Housing, [&](Building *house) {
        if (!repaired || !house || !house->Housing) {
            return;
        }
        const unsigned int migrant_id = house->Housing->state().immigrant_figure_id;
        if (!migrant_id) {
            return;
        }
        Figure *figure = Figure::get(migrant_id);
        if (!figure || figure->id() != migrant_id || figure->state != FIGURE_STATE_ALIVE ||
            (figure->type != FIGURE_IMMIGRANT && figure->type != FIGURE_HOMELESS) ||
            migrant_id >= data.pending_building_refs.size()) {
            char detail[192];
            snprintf(detail, sizeof(detail), "house_id=%u saved_figure_id=%u", static_cast<unsigned int>(house->id), migrant_id);
            log_warning("Clearing unrecoverable legacy housing immigrant reference", detail, 0);
            house->Housing->clear_immigrant_reference();
            return;
        }

        if (figure->immigrant_building == house && figure->destination_building == house &&
            figure->last_destination_id == static_cast<int>(house->id)) {
            return;
        }

        FigureStore::PendingBuildingRefs &refs = data.pending_building_refs[migrant_id];
        refs.immigrant_building_id = static_cast<unsigned int>(house->id);
        refs.destination_building_id = static_cast<unsigned int>(house->id);
        if (!figure->set_immigrant_building(house) || !figure->set_destination_building(house) ||
            !figure->set_last_destination_building(house)) {
            repaired = false;
            return;
        }
        char detail[256];
        snprintf(detail, sizeof(detail), "house_id=%u figure_id=%u", static_cast<unsigned int>(house->id), migrant_id);
        log_warning("Repairing legacy migrant and housing relationship", detail, 0);
    });
    return repaired;
}

void trim_dead_tail()
{
    while (data.figures.size() > 1 && data.figures.back() && !data.figures.back()->state) {
        data.figures.pop_back();
    }
}

void save_figure(buffer *buf, const Figure &f)
{
    buffer_write_u8(buf, f.alternative_location_index);
    buffer_write_u8(buf, f.image_offset);
    buffer_write_u8(buf, f.is_enemy_image);
    buffer_write_u8(buf, f.flotsam_visible);
    buffer_write_i32(buf, f.image_id);
    buffer_write_i32(buf, f.cart_image_id);
    buffer_write_i16(buf, f.next_figure_id_on_same_tile);
    buffer_write_u8(buf, f.type);
    buffer_write_u8(buf, f.resource_id);
    buffer_write_u8(buf, f.use_cross_country);
    buffer_write_u8(buf, f.is_friendly);
    buffer_write_u8(buf, f.state);
    buffer_write_u8(buf, f.faction_id);
    buffer_write_u8(buf, f.action_state_before_attack);
    buffer_write_i8(buf, f.direction);
    buffer_write_i8(buf, f.previous_tile_direction);
    buffer_write_i8(buf, f.attack_direction);
    buffer_write_u8(buf, f.x);
    buffer_write_u8(buf, f.y);
    buffer_write_u8(buf, f.previous_tile_x);
    buffer_write_u8(buf, f.previous_tile_y);
    buffer_write_u8(buf, f.missile_height);
    buffer_write_u8(buf, f.damage);
    buffer_write_i16(buf, f.grid_offset);
    buffer_write_u8(buf, f.destination_x);
    buffer_write_u8(buf, f.destination_y);
    buffer_write_i16(buf, f.destination_grid_offset);
    buffer_write_u8(buf, f.source_x);
    buffer_write_u8(buf, f.source_y);
    buffer_write_u8(buf, f.formation_position_x.soldier);
    buffer_write_u8(buf, f.formation_position_y.soldier);
    buffer_write_i16(buf, f.disallow_diagonal);
    buffer_write_i16(buf, f.wait_ticks);
    buffer_write_u8(buf, f.action_state);
    buffer_write_u8(buf, f.progress_on_tile);
    buffer_write_u32(buf, f.routing_path_id);
    buffer_write_u32(buf, f.routing_path_current_tile);
    buffer_write_u32(buf, f.routing_path_length);
    buffer_write_u8(buf, f.in_building_wait_ticks);
    buffer_write_u8(buf, f.is_on_road);
    buffer_write_i16(buf, f.max_roam_length);
    buffer_write_i16(buf, f.roam_length);
    buffer_write_u8(buf, f.roam_choose_destination);
    buffer_write_u8(buf, f.roam_random_counter);
    buffer_write_i8(buf, f.roam_turn_direction);
    buffer_write_i8(buf, f.roam_ticks_until_next_turn);
    buffer_write_i32(buf, f.cross_country_x);
    buffer_write_i32(buf, f.cross_country_y);
    buffer_write_i32(buf, f.cc_destination_x);
    buffer_write_i32(buf, f.cc_destination_y);
    buffer_write_i32(buf, f.cc_delta_x);
    buffer_write_i32(buf, f.cc_delta_y);
    buffer_write_i32(buf, f.cc_delta_xy);
    buffer_write_u8(buf, f.cc_direction);
    buffer_write_u8(buf, f.speed_multiplier);
    buffer_write_u32(buf, f.home_building_id());
    buffer_write_u32(buf, f.immigrant_building_id());
    buffer_write_u32(buf, f.destination_building_id());
    buffer_write_u32(buf, f.formation_id);
    buffer_write_u8(buf, f.index_in_formation);
    buffer_write_u8(buf, f.formation_at_rest);
    buffer_write_u8(buf, f.migrant_num_people);
    buffer_write_u8(buf, f.is_ghost);
    buffer_write_u8(buf, f.min_max_seen);
    buffer_write_i8(buf, f.progress_to_next_tick);
    buffer_write_i16(buf, f.leading_figure_id);
    buffer_write_u8(buf, f.attack_image_offset);
    buffer_write_u8(buf, f.wait_ticks_missile);
    buffer_write_i8(buf, f.x_offset_cart);
    buffer_write_i8(buf, f.y_offset_cart);
    buffer_write_u8(buf, f.empire_city_id);
    buffer_write_u8(buf, f.trader_amount_bought);
    buffer_write_i16(buf, f.name);
    buffer_write_u8(buf, f.terrain_usage);
    buffer_write_u8(buf, f.loads_sold_or_carrying);
    buffer_write_u8(buf, f.is_boat);
    buffer_write_u8(buf, f.height_adjusted_ticks);
    buffer_write_u8(buf, f.current_height);
    buffer_write_u8(buf, f.target_height);
    buffer_write_u8(buf, f.collecting_item_id);
    buffer_write_u8(buf, f.trade_ship_failed_dock_attempts);
    buffer_write_u8(buf, f.phrase_sequence_exact);
    buffer_write_i8(buf, f.phrase_id);
    buffer_write_u8(buf, f.phrase_sequence_city);
    buffer_write_u8(buf, f.trader_id);
    buffer_write_u8(buf, f.wait_ticks_next_target);
    buffer_write_u8(buf, f.dont_draw_elevated);
    buffer_write_u16(buf, static_cast<uint16_t>(f.target_figure.save_id()));
    buffer_write_u16(buf, static_cast<uint16_t>(f.targeted_by_figure.save_id()));
    buffer_write_u16(buf, f.created_sequence);
    buffer_write_u16(buf, f.target_figure_created_sequence);
    buffer_write_u8(buf, f.figures_on_same_tile_index);
    buffer_write_u8(buf, f.num_attackers);
    buffer_write_i32(buf, f.attacker1.save_id());
    buffer_write_i32(buf, f.attacker2.save_id());
    buffer_write_i32(buf, f.opponent.save_id());
    buffer_write_i16(buf, f.last_visited_index);
    buffer_write_i16(buf, static_cast<int16_t>(f.last_destination_id));
    std::array<char, FIGURE_RUNTIME_PROFILE_ID_CAPACITY> profile_id = {};
    memcpy(profile_id.data(), f.runtime_profile_id(), strlen(f.runtime_profile_id()));
    buffer_write_raw(buf, profile_id.data(), profile_id.size());
    std::array<char, FIGURE_TYPE_IDENTITY_CAPACITY> type_id = {};
    const char *identity = figure_type_identity(static_cast<figure_type>(f.type));
    memcpy(type_id.data(), identity, strlen(identity));
    buffer_write_raw(buf, type_id.data(), type_id.size());
}

void load_figure(buffer *buf, Figure &f, int figure_buf_size, int version)
{
    f.reset(f.id());
    f.alternative_location_index = buffer_read_u8(buf);
    f.image_offset = buffer_read_u8(buf);
    f.is_enemy_image = buffer_read_u8(buf);
    f.flotsam_visible = buffer_read_u8(buf);
    if (version <= SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        f.image_id = buffer_read_i16(buf);
        f.cart_image_id = buffer_read_i16(buf);
    } else {
        f.image_id = buffer_read_i32(buf);
        f.cart_image_id = buffer_read_i32(buf);
    }
    f.next_figure_id_on_same_tile = buffer_read_i16(buf);
    f.type = buffer_read_u8(buf);
    int resource = buffer_read_u8(buf);
    if (f.type == FIGURE_HIPPODROME_HORSES || f.type == FIGURE_FLOTSAM || resource < RESOURCE_NONE) {
        f.resource_id = static_cast<unsigned char>(resource);
    } else {
        f.resource_id = static_cast<unsigned char>(resource_remap(resource));
    }
    f.use_cross_country = buffer_read_u8(buf);
    f.is_friendly = buffer_read_u8(buf);
    f.state = buffer_read_u8(buf);
    f.faction_id = buffer_read_u8(buf);
    f.action_state_before_attack = buffer_read_u8(buf);
    f.direction = buffer_read_i8(buf);
    f.previous_tile_direction = buffer_read_i8(buf);
    f.attack_direction = buffer_read_i8(buf);
    f.x = buffer_read_u8(buf);
    f.y = buffer_read_u8(buf);
    f.previous_tile_x = buffer_read_u8(buf);
    f.previous_tile_y = buffer_read_u8(buf);
    f.missile_height = buffer_read_u8(buf);
    f.damage = buffer_read_u8(buf);
    f.grid_offset = buffer_read_i16(buf);
    f.destination_x = buffer_read_u8(buf);
    f.destination_y = buffer_read_u8(buf);
    f.destination_grid_offset = buffer_read_i16(buf);
    f.source_x = buffer_read_u8(buf);
    f.source_y = buffer_read_u8(buf);
    f.formation_position_x.soldier = buffer_read_u8(buf);
    f.formation_position_y.soldier = buffer_read_u8(buf);
    f.disallow_diagonal = buffer_read_i16(buf);
    f.wait_ticks = buffer_read_i16(buf);
    f.action_state = buffer_read_u8(buf);
    f.progress_on_tile = buffer_read_u8(buf);
    if (version <= SAVE_GAME_LAST_LEGACY_FIGURE_MOVEMENT_GRAIN && !figure_progress_is_action_timer(f)) {
        f.progress_on_tile = static_cast<unsigned char>(figure_movement_legacy_progress_to_runtime(f.progress_on_tile));
    }
    if (version <= SAVE_GAME_LAST_STATIC_PATHS_AND_ROUTES) {
        f.routing_path_id = buffer_read_i16(buf);
        f.routing_path_current_tile = buffer_read_i16(buf);
        f.routing_path_length = buffer_read_i16(buf);
    } else {
        f.routing_path_id = buffer_read_u32(buf);
        f.routing_path_current_tile = buffer_read_u32(buf);
        f.routing_path_length = buffer_read_u32(buf);
    }
    f.in_building_wait_ticks = buffer_read_u8(buf);
    f.is_on_road = buffer_read_u8(buf);
    f.max_roam_length = buffer_read_i16(buf);
    f.roam_length = buffer_read_i16(buf);
    f.roam_choose_destination = buffer_read_u8(buf);
    f.roam_random_counter = buffer_read_u8(buf);
    f.roam_turn_direction = buffer_read_i8(buf);
    f.roam_ticks_until_next_turn = buffer_read_i8(buf);
    if (version <= SAVE_GAME_LAST_LEGACY_FIGURE_MOVEMENT_GRAIN) {
        f.cross_country_x = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cross_country_y = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cc_destination_x = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cc_destination_y = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cc_delta_x = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cc_delta_y = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
        f.cc_delta_xy = migrate_legacy_cross_country_unit(buffer_read_i16(buf));
    } else {
        f.cross_country_x = buffer_read_i32(buf);
        f.cross_country_y = buffer_read_i32(buf);
        f.cc_destination_x = buffer_read_i32(buf);
        f.cc_destination_y = buffer_read_i32(buf);
        f.cc_delta_x = buffer_read_i32(buf);
        f.cc_delta_y = buffer_read_i32(buf);
        f.cc_delta_xy = buffer_read_i32(buf);
    }
    f.cc_direction = buffer_read_u8(buf);
    f.speed_multiplier = buffer_read_u8(buf);

    unsigned int building_id = 0;
    unsigned int immigrant_building_id = 0;
    unsigned int destination_building_id = 0;
    if (version <= SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        building_id = buffer_read_i16(buf);
        immigrant_building_id = buffer_read_i16(buf);
        destination_building_id = buffer_read_i16(buf);
        f.formation_id = buffer_read_i16(buf);
    } else {
        building_id = buffer_read_u32(buf);
        immigrant_building_id = buffer_read_u32(buf);
        destination_building_id = buffer_read_u32(buf);
        f.formation_id = buffer_read_u32(buf);
    }
    store_pending_building_refs(f.id(), building_id, immigrant_building_id, destination_building_id);
    f.legacy_visited_dock_mask = version <= SAVE_GAME_LAST_GLOBAL_BUILDING_INFO ? building_id : 0;

    f.index_in_formation = buffer_read_u8(buf);
    f.formation_at_rest = buffer_read_u8(buf);
    f.migrant_num_people = buffer_read_u8(buf);
    f.is_ghost = buffer_read_u8(buf);
    f.min_max_seen = buffer_read_u8(buf);
    f.progress_to_next_tick = buffer_read_i8(buf);
    f.leading_figure_id = buffer_read_i16(buf);
    f.attack_image_offset = buffer_read_u8(buf);
    f.wait_ticks_missile = buffer_read_u8(buf);
    f.x_offset_cart = buffer_read_i8(buf);
    f.y_offset_cart = buffer_read_i8(buf);
    f.empire_city_id = buffer_read_u8(buf);
    f.trader_amount_bought = buffer_read_u8(buf);
    f.name = buffer_read_i16(buf);
    f.terrain_usage = buffer_read_u8(buf);
    f.loads_sold_or_carrying = buffer_read_u8(buf);
    f.is_boat = buffer_read_u8(buf);
    f.height_adjusted_ticks = buffer_read_u8(buf);
    f.current_height = buffer_read_u8(buf);
    f.target_height = buffer_read_u8(buf);
    f.collecting_item_id = static_cast<unsigned char>((version <= SAVE_GAME_LAST_STATIC_RESOURCES) ?
        get_resource_id(static_cast<figure_type>(f.type), buffer_read_u8(buf)) :
        resource_remap(buffer_read_u8(buf)));
    f.trade_ship_failed_dock_attempts = buffer_read_u8(buf);
    f.phrase_sequence_exact = buffer_read_u8(buf);
    f.phrase_id = buffer_read_i8(buf);
    f.phrase_sequence_city = buffer_read_u8(buf);
    f.trader_id = buffer_read_u8(buf);
    f.wait_ticks_next_target = buffer_read_u8(buf);
    f.dont_draw_elevated = buffer_read_u8(buf);
    FigureStore::PendingFigureRefs &figure_refs = data.pending_figure_refs[f.id()];
    figure_refs.target_figure_id = buffer_read_u16(buf);
    figure_refs.targeted_by_figure_id = buffer_read_u16(buf);
    f.created_sequence = buffer_read_u16(buf);
    f.target_figure_created_sequence = buffer_read_u16(buf);
    f.figures_on_same_tile_index = buffer_read_u8(buf);
    f.num_attackers = buffer_read_u8(buf);
    if (version <= SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        figure_refs.attacker1_id = static_cast<unsigned int>(buffer_read_i16(buf));
        figure_refs.attacker2_id = static_cast<unsigned int>(buffer_read_i16(buf));
        figure_refs.opponent_id = static_cast<unsigned int>(buffer_read_i16(buf));
    } else {
        figure_refs.attacker1_id = static_cast<unsigned int>(buffer_read_i32(buf));
        figure_refs.attacker2_id = static_cast<unsigned int>(buffer_read_i32(buf));
        figure_refs.opponent_id = static_cast<unsigned int>(buffer_read_i32(buf));
    }
    if (version > SAVE_GAME_LAST_GLOBAL_BUILDING_INFO) {
        f.last_visited_index = buffer_read_i16(buf);
    }
    if (version > SAVE_GAME_LAST_GRANARY_WAREHOUSE_NON_ROADBLOCKS) {
        f.last_destination_id = buffer_read_i16(buf);
    }
    if (version > SAVE_GAME_LAST_NO_EXACT_FIGURE_PROFILE_IDENTITY && figure_buf_size >= kFigureExactProfileBufferSize) {
        std::array<char, FIGURE_RUNTIME_PROFILE_ID_CAPACITY> profile_id = {};
        buffer_read_raw(buf, profile_id.data(), profile_id.size());
        const bool terminated = memchr(profile_id.data(), 0, profile_id.size()) != nullptr;
        if (!terminated || !f.set_runtime_profile_id(profile_id.data())) {
            f.set_runtime_profile_id("__invalid_profile_identity__");
        }
    }
    int consumed = version > SAVE_GAME_LAST_NO_EXACT_FIGURE_PROFILE_IDENTITY ? kFigureExactProfileBufferSize : kFigureLastNoExactProfileIdentityBufferSize;
    if (version > SAVE_GAME_LAST_NO_FIGURE_TYPE_IDENTITY && figure_buf_size >= kFigureCurrentBufferSize) {
        std::array<char, FIGURE_TYPE_IDENTITY_CAPACITY> type_id = {};
        buffer_read_raw(buf, type_id.data(), type_id.size());
        const bool terminated = memchr(type_id.data(), 0, type_id.size()) != nullptr;
        const auto type = terminated ? figure_type_from_xml_name(type_id.data()) : FIGURE_NONE;
        if (type != FIGURE_NONE) f.type = static_cast<unsigned char>(type);
        else if (f.state && f.type >= FIGURE_BUILTIN_TYPE_MAX) {
            log_warning("Removing figure with unavailable mod type", terminated ? type_id.data() : "invalid identity", f.id());
            f.state = 0; f.type = FIGURE_NONE;
        }
        consumed = kFigureCurrentBufferSize;
    }
    if (figure_buf_size > consumed) {
        buffer_skip(buf, figure_buf_size - consumed);
    }
}

} // namespace

FigureRelation::FigureRelation(Figure *owner, const char *role) : owner_(owner), relationship_(role) {}

FigureRelation &FigureRelation::operator=(const FigureRelation &)
{
    if (owner_) {
        relationship_.clear(*owner_, RelationshipDisconnectReason::EndpointReassigned);
    }
    return *this;
}

Figure &FigureRelation::get()
{
    return relationship_.get();
}

const Figure &FigureRelation::get() const
{
    return relationship_.get();
}

void FigureRelation::retarget(Figure &figure)
{
    relationship_.retarget(*owner_, &figure);
}

void FigureRelation::clear()
{
    relationship_.clear(*owner_);
}

unsigned int FigureRelation::save_id() const
{
    return relationship_ ? relationship_->id() : 0;
}

unsigned int FigureRelation::debug_known_id() const
{
    return save_id();
}

Figure::Figure(unsigned int slot)
{
    reset(slot);
}

unsigned int Figure::id() const
{
    return slot_;
}

void Figure::reset(unsigned int slot)
{
    clear_building_references();
    *this = Figure();
    slot_ = slot;
}

Figure *Figure::get(unsigned int id)
{
    if (!id || id >= data.figures.size() || !data.figures[id]) {
        return invalid_figure();
    }
    return data.figures[id].get();
}

unsigned int Figure::count()
{
    return static_cast<unsigned int>(data.figures.size());
}

bool Figure::set_building_reference(ObjectRelationship<Figure, Building> &relationship, Building *target)
{
    if (target && (!id() || !target->id || Building::get(target->id) != target)) {
        return false;
    }
    relationship.retarget(*this, target);
    return true;
}

bool Figure::set_home_building(Building *owner)
{
    Building *previous = building;
    if (!set_building_reference(building, owner)) {
        return false;
    }
    if (previous && previous != owner) {
        previous->clear_figure_slot_if_matches(id());
    }
    return true;
}

bool Figure::set_immigrant_building(Building *house)
{
    if (house && ((type != FIGURE_IMMIGRANT && type != FIGURE_HOMELESS) || !house->Housing ||
        (house->Housing->state().immigrant_figure_id &&
            house->Housing->state().immigrant_figure_id != id()))) {
        return false;
    }
    return set_building_reference(immigrant_building, house);
}

bool Figure::set_destination_building(Building *destination)
{
    return set_building_reference(destination_building, destination);
}

bool Figure::set_last_destination_building(Building *destination)
{
    if (!set_building_reference(last_destination_building_, destination)) {
        return false;
    }
    last_destination_id = destination ? static_cast<int>(destination->id) : 0;
    return true;
}

void Figure::set_last_destination_figure_id(unsigned int figure_id)
{
    last_destination_building_.clear(*this);
    last_destination_id = static_cast<int>(figure_id);
}

bool Figure::clear_last_destination_building_if_matches(const Building &target)
{
    if (last_destination_building_.get_ptr() != &target) {
        return false;
    }
    set_last_destination_building(nullptr);
    return true;
}

unsigned int Figure::home_building_id() const
{
    return building ? static_cast<unsigned int>(building->id) : 0;
}

unsigned int Figure::immigrant_building_id() const
{
    return immigrant_building ? static_cast<unsigned int>(immigrant_building->id) : 0;
}

unsigned int Figure::destination_building_id() const
{
    return destination_building ? static_cast<unsigned int>(destination_building->id) : 0;
}

const char *Figure::runtime_profile_id() const
{
    return runtime_profile_id_.data();
}

bool Figure::set_runtime_profile_id(const char *profile_id)
{
    runtime_profile_id_.fill(0);
    if (!profile_id) {
        return true;
    }
    const size_t length = strlen(profile_id);
    if (length >= runtime_profile_id_.size()) {
        return false;
    }
    memcpy(runtime_profile_id_.data(), profile_id, length);
    return true;
}

bool Figure::references_building(const Building &target) const
{
    return building.get_ptr() == &target || immigrant_building.get_ptr() == &target ||
        destination_building.get_ptr() == &target || last_destination_building_.get_ptr() == &target;
}

void Figure::clear_building_references()
{
    set_home_building(nullptr);
    set_immigrant_building(nullptr);
    set_destination_building(nullptr);
    set_last_destination_building(nullptr);
}

void Figure::on_relationship_event(const RelationshipEvent &event)
{
    if (event.type != RelationshipEventType::Disconnected || event.reason != RelationshipDisconnectReason::EndpointRemoved || !state) {
        return;
    }
    if (Figure *removed = dynamic_cast<Figure *>(event.other)) {
        if (&event.relationship == &opponent.relationship_ || &event.relationship == &attacker1.relationship_ ||
            &event.relationship == &attacker2.relationship_) {
            figure_combat_relationship_removed(this, removed);
        } else if (&event.relationship == &target_figure.relationship_) {
            Route::remove(this);
            direction = DIR_FIGURE_REROUTE;
        }
        return;
    }
    if (&event.relationship == &building) {
        if (type == FIGURE_FISHING_BOAT) {
            FishingBoat::from(*this).sink();
        } else {
            remove();
        }
        return;
    }
    if (&event.relationship == &immigrant_building) {
        remove();
        return;
    }
    if (&event.relationship != &destination_building) {
        return;
    }

    Route::remove(this);
    wait_ticks = 0;
    if (type == FIGURE_LABOR_SEEKER) {
        building_local_workforce::labor_seeker_failed(this);
    } else if (type == FIGURE_TRADE_SHIP) {
        figure_trade_ship_destination_removed(*this, *static_cast<Building *>(event.other));
    } else if (type == FIGURE_WAREHOUSEMAN) {
        action_state = FIGURE_ACTION_233_WAREHOUSEMAN_RECONSIDER_TARGET;
    } else if (type == FIGURE_CART_PUSHER) {
        action_state = FIGURE_ACTION_20_CARTPUSHER_INITIAL;
    } else {
        direction = DIR_FIGURE_REROUTE;
    }
}

std::vector<Figure *> Figure::figures_referencing_building(const Building &building)
{
    std::vector<Figure *> figures;
    for (Relationship *relationship : building.relationships()) {
        Figure *figure = dynamic_cast<Figure *>(relationship->other_endpoint(building));
        if (figure && figure->state && std::find(figures.begin(), figures.end(), figure) == figures.end()) {
            figures.push_back(figure);
        }
    }
    return figures;
}

std::vector<Figure *> Figure::figures_directly_referencing_building(const Building &building)
{
    std::vector<Figure *> direct;
    for (Figure *figure : figures_referencing_building(building)) {
        if (figure->building.get_ptr() == &building || figure->immigrant_building.get_ptr() == &building ||
            figure->destination_building.get_ptr() == &building) {
            direct.push_back(figure);
        }
    }
    return direct;
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

static void write_building_reference(FILE *file, const char *name, const Building *building)
{
    fprintf(file, "      \"%s\": {\n", name);
    fprintf(file, "        \"pointer\": ");
    write_debug_pointer(file, building);
    fprintf(file, ",\n");
    fprintf(file, "        \"known_building_id\": %u\n", building_runtime_debug_known_building_id(building));
    fprintf(file, "      }");
}

void figure_debug_dump(FILE *file)
{
    if (!file) {
        return;
    }

    fprintf(file, "  \"figures\": [\n");
    int wrote = 0;
    for (size_t slot = 0; slot < data.figures.size(); ++slot) {
        const std::unique_ptr<Figure> &figure_slot = data.figures[slot];
        if (!figure_slot || !figure_slot->state) {
            continue;
        }

        const Figure &figure = *figure_slot;
        const figure_type_registry_impl::FigureTypeDefinition *definition =
            figure_type_registry_impl::definition_for(static_cast<figure_type>(figure.type));

        if (wrote++) {
            fprintf(file, ",\n");
        }

        fprintf(file, "    {\n");
        fprintf(file, "      \"slot\": %zu,\n", slot);
        fprintf(file, "      \"pointer\": ");
        write_debug_pointer(file, &figure);
        fprintf(file, ",\n");
        fprintf(file, "      \"id\": %u,\n", figure.id());
        fprintf(file, "      \"type_id\": %d,\n", figure.type);
        fprintf(file, "      \"type_attr\": ");
        write_debug_json_string(file, definition ? definition->attr() : nullptr);
        fprintf(file, ",\n");
        fprintf(file, "      \"state\": %d,\n", figure.state);
        fprintf(file, "      \"faction_id\": %d,\n", figure.faction_id);
        fprintf(file, "      \"action_state\": %d,\n", figure.action_state);
        fprintf(file, "      \"previous_action_state\": %d,\n", figure.action_state_before_attack);
        fprintf(file, "      \"created_sequence\": %u,\n", figure.created_sequence);
        fprintf(file, "      \"x\": %d,\n", figure.x);
        fprintf(file, "      \"y\": %d,\n", figure.y);
        fprintf(file, "      \"grid_offset\": %d,\n", figure.grid_offset);
        fprintf(file, "      \"destination_x\": %d,\n", figure.destination_x);
        fprintf(file, "      \"destination_y\": %d,\n", figure.destination_y);
        fprintf(file, "      \"destination_grid_offset\": %d,\n", figure.destination_grid_offset);
        fprintf(file, "      \"source_x\": %d,\n", figure.source_x);
        fprintf(file, "      \"source_y\": %d,\n", figure.source_y);
        fprintf(file, "      \"direction\": %d,\n", figure.direction);
        fprintf(file, "      \"previous_tile_direction\": %d,\n", figure.previous_tile_direction);
        fprintf(file, "      \"wait_ticks\": %d,\n", figure.wait_ticks);
        fprintf(file, "      \"routing_path_id\": %u,\n", figure.routing_path_id);
        fprintf(file, "      \"routing_path_current_tile\": %u,\n", figure.routing_path_current_tile);
        fprintf(file, "      \"routing_path_length\": %u,\n", figure.routing_path_length);
        fprintf(file, "      \"formation_id\": %u,\n", figure.formation_id);
        fprintf(file, "      \"index_in_formation\": %d,\n", figure.index_in_formation);
        fprintf(file, "      \"resource_id\": %d,\n", figure.resource_id);
        fprintf(file, "      \"collecting_item_id\": %d,\n", figure.collecting_item_id);
        fprintf(file, "      \"loads_sold_or_carrying\": %d,\n", figure.loads_sold_or_carrying);
        fprintf(file, "      \"migrant_num_people\": %d,\n", figure.migrant_num_people);
        fprintf(file, "      \"is_ghost\": %d,\n", figure.is_ghost);
        fprintf(file, "      \"is_boat\": %d,\n", figure.is_boat);
        fprintf(file, "      \"leading_figure_id\": %d,\n", figure.leading_figure_id);
        fprintf(file, "      \"target_figure_id\": %u,\n", figure.target_figure.debug_known_id());
        fprintf(file, "      \"targeted_by_figure_id\": %u,\n", figure.targeted_by_figure.debug_known_id());
        fprintf(file, "      \"attacker1_id\": %u,\n", figure.attacker1.debug_known_id());
        fprintf(file, "      \"attacker2_id\": %u,\n", figure.attacker2.debug_known_id());
        fprintf(file, "      \"opponent_id\": %u,\n", figure.opponent.debug_known_id());
        write_building_reference(file, "building", figure.building);
        fprintf(file, ",\n");
        write_building_reference(file, "immigrant_building", figure.immigrant_building);
        fprintf(file, ",\n");
        write_building_reference(file, "destination_building", figure.destination_building);
        fprintf(file, "\n");
        fprintf(file, "    }");
    }
    fprintf(file, "\n  ]");
}

Figure *Figure::create(figure_type figure_type, int x, int y, direction_type dir)
{
    invalid_figure();
    Figure *f = nullptr;
    for (unsigned int i = 1; i < data.figures.size(); i++) {
        if (!data.figures[i]) {
            data.figures[i] = std::make_unique<Figure>(i);
        }
        if (!data.figures[i]->state) {
            f = data.figures[i].get();
            f->reset(i);
            break;
        }
    }
    if (!f) {
        const unsigned int id = static_cast<unsigned int>(data.figures.size());
        data.figures.push_back(std::make_unique<Figure>(id));
        f = data.figures.back().get();
    }

    f->state = FIGURE_STATE_ALIVE;
    f->faction_id = 1;
    f->type = static_cast<unsigned char>(figure_type);
    f->use_cross_country = 0;
    f->is_friendly = 1;
    city_trade_ledger_forget_figure(f->id());
    f->created_sequence = static_cast<unsigned short>(data.created_sequence++);
    f->direction = static_cast<signed char>(dir);
    f->source_x = f->destination_x = f->previous_tile_x = f->x = static_cast<unsigned char>(x);
    f->source_y = f->destination_y = f->previous_tile_y = f->y = static_cast<unsigned char>(y);
    f->grid_offset = static_cast<short>(map_grid_offset(x, y));
    f->cross_country_x = figure_movement_tile_to_cross_country(x);
    f->cross_country_y = figure_movement_tile_to_cross_country(y);
    f->progress_on_tile = FIGURE_TILE_PROGRESS_MAX;
    f->progress_to_next_tick = 0;
    f->dont_draw_elevated = 0;
    f->disallow_diagonal = 0;
    f->resource_id = RESOURCE_NONE;
    f->wait_ticks = 0;
    random_generate_next();
    f->name = static_cast<short>(figure_name_get(figure_type, static_cast<enemy_type_t>(0)));
    f->phrase_sequence_city = f->phrase_sequence_exact = random_byte() & 3;
    map_figure_add(f);
    if (figure_type == FIGURE_TRADE_CARAVAN || figure_type == FIGURE_TRADE_SHIP ||
        figure_type == FIGURE_NATIVE_TRADER) {
        f->trader_id = static_cast<unsigned char>(trader_create());
    }
    figure_runtime_on_created(f);
    return f;
}

void Figure::remove()
{
    if (type == FIGURE_LABOR_SEEKER) {
        building_local_workforce::remove_labor_seeker(*this);
    }
    release_destination_reservations();
    if (formation_id) {
        formation *owner = formation_get(formation_id);
        if (owner && owner->in_use) {
            owner->unpublish_figure(*this);
        }
    }
    clear_known_building_refs_for_figure(*this);
    switch (type) {
        case FIGURE_DOCKER:
        case FIGURE_DEPOT_CART_PUSHER:
            if (building) {
                building->clear_distribution_cartpusher_slot_if_matches(id());
            }
            break;
        case FIGURE_ENEMY_CAESAR_LEGIONARY:
            city_emperor_mark_soldier_killed();
            break;
        case FIGURE_EXPLOSION:
        case FIGURE_ARROW:
        case FIGURE_JAVELIN:
        case FIGURE_FRIENDLY_ARROW:
        case FIGURE_BOLT:
        case FIGURE_CATAPULT_MISSILE:
        case FIGURE_SPEAR:
        case FIGURE_FISH_GULLS:
        case FIGURE_SHEEP:
        case FIGURE_WOLF:
        case FIGURE_ZEBRA:
        case FIGURE_DELIVERY_BOY:
        case FIGURE_PATRICIAN:
        case FIGURE_MESS_HALL_COLLECTOR:
        case FIGURE_TRADE_SHIP:
            break;
        case FIGURE_CART_PUSHER:
        case FIGURE_WORK_CAMP_ARCHITECT:
        case FIGURE_WORK_CAMP_WORKER:
        case FIGURE_WORK_CAMP_SLAVE:
            building_monument_remove_delivery(id());
            break;
        default:
            break;
    }
    clear_building_references();
    disconnect_relationships(RelationshipDisconnectReason::EndpointRemoved);
    if (empire_city_id) {
        empire_city_remove_trader(empire_city_id, id());
    }
    figure_visited_buildings_remove_list(last_visited_index);
    Route::remove(this);
    map_figure_delete(this);
    figure_runtime_on_deleted(this);

    const unsigned int slot = id();
    reset(slot);
    trim_dead_tail();
}

void Figure::release_destination_reservations()
{
    if (!destination_building) {
        return;
    }
    destination_building->release_input_storage_reservation(*this);
    destination_building->release_legacy_storage_reservation(*this);
}

int Figure::retarget_building(Building &from, Building &to)
{
    const unsigned int from_id = from.id;
    const unsigned int to_id = to.id;
    if (!from_id || !to_id || from_id == to_id) {
        return 0;
    }
    if (immigrant_building && immigrant_building->id == from_id && !to.Housing) {
        return 0;
    }

    int changed = 0;
    int destination_changed = 0;
    if (building && building->id == from_id) {
        set_home_building(&to);
        changed = 1;
    }
    if (immigrant_building && immigrant_building->id == from_id) {
        if (immigrant_building->Housing) {
            immigrant_building->Housing->clear_immigrant_if_matches(*this);
        }
        set_immigrant_building(&to);
        changed = 1;
        destination_changed = 1;
    }
    if (destination_building && destination_building->id == from_id) {
        release_destination_reservations();
        set_destination_building(&to);
        changed = 1;
        destination_changed = 1;
    }
    if (last_destination_building_.get_ptr() == &from) {
        set_last_destination_building(&to);
        changed = 1;
    }
    if (destination_changed) {
        if (to.has_cached_road_access()) {
            destination_x = static_cast<unsigned char>(to.road_access_x());
            destination_y = static_cast<unsigned char>(to.road_access_y());
            destination_grid_offset = static_cast<short>(map_grid_offset(destination_x, destination_y));
        }
        if ((type == FIGURE_IMMIGRANT && action_state == FIGURE_ACTION_3_IMMIGRANT_ENTERING_HOUSE) ||
            (type == FIGURE_HOMELESS && action_state == FIGURE_ACTION_9_HOMELESS_ENTERING_HOUSE)) {
            figure_movement_set_cross_country_destination(this, to.x(), to.y());
            destination_grid_offset = static_cast<short>(map_grid_offset(destination_x, destination_y));
        }
        Route::remove(this);
    }
    return changed;
}

int Figure::is_dead() const
{
    return state != FIGURE_STATE_ALIVE || action_state == FIGURE_ACTION_149_CORPSE;
}

int Figure::is_enemy() const
{
    const FormationType *definition = formation_type_registry_impl::find_spawn_formation(static_cast<figure_type>(type));
    return definition && definition->spawn.role == FormationSpawnRole::Enemy;
}

int Figure::is_melee_enemy() const
{
    switch (type) {
        case FIGURE_ENEMY44_SWORD:
        case FIGURE_ENEMY45_SWORD:
        case FIGURE_ENEMY49_FAST_SWORD:
        case FIGURE_ENEMY50_SWORD:
        case FIGURE_ENEMY53_AXE:
            return 1;
        default:
            return 0;
    }
}

int Figure::is_ranged_enemy() const
{
    switch (type) {
        case FIGURE_ENEMY43_SPEAR:
        case FIGURE_ENEMY51_SPEAR:
            return 1;
        default:
            return 0;
    }
}

int Figure::is_mounted_enemy() const
{
    switch (type) {
        case FIGURE_ENEMY46_CAMEL:
        case FIGURE_ENEMY47_ELEPHANT:
        case FIGURE_ENEMY48_CHARIOT:
        case FIGURE_ENEMY52_MOUNTED_ARCHER:
            return 1;
        default:
            return 0;
    }
}

int Figure::is_caesar_enemy() const
{
    switch (type) {
        case FIGURE_ENEMY_CAESAR_JAVELIN:
        case FIGURE_ENEMY_CAESAR_MOUNTED:
        case FIGURE_ENEMY_CAESAR_LEGIONARY:
            return 1;
        default:
            return 0;
    }
}

int Figure::is_legion() const
{
    return (type >= FIGURE_FORT_JAVELIN && type <= FIGURE_FORT_LEGIONARY) ||
        type == FIGURE_FORT_INFANTRY || type == FIGURE_FORT_ARCHER;
}

int Figure::is_herd() const
{
    const FormationType *definition = formation_type_registry_impl::find_spawn_formation(static_cast<figure_type>(type));
    return definition && definition->spawn.role == FormationSpawnRole::Herd;
}

int Figure::is_aggressive_herd() const
{
    if (!is_herd()) {
        return 0;
    }
    formation *owner = formation_get(formation_id);
    if (!owner || !owner->owns_figure(*this)) {
        log_error("Herd member has an invalid formation relationship", 0, static_cast<int>(id()));
        std::terminate();
    }
    return owner->is_aggressive_herd();
}

int Figure::is_category(figure_category_mask category_mask) const
{
    const figure_properties *props = figure_properties_for_type(static_cast<figure_type>(type));
    return figure_category_mask_has_any(props->category, category_mask);
}

int Figure::uses_tall_info_panel() const
{
    return type == FIGURE_TRADE_CARAVAN ||
        type == FIGURE_TRADE_CARAVAN_DONKEY ||
        type == FIGURE_TRADE_SHIP ||
        type == FIGURE_NATIVE_TRADER;
}

int Figure::has_info_action_button() const
{
    if (type == FIGURE_DEPOT_CART_PUSHER) {
        return !reinterpret_cast<const figuretype::DepotCartPusher *>(this)->is_recalled();
    }
    return 0;
}

void Figure::handle_info_action_button()
{
    if (type == FIGURE_DEPOT_CART_PUSHER) {
        reinterpret_cast<figuretype::DepotCartPusher *>(this)->recall();
    }
}

int Figure::big_people_image_id(figure_type figure_type)
{
    figure_type = figure_type_base(figure_type);
    switch (figure_type) {
        case FIGURE_TRADE_CARAVAN_DONKEY:
        case FIGURE_TRADE_CARAVAN:
            if (scenario_property_climate() == CLIMATE_DESERT) {
                return Image::group(GROUP_BIG_PEOPLE) + CAMEL_PORTRAIT - 1;
            }
            break;
        case FIGURE_DEPOT_CART_PUSHER:
            return assets_lookup_image_id(ASSET_OX);
        case FIGURE_FORT_INFANTRY:
            return assets_get_image_id("Warriors", "auxinf_portrait");
        case FIGURE_FORT_ARCHER:
            return assets_get_image_id("Warriors", "auxarch_portrait");
        case FIGURE_ENEMY_CATAPULT:
            return assets_get_image_id("Warriors", "catapult_portrait");
        default:
            break;
    }
    return Image::group(GROUP_BIG_PEOPLE) + FIGURE_TYPE_TO_BIG_FIGURE_IMAGE[figure_type] - 1;
}

void Figure::draw_big_people_image(figure_type figure_type, int x, int y)
{
    ImageGroupEntryRef ref = big_people_image_ref(figure_type);
    if (ref.is_bound()) {
        ref.draw(x, y);
    } else {
        Image::from_id(big_people_image_id(figure_type)).draw(x, y);
    }
}

void Figure::draw_big_people_image(int draw_x, int draw_y) const
{
    draw_big_people_image(static_cast<figure_type>(type), draw_x, draw_y);
}

translation_key Figure::new_type_translation_key(figure_type figure_type)
{
    if (const auto *definition = figure_type_registry_impl::definition_for(figure_type); definition && !definition->name_key().empty()) return translation_key(definition->name_key());
    figure_type = figure_type_base(figure_type);
    if (figure_type < FIGURE_NEW_TYPES || figure_type >= FIGURE_BUILTIN_TYPE_MAX) {
        return {};
    }
    return NEW_FIGURE_TYPES[figure_type - FIGURE_NEW_TYPES];
}

translation_key Figure::type_translation_key() const
{
    return new_type_translation_key(static_cast<figure_type>(type));
}

void Figure::draw(building_info_context *c)
{
    if (action_state == FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE ||
        action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE) {
        Image::from_id(Image::group(GROUP_BIG_PEOPLE) + 18).draw(c->x_offset + 28, c->y_offset + 112);
    } else {
        draw_big_people_image(c->x_offset + 28, c->y_offset + 112);
    }

    lang_text_draw(current_string_key(65, name), c->x_offset + 90, c->y_offset + 108,
        FONT_LARGE_BROWN, screen_ui_to_pixel(font_definition_for(FONT_LARGE_BROWN)->line_height));
    const translation_key custom_type = type_translation_key();
    if (custom_type.id) {
        text_draw(translation_for(custom_type), c->x_offset + 92, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
    } else {
        lang_text_draw(current_string_key(64, figure_type_base(static_cast<figure_type>(type))), c->x_offset + 92, c->y_offset + 139,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }

    if (c->figure.phrase_id >= 0) {
        lang_text_draw_multiline(current_string_key(130, 21 * c->figure.sound_id + c->figure.phrase_id + 1),
            c->x_offset + 90, c->y_offset + 160, BLOCK_SIZE * (c->width_blocks - 8),
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }

    if (tourist.tourist_money_spent) {
        int width = text_draw(translation_for_key("TR_WINDOW_FIGURE_TOURIST"),
            c->x_offset + 92, c->y_offset + 180,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height), 0);
        text_draw_money(tourist.tourist_money_spent, c->x_offset + 92 + width, c->y_offset + 180,
            FONT_NORMAL_BROWN, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BROWN)->line_height));
    }
}

int Figure::graphic_draw_request(FigureGraphicDrawRequest &request) const
{
    return figure_graphics_resolve_draw_request(*this, request);
}

void Figure::draw_figure_info(building_info_context *c)
{
    if (type == FIGURE_TRADE_CARAVAN || type == FIGURE_TRADE_CARAVAN_DONKEY ||
        type == FIGURE_TRADE_SHIP || type == FIGURE_NATIVE_TRADER) {
        reinterpret_cast<figuretype::Trader *>(this)->draw(c);
    } else if (type >= FIGURE_ENEMY43_SPEAR && type <= FIGURE_ENEMY53_AXE) {
        reinterpret_cast<figuretype::Enemy *>(this)->draw(c);
    } else if (type == FIGURE_FISHING_BOAT) {
        reinterpret_cast<figuretype::Boat *>(this)->draw(c);
    } else if (type == FIGURE_SHIPWRECK || is_herd()) {
        reinterpret_cast<figuretype::Animal *>(this)->draw(c);
    } else if (type == FIGURE_CART_PUSHER || type == FIGURE_WAREHOUSEMAN || type == FIGURE_DOCKER) {
        reinterpret_cast<figuretype::CartPusher *>(this)->draw(c);
    } else if (type == FIGURE_MARKET_SUPPLIER || type == FIGURE_MESS_HALL_SUPPLIER ||
        type == FIGURE_PRIEST_SUPPLIER || type == FIGURE_BARKEEP_SUPPLIER ||
        type == FIGURE_CARAVANSERAI_SUPPLIER || type == FIGURE_LIGHTHOUSE_SUPPLIER) {
        reinterpret_cast<figuretype::Supplier *>(this)->draw(c);
    } else if (type == FIGURE_WORK_CAMP_WORKER || type == FIGURE_WORK_CAMP_SLAVE) {
        reinterpret_cast<figuretype::WorkCampWorker *>(this)->draw(c);
    } else if (type == FIGURE_DEPOT_CART_PUSHER) {
        reinterpret_cast<figuretype::DepotCartPusher *>(this)->draw(c);
    } else {
        draw(c);
    }
}

int Figure::target_is_alive() const
{
    if (!target_figure.save_id()) {
        return 0;
    }
    const Figure &target = target_figure.get();
    return !target.is_dead() && target.created_sequence == target_figure_created_sequence;
}

void Figure::select_legacy_corpse_image(int base_image_id)
{
    image_id = base_image_id +
        figure_type_registry_impl::FigureGraphics::corpse_frame_for_wait_ticks(wait_ticks);
}

void Figure::select_legacy_frame_image(int base_image_id, int frame_offset)
{
    image_id = base_image_id + frame_offset;
}

void Figure::select_legacy_static_frame_image(int base_image_id, int frame_count)
{
    image_id = frame_count > 0 ?
        base_image_id + static_cast<int>(id() % static_cast<unsigned int>(frame_count)) :
        base_image_id;
}

void Figure::select_legacy_directional_frame_image(
    int base_image_id,
    int normalized_direction,
    int frame_offset,
    int frame_stride)
{
    image_id = base_image_id + normalized_direction + frame_stride * frame_offset;
}

void Figure::select_legacy_default_or_corpse_image(int base_image_id)
{
    if (action_state == FIGURE_ACTION_149_CORPSE) {
        select_legacy_corpse_image(base_image_id + 96);
    } else {
        select_legacy_directional_frame_image(base_image_id, figure_image_direction(this), image_offset);
    }
}

void Figure::clear_legacy_image()
{
    image_id = 0;
}

void Figure::adjust_legacy_gladiator_attack_image_row()
{
    if (image_id >= 5705 && image_id <= 5706) {
        image_id -= 8;
    } else if (image_id > 5705) {
        image_id -= 2;
    }
}

void Figure::clear_legacy_cart_overlay_image()
{
    cart_image_id = 0;
}

void Figure::init_scenario()
{
    figure_runtime_reset();
    race_bet_reset_runtime();
    map_road_service_history_clear();
    data.figures.clear();
    data.pending_building_refs.clear();
    data.pending_figure_refs.clear();
    data.figures.reserve(kFigureArraySizeStep);
    data.figures.push_back(std::make_unique<Figure>(0));
    data.created_sequence = 0;
}

void Figure::kill_all()
{
    for (const std::unique_ptr<Figure> &f : data.figures) {
        if (!f) {
            continue;
        }
        switch (f->type) {
            default:
                f->state = FIGURE_STATE_DEAD;
                break;
            case FIGURE_EXPLOSION:
            case FIGURE_MAP_FLAG:
            case FIGURE_FISH_GULLS:
            case FIGURE_SHIPWRECK:
                break;
        }
    }
}

void Figure::save_state(buffer *list, buffer *seq)
{
    buffer_write_i32(seq, data.created_sequence);

    const int buf_size = 4 + static_cast<int>(data.figures.size()) * kFigureCurrentBufferSize;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    buffer_init(list, buf_data, buf_size);
    buffer_write_i32(list, kFigureCurrentBufferSize);

    for (const std::unique_ptr<Figure> &f : data.figures) {
        save_figure(list, f ? *f : *invalid_figure());
    }
}

void Figure::load_state(buffer *list, buffer *seq, int version)
{
    figure_runtime_reset();
    race_bet_reset_runtime();
    data.created_sequence = buffer_read_i32(seq);

    int figure_buf_size = kFigureOriginalBufferSize;
    size_t buf_size = list->size;
    if (version > SAVE_GAME_LAST_STATIC_VERSION) {
        figure_buf_size = buffer_read_i32(list);
        buf_size -= 4;
    }

    const int figures_to_load = static_cast<int>(buf_size / figure_buf_size);
    data.figures.clear();
    data.pending_building_refs.clear();
    data.pending_figure_refs.clear();
    data.figures.reserve(figures_to_load > 0 ? figures_to_load : 1);
    data.pending_building_refs.resize(figures_to_load > 0 ? figures_to_load : 1);
    data.pending_figure_refs.resize(figures_to_load > 0 ? figures_to_load : 1);
    for (int i = 0; i < figures_to_load; i++) {
        data.figures.push_back(std::make_unique<Figure>(static_cast<unsigned int>(i)));
    }
    if (data.figures.empty()) {
        data.figures.push_back(std::make_unique<Figure>(0));
    }

    int highest_id_in_use = 0;
    for (int i = 0; i < figures_to_load; i++) {
        Figure &f = *data.figures[i];
        load_figure(list, f, figure_buf_size, version);
        if (f.state) {
            highest_id_in_use = i;
        }
    }
    for (size_t i = 0; i < data.figures.size(); ++i) {
        Figure *figure = data.figures[i].get();
        if (!figure || !figure->state) {
            continue;
        }
        const FigureStore::PendingFigureRefs &refs = data.pending_figure_refs[i];
        resolve_loaded_figure_relation(*figure, figure->target_figure, refs.target_figure_id, "target_figure");
        resolve_loaded_figure_relation(*figure, figure->targeted_by_figure, refs.targeted_by_figure_id, "targeted_by_figure");
        resolve_loaded_figure_relation(*figure, figure->attacker1, refs.attacker1_id, "attacker1");
        resolve_loaded_figure_relation(*figure, figure->attacker2, refs.attacker2_id, "attacker2");
        resolve_loaded_figure_relation(*figure, figure->opponent, refs.opponent_id, "opponent");

        const unsigned int attacker1_before = figure->attacker1.save_id();
        const unsigned int attacker2_before = figure->attacker2.save_id();
        const unsigned int opponent_before = figure->opponent.save_id();
        const unsigned int num_attackers_before = figure->num_attackers;
        const unsigned int action_before = figure->action_state;
        figure_combat_migrate_legacy_relationships(figure);
        if (attacker1_before != figure->attacker1.save_id() || attacker2_before != figure->attacker2.save_id() ||
            opponent_before != figure->opponent.save_id() || num_attackers_before != figure->num_attackers ||
            action_before != figure->action_state) {
            char detail[256];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u saved_num_attackers=%u saved_opponent_id=%u",
                figure->id(), static_cast<unsigned int>(figure->type), num_attackers_before, opponent_before);
            log_warning("Repairing inconsistent saved combat relationships", detail, 0);
        }
    }
    data.figures.resize(highest_id_in_use + 1);
    if (data.pending_building_refs.size() > data.figures.size()) {
        data.pending_building_refs.resize(data.figures.size());
    }
    if (data.pending_figure_refs.size() > data.figures.size()) {
        data.pending_figure_refs.resize(data.figures.size());
    }
    invalid_figure();
}

bool Figure::resolve_loaded_building_references(int save_version)
{
    const size_t count = data.pending_building_refs.size() < data.figures.size() ?
        data.pending_building_refs.size() : data.figures.size();
    for (size_t i = 0; i < count && i < data.figures.size(); i++) {
        Figure *f = data.figures[i].get();
        if (!f) {
            continue;
        }
        FigureStore::PendingBuildingRefs &refs = data.pending_building_refs[i];
        Building *owner = nullptr;
        if (!figure_runtime_resolve_loaded_owner(f, refs.building_id, save_version <= SAVE_GAME_LAST_NO_EXACT_FIGURE_PROFILE_IDENTITY, save_version <= SAVE_GAME_LAST_UNVERIFIED_FIGURE_OWNER_REFERENCES, save_version <= SAVE_GAME_LAST_DELAYED_FIGURE_OWNER_BINDING, &owner) ||
            !f->set_home_building(owner)) {
            log_error("Loaded save failed strict figure owner validation", 0, static_cast<int>(i));
            return false;
        }
        if (!f->state) {
            refs = {};
            continue;
        }
        if (owner) refs.building_id = static_cast<unsigned int>(owner->id);
        else if (save_version <= SAVE_GAME_LAST_UNVERIFIED_FIGURE_OWNER_REFERENCES) refs.building_id = 0;
        Building *immigrant = nullptr;
        Building *destination = nullptr;
        if (!loaded_optional_building_ref(*f, refs.immigrant_building_id, "immigrant", save_version, &immigrant) ||
            !loaded_optional_building_ref(*f, refs.destination_building_id, "destination", save_version, &destination)) {
            log_error("Loaded save failed strict optional figure building reference validation", 0, static_cast<int>(i));
            return false;
        }
        if (!f->set_immigrant_building(immigrant)) {
            if (save_version >= SAVE_GAME_CURRENT_VERSION) {
                log_error("Loaded save failed strict immigrant building relationship validation", 0, static_cast<int>(i));
                return false;
            }
            char detail[192];
            snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u saved_building_id=%u", f->id(), static_cast<unsigned int>(f->type), refs.immigrant_building_id);
            log_warning("Clearing conflicting legacy immigrant building relationship", detail, 0);
            refs.immigrant_building_id = 0;
            f->set_immigrant_building(nullptr);
        }
        if (!f->set_destination_building(destination)) {
            log_error("Loaded save failed strict destination building relationship validation", 0, static_cast<int>(i));
            return false;
        }
        if (f->last_destination_id > 0 &&
            !last_destination_is_figure_id(static_cast<figure_type>(f->type))) {
            Building *last_destination = loaded_building_ref(static_cast<unsigned int>(f->last_destination_id));
            if (!last_destination || !f->set_last_destination_building(last_destination)) {
                if (save_version < SAVE_GAME_CURRENT_VERSION) {
                    char detail[192];
                    snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u saved_building_id=%u", f->id(), static_cast<unsigned int>(f->type), static_cast<unsigned int>(f->last_destination_id));
                    log_warning("Clearing invalid legacy last-destination building reference", detail, 0);
                    f->set_last_destination_building(nullptr);
                    continue;
                }
                log_error("Loaded save contains an invalid last-destination building reference", 0, f->last_destination_id);
                return false;
            }
        }
    }
    if (!repair_loaded_migrant_house_refs(save_version)) {
        log_error("Loaded save failed legacy migrant and housing relationship repair", 0, 0);
        return false;
    }
    repair_loaded_building_figure_slots();
    if (!validate_loaded_migrant_house_refs() || !validate_loaded_building_figure_slots()) {
        return false;
    }
    data.pending_building_refs.clear();
    data.pending_building_refs.shrink_to_fit();
    data.pending_figure_refs.clear();
    data.pending_figure_refs.shrink_to_fit();
    return true;
}
