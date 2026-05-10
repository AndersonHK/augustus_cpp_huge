#include "figure/figure_runtime_native.h"

#include "core/crash_context.h"
#include "map/routing_distance.h"

extern "C" {
#include "assets/assets.h"
#include "building/list.h"
#include "building/local_workforce.h"
#include "building/maintenance.h"
#include "building/building.h"
#include "building/distribution.h"
#include "building/granary.h"
#include "building/market.h"
#include "building/monument.h"
#include "building/warehouse.h"
#include "city/festival.h"
#include "city/figures.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/action.h"
#include "figure/combat.h"
#include "figure/enemy_army.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/route.h"
#include "figuretype/supplier.h"
#include "game/time.h"
#include "game/resource.h"
#include "map/building.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/road_access.h"
#include "map/routing_terrain.h"
#include "map/terrain.h"
#include "scenario/gladiator_revolt.h"
#include "sound/effect.h"
}

#include <cstdint>
#include <limits>
#include <memory>

namespace figure_runtime_native_impl {

constexpr int kInfiniteDistance = 10000;
constexpr int kRecalculateEnemyLocationTicks = 30;

bool owner_state_matches(const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (!owner) {
        return false;
    }

    switch (owner_binding.required_owner_state) {
        case figure_type_registry_impl::OwnerStateRequirement::Any:
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUse:
            if (owner->state != BUILDING_STATE_IN_USE) {
                return false;
            }
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUseOrMothballed:
            if (owner->state != BUILDING_STATE_IN_USE && owner->state != BUILDING_STATE_MOTHBALLED) {
                return false;
            }
            break;
    }

    if (owner_binding.required_building_type != BUILDING_ANY &&
        owner_binding.required_building_type != owner->type) {
        return false;
    }
    return true;
}

figure *slot_figure(const building *owner, figure_type_registry_impl::FigureSlot slot)
{
    if (!owner) {
        return nullptr;
    }

    unsigned int figure_id = 0;
    switch (slot) {
        case figure_type_registry_impl::FigureSlot::Primary:
            figure_id = owner->figure_id;
            break;
        case figure_type_registry_impl::FigureSlot::Secondary:
            figure_id = owner->figure_id2;
            break;
        case figure_type_registry_impl::FigureSlot::Quaternary:
            figure_id = owner->figure_id4;
            break;
        case figure_type_registry_impl::FigureSlot::None:
        default:
            return nullptr;
    }
    return figure_id ? figure_get(figure_id) : nullptr;
}

bool slot_matches(const figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (owner_binding.slot == figure_type_registry_impl::FigureSlot::None) {
        return true;
    }

    figure *tracked = slot_figure(owner, owner_binding.slot);
    return tracked && f && tracked->id == f->id && tracked->created_sequence == f->created_sequence;
}

bool owner_binding_matches(const figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    return owner_state_matches(owner, owner_binding) && slot_matches(f, owner, owner_binding);
}

bool owner_binding_requires_owner(const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    // An "any/none/any" owner node means the profile only wants a source id
    // for save/debug context. Transient walkers can outlive that building.
    return owner_binding.slot != figure_type_registry_impl::FigureSlot::None ||
        owner_binding.required_building_type != BUILDING_ANY ||
        owner_binding.required_owner_state != figure_type_registry_impl::OwnerStateRequirement::Any;
}

void retire_unsupported_native_state(figure *f, const char *native_class)
{
    const ErrorContextScope scope("Native FigureType action", native_class);
    error_context_report_warning(
        "Native FigureType walker reached an unsupported action state.",
        "The legacy fallback for this walker has been retired; the invalid figure will be removed.");
    if (f) {
        f->state = FIGURE_STATE_DEAD;
    }
}

road_service_effect primary_service_effect_for_profile(
    const figure_type_registry_impl::FigureTypeProfile *profile)
{
    if (!profile) {
        return ROAD_SERVICE_EFFECT_NONE;
    }
    // Service identity is profile-owned. Buildings choose a profile at spawn time,
    // and smart service pathing/road history only read this explicit effect.
    return profile->pathing_policy().effect;
}

bool is_road_history_tile(int grid_offset)
{
    return map_terrain_is(grid_offset, TERRAIN_ROAD) ||
        map_routing_citizen_is_road(grid_offset);
}

int image_base_for_definition(const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    // Most walkers start at the image group base. A few XML profiles address a
    // row inside a shared group, so keep that offset centralized.
    if (!definition) {
        return 0;
    }
    const figure_type_registry_impl::GraphicsPolicy &graphics = definition->graphics_policy();
    return image_group(graphics.image_group) + graphics.image_group_offset;
}

int corpse_image_base_for_definition(const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    if (!definition) {
        return 0;
    }
    const figure_type_registry_impl::GraphicsPolicy &graphics = definition->graphics_policy();
    if (graphics.corpse_image_group) {
        return image_group(graphics.corpse_image_group) + graphics.corpse_image_group_offset;
    }
    return image_base_for_definition(definition) + 96;
}

class RoamingServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner || !owner->id) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        if (f->type == FIGURE_PRIEST && f->destination_building_id) {
            return 0;
        }

        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        const figure_type_registry_impl::GraphicsPolicy &graphics = definition()->graphics_policy();

        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, graphics.max_image_offset);

        if (profile()->pathing_policy().mode == &figure_type_registry_impl::NearestUnemployed &&
            building_local_workforce_labor_seeker_is_workforce(f)) {
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= movement.max_roam_length) {
                building_local_workforce_cancel_labor_seeker(f);
            } else {
                if (!building_local_workforce_prepare_labor_seeker_target(f)) {
                    building_local_workforce_cancel_labor_seeker(f);
                } else {
                    figure_movement_move_ticks(f, movement.roam_ticks);
                    if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                        building_local_workforce_labor_seeker_arrived(f);
                    } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                        building_local_workforce_labor_seeker_failed(f);
                    }
                }
            }
            figure_image_update(f, image_base_for_definition(definition()));
            return 1;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_125_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    if (movement.return_mode != figure_type_registry_impl::ReturnMode::ReturnToOwnerRoad) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        int x_road = 0;
                        int y_road = 0;
                        if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                            f->action_state = FIGURE_ACTION_126_ROAMER_RETURNING;
                            f->destination_x = x_road;
                            f->destination_y = y_road;
                            figure_route_remove(f);
                            f->roam_length = 0;
                        } else {
                            f->state = FIGURE_STATE_DEAD;
                        }
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_126_ROAMER_RETURNING:
                if (movement.return_mode != figure_type_registry_impl::ReturnMode::ReturnToOwnerRoad) {
                    return 0;
                }
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE ||
                    f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        figure_image_update(f, image_base_for_definition(definition()));
        return 1;
    }
};

// Ownerless ambient walkers have two generic lifetime policies: stand still on
// their spawn tile, or roam briefly and expire instead of returning home.
class TransientWandererFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition() || !profile()) {
            return 0;
        }

        const figure_type_registry_impl::OwnerBinding &owner_binding = profile()->owner_binding();
        if (owner_binding_requires_owner(owner_binding)) {
            building *owner = building_get(f->building_id);
            if (!owner || !owner->id || !owner_binding_matches(f, owner, owner_binding)) {
                f->state = FIGURE_STATE_DEAD;
                update_image(f);
                return 1;
            }
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        const figure_type_registry_impl::GraphicsPolicy &graphics = definition()->graphics_policy();

        f->cart_image_id = 0;
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, graphics.max_image_offset);

        if (profile()->pathing_policy().mode == &figure_type_registry_impl::StandStill) {
            execute_stand_still(f, movement);
            update_image(f);
            return 1;
        }
        if (profile()->pathing_policy().mode != &figure_type_registry_impl::TransientWander) {
            retire_unsupported_native_state(f, "transient_wanderer");
            update_image(f);
            return 1;
        }

        if (f->action_state == 0) {
            f->action_state = FIGURE_ACTION_125_ROAMING;
            figure_movement_init_roaming(f);
            f->roam_length = 0;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_125_ROAMING:
                f->is_ghost = 0;
                f->wait_ticks++;
                f->roam_length++;
                if (f->wait_ticks > movement.max_roam_length) {
                    f->state = FIGURE_STATE_DEAD;
                    f->image_offset = 0;
                } else {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            default:
                retire_unsupported_native_state(f, "transient_wanderer");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static void clear_stand_still_movement(figure *f)
    {
        // Buggy transient saves may already contain a roaming route. Preserve
        // the current tile, but discard destination state so the policy is idle.
        figure_route_remove(f);
        f->routing_path_current_tile = 0;
        f->routing_path_length = 0;
        f->destination_x = f->x;
        f->destination_y = f->y;
        f->destination_grid_offset = f->grid_offset;
        f->previous_tile_x = f->x;
        f->previous_tile_y = f->y;
        f->previous_tile_direction = f->direction;
        f->progress_on_tile = 15;
        f->roam_length = 0;
        f->roam_choose_destination = 0;
        f->roam_random_counter = 0;
        f->roam_turn_direction = 0;
        f->roam_ticks_until_next_turn = 0;
    }

    static void tick_stand_still_lifetime(
        figure *f,
        const figure_type_registry_impl::MovementProfile &movement)
    {
        f->is_ghost = 0;
        f->wait_ticks++;
        if (f->wait_ticks > movement.max_roam_length) {
            f->state = FIGURE_STATE_DEAD;
            f->image_offset = 0;
        }
    }

    void execute_stand_still(
        figure *f,
        const figure_type_registry_impl::MovementProfile &movement) const
    {
        switch (f->action_state) {
            case 0:
                tick_stand_still_lifetime(f, movement);
                break;
            case FIGURE_ACTION_125_ROAMING:
                clear_stand_still_movement(f);
                f->action_state = 0;
                tick_stand_still_lifetime(f, movement);
                break;
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            default:
                retire_unsupported_native_state(f, "transient_wanderer");
                break;
        }
    }

    void update_image(figure *f) const
    {
        const figure_type_registry_impl::GraphicsPolicy &graphics = definition()->graphics_policy();
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = corpse_image_base_for_definition(definition()) + figure_image_corpse_offset(f);
            return;
        }
        if (graphics.static_frame_count > 0) {
            f->image_id = image_base_for_definition(definition()) + (f->id % graphics.static_frame_count);
            return;
        }
        figure_image_update(f, image_base_for_definition(definition()));
    }
};

class MarketSupplierFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE:
                move_to_storage(f, movement.roam_ticks);
                break;
            case FIGURE_ACTION_146_SUPPLIER_RETURNING:
                return_to_market(f, movement.roam_ticks);
                break;
            default:
                retire_unsupported_native_state(f, "market_supplier");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static int take_food_from_granary(figure *f, int market_id, int granary_id)
    {
        const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
        if (!resource_is_food(resource)) {
            return 0;
        }

        building *granary = building_get(granary_id);
        building *market = building_get(market_id);
        if (!granary || !market) {
            return 0;
        }

        const int market_units = market->resources[resource];
        const int max_units = MAX_FOOD_STOCKED_MARKET - market_units;
        const int granary_loads_stored = building_granary_count_available_resource(granary, resource, 1);
        const int granary_loads_take = granary_loads_stored > (max_units / RESOURCE_ONE_LOAD) ?
            (max_units / RESOURCE_ONE_LOAD) : granary_loads_stored;
        if (!granary_loads_take) {
            return 0;
        }

        const int amount_taken = building_granary_try_remove_resource(granary, resource, granary_loads_take);
        int previous_boy = f->id;
        for (int i = 0; i < amount_taken; i++) {
            previous_boy = figure_supplier_create_delivery_boy(previous_boy, f->id, FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int take_resource_from_generic_building(figure *f, int building_id)
    {
        building *source = building_get(building_id);
        if (!source) {
            return 0;
        }

        const int num_loads = source->resources[RESOURCE_WINE] < 2 ? source->resources[RESOURCE_WINE] : 2;
        if (num_loads <= 0) {
            return 0;
        }

        source->resources[RESOURCE_WINE] -= num_loads;
        int boy = figure_supplier_create_delivery_boy(f->id, f->id, FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy, f->id, FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int take_resource_from_warehouse(figure *f, int warehouse_id)
    {
        building *warehouse = building_get(warehouse_id);
        if (!warehouse) {
            return 0;
        }
        if (warehouse->type != BUILDING_WAREHOUSE) {
            return take_resource_from_generic_building(f, warehouse_id);
        }

        const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
        const int stored = building_warehouse_get_available_amount(warehouse, resource);
        const int num_loads = stored < 2 ? stored : 2;
        if (num_loads <= 0) {
            return 0;
        }

        building_warehouse_try_remove_resource(warehouse, resource, num_loads);
        int boy = figure_supplier_create_delivery_boy(f->id, f->id, FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy, f->id, FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int change_destination(figure *f, int destination_building_id)
    {
        figure_route_remove(f);
        f->destination_building_id = destination_building_id;
        building *destination = building_get(destination_building_id);
        if (!destination) {
            return 0;
        }

        map_point road = { 0, 0 };
        int has_road_access = 0;
        if (destination->type == BUILDING_WAREHOUSE) {
            has_road_access = map_has_road_access_warehouse(destination->x, destination->y, &road);
        } else if (destination->type == BUILDING_GRANARY) {
            has_road_access = map_has_road_access_granary(destination->x, destination->y, &road);
        }
        if (!has_road_access) {
            return 0;
        }

        f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
        f->destination_x = road.x;
        f->destination_y = road.y;
        return 1;
    }

    static bool is_better_destination(figure *f, resource_type resource, resource_storage_info *info)
    {
        building *old_destination = building_get(f->destination_building_id);
        if (!building_is_active(old_destination)) {
            return true;
        }
        if (old_destination->type == BUILDING_GRANARY && old_destination->resources[resource] <= 0) {
            return true;
        }
        if (old_destination->type == BUILDING_WAREHOUSE &&
            building_warehouse_get_amount(old_destination, resource) <= 0) {
            return true;
        }

        const int old_distance = building_dist(f->x, f->y, 1, 1, old_destination);
        return info->min_distance <= old_distance / 2;
    }

    static int recalculate_destination(figure *f)
    {
        const resource_type item = static_cast<resource_type>(f->collecting_item_id);
        building *market = building_get(f->building_id);
        if (!market) {
            return 0;
        }

        resource_storage_info info[RESOURCE_MAX] = { 0 };
        const int road_network = market->road_network_id;
        if (!building_market_get_needed_inventory(market, info) ||
            !building_distribution_get_resource_storages_for_figure(
                info,
                BUILDING_MARKET,
                road_network,
                f,
                config_get(CONFIG_GP_CH_MARKET_RANGE) ? MARKET_MAX_DISTANCE : map_data.width)) {
            return 0;
        }

        if (f->building_id == info[item].building_id ||
            f->destination_building_id == info[item].building_id) {
            return 1;
        }

        if (info[item].building_id) {
            return is_better_destination(f, item, &info[item]) ?
                change_destination(f, info[item].building_id) : 1;
        }

        const resource_type fetch_inventory = building_market_fetch_inventory(market, info);
        if (fetch_inventory == RESOURCE_NONE) {
            return 0;
        }

        market->data.market.fetch_inventory_id = fetch_inventory;
        f->collecting_item_id = fetch_inventory;
        return change_destination(f, info[fetch_inventory].building_id);
    }

    static void move_to_storage(figure *f, int roam_ticks)
    {
        figure_movement_move_ticks(f, roam_ticks);
        if (f->direction == DIR_FIGURE_AT_DESTINATION) {
            f->wait_ticks = 0;
            f->previous_tile_x = f->x;
            f->previous_tile_y = f->y;
            const unsigned int id = f->id;
            if (!resource_is_food(static_cast<resource_type>(f->collecting_item_id))) {
                if (!take_resource_from_warehouse(f, f->destination_building_id)) {
                    f->state = FIGURE_STATE_DEAD;
                }
            } else if (!take_food_from_granary(f, f->building_id, f->destination_building_id)) {
                f->state = FIGURE_STATE_DEAD;
            }

            f = figure_get(id);
            f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
        } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
            f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
            figure_route_remove(f);
        } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
            f->wait_ticks = 0;
            if (!recalculate_destination(f)) {
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->collecting_item_id = RESOURCE_NONE;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                figure_route_remove(f);
            }
        }
    }

    static void return_to_market(figure *f, int roam_ticks)
    {
        figure_movement_move_ticks(f, roam_ticks);
        if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
            f->state = FIGURE_STATE_DEAD;
        } else if (f->direction == DIR_FIGURE_REROUTE) {
            figure_route_remove(f);
        }
    }

    void update_image(figure *f) const
    {
        const int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_death_01") +
                figure_image_corpse_offset(f);
        } else {
            f->image_id = assets_get_image_id("Walkers", "marketbuyer_ne_01") +
                dir * 12 + f->image_offset;
        }
    }
};

class DeliveryFollowerFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->is_ghost = 0;
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);
        f->cart_image_id = 0;

        figure *leader = figure_get(f->leading_figure_id);
        if (f->leading_figure_id <= 0 || leader->action_state == FIGURE_ACTION_149_CORPSE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->state == FIGURE_STATE_ALIVE) {
            if (can_follow(leader)) {
                figure_movement_follow_ticks(f, movement.roam_ticks);
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
        } else {
            building_get(f->building_id)->resources[f->collecting_item_id] += RESOURCE_ONE_LOAD;
            f->state = FIGURE_STATE_DEAD;
        }

        if (leader->is_ghost && !leader->height_adjusted_ticks) {
            f->is_ghost = 1;
        }
        update_image(f);
        return 1;
    }

private:
    static bool can_follow(const figure *leader)
    {
        switch (leader->type) {
            case FIGURE_MARKET_SUPPLIER:
            case FIGURE_DELIVERY_BOY:
            case FIGURE_MESS_HALL_SUPPLIER:
            case FIGURE_MESS_HALL_COLLECTOR:
            case FIGURE_PRIEST_SUPPLIER:
            case FIGURE_PRIEST:
            case FIGURE_BARKEEP_SUPPLIER:
            case FIGURE_CARAVANSERAI_SUPPLIER:
            case FIGURE_CARAVANSERAI_COLLECTOR:
                return true;
            default:
                return false;
        }
    }

    void update_image(figure *f) const
    {
        const int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);
        const int base_image_id = image_base_for_definition(definition());
        if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = base_image_id + 96 + figure_image_corpse_offset(f);
        } else {
            f->image_id = base_image_id + dir + 8 * f->image_offset;
        }
    }
};

class EngineerServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_60_ENGINEER_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING;
                        figure_movement_set_cross_country_destination(f, x_road, y_road);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == f->building_id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_62_ENGINEER_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_62_ENGINEER_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_63_ENGINEER_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_63_ENGINEER_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "engineer_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    void update_image(figure *f) const
    {
        figure_image_update(f, image_base_for_definition(definition()));
    }
};

class PrefectServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        if (!fight_enemy(f)) {
            fight_fire(f, false);
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_70_PREFECT_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_71_PREFECT_ENTERING_EXITING;
                        figure_movement_set_cross_country_destination(f, x_road, y_road);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == f->building_id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_72_PREFECT_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_72_PREFECT_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_73_PREFECT_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_71_PREFECT_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_75_PREFECT_AT_FIRE;
                    figure_route_remove(f);
                    f->roam_length = 0;
                    f->wait_ticks = game_time_scale_legacy_day_ticks(50);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS && !fight_fire(f, true) && !in_combat(f)) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        f->wait_ticks = 0;
                        figure_route_remove(f);
                    }
                }
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                extinguish_fire(f);
                break;
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                if (!figure_target_is_alive(f) && !fight_enemy(f)) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                figure_movement_move_ticks_with_percentage(f, movement.roam_ticks, 20);
                if (f->direction == DIR_FIGURE_AT_DESTINATION || f->wait_ticks++ > kRecalculateEnemyLocationTicks) {
                    figure *target = figure_get(f->target_figure_id);
                    f->destination_x = target->x;
                    f->destination_y = target->y;
                    f->wait_ticks = 0;
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "prefect_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static int get_enemy_distance(figure *f, int x, int y)
    {
        if (f->type == FIGURE_RIOTER || f->type == FIGURE_ENEMY54_GLADIATOR) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_CRIMINAL_LOOTER || f->type == FIGURE_CRIMINAL_ROBBER) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (figure_is_enemy(f)) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_WOLF) {
            return 4 * calc_maximum_distance(x, y, f->x, f->y);
        }
        return kInfiniteDistance;
    }

    static int get_nearest_enemy(int x, int y, int *distance)
    {
        int min_enemy_id = 0;
        int min_dist = kInfiniteDistance;
        for (unsigned int i = 1; i < figure_count(); i++) {
            figure *f = figure_get(i);
            if (figure_is_dead(f)) {
                continue;
            }
            int dist = get_enemy_distance(f, x, y);
            if (dist != kInfiniteDistance && f->targeted_by_figure_id) {
                figure *pursuiter = figure_get(f->targeted_by_figure_id);
                if (get_enemy_distance(f, pursuiter->x, pursuiter->y) < dist * 2) {
                    continue;
                }
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_enemy_id = i;
            }
        }
        *distance = min_dist;
        return min_enemy_id;
    }

    static bool fight_enemy(figure *f)
    {
        if (!city_figures_has_security_breach() && enemy_army_total_enemy_formations() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
        }
        f->wait_ticks_next_target++;
        if (f->wait_ticks_next_target < 10) {
            return false;
        }
        int distance = 0;
        int enemy_id = get_nearest_enemy(f->x, f->y, &distance);
        if (enemy_id > 0 && distance <= 30) {
            figure *enemy = figure_get(enemy_id);
            if (enemy->targeted_by_figure_id) {
                figure_get(enemy->targeted_by_figure_id)->target_figure_id = 0;
            }
            f->wait_ticks = 0;
            f->action_state = FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY;
            f->destination_x = enemy->x;
            f->destination_y = enemy->y;
            f->target_figure_id = enemy_id;
            enemy->targeted_by_figure_id = f->id;
            f->target_figure_created_sequence = enemy->created_sequence;
            figure_route_remove(f);
            return true;
        }
        f->wait_ticks_next_target = 0;
        return false;
    }

    static bool fight_fire(figure *f, bool force)
    {
        if (building_list_burning_size() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE: {
                if (!force) {
                    return false;
                }
                building *burn = building_get(f->destination_building_id);
                if ((burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED) &&
                    burn->type == BUILDING_BURNING_RUIN) {
                    return true;
                }
                break;
            }
        }
        f->wait_ticks_missile++;
        if (f->wait_ticks_missile < 20 && !force) {
            return false;
        }
        int distance = 0;
        int ruin_id = building_maintenance_get_closest_burning_ruin(f->x, f->y, &distance);
        if (ruin_id > 0 && distance <= 25) {
            building *ruin = building_get(ruin_id);
            f->wait_ticks_missile = 0;
            f->action_state = FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE;
            f->wait_ticks = 0;
            f->destination_x = ruin->road_access_x;
            f->destination_y = ruin->road_access_y;
            f->destination_building_id = ruin_id;
            figure_route_remove(f);
            ruin->figure_id4 = f->id;
            return true;
        }
        return false;
    }

    static void extinguish_fire(figure *f)
    {
        building *burn = building_get(f->destination_building_id);
        int distance = calc_maximum_distance(f->x, f->y, burn->x, burn->y);
        if ((burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED)
            && burn->type == BUILDING_BURNING_RUIN && distance < 2) {
            burn->fire_duration = 32;
            sound_effect_play(SOUND_EFFECT_FIRE_SPLASH);
        } else {
            f->wait_ticks = 1;
        }
        f->attack_direction = calc_general_direction(f->x, f->y, burn->x, burn->y);
        if (f->attack_direction >= 8) {
            f->attack_direction = 0;
        }
        f->wait_ticks--;
        if (f->wait_ticks <= 0) {
            if (!fight_fire(f, true)) {
                building *owner = building_get(f->building_id);
                int x_road = 0;
                int y_road = 0;
                if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                    f->destination_x = x_road;
                    f->destination_y = y_road;
                    figure_route_remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
        }
    }

    static bool in_combat(figure *f)
    {
        return f->action_state == FIGURE_ACTION_150_ATTACK || f->action_state == FIGURE_ACTION_149_CORPSE;
    }

    void update_image(figure *f) const
    {
        int dir = 0;
        if (f->action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE ||
            f->action_state == FIGURE_ACTION_150_ATTACK) {
            dir = f->attack_direction;
        } else if (f->direction < 8) {
            dir = f->direction;
        } else {
            dir = f->previous_tile_direction;
        }
        dir = figure_image_normalize_direction(dir);
        const int base_image_id = image_base_for_definition(definition());

        switch (f->action_state) {
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->image_id = image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) +
                    dir + 8 * f->image_offset;
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                f->image_id = image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) +
                    dir + 96 + 8 * (f->image_offset / 2);
                break;
            case FIGURE_ACTION_150_ATTACK:
                if (f->attack_image_offset >= 12) {
                    f->image_id = base_image_id + 104 + dir + 8 * ((f->attack_image_offset - 12) / 2);
                } else {
                    f->image_id = base_image_id + 104 + dir;
                }
                break;
            case FIGURE_ACTION_149_CORPSE:
                f->image_id = base_image_id + 96 + figure_image_corpse_offset(f);
                break;
            default:
                f->image_id = base_image_id + dir + 8 * f->image_offset;
                break;
        }
    }
};

class EntertainmentFigureBase : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

protected:
    static bool is_finished_venue(const building *venue)
    {
        return venue &&
            (venue->type != BUILDING_HIPPODROME || !venue->prev_part_building_id) &&
            ((venue->type != BUILDING_COLOSSEUM && venue->type != BUILDING_HIPPODROME) ||
                venue->monument.phase == MONUMENT_FINISHED);
    }

    static int show_days(const building *venue, figure_type_registry_impl::EntertainmentShowSlot slot)
    {
        if (!venue) {
            return 0;
        }
        switch (slot) {
            case figure_type_registry_impl::EntertainmentShowSlot::Days1:
                return venue->data.entertainment.days1;
            case figure_type_registry_impl::EntertainmentShowSlot::Days2:
                return venue->data.entertainment.days2;
            case figure_type_registry_impl::EntertainmentShowSlot::None:
            default:
                return 0;
        }
    }

    static void set_show_days(building *venue, figure_type_registry_impl::EntertainmentShowSlot slot, int duration)
    {
        if (!venue) {
            return;
        }
        switch (slot) {
            case figure_type_registry_impl::EntertainmentShowSlot::Days1:
                venue->data.entertainment.days1 = static_cast<unsigned char>(duration);
                break;
            case figure_type_registry_impl::EntertainmentShowSlot::Days2:
                venue->data.entertainment.days2 = static_cast<unsigned char>(duration);
                break;
            case figure_type_registry_impl::EntertainmentShowSlot::None:
            default:
                break;
        }
    }

    void update_show_for_arrival(figure *f) const
    {
        building *venue = building_main(building_get(f->destination_building_id));
        if (!is_finished_venue(venue)) {
            return;
        }

        for (const figure_type_registry_impl::EntertainmentVenueTarget &target : profile()->venue_targets()) {
            if (target.building != venue->type) {
                continue;
            }
            if (f->type == FIGURE_ACTOR) {
                venue->data.entertainment.play++;
                if (venue->data.entertainment.play >= 5) {
                    venue->data.entertainment.play = 0;
                }
            }
            set_show_days(venue, target.show_slot, profile()->show_duration());
            return;
        }
    }

    void update_image(figure *f) const
    {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

        if (f->type == FIGURE_CHARIOTEER) {
            f->cart_image_id = 0;
            if (f->action_state == FIGURE_ACTION_150_ATTACK ||
                f->action_state == FIGURE_ACTION_149_CORPSE) {
                f->image_id = image_base_for_definition(definition()) + dir;
            } else {
                f->image_id = image_base_for_definition(definition()) + dir + 8 * f->image_offset;
            }
            return;
        }

        int image_id = image_base_for_definition(definition());
        if (f->type == FIGURE_LION_TAMER) {
            if (f->wait_ticks_missile >= 96 && f->action_state != FIGURE_ACTION_149_CORPSE) {
                image_id = image_group(GROUP_FIGURE_LION_TAMER_WHIP);
            }
            f->cart_image_id = image_group(GROUP_FIGURE_LION);
        }

        if (f->action_state == FIGURE_ACTION_150_ATTACK) {
            if (f->type == FIGURE_GLADIATOR) {
                f->image_id = image_id + 104 + dir + 8 * (f->image_offset / 2);
                if (f->image_id >= 5705 && f->image_id <= 5706) {
                    f->image_id -= 8;
                } else if (f->image_id > 5705) {
                    f->image_id -= 2;
                }
            } else {
                f->image_id = image_id + dir;
            }
        } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            f->image_id = image_id + 96 + figure_image_corpse_offset(f);
            f->cart_image_id = 0;
        } else {
            f->image_id = image_id + dir + 8 * f->image_offset;
        }
        if (f->cart_image_id) {
            f->cart_image_id += dir + 8 * f->image_offset;
            figure_image_set_cart_offset(f, dir);
        }
    }

    static int get_enemy_distance(figure *f, int x, int y)
    {
        if (f->type == FIGURE_RIOTER || f->type == FIGURE_ENEMY54_GLADIATOR) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_CRIMINAL_LOOTER || f->type == FIGURE_CRIMINAL_ROBBER) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (figure_is_enemy(f)) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_WOLF) {
            return 2 * calc_maximum_distance(x, y, f->x, f->y);
        }
        return kInfiniteDistance;
    }

    static bool fight_enemy(figure *f)
    {
        if (!city_figures_has_security_breach() && enemy_army_total_enemy_formations() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
                return false;
        }

        int min_enemy_id = 0;
        int min_dist = kInfiniteDistance;
        for (unsigned int i = 1; i < figure_count(); i++) {
            figure *enemy = figure_get(i);
            if (figure_is_dead(enemy)) {
                continue;
            }
            const int dist = get_enemy_distance(enemy, f->x, f->y);
            if (dist != kInfiniteDistance && enemy->targeted_by_figure_id) {
                figure *pursuiter = figure_get(enemy->targeted_by_figure_id);
                if (get_enemy_distance(enemy, pursuiter->x, pursuiter->y) < dist) {
                    continue;
                }
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_enemy_id = static_cast<int>(i);
            }
        }

        if (min_enemy_id > 0 && min_dist <= 50) {
            figure *enemy = figure_get(min_enemy_id);
            if (enemy->targeted_by_figure_id) {
                figure_get(enemy->targeted_by_figure_id)->target_figure_id = 0;
            }
            f->destination_x = enemy->x;
            f->destination_y = enemy->y;
            f->target_figure_id = min_enemy_id;
            enemy->targeted_by_figure_id = f->id;
            f->target_figure_created_sequence = enemy->created_sequence;
            figure_route_remove(f);
            return true;
        }
        f->wait_ticks_next_target = 0;
        return false;
    }
};

class EntertainmentServiceFigure : public EntertainmentFigureBase {
public:
    using EntertainmentFigureBase::EntertainmentFigureBase;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !profile()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);
        f->wait_ticks_missile++;
        if (f->wait_ticks_missile >= 120) {
            f->wait_ticks_missile = 0;
        }

        if (scenario_gladiator_revolt_is_in_progress() && f->type == FIGURE_GLADIATOR &&
            (f->action_state == FIGURE_ACTION_94_ENTERTAINER_ROAMING ||
                f->action_state == FIGURE_ACTION_95_ENTERTAINER_RETURNING)) {
            f->type = FIGURE_ENEMY54_GLADIATOR;
            figure_route_remove(f);
            f->roam_length = 0;
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            return 1;
        }

        const int speed_factor = f->type == FIGURE_CHARIOTEER ? 2 : movement.roam_ticks;
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                figure_image_increase_offset(f, 32);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_94_ENTERTAINER_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_95_ENTERTAINER_RETURNING;
                        f->destination_x = x_road;
                        f->destination_y = y_road;
                        figure_route_remove(f);
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                figure_movement_roam_ticks(f, speed_factor);
                break;
            case FIGURE_ACTION_95_ENTERTAINER_RETURNING:
                figure_movement_move_ticks(f, speed_factor);
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE ||
                    f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            case FIGURE_ACTION_230_LION_TAMERS_HUNTING_ENEMIES:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                if (!figure_target_is_alive(f) && !fight_enemy(f)) {
                    f->state = FIGURE_STATE_DEAD;
                }
                figure_movement_move_ticks_with_percentage(f, 1, 50);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    figure *target = figure_get(f->target_figure_id);
                    f->destination_x = target->x;
                    f->destination_y = target->y;
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        update_image(f);
        return 1;
    }
};

class EntertainmentVenueSeekerFigure : public EntertainmentFigureBase {
public:
    using EntertainmentFigureBase::EntertainmentFigureBase;

    int execute() override
    {
        figure *f = data_figure();
        if (!f || !profile()) {
            return 0;
        }

        building *owner = building_get(f->building_id);
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->cart_image_id = image_group(GROUP_FIGURE_CARTPUSHER_CART);
        f->terrain_usage = static_cast<unsigned char>(movement.terrain_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics_policy().max_image_offset);

        if (scenario_gladiator_revolt_is_in_progress() && f->type == FIGURE_GLADIATOR &&
            (f->action_state == FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE)) {
            f->type = FIGURE_ENEMY54_GLADIATOR;
            figure_route_remove(f);
            f->roam_length = 0;
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            return 1;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                figure_image_increase_offset(f, 32);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks_missile = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    int x_road = 0;
                    int y_road = 0;
                    if (map_closest_road_within_radius(owner->x, owner->y, owner->size, 2, &x_road, &y_road)) {
                        f->action_state = FIGURE_ACTION_91_ENTERTAINER_EXITING_SCHOOL;
                        figure_movement_set_cross_country_destination(f, x_road, y_road);
                        f->roam_length = 0;
                    } else {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_91_ENTERTAINER_EXITING_SCHOOL:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (!choose_destination(f)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                figure_movement_move_ticks(f, f->type == FIGURE_CHARIOTEER ? 2 : movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    update_show_for_arrival(f);
                    f->state = FIGURE_STATE_DEAD;
                } else if (f->direction == DIR_FIGURE_REROUTE) {
                    figure_route_remove(f);
                } else if (f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        update_image(f);
        return 1;
    }

private:
    struct Candidate {
        building *venue = nullptr;
        map_point road = { 0, 0 };
        int route_distance = 0;
        int show_days = 0;
        int score = 0;
    };

    bool candidate_is_better(const Candidate &candidate, const Candidate &best) const
    {
        if (!best.venue) {
            return true;
        }
        if (candidate.score != best.score) {
            return candidate.score < best.score;
        }
        return candidate.route_distance < best.route_distance;
    }

    bool choose_destination(figure *f) const
    {
        const map_point source_road = { f->x, f->y };
        if (!routing_distance::prepare_from_road(source_road)) {
            return false;
        }

        Candidate best;
        // Training walkers inspect the venue set declared by their profile. XML
        // decides whether this actor/gladiator/lion/charioteer targets one or more
        // venue types; the runtime only applies reachability and ranking.
        for (const figure_type_registry_impl::EntertainmentVenueTarget &target : profile()->venue_targets()) {
            for (building *venue = building_first_of_type(target.building); venue; venue = venue->next_of_type) {
                if (venue->state != BUILDING_STATE_IN_USE ||
                    !is_finished_venue(venue) ||
                    !venue->distance_from_entry) {
                    continue;
                }
                if (city_festival_games_active() && venue->type != city_festival_games_active_venue_type()) {
                    continue;
                }

                const routing_distance::BuildingRoadResult route =
                    routing_distance::find_access_road_to_building(
                        venue,
                        2,
                        profile()->movement_profile().max_roam_length,
                        1);
                if (!route.reachable) {
                    continue;
                }

                Candidate candidate;
                candidate.venue = venue;
                candidate.road = route.road;
                candidate.route_distance = route.distance;
                candidate.show_days = show_days(venue, target.show_slot);
                // Match the legacy weighting, but use route distance so the score
                // reflects the path walkers actually take through the road network.
                candidate.score = 2 * candidate.show_days + candidate.route_distance;
                if (candidate_is_better(candidate, best)) {
                    best = candidate;
                }
            }
        }

        if (!best.venue) {
            return false;
        }

        f->destination_building_id = best.venue->id;
        f->action_state = FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE;
        f->destination_x = best.road.x;
        f->destination_y = best.road.y;
        f->roam_length = 0;
        figure_route_remove(f);
        return true;
    }
};

std::unique_ptr<NativeFigure> make_controller(
    figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    const figure_type_registry_impl::FigureTypeProfile *profile)
{
    if (!f || !definition || !profile) {
        return nullptr;
    }

    switch (profile->native_class()) {
        case figure_type_registry_impl::NativeClassId::RoamingService:
            return std::make_unique<RoamingServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EngineerService:
            return std::make_unique<EngineerServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::PrefectService:
            return std::make_unique<PrefectServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EntertainmentService:
            return std::make_unique<EntertainmentServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EntertainmentVenueSeeker:
            return std::make_unique<EntertainmentVenueSeekerFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::MarketSupplier:
            return std::make_unique<MarketSupplierFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::DeliveryFollower:
            return std::make_unique<DeliveryFollowerFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::TransientWanderer:
            return std::make_unique<TransientWandererFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::None:
        default:
            return nullptr;
    }
}

} // namespace figure_runtime_native_impl
