#include "city/trade_ledger.h"
#include "request.h"

#include "building/granary.h"
#include "building/warehouse.h"
#include "city/finance.h"
#include "city/message.h"
#include "city/population.h"
#include "city/ratings.h"
#include "city/resource.h"
#include "core/log.h"
#include "core/random.h"
#include "game/resource.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include "game/time.h"
#include "game/tutorial.h"
#include "scenario/property.h"

#include <vector>

#define REQUESTS_STRUCT_SIZE_CURRENT (7 * sizeof(int16_t) + 5 * sizeof(uint16_t) + 6 * sizeof(uint8_t))

static std::vector<scenario_request> requests;

static scenario_request new_request(unsigned int index)
{
    scenario_request request = {};
    request.id = index;
    request.resource = REQUESTS_DEFAULT_RESOURCE;
    request.amount.min = REQUESTS_DEFAULT_AMOUNT_MIN;
    request.amount.max = REQUESTS_DEFAULT_AMOUNT_MAX;
    request.deadline_years = REQUESTS_DEFAULT_DEADLINE_YEARS;
    request.extension_months_to_comply = REQUESTS_DEFAULT_MONTHS_TO_COMPLY;
    request.favor = REQUESTS_DEFAULT_FAVOUR;
    request.extension_disfavor = REQUESTS_DEFAULT_EXTENSION_DISFAVOUR;
    request.ignored_disfavor = REQUESTS_DEFAULT_IGNORED_DISFAVOUR;
    return request;
}

static scenario_request inactive_request(unsigned int index)
{
    scenario_request request = {};
    request.id = index;
    return request;
}

static int request_in_use(const scenario_request &request)
{
    return request.resource != RESOURCE_NONE;
}

static scenario_request *request_slot(int id)
{
    return id >= 0 && static_cast<size_t>(id) < requests.size() ? &requests[id] : nullptr;
}

static scenario_request *append_request()
{
    requests.push_back(new_request(static_cast<unsigned int>(requests.size())));
    return &requests.back();
}

static void resize_requests(size_t size)
{
    requests.clear();
    requests.reserve(size);
    for (size_t i = 0; i < size; ++i) {
        append_request();
    }
}

static scenario_request *create_request()
{
    for (scenario_request &request : requests) {
        if (!request_in_use(request)) {
            request = new_request(request.id);
            return &request;
        }
    }
    return append_request();
}

static void trim_requests()
{
    while (requests.size() > 1 && !request_in_use(requests.back())) {
        requests.pop_back();
    }
}

static void make_request_visible_and_send_message(scenario_request *request)
{
    request->visible = 1;
    request->amount.requested = random_between_from_stdlib(request->amount.min, request->amount.max);
    if (city_resource_count_warehouses_amount(request->resource) >= (int) request->amount.requested) {
        request->can_comply_dialog_shown = 1;
    }
    int requested = request->amount.requested;
    if (request->resource == resource_denarii()) {
        city_message_post(1, MESSAGE_CAESAR_REQUESTS_MONEY, request->id, requested);
    } else if (request->resource == resource_troops()) {
        city_message_post(1, MESSAGE_CAESAR_REQUESTS_ARMY, request->id, requested);
    } else {
        city_message_post(1, MESSAGE_CAESAR_REQUESTS_GOODS, request->id, requested);
    }
}

int scenario_request_can_comply(int id)
{
    scenario_request *request = request_slot(id);
    if (!request) {
        return 0;
    }
    if (request->state != REQUEST_STATE_NORMAL && request->state != REQUEST_STATE_OVERDUE) {
        return 0;
    }
    if (!request->visible) {
        return 0;
    }
    int amount = city_resource_get_amount_for_request(request->resource, request->amount.requested);
    return amount >= (int) request->amount.requested;
}

void scenario_request_clear_all(void)
{
    requests.clear();
}

void scenario_request_init(void)
{
    for (scenario_request &request : requests) {
        random_generate_next();
        if (request.resource != RESOURCE_NONE) {
            request.month = (random_byte() & 7) + 2;
            request.months_to_comply = 12 * request.deadline_years;
        }
    }
}

int scenario_request_new(void)
{
    scenario_request *request = create_request();
    return request ? request->id : -1;
}

static void schedule_request_again(scenario_request *request)
{
    if (request->repeat.times == 0) {
        return;
    }
    int base_year = game_time_year() - scenario_property_start_year();
    request->year = base_year + random_between_from_stdlib(request->repeat.interval.min, request->repeat.interval.max);
    request->month = (random_byte() & 7) + 2;
    request->months_to_comply = 12 * request->deadline_years;
    request->state = REQUEST_STATE_NORMAL;
    request->visible = 0;
    request->can_comply_dialog_shown = 0;
    request->amount.requested = 0;
    if (request->repeat.times > 0) {
        request->repeat.times--;
    }
}

void scenario_request_show_ready_message(scenario_request *request)
{
    if (!request->can_comply_dialog_shown) {
        resource_type resource = request->resource;
        int resource_amount = city_resource_get_amount_for_request(resource, request->amount.requested);
        if (resource_amount >= (int) request->amount.requested) {
            request->can_comply_dialog_shown = 1;
            city_message_post(1, MESSAGE_REQUEST_CAN_COMPLY, request->id, 0);
        }
    }
}

static void process_request(scenario_request *request)
{
    if (!request->resource || request->state > REQUEST_STATE_DISPATCHED_LATE) {
        return;
    }
    int state = request->state;
    if (state == REQUEST_STATE_DISPATCHED || state == REQUEST_STATE_DISPATCHED_LATE) {
        --request->months_to_comply;
        if (request->months_to_comply <= 0) {
            int requested_amount = request->amount.requested; // Save until reset
            if (state == REQUEST_STATE_DISPATCHED) {
                city_message_post(1, MESSAGE_REQUEST_RECEIVED, request->id, requested_amount);
                city_ratings_change_favor(request->favor);
            } else {
                city_message_post(1, MESSAGE_REQUEST_RECEIVED_LATE, request->id, requested_amount);
                city_ratings_change_favor(request->favor / 2);
            }
            request->state = REQUEST_STATE_RECEIVED;
            request->visible = 0;
            schedule_request_again(request);
        }
        return;
    }

    // normal or overdue
    if (request->visible) {
        --request->months_to_comply;
        if (state == REQUEST_STATE_NORMAL) {
            if (request->months_to_comply == 12) {
                // reminder
                city_message_post(1, MESSAGE_REQUEST_REMINDER, request->id, 0);
            } else if (request->months_to_comply <= 0) {
                city_message_post(1, MESSAGE_REQUEST_REFUSED, request->id, 0);
                request->state = REQUEST_STATE_OVERDUE;
                request->months_to_comply = request->extension_months_to_comply;
                city_ratings_reduce_favor_missed_request(request->extension_disfavor);
            }
        } else if (state == REQUEST_STATE_OVERDUE) {
            if (request->months_to_comply <= 0) {
                city_message_post(1, MESSAGE_REQUEST_REFUSED_OVERDUE, request->id, 0);
                request->state = REQUEST_STATE_IGNORED;
                request->visible = 0;
                city_ratings_reduce_favor_missed_request(request->ignored_disfavor);
                schedule_request_again(request);
            }
        }
        if (!request->can_comply_dialog_shown) {
            resource_type resource = request->resource;
            int resource_amount = city_resource_get_amount_for_request(resource, request->amount.requested);
            if (resource_amount >= (int) request->amount.requested) {
                request->can_comply_dialog_shown = 1;
                city_message_post(1, MESSAGE_REQUEST_CAN_COMPLY, request->id, 0);
            }
        }
        return;
    }

    // request is not visible
    int year = scenario_property_start_year();
    if (!tutorial_adjust_request_year(&year)) {
        return;
    }
    if (game_time_year() == year + request->year &&
        game_time_month() == request->month) {
        make_request_visible_and_send_message(request);
    }
}

void scenario_request_process(void)
{
    for (scenario_request &request : requests) {
        process_request(&request);
    }
}

void scenario_request_dispatch(int id)
{
    scenario_request *request = request_slot(id);
    if (!request) {
        return;
    }
    if (request->state == REQUEST_STATE_NORMAL) {
        request->state = REQUEST_STATE_DISPATCHED;
    } else {
        request->state = REQUEST_STATE_DISPATCHED_LATE;
    }
    request->months_to_comply = (random_byte() & 3) + 1;
    request->visible = 0;
    request->can_comply_dialog_shown = 1;
    int amount = request->amount.requested;
    if (request->resource == resource_denarii()) {
        city_finance_process_sundry(amount);
    } else if (request->resource == resource_troops()) {
        city_population_remove_for_troop_request(amount);
        const int remaining = building_warehouses_remove_resource(resource_weapons(), amount);
        city_trade_ledger_consumed(resource_weapons(), (amount - remaining) * resource_units_per_load());
    } else {
        int amount_left = building_warehouses_send_resources_to_rome(request->resource, amount);
        if (amount_left > 0 && resource_is_food(request->resource)) {
            building_granaries_send_resources_to_rome(request->resource, amount_left);
        }
    }
}

const scenario_request *scenario_request_get(int id)
{
    return request_slot(id);
}

void scenario_request_update(const scenario_request *request)
{
    scenario_request *base_request = request_slot(request->id);
    if (base_request) {
        *base_request = *request;
        trim_requests();
    }
}

void scenario_request_delete(int id)
{
    scenario_request *request = request_slot(id);
    if (request) {
        *request = inactive_request(request->id);
        trim_requests();
    }
}

void scenario_request_remap_resource(void)
{
    for (scenario_request &request : requests) {
        request.resource = resource_remap(request.resource);
    }
}

unsigned int scenario_request_count_total(void)
{
    return static_cast<unsigned int>(requests.size());
}

unsigned int scenario_request_count_active(void)
{
    int num_requests = 0;
    for (const scenario_request &request : requests) {
        if (request.resource) {
            num_requests++;
        }
    }
    return num_requests;
}

int scenario_request_count_visible(void)
{
    int count = 0;
    for (const scenario_request &request : requests) {
        if (request.resource && request.visible) {
            count++;
        }
    }
    return count;
}

int scenario_request_foreach_visible(int start_index, void (*callback)(int index, const scenario_request *request))
{
    int index = start_index;
    for (const scenario_request &request : requests) {
        if (request.resource && request.visible) {
            callback(index, scenario_request_get(request.id));
            index++;
        }
    }
    return index;
}

const scenario_request *scenario_request_get_visible(int index)
{
    for (const scenario_request &request : requests) {
        if (request.resource && request.visible && request.state <= 1) {
            if (index == 0) {
                return scenario_request_get(request.id);
            }
            index--;
        }
    }
    return 0;
}

int scenario_request_is_ongoing(int id)
{
    if (id < 0 || static_cast<size_t>(id) >= requests.size()) {
        return 0;
    }

    const scenario_request *request = request_slot(id);

    if (!request->resource) {
        return 0;
    }

    if (request->visible
        && (request->state == REQUEST_STATE_NORMAL
            || request->state == REQUEST_STATE_OVERDUE)
        ) {
        return 1;
    }

    if (!request->visible
        && (request->state == REQUEST_STATE_DISPATCHED
            || request->state == REQUEST_STATE_DISPATCHED_LATE)
        ) {
        return 1;
    }

    return 0;
}

int scenario_request_force_start(int id)
{
    if (id < 0 || static_cast<size_t>(id) >= requests.size()) {
        return 0;
    }

    scenario_request *request = request_slot(id);

    if (!request->resource) {
        return 0;
    }

    if (scenario_request_is_ongoing(id)) {
        return 0;
    }

    request->state = REQUEST_STATE_NORMAL;
    request->months_to_comply = 12 * request->deadline_years;
    request->year = game_time_year();
    request->month = game_time_month();
    request->can_comply_dialog_shown = 0;

    make_request_visible_and_send_message(request);

    return 1;
}

static void request_save(buffer *list, const scenario_request *request)
{
    buffer_write_i16(list, static_cast<int16_t>(request->year));
    buffer_write_i16(list, static_cast<int16_t>(request->resource));
    buffer_write_u16(list, static_cast<uint16_t>(request->amount.min));
    buffer_write_u16(list, static_cast<uint16_t>(request->amount.max));
    buffer_write_u16(list, static_cast<uint16_t>(request->amount.requested));
    buffer_write_i16(list, static_cast<int16_t>(request->deadline_years));

    buffer_write_u8(list, static_cast<uint8_t>(request->can_comply_dialog_shown));

    buffer_write_u8(list, static_cast<uint8_t>(request->favor));
    buffer_write_u8(list, static_cast<uint8_t>(request->month));
    buffer_write_u8(list, static_cast<uint8_t>(request->state));
    buffer_write_u8(list, static_cast<uint8_t>(request->visible));
    buffer_write_u8(list, static_cast<uint8_t>(request->months_to_comply));

    buffer_write_i16(list, static_cast<int16_t>(request->extension_months_to_comply));
    buffer_write_i16(list, static_cast<int16_t>(request->extension_disfavor));
    buffer_write_i16(list, static_cast<int16_t>(request->ignored_disfavor));

    buffer_write_i16(list, static_cast<int16_t>(request->repeat.times));
    buffer_write_u16(list, static_cast<uint16_t>(request->repeat.interval.min));
    buffer_write_u16(list, static_cast<uint16_t>(request->repeat.interval.max));
}

void scenario_request_save_state(buffer *list)
{
    uint32_t struct_size = REQUESTS_STRUCT_SIZE_CURRENT;
    buffer_init_dynamic_array(list, requests.size(), struct_size);

    for (const scenario_request &request : requests) {
        request_save(list, &request);
    }
}

static void request_load(buffer *list, scenario_request *request, int version)
{
    request->year = buffer_read_i16(list);
    request->resource = resource_remap(buffer_read_i16(list));
    request->amount.min = buffer_read_u16(list);
    if (version > SCENARIO_LAST_STATIC_ORIGINAL_DATA) {
        request->amount.max = buffer_read_u16(list);
        request->amount.requested = buffer_read_u16(list);
    } else {
        request->amount.max = request->amount.min;
        request->amount.requested = request->amount.min;
    }
    request->deadline_years = buffer_read_i16(list);

    request->can_comply_dialog_shown = buffer_read_u8(list);

    request->favor = buffer_read_u8(list);
    request->month = buffer_read_u8(list);
    request->state = static_cast<scenario_request_state>(buffer_read_u8(list));
    request->visible = buffer_read_u8(list);
    request->months_to_comply = buffer_read_u8(list);

    request->extension_months_to_comply = buffer_read_i16(list);
    request->extension_disfavor = buffer_read_i16(list);
    request->ignored_disfavor = buffer_read_i16(list);

    if (version > SCENARIO_LAST_STATIC_ORIGINAL_DATA) {
        request->repeat.times = buffer_read_i16(list);
        request->repeat.interval.min = buffer_read_u16(list);
        request->repeat.interval.max = buffer_read_u16(list);
    } else {
        request->repeat.times = 0;
        request->repeat.interval.min = 0;
        request->repeat.interval.max = 0;
    }
}

void scenario_request_load_state(buffer *list, int version)
{
    size_t array_size = buffer_load_dynamic_array(list);

    resize_requests(array_size);

    for (size_t i = 0; i < array_size; i++) {
        scenario_request *request = &requests[i];
        request_load(list, request, version);
    }

    trim_requests();
}

void scenario_request_load_state_old_version(buffer *list, requests_old_state_sections section)
{
    // Old savegames had request data split out into multiple chunks,
    // and saved as multiple arrays of variables, rather than an array of struct approach.
    // So here we need to load in a similar section / varaible array manner when dealing with old versions.
    if (section == REQUESTS_OLD_STATE_SECTIONS_TARGET) {
        resize_requests(MAX_ORIGINAL_REQUESTS);
        for (scenario_request &request : requests) {
            request.year = buffer_read_i16(list);
        }
        for (scenario_request &request : requests) {
            request.resource = buffer_read_i16(list);
        }
        for (scenario_request &request : requests) {
            request.amount.min = buffer_read_i16(list);
            request.amount.max = request.amount.min;
            request.amount.requested = request.amount.min;
        }
        for (scenario_request &request : requests) {
            request.deadline_years = buffer_read_i16(list);
        }
    } else if (section == REQUESTS_OLD_STATE_SECTIONS_CAN_COMPLY) {
        for (scenario_request &request : requests) {
            request.can_comply_dialog_shown = buffer_read_u8(list);
        }
    } else if (section == REQUESTS_OLD_STATE_SECTIONS_FAVOR_REWARD) {
        for (scenario_request &request : requests) {
            request.favor = buffer_read_u8(list);
        }
    } else if (section == REQUESTS_OLD_STATE_SECTIONS_ONGOING_INFO) {
        for (scenario_request &request : requests) {
            request.month = buffer_read_u8(list);
        }
        for (scenario_request &request : requests) {
            request.state = static_cast<scenario_request_state>(buffer_read_u8(list));
        }
        for (scenario_request &request : requests) {
            request.visible = buffer_read_u8(list);
        }
        for (scenario_request &request : requests) {
            request.months_to_comply = buffer_read_u8(list);
        }
        // Setup any default values we need for values that didn't exist in old versions.
        for (scenario_request &request : requests) {
            request.extension_months_to_comply = REQUESTS_DEFAULT_MONTHS_TO_COMPLY;
            request.extension_disfavor = REQUESTS_DEFAULT_EXTENSION_DISFAVOUR;
            request.ignored_disfavor = REQUESTS_DEFAULT_IGNORED_DISFAVOUR;

            request.repeat.times = 0;
            request.repeat.interval.min = 0;
            request.repeat.interval.max = 0;
        }
        trim_requests();
    }
}
