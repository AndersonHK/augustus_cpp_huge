#include "figure/figure.h"
#include "building/building_record.h"
#include "translation/translation.h"
#include "storage.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/granary.h"
#include "building/properties.h"
#include "building/warehouse.h"
#include "building/building_type.h"
#include "city/resource.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/log.h"
#include "core/string.h"
#include "city/resource.h"
#include "empire/city.h"
#include "game/resource.h"
#include "game/resource_id_bridge.h"
#include "game/save_version.h"
#include "graphics/text.h"

#include <cstdlib>
#include <vector>

#define STORAGE_ORIGINAL_BUFFER_SIZE 32
#define STORAGE_STATIC_BUFFER_SIZE 10
#define STORAGE_CURRENT_BUFFER_SIZE (STORAGE_STATIC_BUFFER_SIZE + RESOURCE_SLOT_COUNT * 2)

static std::vector<data_storage> storages;

static void initialize_storage(data_storage &storage, unsigned int position)
{
    storage = {};
    storage.id = position;
}

static void resize_storages(size_t size)
{
    const size_t old_size = storages.size();
    storages.resize(size);
    for (size_t i = old_size; i < storages.size(); i++) {
        initialize_storage(storages[i], static_cast<unsigned int>(i));
    }
}

static data_storage *storage_at(int storage_id)
{
    if (storage_id < 0 || static_cast<size_t>(storage_id) >= storages.size()) {
        return nullptr;
    }
    return &storages[storage_id];
}

static data_storage *storage_at_or_zero(int storage_id)
{
    if (storages.empty()) {
        resize_storages(1);
    }
    data_storage *storage = storage_at(storage_id);
    return storage ? storage : storage_at(0);
}

static void trim_storages()
{
    while (storages.size() > 1 && !storages.back().in_use) {
        storages.pop_back();
    }
}

void building_storage_clear_all(void)
{
    storages.clear();
    resize_storages(1); // Ignore first storage
}

int building_storage_get_array_size(void)
{
    return static_cast<int>(storages.size());
}

int building_storage_try_add_resource(Building &b, int resource, int amount, int is_produced)
{
    resource_type resource_id = static_cast<resource_type>(resource);
    if (b.type && b.type->is_granary()) {
        return building_granary_try_add_resource(b, resource_id, amount, is_produced, 1);
    } else if (b.type && b.type->is_warehouse()) {
        return building_warehouse_try_add_resource(b, resource_id, amount, 1);
    }
    return 0;
}

void building_storage_reset_building_ids(void)
{
    for (data_storage &storage : storages) {
        storage.building_id = 0;
        storage.runtime_building = nullptr;
    }

    Building::for_each(BuildingRuntimeList::Storage, [&](Building *b) {
        if (b->storage_id) {
            data_storage *storage = storage_at(b->storage_id);
            if (storage && storage->building_id) {
                // storage is already connected to a building: corrupt, create new
                b->set_storage_id(building_storage_create(b->id));
            } else if (storage) {
                storage->building_id = b->id;
                storage->runtime_building = b;
            }
        }
    });
}

int building_storage_create(int building_id)
{
    data_storage *storage = nullptr;
    for (size_t i = 1; i < storages.size(); i++) {
        if (!storages[i].in_use) {
            initialize_storage(storages[i], static_cast<unsigned int>(i));
            storage = &storages[i];
            break;
        }
    }
    if (!storage) {
        resize_storages(storages.size() + 1);
        storage = &storages.back();
    }
    storage->in_use = 1;
    storage->building_id = building_id;
    storage->runtime_building = Building::get(static_cast<unsigned int>(building_id));
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        //default the storage quantity to max
        storage->storage.resource_state[r].quantity = BUILDING_STORAGE_QUANTITY_MAX;
    }

    if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
        building_storage_accept_none(storage->id);
    } else {
        building_storage_accept_all(storage->id);
    }
    return storage->id;
}

int building_storage_restore(int storage_id)
{
    if (storage_id < 0) {
        return 0;
    }
    if (static_cast<size_t>(storage_id) >= storages.size()) {
        resize_storages(static_cast<size_t>(storage_id) + 1);
    }
    if (storages[storage_id].in_use) {
        return 0;
    }
    storages[storage_id].in_use = 1;
    storages[storage_id].runtime_building =
        Building::get(static_cast<unsigned int>(storages[storage_id].building_id));
    return storage_id;
}

void building_storage_delete(int storage_id)
{
    if (data_storage *storage = storage_at(storage_id)) {
        storage->in_use = 0;
        storage->runtime_building = nullptr;
        trim_storages();
    }
}

const building_storage *building_storage_get(int storage_id)
{
    return &storage_at_or_zero(storage_id)->storage;
}

const data_storage *building_storage_get_array_entry(int storage_id)
{
    return storage_at_or_zero(storage_id);
}

void building_storage_set_data(int storage_id, building_storage new_data)
{
    if (data_storage *storage = storage_at(storage_id)) {
        storage->storage = new_data;
    }
}

void building_storage_toggle_empty_all(int storage_id)
{
    if (data_storage *storage = storage_at(storage_id)) {
        storage->storage.empty_all ^= 1;
    }
}

int building_storage_get_empty_all(const Building &building)
{
    unsigned int storage_id = building.storage_id;
    if (storage_id >= storages.size()) {
        return 0;
    }
    return storages[storage_id].storage.empty_all;
}

int building_storage_count_stored_resource_types(const Building &building)
{
    if (!building.storage_id) {
        return 0;
    }
    int stored_types_count = 0;
    for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
        if (building.resource_amount(r) > 0) {
            stored_types_count++;
        }
    }
    return stored_types_count;
}

int building_storage_get_amount(const Building &b, resource_type resource)
{
    if (b.type && b.type->is_granary()) {
        return b.resource_amount(resource);
    } else if (b.type && b.type->is_warehouse()) {
        return building_warehouse_get_amount(b, resource);
    }
    return 0;
}

int building_storage_get_storage_state_quantity(const Building &b, resource_type resource)
{
    const building_storage *s = building_storage_get(b.storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];
    return entry->quantity;
}

building_storage_state building_storage_get_state(const Building &b, int resource, int relative)
{
    if (b.has_plague() || (b.state_id() != BUILDING_STATE_IN_USE && b.state_id() != BUILDING_STATE_CREATED)) {
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    if (!b.type || !b.type->is_storage()) {
        return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
    const building_storage *s = building_storage_get(b.storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource];

    if (!relative) {
        // If relative is 0, return raw state without checking amounts
        return entry->state;
    }
    int amount = b.type->is_warehouse() ?
        building_warehouse_get_amount(b, static_cast<resource_type>(resource)) : b.resource_amount(static_cast<resource_type>(resource));

    switch (entry->state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:
            if (amount < entry->quantity) {
                return BUILDING_STORAGE_STATE_ACCEPTING;
            }
            break;

        case BUILDING_STORAGE_STATE_GETTING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_GETTING;
            }
            break;

        case BUILDING_STORAGE_STATE_MAINTAINING:
            if (amount <= entry->quantity) {
                return BUILDING_STORAGE_STATE_MAINTAINING;
            }
            break;

        default:
            break;
    }
    return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
}

resource_type building_storage_get_highest_quantity_resource(Building &b)
{
    resource_type highest_resource = RESOURCE_NONE;
    if (b.type && b.type->is_warehouse()) {
        building_warehouse_recount_resources(b);
    }
    for (resource_type resource = static_cast<resource_type>(RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource = static_cast<resource_type>(resource + 1)) {
        if (b.resource_amount(resource) > b.resource_amount(highest_resource)) {
            highest_resource = resource;
        }
    }
    return highest_resource;
}

void building_storage_cycle_resource_state(int storage_id, resource_type resource_id, int reverse_order)
{
    data_storage *storage = storage_at(storage_id);
    if (!storage) {
        return;
    }
    resource_storage_entry *entry = &storage->storage.resource_state[resource_id];
    int num_states = BUILDING_STORAGE_STATE_MAX;
    building_storage_state ordered[BUILDING_STORAGE_STATE_MAX] = { BUILDING_STORAGE_STATE_NOT_ACCEPTING, BUILDING_STORAGE_STATE_ACCEPTING,
        BUILDING_STORAGE_STATE_GETTING, BUILDING_STORAGE_STATE_MAINTAINING };
    int idx = 0;    // find current state's index in ordered[]
    for (int i = 0; i < num_states; i++) {
        if (ordered[i] == entry->state) {
            idx = i;
            break;
        }
    }
    // step forward/backward with wrap
    if (reverse_order) {
        idx = (idx - 1 + num_states) % num_states;
    } else {
        idx = (idx + 1) % num_states;
    }
    entry->state = ordered[idx];
}


void building_storage_toggle_permission(building_storage_permission_states p, Building &b)
{
    int permission_bit = 1 << p;
    if (data_storage *storage = storage_at(b.storage_id)) {
        storage->storage.permissions ^= permission_bit;
        b.invalidate_graphic();
    }
}

int building_storage_get_permission(building_storage_permission_states p, const Building &b)
{
    const building_storage *s = building_storage_get(b.storage_id);
    int permission_bit = 1 << p;
    return (s->permissions & permission_bit) == 0;
}

void building_storage_set_permission(building_storage_permission_states p, Building &b, int enable)
{
    int permission_bit = 1 << p;
    data_storage *storage = storage_at(b.storage_id);
    if (!storage) {
        return;
    }
    int *permissions = &storage->storage.permissions;
    const int previous = *permissions;

    if (enable) {
        *permissions |= permission_bit;
    } else {
        *permissions &= ~permission_bit;
    }
    if (*permissions != previous) {
        b.invalidate_graphic();
    }
}

void building_storage_cycle_partial_resource_state(int storage_id, resource_type resource_id, int reverse_order)
{
    data_storage *storage = storage_at(storage_id);
    if (!storage) {
        return;
    }
    resource_storage_entry *entry = &storage->storage.resource_state[resource_id];

    if (entry->state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return; // not accepting is always MAX
    }

    int step = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 4 : 8;

    int current = entry->quantity;

    // If current quantity is out of bounds, reset it.
    if (current > BUILDING_STORAGE_QUANTITY_MAX || current < step) {
        entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
        return;
    }

    if (reverse_order) {
        current -= step;
        if (current < step) {
            current = BUILDING_STORAGE_QUANTITY_MAX;
        }
    } else {
        current += step;
        if (current > BUILDING_STORAGE_QUANTITY_MAX) {
            current = step;
        }
    }

    entry->quantity = static_cast<building_storage_quantity>(current);
}

int building_storage_accepts_storage(Building &b, resource_type resource, int *understaffed)
{
    if (b.type ? b.type->is_warehouse() : 0) {
        return building_warehouse_accepts_storage(b, resource, understaffed);
    } else if (b.type ? b.type->is_granary() : 0) {
        return building_granary_accepts_storage(b, resource, understaffed);
    }
    return 0;
}

void building_storage_accept_none(int storage_id)
{
    data_storage *s = storage_at_or_zero(storage_id);
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
}

void building_storage_accept_all(int storage_id)
{
    data_storage *s = storage_at_or_zero(storage_id);
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        s->storage.resource_state[r].state = BUILDING_STORAGE_STATE_ACCEPTING;
    }
}

int building_storage_check_if_accepts_nothing(int storage_id)
{
    data_storage *s = storage_at_or_zero(storage_id);
    for (int r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r++) {
        if (s->storage.resource_state[r].state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
            return 0;
        }
    }
    return 1;
}

static const uint8_t *storage_state_text(building_storage_state state, const Building &building_object)
{
    switch (state) {
        case BUILDING_STORAGE_STATE_ACCEPTING:   return lang_get_string("main_strings.99.7");
        case BUILDING_STORAGE_STATE_NOT_ACCEPTING: return lang_get_string("main_strings.99.8");
        case BUILDING_STORAGE_STATE_GETTING:
            return lang_get_string(current_string_key(99, 9 + (building_object.type ? building_object.type->is_granary() : 0)));
        case BUILDING_STORAGE_STATE_MAINTAINING: return lang_get_string("TR_WINDOW_BUILDING_DISTRIBUTION_MAINTAINING");
        default: return (const uint8_t *) "";
    }
}

int building_storage_summary_tooltip(const Building &building_object, char *tooltip_text, int max_length, storage_summary_style style)
{
    if (!building_object.id || !tooltip_text || max_length <= 0) return 0;
    if (style == STORAGE_SUMMARY_STYLE_NONE) {
        tooltip_text[0] = '\0';
        return 0;
    } else if (building_object.is_mothballed()) {
        const uint8_t *mothballed_text = lang_get_string("TR_TOOLTIP_OVERLAY_PROBLEMS_MOTHBALLED");
        string_copy(mothballed_text, (uint8_t *) tooltip_text, max_length);
        return 1;
    }

    const resource_list *list = (building_object.type ? building_object.type->is_warehouse() : 0)
        ? city_resource_get_potential() : city_resource_get_potential_foods();

    building_storage_state state;
    int name_w = 0, state_w = 0;

    // Measure max widths
    for (unsigned int i = 0; i < list->size; i++) {
        const resource_data *res = resource_get_data(list->items[i]);
        if (!res) continue;
        state = building_storage_get_state(building_object, list->items[i], 0);
        if (style == STORAGE_SUMMARY_STYLE_MINIMAL && state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) continue;

        int rn_pixels = text_get_width(res->text, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));
        int sn_pixels = text_get_width(storage_state_text(state, building_object), FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));
        if (rn_pixels > name_w) name_w = rn_pixels;
        if (sn_pixels > state_w) state_w = sn_pixels;
    }

    uint8_t *out = (uint8_t *) tooltip_text, *start = out;
    out[0] = '\0'; int written = 0;

    int space_width = font_definition_for(FONT_SMALL_PLAIN)->space_width;
    const uint8_t *space_str = (const uint8_t *) " ";
    const uint8_t *newline_str = (const uint8_t *) "\n";
    const uint8_t *dash_str = (const uint8_t *) " - ";
    const uint8_t empty_char[] = { 0x01, 0x00 };  // 1px spacer (NOT allowed before newline)

    for (unsigned int i = 0; i < list->size; i++) {
        resource_type r = list->items[i];
        const resource_data *res = resource_get_data(r);
        if (!res) continue;

        state = building_storage_get_state(building_object, r, 0);
        if (style == STORAGE_SUMMARY_STYLE_MINIMAL && state == BUILDING_STORAGE_STATE_NOT_ACCEPTING) continue;

        int qty = building_storage_get_storage_state_quantity(building_object, r);
        const uint8_t *rn = res->text;
        const uint8_t *st = storage_state_text(state, building_object);

        // Resource column
        out = string_copy(rn, out, max_length - (out - start));
        int resource_name_width = text_get_width(rn, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));

        // Pad to resource name column width (mid-line padding is OK)
        int j = resource_name_width;
        while (j < name_w) {
            if (j + space_width <= name_w) {
                out = string_copy(space_str, out, max_length - (out - start));
                j += space_width;
            } else {
                out = string_copy(empty_char, out, max_length - (out - start));
                j += 1;
            }
        }

        // Separator
        out = string_copy(dash_str, out, max_length - (out - start));

        // State column text
        out = string_copy(st, out, max_length - (out - start));
        int state_name_width = text_get_width(st, FONT_SMALL_PLAIN, screen_ui_to_pixel(font_definition_for(FONT_SMALL_PLAIN)->line_height));

        if (state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
            j = state_name_width;
            while (j < state_w) {
                if (j + space_width <= state_w) {
                    out = string_copy(space_str, out, max_length - (out - start));
                    j += space_width;
                } else {
                    out = string_copy(empty_char, out, max_length - (out - start));
                    j += 1;
                }
            }

            // Quantity column
            out = string_copy(dash_str, out, max_length - (out - start));
            uint8_t qty_buf[12];
            string_from_int(qty_buf, qty, 0);
            out = string_copy(qty_buf, out, max_length - (out - start)); // Line must end on the last digit 
            out = string_copy(newline_str, out, max_length - (out - start));// (no padding before \n)
        } else {
            // IMPORTANT: For NOT_ACCEPTING we must NOT pad after 'st' and must
            // NOT let newline follow a space or empty_char. Go straight to \n.
            out = string_copy(newline_str, out, max_length - (out - start));
        }

        written = 1;
        if ((out - start) >= max_length) {
            start[max_length - 1] = '\0';
            break;
        }
    }
    // Remove trailing '\n' on the very last line (if any).
    if (written && out > (uint8_t *) tooltip_text && out[-1] == '\n') {
        out[-1] = '\0';
    }
    return written;
}

int building_storage_resource_max_storable(const Building &building_object, resource_type resource_id)
{
    if (building_object.type ? building_object.type->is_granary() : 0) {
        if (resource_id >= (RESOURCE_NONE + 1)) {
            return 0;
        }
    }

    const building_storage *s = building_storage_get(building_object.storage_id);
    const resource_storage_entry *entry = &s->resource_state[resource_id];

    if (entry->state != BUILDING_STORAGE_STATE_NOT_ACCEPTING) {
        return entry->quantity;
    }

    return 0;
}

void decode_legacy_storage_state(uint8_t legacy, resource_storage_entry *entry)
{
    // helper function for the legacy 12 storage states
    switch (legacy) {
        case 0: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 1: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 2: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_MAX; break;
        case 3: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 4: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 5: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 6: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 7: entry->state = BUILDING_STORAGE_STATE_GETTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 8: entry->state = BUILDING_STORAGE_STATE_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        case 9: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_16; break;
        case 10: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_8; break;
        case 11: entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING; entry->quantity = BUILDING_STORAGE_QUANTITY_24; break;
        default:
            entry->state = BUILDING_STORAGE_STATE_NOT_ACCEPTING;
            entry->quantity = BUILDING_STORAGE_QUANTITY_MAX;
            break;
    }
}


void building_storage_save_state(buffer *buf)
{
    int buf_size = 4 + static_cast<int>(storages.size()) * STORAGE_CURRENT_BUFFER_SIZE;
    uint8_t *buf_data = static_cast<uint8_t *>(malloc(buf_size));
    buffer_init(buf, buf_data, buf_size);
    buffer_write_i32(buf, STORAGE_CURRENT_BUFFER_SIZE);

    for (const data_storage &s : storages) {
        buffer_write_i32(buf, s.storage.permissions); // Originally unused
        buffer_write_i32(buf, s.building_id);
        buffer_write_u8(buf, (uint8_t) s.in_use);
        buffer_write_u8(buf, (uint8_t) s.storage.empty_all);

        for (int r = 0; r < RESOURCE_SLOT_COUNT; r++) {
            buffer_write_u8(buf, (uint8_t) s.storage.resource_state[r].state);
            buffer_write_u8(buf, (uint8_t) s.storage.resource_state[r].quantity);
        }
    }
}

building_storage_permission_states building_storage_get_permission_from_building_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    if (!definition) {
        return BUILDING_STORAGE_PERMISSION_MARKET;
    }
    if (definition->attr_is("market")) {
        return BUILDING_STORAGE_PERMISSION_MARKET;
    }
    if (definition->is_mess_hall()) {
        return BUILDING_STORAGE_PERMISSION_QUARTERMASTER;
    }
    if (definition->attr_is("tavern")) {
        return BUILDING_STORAGE_PERMISSION_BARKEEP;
    }
    if (definition->is_caravanserai()) {
        return BUILDING_STORAGE_PERMISSION_CARAVANSERAI;
    }
    if (definition->is_lighthouse()) {
        return BUILDING_STORAGE_PERMISSION_LIGHTHOUSE;
    }
    if (definition->is_armoury()) {
        return BUILDING_STORAGE_PERMISSION_ARMOURY;
    }
    return BUILDING_STORAGE_PERMISSION_MARKET; // assume market for other types (usually priests)
}

static building_storage_state states_remap(int state)
{
    // prior to version LAST_U16_GRIDS, NOT_ACCEPTING was 1, ACCEPTING was 0
    switch (state) {
        case 0: return BUILDING_STORAGE_STATE_ACCEPTING;
        case 1: return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
        case 2: return BUILDING_STORAGE_STATE_GETTING;
        case 3: return BUILDING_STORAGE_STATE_MAINTAINING;
        default: return BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }
}

void building_storage_load_state(buffer *buf, int version)
{
    int storage_buf_size;
    size_t buf_size = buf->size;
    int storages_to_load;
    int highest_id_in_use = 0;

    //  state + quantity stored separately
    if (version > SAVE_GAME_LAST_STORAGE_STATE_AND_QUANTITY_TOGETHER) {
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;

        int num_resources = (storage_buf_size - STORAGE_STATIC_BUFFER_SIZE) / 2;
        storages_to_load = (int) buf_size / storage_buf_size;

        storages.clear();
        resize_storages(storages_to_load);

        for (int i = 0; i < storages_to_load; i++) {
            data_storage *s = &storages[i];

            s->storage.permissions = buffer_read_i32(buf);
            s->building_id = buffer_read_i32(buf);
            s->in_use = buffer_read_u8(buf);
            s->storage.empty_all = buffer_read_u8(buf);

            if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
                building_storage_accept_none(s->id);
            }

            for (int r = 0; r < num_resources; r++) {
                int remapped = resource_remap(r);
                s->storage.resource_state[remapped].state = static_cast<building_storage_state>(buffer_read_u8(buf));
                if (version <= SAVE_GAME_LAST_U16_GRIDS) {
                    s->storage.resource_state[remapped].state = states_remap(s->storage.resource_state[remapped].state);
                }
                s->storage.resource_state[remapped].quantity = static_cast<building_storage_quantity>(buffer_read_u8(buf));
            }

            if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
                buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
            }

            if (s->in_use) {
                highest_id_in_use = i;
            }
        }

        resize_storages(highest_id_in_use + 1);
        return;
    }


    int includes_storage_size = version > SAVE_GAME_LAST_STATIC_VERSION;
    int num_resources = resource_id_bridge_legacy_resource_count();

    if (includes_storage_size) { // Augustus 4.0 
        storage_buf_size = buffer_read_i32(buf);
        buf_size -= 4;
        num_resources = storage_buf_size - STORAGE_STATIC_BUFFER_SIZE;
    } else {
        storage_buf_size = STORAGE_ORIGINAL_BUFFER_SIZE; //Julius and vanilla
    }

    storages_to_load = (int) buf_size / storage_buf_size;

    storages.clear();
    resize_storages(storages_to_load);

    for (int i = 0; i < storages_to_load; i++) {
        data_storage *s = &storages[i];

        s->storage.permissions = buffer_read_i32(buf);
        s->building_id = buffer_read_i32(buf);
        s->in_use = buffer_read_u8(buf);
        s->storage.empty_all = buffer_read_u8(buf);

        if (config_get(CONFIG_GP_CH_WAREHOUSES_DONT_ACCEPT)) {
            building_storage_accept_none(s->id);
        }

        for (int r = 0; r < num_resources; r++) {
            int remapped = resource_remap(r);
            uint8_t legacy = buffer_read_u8(buf);
            decode_legacy_storage_state(legacy, &s->storage.resource_state[remapped]);
        }

        if (!includes_storage_size) {
            buffer_skip(buf, 6); // hardcoded old unused bytes
        } else if (storage_buf_size > STORAGE_CURRENT_BUFFER_SIZE) {
            buffer_skip(buf, storage_buf_size - STORAGE_CURRENT_BUFFER_SIZE);
        }

        if (s->in_use) {
            highest_id_in_use = i;
        }
    }

    // fix granary storage
    Building::for_each(BuildingRuntimeList::Granaries, [](Building *b) {
        int granary_free_space = BUILDING_STORAGE_QUANTITY_MAX;
        for (resource_type r = (RESOURCE_NONE + 1); r < RESOURCE_SLOT_COUNT; r = static_cast<resource_type>(r + 1)) {
            int amount = b->resource_amount(r) / 100;
            b->set_resource_amount(r, amount);
            granary_free_space -= amount;
        }
        b->set_resource_amount(RESOURCE_NONE, granary_free_space);
    });

    resize_storages(highest_id_in_use + 1);
}
