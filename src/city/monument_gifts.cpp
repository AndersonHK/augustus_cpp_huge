#include "city/monument_gifts.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/data_private.h"
#include "city/map.h"
#include "core/log.h"
#include "figure/figure.h"
#include "figure/action.h"
#include "figure/figure_runtime_api.h"
#include "game/time.h"
#include <algorithm>
#include <limits>
#include <map>

namespace {
std::map<std::string, int> grants;
}

void city_monument_gifts_reset() { grants.clear(); }

void city_monument_gifts_grant(building_type type, int count)
{
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || count <= 0) return;
    int &earned = grants[definition->attr()];
    earned = static_cast<int>(std::min<int64_t>(static_cast<int64_t>(earned) + count, std::numeric_limits<int>::max()));
}

void city_monument_gifts_award(std::string_view event, int count)
{
    for (const auto &type : building_type_registry_impl::g_building_types) {
        if (type && !event.empty() && type->construction().gift.event == event) city_monument_gifts_grant(type->type(), count);
    }
}

bool city_monument_gift_available(building_type type)
{
    const auto *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition || definition->construction().gift.event.empty()) return true;
    const auto found = grants.find(definition->attr());
    if (found == grants.end()) return false;
    int placed = 0;
    for (const Building &building : Building::of_type(type)) if (building.is_in_use() || building.is_created() || building.is_mothballed()) ++placed;
    return found->second > placed;
}

void city_monument_gifts_save(buffer *buf)
{
    buffer_write_u32(buf, static_cast<uint32_t>(grants.size()));
    for (const auto &[type, count] : grants) {
        buffer_write_u16(buf, static_cast<uint16_t>(type.size()));
        buffer_write_raw(buf, type.data(), type.size());
        buffer_write_i32(buf, count);
    }
}

void city_monument_gifts_load(buffer *buf)
{
    grants.clear();
    const uint32_t count = buffer_read_u32(buf);
    if (count > BUILDING_TYPE_MAX) { log_error("Invalid monument gift count", 0, count); return; }
    for (uint32_t i = 0; i < count; ++i) {
        const uint16_t size = buffer_read_u16(buf);
        if (!size || size > 4096 || buf->index + size + sizeof(int32_t) > buf->size) { log_error("Invalid monument gift identity", 0, size); return; }
        std::string type(size, '\0');
        buffer_read_raw(buf, type.data(), size);
        const int earned = buffer_read_i32(buf);
        if (earned < 0) log_warning("Repaired negative monument gift count", type.c_str(), earned);
        grants[type] = std::max(0, earned);
    }
}

void city_monument_gifts_import_legacy()
{
    // The pre-module schema had only one reward. Keep that identity confined to its save bridge.
    grants["triumphal_arch"] = std::max<int>(0, city_data.building.triumphal_arches_available);
}

void city_monument_gift_supply(Building &building, bool materials, bool workers, int loads_per_delivery)
{
    auto *record = const_cast<::building *>(building.record());
    if (!record || !building.is_in_use() || !building_monument_is_unfinished_monument(record) || building_monument_is_construction_halted(record)) return;
    if (record->figure_id || building_monument_has_delivery_for_building(building.id)) return;
    map_point destination;
    if (!building_monument_access_point(record, &destination)) return;
    resource_type resource = RESOURCE_NONE;
    int loads = 0;
    if (materials) {
        for (int i = 0; i < resource_loaded_count(); ++i) {
            const auto candidate = resource_get_loaded(i);
            if (candidate == RESOURCE_NONE) continue;
            loads = std::min(loads_per_delivery, building.resource_amount(candidate));
            if (loads > 0) { resource = candidate; break; }
        }
    }
    if (resource == RESOURCE_NONE) loads = 0;
    if (resource == RESOURCE_NONE && (!workers || building_monument_needs_resources(record) || building.resource_amount(RESOURCE_NONE) <= 0)) return;
    const auto *entry = city_map_entry_point();
    Figure *leader = Figure::create(resource == RESOURCE_NONE ? FIGURE_WORK_CAMP_ARCHITECT : FIGURE_WORK_CAMP_WORKER, entry->x, entry->y, DIR_4_BOTTOM);
    if (!leader || !leader->id()) return;
    leader->set_home_building(&building);
    leader->set_destination_building(&building);
    record->figure_id = leader->id();
    leader->collecting_item_id = static_cast<unsigned char>(resource);
    leader->destination_x = static_cast<unsigned char>(destination.x);
    leader->destination_y = static_cast<unsigned char>(destination.y);
    leader->action_state = static_cast<unsigned char>(resource == RESOURCE_NONE ? FIGURE_ACTION_207_WORK_CAMP_ARCHITECT_GOING_TO_MONUMENT : FIGURE_ACTION_205_WORK_CAMP_WORKER_GOING_TO_MONUMENT);
    leader->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(60));
    building_monument_add_delivery(building.id, leader->id(), resource, resource == RESOURCE_NONE ? 10 : 0);
    const int leader_id = leader->id();
    int previous = leader_id;
    for (int i = 0; i < loads; ++i) {
        Figure *slave = Figure::create(FIGURE_WORK_CAMP_SLAVE, entry->x, entry->y, DIR_4_BOTTOM);
        if (!slave || !slave->id()) break;
        slave->set_home_building(&building);
        slave->set_destination_building(&building);
        slave->leading_figure_id = static_cast<short>(previous);
        slave->collecting_item_id = static_cast<unsigned char>(resource);
        slave->loads_sold_or_carrying = 1;
        slave->destination_x = static_cast<unsigned char>(destination.x);
        slave->destination_y = static_cast<unsigned char>(destination.y);
        slave->action_state = FIGURE_ACTION_209_WORK_CAMP_SLAVE_FOLLOWING;
        slave->wait_ticks = static_cast<short>(game_time_scale_legacy_day_ticks(60));
        building_monument_add_delivery(building.id, slave->id(), resource, 1);
        previous = slave->id();
    }
}
