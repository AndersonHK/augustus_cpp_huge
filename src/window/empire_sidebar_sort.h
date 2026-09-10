#pragma once

#include "empire/city.h"
#include "game/resource.h"
#include "input/mouse.h"

// Enums
typedef enum {
    SORT_BY_NAME,
    SORT_BY_QUOTA_FILL_EXPORT,
    SORT_BY_QUOTA_FILL_IMPORT,
    SORT_BY_ROUTE_COST,
    SORT_BY_PROFIT,
    MAX_SORTING_KEY
} sort_method;

typedef enum {
    FILTER_BY_RESOURCE,
    FILTER_BY_RESOURCE_SELL,
    FILTER_BY_RESOURCE_BUY,
    FILTER_BY_OPEN,
    FILTER_BY_CLOSED,
    FILTER_BY_LAND,
    FILTER_BY_SEA,
    FILTER_NONE,
    MAX_FILTER_KEY
} filter_method;

// Forward declaration for sidebar_city_entry (defined in empire.c)
struct sidebar_city_entry;

// Core sorting and filtering functions
int window_empire_sidebar_sort_sidebar_city_sorter(const void *a, const void *b);
int window_empire_sidebar_sort_city_matches_current_filter(const empire_city *city);

// Sorting and filtering state management
int window_empire_sidebar_sort_get_current_sorting(void);
int window_empire_sidebar_sort_get_current_filtering(void);
resource_type window_empire_sidebar_sort_get_selected_filter_resource(void);
int window_empire_sidebar_sort_get_sorting_reversed(void);
int window_empire_sidebar_sort_count_trade_resources(const empire_city *city, int is_sell);
void window_empire_sidebar_sort_set_current_sorting(int sorting);
void window_empire_sidebar_sort_set_current_filtering(int filtering);
void window_empire_sidebar_sort_set_selected_filter_resource(resource_type resource);
void window_empire_sidebar_sort_set_sorting_reversed(int reversed);


// Initialization
void window_empire_sidebar_sort_init(void);
