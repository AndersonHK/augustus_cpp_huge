#pragma once

#include "core/buffer.h"

#include <stdint.h>

/**
 * @file
 * Runtime-facing resource definitions.
 *
 * Resource identity is authored by Mods/<Mod>/Resources/*.xml. Runtime ids are
 * process-local slots from the active XML definitions; old raw save ids are
 * private to the resource save bridge.
 */

typedef int resource_type;

enum {
    RESOURCE_NONE = 0,
    RESOURCE_SLOT_COUNT = 24
};

#define RESOURCE_SUPPLY_CHAIN_MAX_SIZE 10

typedef enum {
    RESOURCE_FLAG_NONE = 0,
    RESOURCE_FLAG_FOOD = 1,
    RESOURCE_FLAG_STORABLE = 2,
    RESOURCE_FLAG_INVENTORY = 4 | RESOURCE_FLAG_STORABLE, // Inventory goods are always storable
    RESOURCE_FLAG_SPECIAL = 8
} resource_flags;

typedef struct {
    int raw_amount;
    resource_type raw_material;
    resource_type good;
} resource_supply_chain;

typedef struct {
    resource_type type;
    resource_flags flags;
    const uint8_t *text;
    const char *text_id;
    const char *name_key;
    const char *xml_attr_name;
    struct {
        int buy;
        int sell;
    } default_trade_price;
} resource_data;


int resource_init(void);
const char *resource_get_failure_reason(void);
const char *resource_definition_source_path(const char *text_id);
int resource_definition_is_suppressed(const char *text_id);

int resource_is_food(resource_type resource);
int resource_is_raw_material(resource_type resource);
int resource_is_inventory(resource_type resource);
int resource_is_inventory_good(resource_type resource);
int resource_is_storable(resource_type resource);
int resource_is_special(resource_type resource);
int resource_is_declared(resource_type resource);
int resource_is_tradeable(resource_type resource);

int resource_get_supply_chain_for_good(resource_supply_chain *chain, resource_type good);
int resource_get_supply_chain_for_raw_material(resource_supply_chain *chain, resource_type raw_material);

resource_data *resource_get_data(resource_type resource);

const char *resource_text_id(resource_type resource);
resource_type resource_type_from_text_id(const char *text_id);
resource_type resource_type_from_xml_attr(const char *name);
int resource_matches_text_id(resource_type resource, const char *text_id);

resource_type resource_get_loaded(int index);
int resource_loaded_count(void);
resource_type resource_get_production(int index);
int resource_production_count(void);

int resource_units_per_load(void);

void resource_save_write_ref(buffer *buf, resource_type resource);
resource_type resource_save_read_ref(buffer *buf);

resource_type resource_wheat(void);
resource_type resource_vegetables(void);
resource_type resource_fruit(void);
resource_type resource_meat(void);
resource_type resource_fish(void);
resource_type resource_clay(void);
resource_type resource_timber(void);
resource_type resource_olives(void);
resource_type resource_vines(void);
resource_type resource_iron(void);
resource_type resource_marble(void);
resource_type resource_gold(void);
resource_type resource_sand(void);
resource_type resource_stone(void);
resource_type resource_pottery(void);
resource_type resource_furniture(void);
resource_type resource_oil(void);
resource_type resource_wine(void);
resource_type resource_weapons(void);
resource_type resource_concrete(void);
resource_type resource_bricks(void);
resource_type resource_denarii(void);
resource_type resource_troops(void);

void production_rates_load(buffer *buf);
void production_rates_save(buffer *buf);

#ifdef STARTUP_PARSER_TEST
typedef struct {
    const char *xml;
    int layer_index;
    const char *mod_name;
    const char *source_path;
} resource_layer_test_input;

typedef struct {
    int active_count;
    int suppressed_count;
    int queried_slot;
    int queried_disabled;
    int queried_buy;
    int queried_sell;
    int queried_source_layer;
} resource_layer_test_result;

int resource_layered_definition_buffers_are_valid_for_test(
    const resource_layer_test_input *inputs,
    int input_count,
    const char *query_id,
    resource_layer_test_result *result);
#endif

