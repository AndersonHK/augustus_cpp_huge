#pragma once

#include "game/resource.h"

typedef enum order_condition_type {
    ORDER_CONDITION_NEVER = 0,
    ORDER_CONDITION_ALWAYS,
    ORDER_CONDITION_SOURCE_HAS_MORE_THAN,
    ORDER_CONDITION_DESTINATION_HAS_LESS_THAN
} order_condition_type;

typedef struct order {
    resource_type resource_type;
    unsigned int src_storage_id; // this is actually building_id, not storage_id
    unsigned int dst_storage_id; // this is actually building_id, not storage_id
    struct {
        order_condition_type condition_type;
        int threshold;
    } condition;
} order;
