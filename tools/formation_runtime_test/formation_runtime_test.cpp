#include "formation_runtime_test.h"

#include "building/barracks.h"
#include "building/construction_clear.h"
#include "building/dock.h"
#include "building/building_type_registry_internal.h"
#include "building/building_runtime.h"
#include "building/building_record.h"
#include "building/production_runtime.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/formation.h"
#include "figure/formation_legion.h"
#include "figure/route.h"
#include "figure/figure_runtime_native.h"
#include "figure/FigureGraphics.h"
#include "figuretype/trader.h"
#include "figuretype/soldier.h"
#include "empire/city.h"
#include "game/undo.h"
#include "map/grid.h"
#include "city/view.h"
#include "widget/sidebar/military.h"
#include "widget/sidebar/common.h"

#include <algorithm>
#include <cstdio>
#include <cmath>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void check_production_lifetime()
{
    const auto *type = building_type_registry_impl::definition_for_type(building_type_registry_impl::type_from_attr("shipyard"));
    require(type && type->has_native_production(), "Production lifetime fixture needs a shipyard definition");
    unsigned int reused_id = 0;
    for (int cycle = 0; cycle < 3; cycle++) {
        Building &owner = city_building_runtime().create(*type, 1, 1);
        require(!reused_id || owner.id == reused_id, "Production lifetime fixture did not reuse its deleted building slot");
        reused_id = owner.id;
        Production *production = production_runtime_impl::get_or_create_primary(owner);
        require(production && &production->building() == &owner, "Production retained a building copy instead of its live owner");
        require(production->max_progress() > 0, "Replacement shipyard could not query its current production context");
        const_cast<building *>(owner.record())->state = BUILDING_STATE_DELETED_BY_GAME;
        building_update_state();
    }
    std::printf("Production regression: repeated shipyard deletion and slot reuse retained current owner references\n");
}

void check_dock_removal()
{
    Figure *visitor = nullptr;
    for (unsigned int id = 1; id < Figure::count(); id++) {
        Figure *ship = Figure::get(id);
        if (ship->state == FIGURE_STATE_ALIVE && ship->type == FIGURE_TRADE_SHIP && ship->destination_building &&
            building_dock_is_working(*ship->destination_building)) {
            visitor = ship;
            break;
        }
    }
    require(visitor, "Dock deletion test needs a live ship assigned to a working dock");
    Building &removed = *visitor->destination_building;
    const int city_id = visitor->empire_city_id;
    const int route_id = empire_city_get_route_id(city_id);
    int other_docks = 0;
    Building::for_each([&](Building *dock) {
        if (dock != &removed && dock->is_in_use() && dock->matches("dock")) {
            building_dock_set_can_trade_with_route(route_id, *dock, 0);
            other_docks++;
        }
    });
    require(other_docks > 0, "Dock deletion test needs a surviving dock restricted to another route");
    std::vector<Figure *> affected;
    const int states[] = { FIGURE_ACTION_111_TRADE_SHIP_GOING_TO_DOCK, FIGURE_ACTION_112_TRADE_SHIP_MOORED,
        FIGURE_ACTION_113_TRADE_SHIP_GOING_TO_DOCK_QUEUE, FIGURE_ACTION_114_TRADE_SHIP_ANCHORED };
    for (int state : states) {
        Figure *ship = Figure::create(FIGURE_TRADE_SHIP, visitor->x, visitor->y, DIR_0_TOP);
        require(ship && ship->id(), "Cannot create dock deletion fixture ships");
        ship->empire_city_id = static_cast<unsigned char>(city_id);
        ship->loads_sold_or_carrying = static_cast<unsigned char>(figure_trade_sea_trade_units());
        ship->action_state = static_cast<unsigned char>(state);
        ship->set_destination_building(&removed);
        affected.push_back(ship);
    }
    std::vector<std::pair<Figure *, Building *>> unaffected;
    for (unsigned int id = 1; id < Figure::count(); id++) {
        Figure *ship = Figure::get(id);
        if (ship->state == FIGURE_STATE_ALIVE && ship->type == FIGURE_TRADE_SHIP && ship->destination_building &&
            ship->destination_building.get_ptr() != &removed) unaffected.emplace_back(ship, ship->destination_building);
    }
    std::printf("Dock regression: deleting dock %u with ships in all four active docking states; %zu other visitors\n",
        static_cast<unsigned int>(removed.id), unaffected.size());
    std::fflush(stdout);
    require(game_undo_start_build(building_type_registry_impl::type_from_attr("clear_land")), "Cannot begin the player demolition transaction");
    require(building_construction_clear_land(0, removed.x(), removed.y(), removed.x(), removed.y()) > 0,
        "Player demolition did not delete the test dock");
    require(!removed.is_in_use() && removed.relationships().empty(), "Deleted dock retained live relationships");
    for (Figure *ship : affected) {
        require(ship->state == FIGURE_STATE_ALIVE && ship->action_state == FIGURE_ACTION_115_TRADE_SHIP_LEAVING && !ship->destination_building,
            "The removal callback must immediately send a ship with no permitted surviving dock away");
    }
    for (const auto &[ship, dock] : unaffected) {
        require(ship->destination_building.get_ptr() == dock, "Deleting one dock disrupted a visitor to another dock");
    }
    for (int tick = 0; tick < 3000; tick++) {
        for (Figure *ship : affected) {
            if (ship->state == FIGURE_STATE_ALIVE) figure_trade_ship_action(ship);
        }
    }
    for (Figure *ship : affected) require(ship->state != FIGURE_STATE_ALIVE, "A ship without a permitted dock failed to finish leaving");
    std::printf("Dock regression: demolition returned, all four ship states left, and other dock visitors retained their destinations\n");
}

void make_idle(formation &owner)
{
    owner.in_distant_battle = owner.cursed_by_mars = 0;
    owner.recent_fight = owner.missile_fired = owner.missile_attack_timeout = 0;
    owner.set_station_origin(owner.x, owner.y);
    owner.for_each_alive_figure([](Figure &member, int) {
        member.action_state = FIGURE_ACTION_80_SOLDIER_AT_REST;
        member.formation_at_rest = 1;
        Route::remove(&member);
    });
}

void check_roster_and_recruitment(std::vector<formation *> &legions, const std::vector<Building *> &barracks)
{
    require(legions.size() >= 2, "Roster test needs a save with two populated forts");
    for (formation *owner : legions) {
        make_idle(*owner);
        owner->cursed_by_mars = 1;
    }
    formation &first = *legions[0];
    formation &second = *legions[1];
    first.cursed_by_mars = second.cursed_by_mars = 0;

    const std::vector<int> before = first.figures;
    Figure *casualty = Figure::get(first.roster_figure_id(1));
    require(casualty && casualty->id(), "Casualty test needs at least three contiguous members");
    first.recent_fight = 1;
    casualty->remove();
    formation_calculate_figures();
    require(first.roster_figure_id(1) == 0 && first.roster_compaction_pending, "Casualties must leave assignments stable during combat");
    first.recent_fight = 0;
    Figure *survivor = Figure::get(before[2]);
    survivor->action_state = FIGURE_ACTION_83_SOLDIER_GOING_TO_STANDARD;
    formation_calculate_figures();
    require(first.roster_figure_id(1) == 0 && first.roster_compaction_pending, "Movement must defer casualty compaction");
    survivor->action_state = FIGURE_ACTION_84_SOLDIER_AT_STANDARD;
    first.missile_attack_timeout = 1;
    formation_calculate_figures();
    require(first.roster_figure_id(1) == 0 && first.roster_compaction_pending, "Incoming fire must defer casualty compaction even while stopped");
    first.missile_attack_timeout = 0;
    formation_calculate_figures();
    require(!first.roster_compaction_pending && first.roster_figure_id(1) == before[2], "Idle casualties must compact survivor assignments from slot zero");
    const std::vector<int> compacted = first.figures;
    for (int repeat = 0; repeat < 20; repeat++) formation_calculate_figures();
    require(first.figures == compacted && !first.roster_compaction_pending, "Idle roster refresh must not reorder or continuously compact soldiers");
    std::printf("Formation regression: casualty compaction and stable idle roster passed\n");
    require(barracks.size() >= 2, "Recruitment test needs two road-connected barracks");

    if (second.num_figures >= second.barracks_recruit_capacity()) {
        Figure::get(second.first_figure_id())->remove();
        formation_calculate_figures();
    }
    for (Building *source : barracks) source->set_resource_amount(resource_weapons(), MAX_WEAPONS_BARRACKS);
    const int initial = first.num_figures + second.num_figures;
    const auto recruit = [](Building &source) {
        map_point road;
        require(source.has_road_access(&road), "Test barracks lost road access");
        return Barracks(source).create_soldier(road.x, road.y);
    };
    require(recruit(*barracks[0]) == 1, "First barracks failed to recruit");
    require(first.num_figures + second.num_figures == initial + 1, "One recruitment pulse must publish one soldier");
    require(recruit(*barracks[1]) == 1, "Second barracks failed to select a different available formation");
    require(first.num_figures + second.num_figures == initial + 2 && first.has_incoming_recruit() && second.has_incoming_recruit(),
        "Two barracks pulses must recruit two soldiers to different formations");
    require(!first.can_receive_recruit() && !second.can_receive_recruit() && recruit(*barracks[0]) == 0,
        "Recruit commute must reserve the formation until arrival, including academy travel");
    make_idle(first);
    make_idle(second);
    formation_calculate_figures();
    if (first.num_figures >= first.barracks_recruit_capacity()) {
        Figure::get(first.first_figure_id())->remove();
        formation_calculate_figures();
    }
    require(first.can_receive_recruit(), "A completed commute must release recruitment eligibility");
    std::printf("Formation regression: casualty compaction, stable idle roster, parallel barracks, and commute reservation passed\n");
}

void check_shapes_and_movement(formation &owner)
{
    make_idle(owner);
    formation_calculate_figures();
    while (owner.num_figures < owner.declared_capacity()) {
        const Figure *first = owner.first_alive_figure();
        Figure *member = Figure::create(owner.figure_type_id(), first->x, first->y, DIR_4_BOTTOM);
        require(member && member->id(), "Cannot create a full-capacity formation test fixture");
        member->formation_id = static_cast<short>(owner.id);
        member->action_state = FIGURE_ACTION_80_SOLDIER_AT_REST;
        member->formation_at_rest = 1;
        owner.refresh_published_figure(*member);
    }
    const auto *graphics = figure_type_registry_impl::FigureGraphics::for_type(FIGURE_FORT_STANDARD);
    const auto layers = graphics->standard_layers(owner.figure_type_id(), owner.is_halted, owner.legion_flag_id, std::clamp(20 - owner.morale / 5, 0, 20), 0);
    const auto flag = owner.standard.graphic_draw_request();
    const float units = static_cast<float>(layers.layers[0].slice.fixed_logical_size.width) / layers.layers[0].slice.width;
    require(flag.has_base_slice() && flag.is_semantically_complete() &&
        flag.scaled_sprite_offset_x() == std::lround(layers.sprite_offset.x * units / static_cast<float>(RENDER_LOGICAL_UNITS_PER_PIXEL)) &&
        flag.scaled_sprite_offset_y() == std::lround(layers.sprite_offset.y * units / static_cast<float>(RENDER_LOGICAL_UNITS_PER_PIXEL)),
        "Fort flag offsets must use the pole's declared logical scale");
    std::printf("Formation regression: fort flag uses scaled pole offset (%d,%d) at legacy tile anchor (29,15)\n", flag.scaled_sprite_offset_x(), flag.scaled_sprite_offset_y());
    const auto *type = owner.formation_type_definition;
    Building &ground = owner.fort()->Composition->require_child_for_role("mustering_ground", "formation regression");
    const int rotation = ground.Foundation->state().rotation();
    const int ground_width = ground.Foundation->width(rotation);
    const int ground_height = ground.Foundation->height(rotation);
    const Route::DistanceQuery route = Route::DistanceQuery::fromFigure(*owner.first_alive_figure());
    std::vector<const char *> shapes = {"column", "double_line_1", "double_line_2", "single_line_1", "single_line_2", "tortoise"};
    if (!formation_layout_registry_impl::find_layout("mop_up")->restore_when_idle) shapes.push_back("mop_up");
    int origin_x = -1;
    int origin_y = -1;
    for (int y = 10; y < map_grid_height() - 10 && origin_x < 0; y += 2) {
        for (int x = 10; x < map_grid_width() - 10 && origin_x < 0; x += 2) {
            bool reachable = true;
            for (const char *shape : shapes) {
                const auto *layout = formation_layout_registry_impl::find_layout(shape);
                for (int slot = 0; slot < owner.declared_capacity() && reachable; slot++) {
                    FormationSlotOffset offset;
                    require(type->resolve_station_offset(*layout, slot, owner.declared_capacity(), ground_width, ground_height, &offset), "Shape cannot resolve a declared station");
                    const int target_x = figure_movement_cross_country_to_tile(x * FIGURE_CROSS_COUNTRY_TILE_UNITS + offset.x);
                    const int target_y = figure_movement_cross_country_to_tile(y * FIGURE_CROSS_COUNTRY_TILE_UNITS + offset.y);
                    reachable = map_grid_is_inside(target_x, target_y, 1) && route.distanceTo(map_grid_offset(target_x, target_y)) > 0;
                }
                if (!reachable) break;
            }
            if (reachable) { origin_x = x; origin_y = y; }
        }
    }
    require(origin_x >= 0, "Movement test could not find reachable terrain for all six tactical shapes");
    owner.set_station_origin(origin_x, origin_y);
    owner.send_non_combat_figures_to_standard();
    owner.direction = DIR_4_BOTTOM;
    owner.has_military_training = 1;
    city_view_reset_orientation();
    widget_sidebar_military_enter(owner.id);
    widget_sidebar_military_draw_background();
    std::set<std::pair<int, int>> previous_endpoints;
    for (const char *shape : shapes) {
        if (owner.figure_type == FIGURE_FORT_LEGIONARY && std::string(shape).find("single_line") != 0) {
            const std::string key(shape);
            const int button = key == "tortoise" ? 0 : key == "column" ? 1 : key == "double_line_1" ? 2 : key == "double_line_2" ? 3 : 4;
            const int xs[] = {33, 83, 8, 58, 108};
            mouse click = {};
            click.x = sidebar_common_get_x_offset_expanded() + xs[button] + 10;
            click.y = 176 + 156 + (button < 2 ? 0 : 50) + 10;
            click.left.went_up = 1;
            require(widget_sidebar_military_handle_input(&click), "Sidebar did not consume the formation button click");
            require(owner.uses_layout(shape), "Sidebar click did not select the requested formation shape");
        } else {
            formation_legion_change_layout(&owner, shape);
        }
        bool all_stationed = false;
        for (int tick = 0; tick < 5000 && !all_stationed; tick++) {
            formation_calculate_figures();
            all_stationed = true;
            owner.for_each_alive_figure([&](Figure &member, int) {
                figure_soldier_action(&member);
                if (member.action_state != FIGURE_ACTION_84_SOLDIER_AT_STANDARD) all_stationed = false;
            });
        }
        require(all_stationed, "Formation movement did not converge within 5000 steps");
        std::set<std::pair<int, int>> endpoints;
        owner.for_each_alive_figure([&](Figure &member, int) {
            require(endpoints.emplace(member.cross_country_x, member.cross_country_y).second, "Soldiers stacked at one exact station");
            const auto result = owner.move_member_to_slot(member, FormationMemberDestination::Standard, 0, 0);
            require(result == FormationMemberMovementResult::Stationed, "An idle stationed member restarted routing");
        });
        require(previous_endpoints.empty() || endpoints != previous_endpoints, "Changing tactical layout failed to change its physical shape");
        if (owner.uses_layout("mop_up")) {
            formation_legion_update();
            require(owner.uses_layout("mop_up"), "The mod's loose formation unexpectedly restored its previous shape while idle");
            Figure *pursuer = owner.first_alive_figure();
            pursuer->action_state = FIGURE_ACTION_86_SOLDIER_MOPPING_UP;
            formation_legion_change_layout(&owner, "column");
            require(pursuer->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD, "Changing out of pursuit did not release the soldier to its new stations");
        }
        previous_endpoints = std::move(endpoints);
        std::printf("Formation regression: %s converged with %zu unique exact stations\n", shape, previous_endpoints.size());
    }

    owner.months_low_morale = 1;
    formation_legion_update();
    require(owner.is_commanded_home() && owner.has_figure_in_action(FIGURE_ACTION_148_FLEEING),
        "Morale retreat must publish the home command before soldiers start returning");
    owner.months_low_morale = 0;
    std::printf("Formation regression: morale retreat kept its command anchor consistent with returning soldiers\n");

    FormationType reduced = *type;
    reduced.set_grid(2, 2, FormationStationAlignment::ScaledTileAnchor);
    reduced.set_recruit_capacity(reduced.capacity());
    owner.bind_definition(reduced);
    Figure *excess = Figure::get(owner.roster_figure_id(4));
    const auto excess_result = owner.move_member_to_slot(*excess, FormationMemberDestination::Standard, 0, 0);
    require(excess_result == FormationMemberMovementResult::SettledUnplaced && !excess->is_dead(),
        "A reduced declared footprint must keep excess soldiers alive and explicitly unplaced");
    owner.mark_barracks_recruit_overflow_returning();
    Figure *far_member = Figure::get(owner.roster_figure_id(owner.num_figures - 1));
    const auto return_destination = figure_movement_destination_for_tile(far_member->x, far_member->y, FigureMovementPlane::Ground);
    for (int tick = 0; tick < 16 && !excess->routing_path_id; tick++) excess->move_ticks_to(return_destination, 1);
    require(excess->routing_path_id != 0, "Overflow route fixture did not begin routing");
    const unsigned int return_route = excess->routing_path_id;
    for (int repeat = 0; repeat < 20; repeat++) owner.mark_barracks_recruit_overflow_returning();
    require(excess->routing_path_id == return_route, "Repeated overflow checks must preserve an existing return route");
    owner.bind_definition(*type);
    std::printf("Formation regression: reduced-capacity roster retained excess soldiers without aliasing stations\n");
}

} // namespace

bool run_formation_runtime_test()
{
    try {
        formation_calculate_figures();
        std::vector<formation *> legions;
        for (int id = 1; id < formation_count(); id++) {
            formation *owner = formation_get(id);
            if (owner->in_use && owner->is_legion && owner->num_figures >= 3) {
                legions.push_back(owner);
                std::printf("Loaded legion %d: type=%d layout=%s trained=%d fight=%d fired=%d incoming=%d first_action=%d\n", id,
                    owner->figure_type_id(), owner->layout_definition->key(), owner->has_military_training, owner->recent_fight,
                    owner->missile_fired, owner->missile_attack_timeout, owner->first_alive_figure()->action_state);
            }
        }
        std::vector<Building *> barracks;
        Building::for_each([&](Building *building) {
            if (building->is_in_use() && building->matches("barracks") && building->has_road_access(nullptr)) barracks.push_back(building);
        });
        std::printf("Formation regression fixture: %zu populated legions, %zu road-connected barracks\n", legions.size(), barracks.size());
        require(!legions.empty(), "Formation test needs a populated legion");
        for (formation *owner : legions) {
            if (owner->is_commanded_home()) continue;
            std::set<std::pair<int, int>> before;
            owner->for_each_alive_figure([&](Figure &member, int) { before.emplace(member.cross_country_x, member.cross_country_y); });
            formation_legion_change_layout(owner, owner->uses_layout("double_line_1") ? "double_line_2" : "double_line_1");
            for (int tick = 0; tick < 2000; tick++) {
                formation_calculate_figures();
                owner->for_each_alive_figure([](Figure &member, int) { figure_soldier_action(&member); });
            }
            std::set<std::pair<int, int>> after;
            owner->for_each_alive_figure([&](Figure &member, int) { after.emplace(member.cross_country_x, member.cross_country_y); });
            std::printf("Deployed legion %u: %zu stations before, %zu after, changed=%d\n", owner->id, before.size(), after.size(), before != after);
            require(before != after, "Changing a saved deployed legion's shape did not move its soldiers");
        }
        check_shapes_and_movement(*legions[0]);
        check_roster_and_recruitment(legions, barracks);
        check_production_lifetime();
        check_dock_removal();
        return true;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Formation regression failed: %s\n", error.what());
        return false;
    }
}
