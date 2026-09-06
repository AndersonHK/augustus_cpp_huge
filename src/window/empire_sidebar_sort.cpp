#include "core/config.h"
#include "game/ResourceGraphics.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"

#include "empire_sidebar_sort.h"

#include "graphics/grid_box.h"
#include "graphics/runtime_texture.h"
#include "translation/translation.h"
#include "empire/city.h"
#include "empire/trade_prices.h"
#include "empire/trade_route.h"
#include "game/resource.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/text.h"
#include "core/image_group.h"
#include "core/string.h"
#include "input/mouse.h"


#include <string.h>
#include <algorithm>

#define NO_POSITION (-1)
#define WIDTH_BORDER 16

// Forward declaration of sidebar_city_entry structure
struct sidebar_city_entry {
    int sidebar_item_id;
    int empire_object_id;
    int city_id;
    int x, y;
    int width;
    int height;
};

// Static variables for sorting and filtering state
static struct {
    sort_method current_sorting;
    filter_method current_filtering;
    resource_type selected_filter_resource;
    int sorting_reversed;
} sort_data = {
    .current_sorting = SORT_BY_NAME,
    .current_filtering = FILTER_NONE,
    .selected_filter_resource = RESOURCE_NONE,
    .sorting_reversed = 0,
};

int window_empire_sidebar_sort_count_trade_resources(const empire_city *city, int is_sell)
{
    int count = 0;
    for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
        const resource_type r = static_cast<resource_type>(resource);
        if (resource_is_storable(r)) {
            if ((is_sell && city->sells_resource[r]) ||
                (!is_sell && city->buys_resource[r])) {
                count++;
            }
        }
    }
    return count;
}

static int get_city_trade_quota_fill(const empire_city *city, int is_sell)
{
    int total_now = 0;
    int total_max = 0;

    for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
        const resource_type r = static_cast<resource_type>(resource);
        if (!resource_is_storable(r)) continue;

        if ((is_sell && !city->sells_resource[r]) || (!is_sell && !city->buys_resource[r])) continue;

        int max = trade_route_limit(city->route_id, r, !is_sell);
        int now = trade_route_traded(city->route_id, r, !is_sell);

        total_max += max;
        total_now += now;
    }

    if (total_max == 0) return 0;
    return (100 * total_now) / total_max;
}

// Initialization
void window_empire_sidebar_sort_init(void)
{
    sort_data.current_sorting = static_cast<sort_method>(std::clamp(config_get(CONFIG_UI_EMPIRE_SIDEBAR_SORT_METHOD), 0, MAX_SORTING_KEY - 1));
    sort_data.current_filtering = FILTER_NONE;
    sort_data.selected_filter_resource = RESOURCE_NONE;
    sort_data.sorting_reversed = config_get(CONFIG_UI_EMPIRE_SIDEBAR_SORT_REVERSED) != 0;
}

// Getter functions
int window_empire_sidebar_sort_get_current_sorting(void) { return sort_data.current_sorting; }
int window_empire_sidebar_sort_get_current_filtering(void) { return sort_data.current_filtering; }
resource_type window_empire_sidebar_sort_get_selected_filter_resource(void) { return sort_data.selected_filter_resource; }
int window_empire_sidebar_sort_get_sorting_reversed(void) { return sort_data.sorting_reversed; }

// Setter functions
void window_empire_sidebar_sort_set_current_sorting(int sorting) { sort_data.current_sorting = static_cast<sort_method>(sorting); config_set(CONFIG_UI_EMPIRE_SIDEBAR_SORT_METHOD, sorting); }
void window_empire_sidebar_sort_set_current_filtering(int filtering) { sort_data.current_filtering = static_cast<filter_method>(filtering); }
void window_empire_sidebar_sort_set_selected_filter_resource(resource_type resource) { sort_data.selected_filter_resource = resource; }
void window_empire_sidebar_sort_set_sorting_reversed(int reversed) { sort_data.sorting_reversed = reversed; config_set(CONFIG_UI_EMPIRE_SIDEBAR_SORT_REVERSED, reversed); }

int window_empire_sidebar_sort_sidebar_city_sorter(const void *a, const void *b)
{
    const sidebar_city_entry *entry_a = (const sidebar_city_entry *) a;
    const sidebar_city_entry *entry_b = (const sidebar_city_entry *) b;

    const empire_city *city_a = empire_city_get(entry_a->city_id);
    const empire_city *city_b = empire_city_get(entry_b->city_id);

    // Add null pointer checks to prevent crashes
    if (!city_a || !city_b) {
        // If one is null and the other isn't, put the null one at the end
        if (!city_a && !city_b) return 0;
        if (!city_a) return 1;
        if (!city_b) return -1;
    }

    int result = 0;

    switch (sort_data.current_sorting) {
        case SORT_BY_NAME:
        {
            const char *name_a = (const char *) empire_city_get_name(city_a);
            const char *name_b = (const char *) empire_city_get_name(city_b);
            result = strcmp(name_a, name_b);
            break;
        }

        case SORT_BY_QUOTA_FILL_EXPORT:
        case SORT_BY_QUOTA_FILL_IMPORT:
        {
            int is_sell = (sort_data.current_sorting == SORT_BY_QUOTA_FILL_IMPORT);
            int quota_a = get_city_trade_quota_fill(city_a, is_sell);
            int quota_b = get_city_trade_quota_fill(city_b, is_sell);
            result = (quota_a > quota_b) - (quota_a < quota_b);
            break;
        }

        case SORT_BY_ROUTE_COST:
        {
            int cost_a = city_a->cost_to_open;
            int cost_b = city_b->cost_to_open;
            result = (cost_a > cost_b) - (cost_a < cost_b);
            break;
        }

        case SORT_BY_PROFIT:
        {
            int profit_a = 0;
            int profit_b = 0;

            for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
                const resource_type r = static_cast<resource_type>(resource);
                if (!resource_is_storable(r)) continue;

                if (city_a->sells_resource[r]) {
                    int amount = trade_route_traded(city_a->route_id, r, 0);
                    int price = trade_price_sell(r, !city_a->is_sea_trade);
                    profit_a += amount * price;
                }
                if (city_a->buys_resource[r]) {
                    int amount = trade_route_traded(city_a->route_id, r, 1);
                    int price = trade_price_buy(r, !city_a->is_sea_trade);
                    profit_a -= amount * price;
                }

                if (city_b->sells_resource[r]) {
                    int amount = trade_route_traded(city_b->route_id, r, 0);
                    int price = trade_price_sell(r, !city_b->is_sea_trade);
                    profit_b += amount * price;
                }
                if (city_b->buys_resource[r]) {
                    int amount = trade_route_traded(city_b->route_id, r, 1);
                    int price = trade_price_buy(r, !city_b->is_sea_trade);
                    profit_b -= amount * price;
                }
            }

            result = (profit_a > profit_b) - (profit_a < profit_b);
            break;
        }

        default:
            break;
    }

    if (sort_data.sorting_reversed)
        result = -result;

    return result;
}

int window_empire_sidebar_sort_city_matches_current_filter(const empire_city *city)
{
    if (!city) {
        return 0; // Null cities don't match any filter
    }

    switch (sort_data.current_filtering) {
        case FILTER_BY_OPEN:
            return city->is_open;
        case FILTER_BY_CLOSED:
            return !city->is_open;
        case FILTER_BY_RESOURCE:
        {
            for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
                const resource_type r = static_cast<resource_type>(resource);
                if ((city->buys_resource[r] || city->sells_resource[r]) &&
                    sort_data.selected_filter_resource == r) {
                    return 1;
                }
            }
            return 0;
        }
        case FILTER_BY_RESOURCE_SELL:
        {
            for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
                const resource_type r = static_cast<resource_type>(resource);
                if (city->sells_resource[r] &&
                    sort_data.selected_filter_resource == r) {
                    return 1;
                }
            }
            return 0;
        }
        case FILTER_BY_RESOURCE_BUY:
        {
            for (int resource = (RESOURCE_NONE + 1); resource < RESOURCE_SLOT_COUNT; resource++) {
                const resource_type r = static_cast<resource_type>(resource);
                if (city->buys_resource[r] &&
                    sort_data.selected_filter_resource == r) {
                    return 1;
                }
            }
            return 0;
        }
        case FILTER_BY_LAND:
            return !city->is_sea_trade;
        case FILTER_BY_SEA:
            return city->is_sea_trade;
        default: // FILTER_NONE
            return 1;
    }
}
