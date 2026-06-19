#include "figure/figure.h"

#include "building/building_record.h"
#include "building/monument.h"
#include "building/properties.h"
#include "city/emperor.h"
#include "core/log.h"
#include "core/random.h"
#include "empire/city.h"
#include "figure/figure_runtime_api.h"
#include "figure/name.h"
#include "figure/trader.h"
#include "figure/visited_buildings.h"
#include "game/resource.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/road_service_history.h"

#include <cstdlib>
#include <memory>
#include <vector>

void figure_route_remove(Figure *f);

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

unsigned int save_id_for(const Building &building)
{
    return building.id();
}

Building loaded_building_ref(unsigned int id)
{
    return id ? Building(building_get(id)) : Building(nullptr);
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

void clear_building_figure_slot_if_matches(::building *b, unsigned int figure_id)
{
    if (!b || !figure_id) {
        return;
    }
    if (b->figure_id == figure_id) {
        b->figure_id = 0;
    }
    if (b->figure_id2 == figure_id) {
        b->figure_id2 = 0;
    }
    if (b->figure_id4 == figure_id) {
        b->figure_id4 = 0;
    }
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
    buffer_write_u16(buf, f.target_figure.save_id());
    buffer_write_u16(buf, f.targeted_by_figure.save_id());
    buffer_write_u16(buf, f.created_sequence);
    buffer_write_u16(buf, f.target_figure_created_sequence);
    buffer_write_u8(buf, f.figures_on_same_tile_index);
    buffer_write_u8(buf, f.num_attackers);
    buffer_write_i32(buf, f.attacker1.save_id());
    buffer_write_i32(buf, f.attacker2.save_id());
    buffer_write_i32(buf, f.opponent.save_id());
    buffer_write_i16(buf, f.last_visited_index);
    buffer_write_i16(buf, f.last_destination_id);
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
        f.resource_id = resource;
    } else {
        f.resource_id = resource_remap(resource);
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
    f.collecting_item_id = (version <= SAVE_GAME_LAST_STATIC_RESOURCES) ?
        get_resource_id(static_cast<figure_type>(f.type), buffer_read_u8(buf)) :
        resource_remap(buffer_read_u8(buf));
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
    f->type = figure_type;
    f->use_cross_country = 0;
    f->is_friendly = 1;
    f->created_sequence = data.created_sequence++;
    f->direction = dir;
    f->source_x = f->destination_x = f->previous_tile_x = f->x = x;
    f->source_y = f->destination_y = f->previous_tile_y = f->y = y;
    f->grid_offset = map_grid_offset(x, y);
    f->cross_country_x = 15 * x;
    f->cross_country_y = 15 * y;
    f->progress_on_tile = 15;
    f->progress_to_next_tick = 0;
    f->dont_draw_elevated = 0;
    f->disallow_diagonal = 0;
    f->resource_id = RESOURCE_NONE;
    f->wait_ticks = 0;
    random_generate_next();
    f->name = figure_name_get(figure_type, static_cast<enemy_type_t>(0));
    f->phrase_sequence_city = f->phrase_sequence_exact = random_byte() & 3;
    map_figure_add(f);
    if (figure_type == FIGURE_TRADE_CARAVAN || figure_type == FIGURE_TRADE_SHIP ||
        figure_type == FIGURE_NATIVE_TRADER) {
        f->trader_id = trader_create();
    }
    figure_runtime_on_created(f);
    return f;
}

void Figure::remove()
{
    ::building *b = building_get(building.id());
    switch (type) {
        case FIGURE_LABOR_SEEKER:
        case FIGURE_MARKET_SUPPLIER:
        case FIGURE_PRIEST_SUPPLIER:
        case FIGURE_BARKEEP_SUPPLIER:
        case FIGURE_MESS_HALL_SUPPLIER:
        case FIGURE_CARAVANSERAI_SUPPLIER:
        case FIGURE_LIGHTHOUSE_SUPPLIER:
            clear_building_figure_slot_if_matches(b, id());
            break;
        case FIGURE_SURGEON:
        case FIGURE_DOCTOR:
            clear_building_figure_slot_if_matches(building_get(destination_building.id()), id());
            break;
        case FIGURE_BALLISTA:
        case FIGURE_WATCHTOWER_ARCHER:
            if (b) {
                b->figure_id4 = 0;
            }
            break;
        case FIGURE_DOCKER:
        case FIGURE_DEPOT_CART_PUSHER:
            if (b) {
                for (int i = 0; i < 3; i++) {
                    if (b->data.distribution.cartpusher_ids[i] == id()) {
                        b->data.distribution.cartpusher_ids[i] = 0;
                    }
                }
            }
            break;
        case FIGURE_PRIEST:
            if (destination_building.id()) {
                clear_building_figure_slot_if_matches(b, id());
            }
            break;
        case FIGURE_ACTOR:
        case FIGURE_GLADIATOR:
        case FIGURE_LION_TAMER:
            clear_building_figure_slot_if_matches(b, id());
            break;
        case FIGURE_ENEMY_CAESAR_LEGIONARY:
            city_emperor_mark_soldier_killed();
            break;
        case FIGURE_CHARIOTEER:
            if (b && building_is_neptune_temple(b->type) && id() == b->figure_id2) {
                b->figure_id2 = 0;
            } else if (b && id() == b->figure_id) {
                b->figure_id = 0;
            }
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
        case FIGURE_WATCHMAN:
            if (b && id() == b->figure_id2) {
                b->figure_id2 = 0;
            } else if (b && id() == b->figure_id) {
                b->figure_id = 0;
            }
            break;
        case FIGURE_MESS_HALL_FORT_SUPPLIER:
            if (::building *fort = building_get(destination_building.id())) {
                if (fort->figure_id2 == id()) {
                    fort->figure_id2 = 0;
                }
            }
            break;
        case FIGURE_WAREHOUSEMAN:
            if (b && id() == b->figure_id4) {
                b->figure_id4 = 0;
            } else if (b && id() == b->figure_id) {
                b->figure_id = 0;
            }
            break;
        case FIGURE_CART_PUSHER:
        case FIGURE_WORK_CAMP_ARCHITECT:
        case FIGURE_WORK_CAMP_WORKER:
        case FIGURE_WORK_CAMP_SLAVE:
            building_monument_remove_delivery(id());
            if (b) {
                b->figure_id = 0;
            }
            break;
        default:
            if (b) {
                b->figure_id = 0;
            }
            break;
    }
    if (empire_city_id) {
        empire_city_remove_trader(empire_city_id, id());
    }
    if (::building *immigrant = building_get(immigrant_building.id())) {
        immigrant->immigrant_figure_id = 0;
    }
    figure_visited_buildings_remove_list(last_visited_index);
    figure_route_remove(this);
    map_figure_delete(this);
    figure_runtime_on_deleted(this);

    const unsigned int slot = id();
    reset(slot);
    trim_dead_tail();
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

int Figure::target_is_alive() const
{
    if (!target_figure.save_id()) {
        return 0;
    }
    const Figure &target = target_figure.get();
    return !target.is_dead() && target.created_sequence == target_figure_created_sequence;
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
    const size_t count = data.pending_building_refs.size() < data.figures.size() ?
        data.pending_building_refs.size() : data.figures.size();
    for (size_t i = 0; i < count; i++) {
        Figure *f = data.figures[i].get();
        if (!f) {
            continue;
        }
        const FigureStore::PendingBuildingRefs &refs = data.pending_building_refs[i];
        f->building = loaded_building_ref(refs.building_id);
        f->immigrant_building = loaded_building_ref(refs.immigrant_building_id);
        f->destination_building = loaded_building_ref(refs.destination_building_id);
    }
    data.pending_building_refs.clear();
}
