#include "figure/figure.h"

#include "assets/assets.h"
#include "building/building_runtime.h"
#include "building/building_record.h"
#include "building/building_runtime_internal.h"
#include "building/building_type_registry_internal.h"
#include "building/local_workforce.h"
#include "building/monument.h"
#include "building/production_method.h"
#include "building/properties.h"
#include "city/emperor.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "figure/image.h"
#include "figure/figure_runtime_api.h"
#include "figure/figure_runtime_native.h"
#include "figure/figure_type_registry_internal.h"
#include "figure/movement.h"
#include "figure/name.h"
#include "figure/route.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "figuretype/animal.h"
#include "figuretype/cartpusher.h"
#include "figuretype/depot.h"
#include "figuretype/enemy.h"
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
constexpr int kFigureCurrentBufferSize = 170;
// around 12 bytes left free in the current buffer size - save version 0xa7, August 2025

struct FigureStore {
    int created_sequence = 0;
    std::vector<std::unique_ptr<Figure>> figures;
    struct PendingBuildingRefs {
        unsigned int building_id = 0;
        unsigned int immigrant_building_id = 0;
        unsigned int destination_building_id = 0;
    };
    std::vector<PendingBuildingRefs> pending_building_refs;
};

FigureStore data;

Figure *invalid_figure()
{
    if (data.figures.empty()) {
        data.figures.push_back(std::make_unique<Figure>(0));
    }
    return data.figures.front().get();
}

unsigned int save_id_for(const Building *building)
{
    return building ? building->id : 0;
}

Building *loaded_building_ref(unsigned int id)
{
    return Building::get(id);
}

Building *loaded_optional_building_ref(const Figure &figure, unsigned int id, const char *relation)
{
    Building *building = loaded_building_ref(id);
    if (id && !building) {
        char detail[256];
        snprintf(detail, sizeof(detail), "figure_id=%u figure_type=%u relation=%s saved_building_id=%u",
            figure.id(),
            static_cast<unsigned int>(figure.type),
            relation ? relation : "<unknown>",
            id);
        log_warning("Discarding invalid saved figure building reference", detail, 0);
    }
    return building;
}

void store_pending_building_refs(
    unsigned int figure_id,
    unsigned int building_id,
    unsigned int immigrant_building_id,
    unsigned int destination_building_id)
{
    if (data.pending_building_refs.size() <= figure_id) {
        data.pending_building_refs.resize(figure_id + 1);
    }
    data.pending_building_refs[figure_id] = { building_id, immigrant_building_id, destination_building_id };
}

void read_figure_relation(FigureRelation &relation, unsigned int id)
{
    if (!id || id >= Figure::count()) {
        relation.clear();
        return;
    }
    relation.retarget(*Figure::get(id));
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

int clear_known_building_refs_for_figure(Figure &figure)
{
    int cleared = 0;
    if (figure.building) {
        cleared |= figure.building->clear_figure_slot_if_matches(figure.id());
    }
    if (figure.destination_building) {
        cleared |= figure.destination_building->clear_figure_slot_if_matches(figure.id());
    }
    if (figure.immigrant_building) {
        cleared |= figure.immigrant_building->clear_figure_slot_if_matches(figure.id());
    }
    return cleared;
}

int figure_may_be_tracked_by_building_slot(const Figure &figure)
{
    if (figure.building || figure.destination_building || figure.immigrant_building) {
        return 1;
    }
    switch (static_cast<figure_type>(figure.type)) {
        case FIGURE_LABOR_SEEKER:
        case FIGURE_MARKET_SUPPLIER:
        case FIGURE_PRIEST_SUPPLIER:
        case FIGURE_BARKEEP_SUPPLIER:
        case FIGURE_MESS_HALL_SUPPLIER:
        case FIGURE_CARAVANSERAI_SUPPLIER:
        case FIGURE_LIGHTHOUSE_SUPPLIER:
        case FIGURE_SURGEON:
        case FIGURE_DOCTOR:
        case FIGURE_BALLISTA:
        case FIGURE_WATCHTOWER_ARCHER:
        case FIGURE_PRIEST:
        case FIGURE_ACTOR:
        case FIGURE_GLADIATOR:
        case FIGURE_LION_TAMER:
        case FIGURE_CHARIOTEER:
        case FIGURE_WATCHMAN:
        case FIGURE_MESS_HALL_FORT_SUPPLIER:
        case FIGURE_WAREHOUSEMAN:
        case FIGURE_CART_PUSHER:
        case FIGURE_WORK_CAMP_ARCHITECT:
        case FIGURE_WORK_CAMP_WORKER:
        case FIGURE_WORK_CAMP_SLAVE:
            return 1;
        default:
            return 0;
    }
}

void clear_any_building_refs_for_figure(const Figure &figure)
{
    const unsigned int figure_id = figure.id();
    Building::for_each([figure_id](Building *building) {
        building->clear_figure_slot_if_matches(figure_id);
    });
}

int loaded_figure_id_is_alive(unsigned int figure_id)
{
    if (!figure_id || figure_id >= Figure::count()) {
        return 0;
    }
    const Figure *figure = Figure::get(figure_id);
    return figure && figure->id() == figure_id && figure->state == FIGURE_STATE_ALIVE;
}

int building_uses_output_cart_slot(const ::building *b)
{
    if (!b || b->state != BUILDING_STATE_IN_USE) {
        return 0;
    }
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(b->type);
    if (!definition) {
        return 0;
    }
    for (const building_type_registry_impl::ProductionMethod *method : definition->production_methods()) {
        if (method && method->is_figure_delivery_output()) {
            return 1;
        }
    }
    return 0;
}

void clear_dead_loaded_producer_cart_slots()
{
    Building::for_each({ .hasProductionMethod = true }, [](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (!building_uses_output_cart_slot(b)) {
            return;
        }
        if (b->figure_id && !loaded_figure_id_is_alive(b->figure_id)) {
            b->figure_id = 0;
        }
    });
}

void resolve_loaded_migrant_house_refs()
{
    Building::for_each({ .hasHousing = true }, [](Building *house) {
        const unsigned int migrant_id = house ? house->immigrant_figure_id() : 0;
        if (!migrant_id || migrant_id >= Figure::count()) {
            return;
        }

        Figure *figure = Figure::get(migrant_id);
        if (!figure || figure->id() != migrant_id || figure->state != FIGURE_STATE_ALIVE) {
            return;
        }

        figure->immigrant_building = house;
        figure->destination_building = house;
        figure->last_destination_id = static_cast<int>(house->id);
    });
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
    buffer_write_i16(buf, f.cross_country_x);
    buffer_write_i16(buf, f.cross_country_y);
    buffer_write_i16(buf, f.cc_destination_x);
    buffer_write_i16(buf, f.cc_destination_y);
    buffer_write_i16(buf, f.cc_delta_x);
    buffer_write_i16(buf, f.cc_delta_y);
    buffer_write_i16(buf, f.cc_delta_xy);
    buffer_write_u8(buf, f.cc_direction);
    buffer_write_u8(buf, f.speed_multiplier);
    buffer_write_u32(buf, save_id_for(f.building));
    buffer_write_u32(buf, save_id_for(f.immigrant_building));
    buffer_write_u32(buf, save_id_for(f.destination_building));
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
    f.cross_country_x = buffer_read_i16(buf);
    f.cross_country_y = buffer_read_i16(buf);
    f.cc_destination_x = buffer_read_i16(buf);
    f.cc_destination_y = buffer_read_i16(buf);
    f.cc_delta_x = buffer_read_i16(buf);
    f.cc_delta_y = buffer_read_i16(buf);
    f.cc_delta_xy = buffer_read_i16(buf);
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
    read_figure_relation(f.target_figure, buffer_read_u16(buf));
    read_figure_relation(f.targeted_by_figure, buffer_read_u16(buf));
    f.created_sequence = buffer_read_u16(buf);
    f.target_figure_created_sequence = buffer_read_u16(buf);
    f.figures_on_same_tile_index = buffer_read_u8(buf);
    f.num_attackers = buffer_read_u8(buf);
    if (version <= SAVE_GAME_LAST_NO_FORMULAS_AND_MODEL_DATA) {
        read_figure_relation(f.attacker1, buffer_read_i16(buf));
        read_figure_relation(f.attacker2, buffer_read_i16(buf));
        read_figure_relation(f.opponent, buffer_read_i16(buf));
    } else {
        read_figure_relation(f.attacker1, buffer_read_i32(buf));
        read_figure_relation(f.attacker2, buffer_read_i32(buf));
        read_figure_relation(f.opponent, buffer_read_i32(buf));
    }
    if (version > SAVE_GAME_LAST_GLOBAL_BUILDING_INFO) {
        f.last_visited_index = buffer_read_i16(buf);
    }
    if (version > SAVE_GAME_LAST_GRANARY_WAREHOUSE_NON_ROADBLOCKS) {
        f.last_destination_id = buffer_read_i16(buf);
    }
    if (figure_buf_size > kFigureCurrentBufferSize) {
        buffer_skip(buf, figure_buf_size - kFigureCurrentBufferSize);
    }
}

} // namespace

Figure &FigureRelation::get()
{
    return *figure_;
}

const Figure &FigureRelation::get() const
{
    return *figure_;
}

void FigureRelation::retarget(Figure &figure)
{
    figure_ = &figure;
}

void FigureRelation::clear()
{
    figure_ = nullptr;
}

unsigned int FigureRelation::save_id() const
{
    return figure_ ? figure_->id() : 0;
}

unsigned int FigureRelation::debug_known_id() const
{
    if (!figure_) {
        return 0;
    }
    for (size_t i = 0; i < data.figures.size(); ++i) {
        if (data.figures[i].get() == figure_) {
            return data.figures[i]->id();
        }
    }
    return 0;
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
    f->created_sequence = static_cast<unsigned short>(data.created_sequence++);
    f->direction = static_cast<signed char>(dir);
    f->source_x = f->destination_x = f->previous_tile_x = f->x = static_cast<unsigned char>(x);
    f->source_y = f->destination_y = f->previous_tile_y = f->y = static_cast<unsigned char>(y);
    f->grid_offset = static_cast<short>(map_grid_offset(x, y));
    f->cross_country_x = static_cast<short>(figure_movement_tile_to_cross_country(x));
    f->cross_country_y = static_cast<short>(figure_movement_tile_to_cross_country(y));
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
    const int cleared_known_slot = clear_known_building_refs_for_figure(*this);
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
        case FIGURE_FORT_STANDARD:
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
    if (!cleared_known_slot && figure_may_be_tracked_by_building_slot(*this)) {
        clear_any_building_refs_for_figure(*this);
    }
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
    destination_building->release_input_storage_reservation(id());
    destination_building->release_legacy_storage_reservation(id());
}

int Figure::retarget_building(Building &from, Building &to)
{
    const unsigned int from_id = from.id;
    const unsigned int to_id = to.id;
    if (!from_id || !to_id || from_id == to_id) {
        return 0;
    }

    int changed = 0;
    int destination_changed = 0;
    if (building && building->id == from_id) {
        building = &to;
        changed = 1;
    }
    if (immigrant_building && immigrant_building->id == from_id) {
        immigrant_building = &to;
        changed = 1;
        destination_changed = 1;
    }
    if (destination_building && destination_building->id == from_id) {
        destination_building = &to;
        changed = 1;
        destination_changed = 1;
    }
    if (last_destination_id == static_cast<int>(from_id)) {
        last_destination_id = static_cast<int>(to_id);
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
    return (type >= FIGURE_ENEMY43_SPEAR && type <= FIGURE_ENEMY_CAESAR_LEGIONARY) ||
        type == FIGURE_ENEMY_CATAPULT;
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
    return type >= FIGURE_SHEEP && type <= FIGURE_ZEBRA;
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
    if (figure_type < FIGURE_NEW_TYPES || figure_type >= FIGURE_TYPE_MAX) {
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
        lang_text_draw(current_string_key(64, type), c->x_offset + 92, c->y_offset + 139,
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

int Figure::legacy_corpse_image_id(int base_image_id) const
{
    return base_image_id + figure_image_corpse_offset(const_cast<Figure *>(this));
}

int Figure::legacy_frame_image_id(int base_image_id, int frame_offset) const
{
    return base_image_id + frame_offset;
}

int Figure::legacy_static_frame_image_id(int base_image_id, int frame_count) const
{
    if (frame_count <= 0) {
        return base_image_id;
    }
    return base_image_id + static_cast<int>(id() % static_cast<unsigned int>(frame_count));
}

int Figure::legacy_directional_frame_image_id(
    int base_image_id,
    int frame_direction,
    int frame_offset,
    int frame_stride) const
{
    const int normalized_direction = figure_image_normalize_direction(frame_direction);
    return base_image_id + normalized_direction + frame_stride * frame_offset;
}

int Figure::legacy_image_id_for_direction_major_frame(
    int base_image_id,
    int frame_direction,
    int frame_offset,
    int direction_stride) const
{
    const int normalized_direction = figure_image_normalize_direction(frame_direction);
    return base_image_id + normalized_direction * direction_stride + frame_offset;
}

void Figure::select_legacy_corpse_image(int base_image_id)
{
    image_id = legacy_corpse_image_id(base_image_id);
}

void Figure::select_legacy_frame_image(int base_image_id, int frame_offset)
{
    image_id = legacy_frame_image_id(base_image_id, frame_offset);
}

void Figure::select_legacy_static_frame_image(int base_image_id, int frame_count)
{
    image_id = legacy_static_frame_image_id(base_image_id, frame_count);
}

void Figure::select_legacy_directional_frame_image(
    int base_image_id,
    int frame_direction,
    int frame_offset,
    int frame_stride)
{
    image_id = legacy_directional_frame_image_id(base_image_id, frame_direction, frame_offset, frame_stride);
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

void Figure::select_legacy_cart_overlay_base_image(int base_image_id)
{
    cart_image_id = base_image_id;
}

void Figure::select_legacy_cart_overlay_image(int base_image_id, int frame_direction)
{
    if (!base_image_id) {
        clear_legacy_cart_overlay_image();
        return;
    }

    const int normalized_direction = figure_image_normalize_direction(frame_direction);
    cart_image_id = base_image_id + normalized_direction + 8 * image_offset;
    figure_image_set_cart_offset(this, normalized_direction);
}

void Figure::finalize_legacy_cartpusher_overlay_image(int frame_direction, bool lift_full_food_load)
{
    if (!cart_image_id) {
        return;
    }

    const int normalized_direction = figure_image_normalize_direction(frame_direction);
    if (figure_type_registry_impl::FigureGraphics::resource_cart_marker_is(cart_image_id)) {
        cart_image_id = figure_type_registry_impl::FigureGraphics::resource_cart_marker_for_direction(normalized_direction);
    } else {
        cart_image_id += normalized_direction;
    }
    figure_image_set_cart_offset(this, normalized_direction);
    if (lift_full_food_load) {
        y_offset_cart -= 40;
    }
}

void Figure::init_scenario()
{
    figure_runtime_reset();
    map_road_service_history_clear();
    data.figures.clear();
    data.pending_building_refs.clear();
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
            case FIGURE_FORT_STANDARD:
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
    data.figures.reserve(figures_to_load > 0 ? figures_to_load : 1);
    data.pending_building_refs.resize(figures_to_load > 0 ? figures_to_load : 1);
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
    data.figures.resize(highest_id_in_use + 1);
    if (data.pending_building_refs.size() > data.figures.size()) {
        data.pending_building_refs.resize(data.figures.size());
    }
    invalid_figure();
}

void Figure::resolve_loaded_building_references()
{
    std::vector<unsigned int> invalid_figures;
    const size_t count = data.pending_building_refs.size() < data.figures.size() ?
        data.pending_building_refs.size() : data.figures.size();
    for (size_t i = 0; i < count; i++) {
        Figure *f = data.figures[i].get();
        if (!f) {
            continue;
        }
        FigureStore::PendingBuildingRefs &refs = data.pending_building_refs[i];
        if (!figure_runtime_resolve_loaded_owner(f, refs.building_id, &f->building)) {
            f->building = nullptr;
            f->immigrant_building = nullptr;
            f->destination_building = nullptr;
            invalid_figures.push_back(static_cast<unsigned int>(i));
            continue;
        }
        if (!f->building) {
            refs.building_id = 0;
        }
        f->immigrant_building = loaded_optional_building_ref(*f, refs.immigrant_building_id, "immigrant");
        f->destination_building = loaded_optional_building_ref(*f, refs.destination_building_id, "destination");
        if (!f->immigrant_building) {
            refs.immigrant_building_id = 0;
        }
        if (!f->destination_building) {
            refs.destination_building_id = 0;
        }
        if ((f->type == FIGURE_IMMIGRANT || f->type == FIGURE_HOMELESS) && f->last_destination_id <= 0) {
            const unsigned int house_id = refs.destination_building_id ?
                refs.destination_building_id :
                refs.immigrant_building_id;
            if (house_id) {
                f->last_destination_id = static_cast<int>(house_id);
            }
        }
    }
    for (auto it = invalid_figures.rbegin(); it != invalid_figures.rend(); ++it) {
        if (Figure *figure = Figure::get(*it)) {
            figure->remove();
        }
    }
    resolve_loaded_migrant_house_refs();
    // Saved ids remain available until the final saved-game initialization pass,
    // because building runtime initialization can rebuild Building wrappers.
    clear_dead_loaded_producer_cart_slots();
}
