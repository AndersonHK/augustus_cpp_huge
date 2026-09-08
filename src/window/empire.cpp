#include "assets/image_group_payload.h"
#include "scenario/definition_overrides.h"
#include "core/Logger.h"
#include <set>
#include "graphics/declarative_window.h"
#include "city/trade_ledger.h"
#include "window/trade_ledger.h"
#include <memory>
#include <filesystem>
#include <stdexcept>
#include "SDL.h"
#include "city/warning.h"
#include "empire/empire.h"
#include "graphics/arrow_button.h"
#include "graphics/generic_button.h"
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "graphics/lang_text.h"
#include "input/input.h"
#include "input/cursor.h"
#include "platform/cursor.h"
#include "window/city.h"
#include "window/empire_sidebar_sort.h"
#include "window/message_dialog.h"
#include "window/resource_settings.h"
#include "window/trade_prices.h"

#include "empire.h"

#include "graphics/grid_box.h"
#include "window/advisors.h"
#include "window/popup_dialog.h"
#include "window/trade_opened.h"
#include "graphics/empire_trade_route_button_widget.h"
#include "graphics/ui_runtime.h"
#include "game/ResourceGraphics.h"


#include "assets/assets.h"
#include "building/menu.h"
#include "city/military.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image_group.h"
#include "empire/city.h"
#include "empire/object.h"
#include "empire/trade_route.h"
#include "empire/trade_prices.h"
#include "empire/type.h"
#include "game/tutorial.h"
#include "game/system.h"
#include "graphics/image_button.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/screen.h"
#include "graphics/scrollbar.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/scroll.h"
#include "scenario/empire.h"
#include "scenario/invasion.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>  
#include <string.h>  

#define WIDTH_BORDER 16 //dimensions the border image in px, informative only
#define HEIGHT_BORDER 136 
#define SIDEBAR_ENTRY_HEIGHT 120 
#define BOTTOM_PANEL_HEIGHT 120

#define RESOURCE_ICON_WIDTH 26 //dimensions the resource icon in px, informative only
#define RESOURCE_ICON_HEIGHT 26

#define VERTICAL_TILE_WIDTH 40 //dimensions the vertical background tile in px, informative only
#define VERTICAL_TILE_HEIGHT 72
#define CLAMP(a, x, b) (((x) < (a)) ? (a) : \
			((b) < (x)) ? (b) : (x))
#define TRADE_DOT_SPACING 10 //spacing between dots in trade route line
#define MAX_SIDEBAR_CITIES 256 
#define MAX_RESOURCE_BUTTONS 256
#define MAX_TRADE_OPEN_BUTTONS 64
#define MAX_TRADE_EDGES 4096
#define MAX_DOTS_PER_ROUTE 1024
#define MAX_DOTS_ON_MAP (MAX_DOTS_PER_ROUTE * MAX_SIDEBAR_CITIES)
#define TRADE_PULSE_DOT_MS 180 
#define TRADE_DOT_ANIMATION_SCALE 160

#define FONT_SPACE_WIDTH font_definition_for(FONT_NORMAL_GREEN)->space_width
#define FONT_HEIGHT_NORMAL font_definition_for(FONT_NORMAL_GREEN)->line_height
#define FONT_HEIGHT_LARGE font_definition_for(FONT_LARGE_BLACK)->line_height

#define NO_POSITION ((unsigned int) -1) //used as an alterntive to 0 for some of new pointers
//to avoid confusion with when relying on external indexing, which can be 0-based

//typedefs 

typedef enum {
    EMPIRE_WINDOW_OUTSIDE = 0,
    EMPIRE_WINDOW_MAP = 1,
    EMPIRE_WINDOW_BOTTOM_PANEL = 2,
    EMPIRE_WINDOW_SIDEBAR = 3
} empire_window_area;


typedef struct {
    int x;
    int y;
} px_point;

struct sidebar_city_entry {
    int sidebar_item_id; // number on the list
    int empire_object_id; // empire object id of the city
    int city_id; // city index in the empire's array of cities
    int x;
    int y;
    int width;
    int height;
};




// Edges are DIRECTIONAL (start->end). No normalization.
// One edge can appear in multiple routes; we draw it once per frame via `drawn`.

typedef struct {
    int id;                 // 0-based index in g_trade_edges
    int x1, y1, x2, y2;     // start -> end (exact order preserved)
    int trade_route_id;     // for reference
    int is_sea;             // 1 sea, 0 land
    int drawn;              // set during draw pass to avoid double-drawing same edge
} trade_edge;



static trade_edge g_trade_edges[MAX_TRADE_EDGES];
static int g_trade_edge_count = 0;

// For each route_id: a list of 0-based edge indices, terminated by -1.
static int trade_city_edges[MAX_SIDEBAR_CITIES][MAX_TRADE_EDGES];
// measurements and scales helper functions
static int draw_images_at_interval(int image_id, int x_draw_offset, int y_draw_offset,
    int start_x, int start_y, int end_x, int end_y, int interval, int remaining);
void window_empire_collect_trade_edges(void);
static void window_empire_draw_trade_route_pulses(const empire_object *route_object, int x_offset, int y_offset);
// 'styles' get functions
//buttons
static void button_help(int param1, int param2);
static void button_return_to_city(int param1, int param2);
static void button_advisor(int advisor, int param2);
static void button_show_prices(int param1, int param2);
static void button_open_trade_by_route(int route_id);

//sidebar show/hide
static void sidebar_collapse(void);
static void sidebar_expand(void);

//helpers for integrating sidebar and map
static void process_selection(void);

//positioning and area checking
static int is_sidebar(const mouse *m);
static int is_sidebar_border(const mouse *m);
static int is_map(const mouse *m);
static void handle_sidebar_border(const mouse *m);
//buttons position registrators to enable dynamic positioning
//arrays and counts for sidebar trade, resource and sorting buttons

//sidebar-related arrays and variables

//original button properties
typedef struct {
    int x, y, width, height;
    int is_down; // 1 for down arrow, 0 for up arrow
} arrow_button_info;

//values for drawing resource shields
static px_point trade_amount_px_offsets[5] = {
    { 2, 0 },
    { 5, 2 },
    { 8, 4 },
    { 0, 3 },
    { 4, 6 },
};

static struct {
    unsigned int selected_button;
    int selected_city;
    int selected_trade_route;
    int x_min, x_max, y_min, y_max;
    int x_draw_offset, y_draw_offset;
    int usable_map_width;
    unsigned int focus_button_id;
    int is_scrolling;
    int finished_scroll;
    int hovered_object;
    int hovered_resource_button;
    resource_type focus_resource;
    struct {
        int x_min;
        int x_max;
    } panel;
    struct {
        int x_min, x_max, y_min, y_max;
        int margin_left, margin_right, margin_top, margin_bottom;
        int width, height;
        int scroll;
        int scroll_max;
        int initialised;
        uint8_t width_percent; // sidebar width as percentage of map width (0-100)
        int dragging; // is sidebar being dragged
        uint8_t dragging_width; // width during dragging (0-100)
        uint8_t previous_width; // used to restore the width when dragging ends (0-100)
        struct {
            int x_min;
            int x_max;
            int y_min;
            int y_max;
            int is_collapsed;
            int is_hovered;
        } border_btn;
    } sidebar;
    time_millis trade_route_anim_start;
} data = { 0, 1 , 0 };

// -------------------------------------------------------------------------------------------------------
//                                              INIT + DATA
// -------------------------------------------------------------------------------------------------------

static void init(void)
{
    data.selected_button = NO_POSITION; // no button selected
    data.trade_route_anim_start = 0;
    if (!data.sidebar.initialised) {
        window_empire_sidebar_sort_init();
        window_empire_sidebar_sort_set_current_filtering(FILTER_NONE); // default to no filtering
        window_empire_sidebar_sort_set_selected_filter_resource(RESOURCE_NONE); // no resource selected
    }

    data.sidebar.width_percent = static_cast<uint8_t>(config_get(CONFIG_UI_EMPIRE_SIDEBAR_WIDTH)); // default sidebar width (25%)
    data.sidebar.dragging = 0; // not dragging initially
    data.sidebar.dragging_width = 0;
    data.sidebar.previous_width = 0;
    data.sidebar.border_btn.is_hovered = 0; // not hovered initially
    data.sidebar.initialised = 1;
    process_selection();
    data.focus_button_id = 0;
    window_empire_collect_trade_edges();
    data.trade_route_anim_start = time_get_millis();
}

static void setup_sidebar(void)
{
    // Calculate sidebar bounds
    int s_width = screen_width();
    int s_height = screen_height();
    int map_width, map_height;
    empire_get_map_size(&map_width, &map_height);

    int max_width = map_width + WIDTH_BORDER;
    int max_height = map_height + HEIGHT_BORDER;

    data.x_min = s_width <= max_width ? 0 : (s_width - max_width) / 2;
    data.x_max = s_width <= max_width ? s_width : data.x_min + max_width;
    data.y_min = s_height <= max_height ? 0 : (s_height - max_height) / 2;
    data.y_max = s_height <= max_height ? s_height : data.y_min + max_height;

    int map_draw_x_min = data.x_min + WIDTH_BORDER;
    int map_draw_x_max = data.x_max - WIDTH_BORDER;
    int map_draw_y_min = data.y_min + WIDTH_BORDER;
    int map_draw_y_max = data.y_max - BOTTOM_PANEL_HEIGHT;

    data.sidebar.margin_left = 3; //margins betwene sidebar and gridbox
    data.sidebar.margin_right = 3;
    data.sidebar.margin_top = 2 * BLOCK_SIZE + 6; //space for sorting buttons
    data.sidebar.margin_bottom = 6;
    data.usable_map_width = map_draw_x_max - map_draw_x_min;

    // Use only one width source - prefer dragging width when actively dragging
    uint8_t active_width_percent = data.sidebar.dragging ? data.sidebar.dragging_width : data.sidebar.width_percent;
    int raw = (data.usable_map_width * active_width_percent) / 100;
    data.sidebar.width = ((raw + (BLOCK_SIZE / 2)) / BLOCK_SIZE) * BLOCK_SIZE + data.sidebar.margin_left + data.sidebar.margin_right;
    data.sidebar.border_btn.is_collapsed = active_width_percent <= 5;
    const auto *sidebar_definition = declarative_window_definition("empire_sidebar");
    if (sidebar_definition && !data.sidebar.border_btn.is_collapsed) data.sidebar.width = std::max(sidebar_definition->min_blocks_width() * BLOCK_SIZE, data.sidebar.width);
    if (!declarative_window_definition("empire_sidebar")) {
        data.sidebar.width = 0;
        data.sidebar.dragging = 0;
        data.sidebar.border_btn.is_collapsed = 1;
        data.sidebar.border_btn.is_hovered = 0;
    }
    // Keep the XML controls usable at the narrowest expanded width.

    data.sidebar.height = map_draw_y_max - map_draw_y_min;

    data.sidebar.x_min = map_draw_x_max - data.sidebar.width;
    data.sidebar.x_max = map_draw_x_max;
    data.sidebar.y_min = map_draw_y_min;
    data.sidebar.y_max = map_draw_y_max;
}

// -------------------------------------------------------------------------------------------------------
//                                              SIDEBAR HELPERS
// -------------------------------------------------------------------------------------------------------


static void sidebar_collapse(void)
{
    data.sidebar.width_percent = 0;
    data.sidebar.border_btn.is_collapsed = 1;
    window_invalidate();
}
static void sidebar_expand(void)
{
    data.sidebar.width_percent = 25;
    data.sidebar.border_btn.is_collapsed = 0;
    window_invalidate();
}

// -------------------------------------------------------------------------------------------------------
//                                              DRAW PANELING (BACKGROUND)
// -------------------------------------------------------------------------------------------------------

static void draw_paneling(void)
{
    int image_base = Image::group(GROUP_EMPIRE_PANELS);
    int bottom_panel_is_larger = data.x_min != data.panel.x_min;
    int vertical_y_limit = bottom_panel_is_larger ? data.y_max - BOTTOM_PANEL_HEIGHT : data.y_max;

    graphics_set_clip_rectangle(data.panel.x_min, data.y_min,
        data.panel.x_max - data.panel.x_min, data.y_max - data.y_min);

    // bottom panel background
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 70) {
        Image::from_id(image_base + 3).draw(x, data.y_max - BOTTOM_PANEL_HEIGHT);
        Image::from_id(image_base + 3).draw(x, data.y_max - 80);
        Image::from_id(image_base + 3).draw(x, data.y_max - 40);
    }

    // horizontal bar borders
    for (int x = data.panel.x_min; x < data.panel.x_max; x += 86) {
        Image::from_id(image_base + 1).draw(x, data.y_max - BOTTOM_PANEL_HEIGHT);
        Image::from_id(image_base + 1).draw(x, data.y_max - WIDTH_BORDER);
    }

    // extra vertical bar borders
    if (bottom_panel_is_larger) {
        for (int y = vertical_y_limit + WIDTH_BORDER; y < data.y_max; y += 86) {
            Image::from_id(image_base).draw(data.panel.x_min, y);
            Image::from_id(image_base).draw(data.panel.x_max - WIDTH_BORDER, y);
        }
    }

    graphics_set_clip_rectangle(data.x_min, data.y_min, data.x_max - data.x_min, vertical_y_limit - data.y_min);

    for (int x = data.x_min; x < data.x_max; x += 86) {
        Image::from_id(image_base + 1).draw(x, data.y_min);
    }

    // vertical bar borders
    for (int y = data.y_min + WIDTH_BORDER; y < vertical_y_limit; y += 86) {
        Image::from_id(image_base).draw(data.x_min, y);
        Image::from_id(image_base).draw(data.x_max - WIDTH_BORDER, y);
    }

    graphics_reset_clip_rectangle();

    // crossbars
    Image::from_id(image_base + 2).draw(data.x_min, data.y_min);
    Image::from_id(image_base + 2).draw(data.x_min, data.y_max - BOTTOM_PANEL_HEIGHT);
    Image::from_id(image_base + 2).draw(data.panel.x_min, data.y_max - WIDTH_BORDER);
    Image::from_id(image_base + 2).draw(data.x_max - WIDTH_BORDER, data.y_min);
    Image::from_id(image_base + 2).draw(data.x_max - WIDTH_BORDER, data.y_max - BOTTOM_PANEL_HEIGHT);
    Image::from_id(image_base + 2).draw(data.panel.x_max - WIDTH_BORDER, data.y_max - WIDTH_BORDER);

    if (bottom_panel_is_larger) {
        Image::from_id(image_base + 2).draw(data.panel.x_min, data.y_max - BOTTOM_PANEL_HEIGHT);
        Image::from_id(image_base + 2).draw(data.panel.x_max - WIDTH_BORDER, data.y_max - BOTTOM_PANEL_HEIGHT);
    }
    // Sidebar background 
    graphics_set_clip_rectangle(data.sidebar.x_min - WIDTH_BORDER, data.sidebar.y_min, //clipping - border, to let border be drawn OUTSIDE
        data.sidebar.width + WIDTH_BORDER, //account for width border substracted earlier to make sure textures stretch all the way
        data.sidebar.y_max - data.sidebar.y_min);
    // Sidebar border 
    for (int y = data.sidebar.y_min; y < data.sidebar.y_max; y += 86) {
        Image::from_id(image_base).draw(data.sidebar.x_min - WIDTH_BORDER, y);
    }

    data.sidebar.border_btn.x_min = data.sidebar.x_min - WIDTH_BORDER;
    data.sidebar.border_btn.x_max = data.sidebar.border_btn.is_collapsed ? data.sidebar.x_max : data.sidebar.x_min;
    data.sidebar.border_btn.y_min = data.sidebar.y_min;
    data.sidebar.border_btn.y_max = data.sidebar.y_max;

    // Draw border button highlight if hovered
    if (data.sidebar.border_btn.is_hovered) {
        graphics_shade_rect(
            data.sidebar.border_btn.x_min,
            data.sidebar.border_btn.y_min,
            data.sidebar.border_btn.x_max - data.sidebar.border_btn.x_min,
            data.sidebar.border_btn.y_max - data.sidebar.border_btn.y_min,
            2 // shade style (0-7)
        );
    }

    graphics_reset_clip_rectangle();

}

// -------------------------------------------------------------------------------------------------------
//                                          NEW TRADE ROUTES
// -------------------------------------------------------------------------------------------------------

// Return existing/new edge index (0-based), or -1 on overflow.
// Equality is order-sensitive: same (x1,y1)->(x2,y2) AND same is_sea.
static int add_or_get_trade_edge(int start_x, int start_y, int end_x, int end_y, int route_id, int is_sea)
{
    for (int edge_index = 0; edge_index < g_trade_edge_count; edge_index++) {
        trade_edge *edge = &g_trade_edges[edge_index];
        if (edge->is_sea == is_sea &&
            edge->x1 == start_x && edge->y1 == start_y &&
            edge->x2 == end_x && edge->y2 == end_y) {
            return edge_index; // found existing directed edge
        }
    }

    if (g_trade_edge_count >= MAX_TRADE_EDGES) {
        return -1; // cannot add more edges
    }

    trade_edge new_edge = {
        .id = g_trade_edge_count,
        .x1 = start_x,
        .y1 = start_y,
        .x2 = end_x,
        .y2 = end_y,
        .trade_route_id = route_id,
        .is_sea = is_sea,
        .drawn = 0
    };

    g_trade_edges[g_trade_edge_count] = new_edge;
    g_trade_edge_count++;

    return new_edge.id;
}

void window_empire_collect_trade_edges(void)
{
    const empire_object *our_city_object = empire_object_get_our_city();
    g_trade_edge_count = 0;
    memset(g_trade_edges, 0, sizeof(g_trade_edges));
    memset(trade_city_edges, 0xFF, sizeof(trade_city_edges));
    // Pre-fill per-route edge lists with -1 (sentinel terminator).
    for (int object_index = 0; object_index < empire_object_count(); object_index++) {
        const empire_object *route_object = empire_object_get(object_index);
        if (!empire_object_get_full(object_index)->in_use) {
            continue;
        }
        int is_sea_route = -1;
        if (route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
            is_sea_route = 1;
        } else if (route_object->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
            is_sea_route = 0;
        } else {
            continue; // not a trade route object
        }

        int route_id = route_object->trade_route_id;
        if (route_id < 0 || route_id >= MAX_SIDEBAR_CITIES) {
            continue; // invalid route id; skip this route
        }

        const empire_object *trade_city_object = empire_object_get_trade_city(route_id);
        int segment_start_x = our_city_object->x + 25;
        int segment_start_y = our_city_object->y + 25;
        int route_edge_count = 0;

        for (int waypoint_index = 0; waypoint_index < empire_object_count(); ) {
            int waypoint_object_id = empire_object_get_next_in_order(object_index, &waypoint_index);
            if (!waypoint_object_id) {
                break;
            }
            const empire_object *waypoint_object = empire_object_get(waypoint_object_id);
            if (waypoint_object->type != EMPIRE_OBJECT_TRADE_WAYPOINT || waypoint_object->trade_route_id != route_id) {
                break; // reached non-waypoint or different route; waypoint sequence ends
            }

            int edge_index = add_or_get_trade_edge(
                segment_start_x, segment_start_y, waypoint_object->x, waypoint_object->y, route_id, is_sea_route);

            if (edge_index >= 0) {
                if (route_edge_count < MAX_TRADE_EDGES) {
                    trade_city_edges[route_id][route_edge_count] = edge_index;
                    route_edge_count++;
                }
            }

            segment_start_x = waypoint_object->x;
            segment_start_y = waypoint_object->y;
        }

        // Final leg to destination city center
        int city_center_x = trade_city_object->x + 25;
        int city_center_y = trade_city_object->y + 25;

        int final_edge_index = add_or_get_trade_edge(
            segment_start_x, segment_start_y, city_center_x, city_center_y, route_id, is_sea_route);

        if (final_edge_index >= 0) {
            if (route_edge_count < MAX_TRADE_EDGES) {
                trade_city_edges[route_id][route_edge_count] = final_edge_index;
                route_edge_count++;
            }
        }

        if (route_edge_count < MAX_TRADE_EDGES) {
            trade_city_edges[route_id][route_edge_count] = -1; // explicit terminator for clarity
        }
    }
}

void window_empire_draw_static_trade_waypoints(const empire_object *route_object, int x_offset, int y_offset)
{
    if (scenario_empire_id() != SCENARIO_CUSTOM_EMPIRE) {
        return;
    }
    int is_sea_route = route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE;

    int image_id = assets_get_image_id(is_sea_route ? "UI\\SeaRouteDot" : "UI\\LandRouteDot", is_sea_route ? "SeaRouteDot" : "LandRouteDot");
    int route_id = route_object->trade_route_id;
    if (route_id < 0 || route_id >= MAX_SIDEBAR_CITIES) {
        return; // invalid route id; nothing to draw
    }

    // Even spacing across the whole polychain; remainder carries between edges.
    const empire_object *our_city_object = empire_object_get_our_city();
    (void) our_city_object; // not needed here but kept for parity with collection step

    int remaining_spacing = TRADE_DOT_SPACING;

    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];

        if (edge_index < 0) {
            break; // reached sentinel; no more edges for this route
        }

        if (edge_index >= g_trade_edge_count) {
            continue; // stale or out-of-range mapping; skip this edge
        }

        trade_edge *edge = &g_trade_edges[edge_index];

        int draw_image_id = edge->drawn ? 0 : image_id;
        remaining_spacing = draw_images_at_interval(draw_image_id, x_offset, y_offset, edge->x1, edge->y1,
                                                    edge->x2, edge->y2, TRADE_DOT_SPACING, remaining_spacing);

        edge->drawn = 1; // mark as processed for this frame
    }
    if (config_get(CONFIG_UI_ANIMATE_TRADE_ROUTES)) {
        window_empire_draw_trade_route_pulses(route_object, x_offset, y_offset);
    }
}

static void draw_trade_route_pulse_index(int image_id, int x_offset, int y_offset, int route_id, int dot_index)
{
    if (!image_id || !route_id) {
        return;
    }

    int target_distance = dot_index * TRADE_DOT_SPACING;
    int accumulated_distance = 0;

    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];
        if (edge_index < 0) {
            break;
        }

        trade_edge *edge = &g_trade_edges[edge_index];
        int dx = edge->x2 - edge->x1;
        int dy = edge->y2 - edge->y1;
        int segment_length = (int) sqrt(dx * dx + dy * dy);

        if (target_distance <= accumulated_distance + segment_length) {
            int along = target_distance - accumulated_distance;
            int x_factor = calc_percentage(dx, segment_length);
            int y_factor = calc_percentage(dy, segment_length);
            int x = calc_adjust_with_percentage(along, x_factor) + edge->x1;
            int y = calc_adjust_with_percentage(along, y_factor) + edge->y1;

            Image::from_id(image_id).draw_scaled_centered(x_offset + x, y_offset + y, COLOR_MASK_NONE, TRADE_DOT_ANIMATION_SCALE);
            return;
        }
        accumulated_distance += segment_length;
    }
}

static void window_empire_draw_trade_route_pulses(const empire_object *route_object, int x_offset, int y_offset)
{
    int is_sea_route = 0;
    if (route_object->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        is_sea_route = 1;
    } else if (route_object->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE) {
        is_sea_route = 0;
    } else {
        return;
    }
    int route_id = route_object->trade_route_id;
    int pulse_image_id = assets_get_image_id(!is_sea_route ? "UI\\SeaRouteDot" : "UI\\LandRouteDot", !is_sea_route ? "SeaRouteDot" : "LandRouteDot"); //opposite image

    int total_length_pixels = 0;
    for (int list_index = 0; list_index < MAX_TRADE_EDGES; list_index++) {
        int edge_index = trade_city_edges[route_id][list_index];
        if (edge_index < 0) {
            break; // no more edges
        }
        trade_edge *edge = &g_trade_edges[edge_index];
        int delta_x = edge->x2 - edge->x1;
        int delta_y = edge->y2 - edge->y1;
        int segment_length = (int) sqrt(delta_x * delta_x + delta_y * delta_y);
        total_length_pixels += segment_length;
    }

    int dot_count = (total_length_pixels / TRADE_DOT_SPACING) + 1;
    if (!dot_count) {
        return;
    }
    time_millis elapsed_millis = time_get_millis() - data.trade_route_anim_start;

    int ticks_since_start = (int) (elapsed_millis / TRADE_PULSE_DOT_MS);
    int forward_index_from_start = ticks_since_start % dot_count;

    int index_from_trade_city = (dot_count - 1) - forward_index_from_start;
    draw_trade_route_pulse_index(pulse_image_id, x_offset, y_offset, route_id, index_from_trade_city);
}

// -------------------------------------------------------------------------------------------------------
//                                              FOREGROUND ELEMENTS DRAWING 
// -------------------------------------------------------------------------------------------------------

void window_empire_draw_resource_shields(int trade_max, int x_offset, int y_offset)
{
    trade_max = std::max(0, trade_max);
    int num_gold_shields = trade_max / 100;
    int num_bronze_shields = std::min(5, ((trade_max % 100) + 10) / 20);
    if (num_gold_shields > 7) { num_gold_shields = 7; num_bronze_shields = 0; }

    int top_left_x;
    if (num_bronze_shields == 1) {
        top_left_x = x_offset + 19;
    } else if (num_bronze_shields == 2) {
        top_left_x = x_offset + 15;
    } else {
        top_left_x = x_offset + 11;
    }
    int top_left_y = y_offset - 1;
    int bronze_shield = Image::group(GROUP_TRADE_AMOUNT);
    for (int i = 0; i < num_bronze_shields; i++) {
        px_point pt = trade_amount_px_offsets[i];
        Image::from_id(bronze_shield).draw(top_left_x + pt.x, top_left_y + pt.y);
    }

    top_left_x = x_offset - 1;
    top_left_y = y_offset + 22;
    int gold_shield = assets_lookup_image_id(ASSET_GOLD_SHIELD);
    for (int i = 0; i < num_gold_shields; i++) {
        Image::from_id(gold_shield).draw(top_left_x + i * 3, top_left_y);
    }
}

static void draw_background(void)
{
    int s_width = screen_width();
    int s_height = screen_height();
    int map_width, map_height;
    empire_get_map_size(&map_width, &map_height);
    int max_width = map_width + WIDTH_BORDER;
    int max_height = map_height + HEIGHT_BORDER;

    data.x_min = s_width <= max_width ? 0 : (s_width - max_width) / 2;
    data.x_max = s_width <= max_width ? s_width : data.x_min + max_width;
    data.y_min = s_height <= max_height ? 0 : (s_height - max_height) / 2;
    data.y_max = s_height <= max_height ? s_height : data.y_min + max_height;

    int bottom_panel_width = data.x_max - data.x_min;
    if (bottom_panel_width < 608) {
        bottom_panel_width = 640;
        int difference = bottom_panel_width - (data.x_max - data.x_min);
        int odd = difference % 1;
        difference /= 2;
        data.panel.x_min = data.x_min - difference - odd;
        data.panel.x_max = data.x_max + difference;
    } else {
        data.panel.x_min = data.x_min;
        data.panel.x_max = data.x_max;
    }

    if (data.x_min || data.y_min) {
        Image::from_id(Image::group(GROUP_EMPIRE_MAP)).draw_blurred_fullscreen(3);
        graphics_shade_rect(0, 0, screen_width(), screen_height(), 7);
    }
    setup_sidebar();

}

static int draw_images_at_interval(int image_id, int x_draw_offset, int y_draw_offset,
    int start_x, int start_y, int end_x, int end_y, int interval, int remaining)
{
    int x_diff = end_x - start_x;
    int y_diff = end_y - start_y;
    int dist = (int) sqrt(x_diff * x_diff + y_diff * y_diff);
    int x_factor = calc_percentage(x_diff, dist);
    int y_factor = calc_percentage(y_diff, dist);
    int offset = interval - remaining;
    if (offset > dist) {
        return offset;
    }
    dist -= offset;
    int num_dots = dist / interval;
    remaining = dist % interval;
    if (image_id) {
        for (int j = 0; j <= num_dots; j++) {
            int x = calc_adjust_with_percentage(j * interval + offset, x_factor) + start_x;
            int y = calc_adjust_with_percentage(j * interval + offset, y_factor) + start_y;
            Image::from_id(image_id).draw(x_draw_offset + x, y_draw_offset + y);
        }
    }
    return remaining;
}

void window_empire_draw_border(const empire_object *border, int x_offset, int y_offset)
{
    int first = 0;
    int first_edge_id = empire_object_get_next_in_order(border->id, &first);
    if (!first_edge_id) {
        return;
    }
    const empire_object *first_edge = empire_object_get(first_edge_id);
    if (first_edge->type != EMPIRE_OBJECT_BORDER_EDGE) {
        return;
    }
    int last_x = first_edge->x;
    int last_y = first_edge->y;
    int image_id = first_edge->image_id;
    int remaining = border->width;

    // Align the coordinate to the base of the border flag's mast
    x_offset -= 0;
    y_offset -= 14;

    for (int i = first; i < empire_object_count(); ) {
        int obj_id = empire_object_get_next_in_order(border->id, &i);
        if (!obj_id) {
            break;
        }
        empire_object *obj = empire_object_get(obj_id);
        if (obj->type != EMPIRE_OBJECT_BORDER_EDGE) {
            break;
        }
        int animation_offset = 0;
        int x = x_offset;
        int y = y_offset;
        if (image_id) {
            const image *img = image_get(image_id);
            draw_images_at_interval(image_id, x, y, last_x, last_y, obj->x, obj->y, border->width, remaining);
            if (img->animation && img->animation->speed_id) {
                animation_offset = empire_object_update_animation(obj, image_id);
                x += img->animation->sprite_offset_x;
                y += img->animation->sprite_offset_y;
            }
            remaining = draw_images_at_interval(image_id + animation_offset, x, y, last_x, last_y, obj->x, obj->y,
                border->width, remaining);
        } else {
            remaining = border->width;
        }
        last_x = obj->x;
        last_y = obj->y;
        image_id = obj->image_id;
    }
    if (!image_id) {
        return;
    }
    int animation_offset = 0;
    const image *img = image_get(image_id);
    if (img->animation && img->animation->speed_id) {
        animation_offset = empire_object_update_animation(border, image_id);
    }
    draw_images_at_interval(image_id, x_offset, y_offset, last_x, last_y, first_edge->x, first_edge->y,
        border->width, remaining);
    if (animation_offset) {
        draw_images_at_interval(image_id + animation_offset,
                x_offset + img->animation->sprite_offset_x, y_offset + img->animation->sprite_offset_y,
                last_x, last_y, first_edge->x, first_edge->y, border->width, remaining);
    }
}

static void draw_empire_object(const empire_object *obj)
{
    if (obj->type == EMPIRE_OBJECT_TRADE_WAYPOINT || obj->type == EMPIRE_OBJECT_BORDER_EDGE) {
        return;
    }
    if (obj->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE || obj->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        if (!empire_city_is_trade_route_open(obj->trade_route_id) || scenario_definition_override_value(ScenarioOverrideKind::HiddenRoute, std::to_string(obj->trade_route_id), 0, {}, 0)) {
            return; // dont draw the icon if route is closed
        }
    }
    int x, y, image_id;
    if (scenario_empire_is_expanded()) {
        x = obj->expanded.x;
        y = obj->expanded.y;
        image_id = obj->expanded.image_id;
    } else {
        x = obj->x;
        y = obj->y;
        image_id = obj->image_id;
    }
    if (obj->type == EMPIRE_OBJECT_BORDER) {
        window_empire_draw_border(obj, data.x_draw_offset, data.y_draw_offset);
    }
    if (obj->type == EMPIRE_OBJECT_CITY) {
        const empire_city *city = empire_city_get(empire_city_get_for_object(obj->id));
        if (city->type == EMPIRE_CITY_DISTANT_FOREIGN ||
            city->type == EMPIRE_CITY_FUTURE_ROMAN) {
            image_id = Image::group(GROUP_EMPIRE_FOREIGN_CITY);
        } else if (city->type == EMPIRE_CITY_TRADE) {
            // Fix cases where empire map still gives a blue flag for new trade cities
            // (e.g. Massilia in campaign Lugdunum)
            image_id = Image::group(GROUP_EMPIRE_CITY_TRADE);
        }
    }
    if (obj->type == EMPIRE_OBJECT_BATTLE_ICON) {
        // handled later
        return;
    }
    if (obj->type == EMPIRE_OBJECT_ENEMY_ARMY) {
        if (city_military_months_until_distant_battle() <= 0) {
            return;
        }
        if (city_military_distant_battle_enemy_months_traveled() != obj->distant_battle_travel_months) {
            return;
        }
    }
    if (obj->type == EMPIRE_OBJECT_ROMAN_ARMY) {
        if (!city_military_distant_battle_roman_army_is_traveling()) {
            return;
        }
        if (city_military_distant_battle_roman_months_traveled() != obj->distant_battle_travel_months) {
            return;
        }
    }
    if (obj->type == EMPIRE_OBJECT_ORNAMENT) {
        if (image_id < 0) {
            image_id = assets_lookup_image_id(ASSET_FIRST_ORNAMENT) - 1 - image_id;
        }
    }
    if (obj->type == EMPIRE_OBJECT_CITY) {
        const auto *city = empire_city_get(empire_city_get_for_object(obj->id));
        auto kind = obj->empire_city_icon;
        if (city->type == EMPIRE_CITY_TRADE && obj->future_trade_after_icon) kind = obj->future_trade_after_icon;
        if (kind == EMPIRE_CITY_ICON_DEFAULT) {
            kind = city->type == EMPIRE_CITY_TRADE ? EMPIRE_CITY_ICON_TRADE_CITY : city->type == EMPIRE_CITY_OURS ? EMPIRE_CITY_ICON_OUR_CITY :
                city->type == EMPIRE_CITY_DISTANT_FOREIGN || city->type == EMPIRE_CITY_FUTURE_ROMAN ? EMPIRE_CITY_ICON_DISTANT_CITY : EMPIRE_CITY_ICON_ROMAN_CITY;
        }
        const auto icon = empire_city_icon(kind);
        const auto base = icon.runtime_slice();
        if (!base.is_valid()) { Logger::errorf("Empire city %u has an unresolved icon %d", obj->id, kind); return; }
        const bool selected = empire_selected_object() == obj->id + 1;
        const bool hovered = data.hovered_object == static_cast<int>(obj->id) + 1;
        const float factor = selected ? 1.3f : hovered ? 1.2f : 1.0f;
        const float draw_x = data.x_draw_offset + x + base.width * (1 - factor) / 2;
        const float draw_y = data.y_draw_offset + y + base.height * (1 - factor) / 2;
        if (selected) {
            for (auto offset : {std::pair{-2, 0}, std::pair{2, 0}, std::pair{0, -2}, std::pair{0, 2}})
                runtime_texture_draw(base, static_cast<int>((draw_x + offset.first) / factor), static_cast<int>((draw_y + offset.second) / factor), COLOR_MASK_ORANGE_GOLD, 1 / factor);
        }
        runtime_texture_draw(base, static_cast<int>(draw_x / factor), static_cast<int>(draw_y / factor), COLOR_MASK_NONE, 1 / factor);
        const auto *payload = image_group_payload_get(icon.group_path().c_str());
        const auto *entry = payload ? payload->entry_for(icon.entry_id().c_str()) : nullptr;
        if (entry && entry->has_animation()) {
            const auto &animation = entry->animation();
            auto *live = empire_object_get(obj->id);
            if (Animation::should_advance(animation.speed_id()) || live->animation_index < 1 || live->animation_index > animation.frame_count())
                live->animation_index = live->animation_index % animation.frame_count() + 1;
            const auto frame = animation.frame_slice_at_offset(live->animation_index, 0);
            runtime_texture_draw(frame, static_cast<int>(draw_x / factor) + animation.sprite_offset_x(), static_cast<int>(draw_y / factor) + animation.sprite_offset_y(), COLOR_MASK_NONE, 1 / factor);
        }
        return;
    }    if (image_id <= 0) {
        Logger::errorf("Empire object %d has no image (type=%d, city icon=%d).", obj->id, obj->type, obj->empire_city_icon);
        return;
    }
    const image *img = image_get(image_id);
    Image::from_id(image_id).draw(data.x_draw_offset + x, data.y_draw_offset + y);
    if (img->animation && img->animation->speed_id) {
        const int frame = empire_object_update_animation(obj, image_id);
        Image::from_id(image_id + frame).draw(data.x_draw_offset + x + img->animation->sprite_offset_x, data.y_draw_offset + y + img->animation->sprite_offset_y);
    }
    // Manually fix the Hagia Sophia
    if (obj->image_id == 8122) {
        image_id = assets_lookup_image_id(ASSET_HAGIA_SOPHIA_FIX);
        Image::from_id(image_id).draw(data.x_draw_offset + x, data.y_draw_offset + y);
    }
}

static void empire_draw_object_trade_route(const empire_object *obj)
{
    if (scenario_definition_override_value(ScenarioOverrideKind::HiddenRoute, std::to_string(obj->trade_route_id), 0, {}, 0)) return;
    if (obj->type == EMPIRE_OBJECT_LAND_TRADE_ROUTE || obj->type == EMPIRE_OBJECT_SEA_TRADE_ROUTE) {
        if (scenario_empire_id() == SCENARIO_CUSTOM_EMPIRE) {
            if (empire_city_is_trade_route_open(obj->trade_route_id)) {
                window_empire_draw_static_trade_waypoints(obj, data.x_draw_offset, data.y_draw_offset);
            }
        }
    }
    return;
}

static void draw_invasion_warning(int x, int y, int image_id)
{
    Image::from_id(image_id).draw(data.x_draw_offset + x, data.y_draw_offset + y);
}

void empire_reset_route_drawn_flags(void)
{
    for (int i = 0; i < g_trade_edge_count; i++) {
        g_trade_edges[i].drawn = 0;
    }
}

static void draw_map(void)
{
    // Recalculate inner bounds (same as draw_background)
    int map_clip_x_min = data.x_min + WIDTH_BORDER;
    int map_clip_y_min = data.y_min + WIDTH_BORDER;
    int map_clip_x_max = data.sidebar.x_min;  // Stop before sidebar starts
    int map_clip_y_max = data.y_max - BOTTOM_PANEL_HEIGHT;

    graphics_set_clip_rectangle(map_clip_x_min, map_clip_y_min, map_clip_x_max - map_clip_x_min, map_clip_y_max - map_clip_y_min);
    // Reset all edge drawn flags for this frame
    empire_reset_route_drawn_flags();

    empire_set_viewport(map_clip_x_max - map_clip_x_min, map_clip_y_max - map_clip_y_min);

    data.x_draw_offset = map_clip_x_min;
    data.y_draw_offset = map_clip_y_min;
    empire_adjust_scroll(&data.x_draw_offset, &data.y_draw_offset);

    Image::from_id(empire_get_image_id()).draw(data.x_draw_offset, data.y_draw_offset);
    if (data.trade_route_anim_start == 0) {
        data.trade_route_anim_start = time_get_millis();
    }

    empire_object_foreach(draw_empire_object);
    empire_object_foreach_of_type(empire_draw_object_trade_route, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(empire_draw_object_trade_route, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_LAND_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_SEA_TRADE_ROUTE);
    empire_object_foreach_of_type(draw_empire_object, EMPIRE_OBJECT_CITY);

    scenario_invasion_foreach_warning(draw_invasion_warning);

    graphics_reset_clip_rectangle();
}

// -------------------------------------------------------------------------------------------------------
//                                              DRAW FOREGROUND
// -------------------------------------------------------------------------------------------------------

#include "empire_trade_widgets.h"

namespace {
class EmpireController final : public EmpireTradeController {
public:
    const empire_city *trade_city() const override { return selected(); }
    int trade_city_id() const override { return data.selected_city; }
    void draw_custom(const DeclarativeWidgetDefinition &widget, int item, int x, int y, int width, int height, bool focused) const override
    {
        if (widget.binding == "city.list") empire_city_list.draw(x, y, width, height);
        else EmpireTradeController::draw_custom(widget, item, x, y, width, height, focused);
    }
    int handle_custom(const DeclarativeWidgetDefinition &widget, int item, const mouse &local, int width, int height) override
    {
        if (widget.binding == "city.list") return empire_city_list.handle(local);
        return EmpireTradeController::handle_custom(widget, item, local, width, height);
    }
    int page = 0, resource_page = 0, capacity = 4;
    int open_filter = 0, route_filter = 0, direction_filter = 0;
    resource_type resource_filter = RESOURCE_NONE;
    std::vector<resource_type> picker_resources() const
    {
        std::vector<resource_type> result;
        for (int i = 0; i < resource_loaded_count(); ++i) {
            const auto resource = resource_get_loaded(i);
            for (int city = 1; city < empire_city_get_array_size(); ++city) {
                const auto *entry = empire_city_get(city);
                if (entry->in_use && entry->type == EMPIRE_CITY_TRADE && (entry->sells_resource[resource] || entry->buys_resource[resource])) { result.push_back(resource); break; }
            }
        }
        return result;
    }
    std::vector<int> cities;
    std::vector<resource_type> resources;
    std::map<int, int64_t> trade_balances;
    std::set<int> unknown_trade_balances;
    AccountingPeriod period;
    size_t period_index = 0, period_count = 0;
    void select_period(size_t index)
    {
        const auto &history = city_trade_ledger_periods();
        period_count = history.size();
        period_index = std::min(index, history.size() - 1);
        period = history[period_index];
        empire_display_period = &period; empire_display_period_index = period_index;
        trade_balances.clear();
        unknown_trade_balances.clear();
        for (const auto &t : period.transactions) {
            if (!t.direction_known) unknown_trade_balances.insert(t.city);
            else trade_balances[t.city] += (t.imported ? -1 : 1) * t.units * t.price / resource_units_per_load();
        }
        page = resource_page = 0;
    }
    const RouteAccounts *route(int city) const
    {
        const auto found = period.routes.find(city);
        return found == period.routes.end() ? nullptr : &found->second;
    }
    bool matches_filter(int city) const
    {
        const auto *layout = declarative_window_definition("empire_sidebar");
        if (layout && layout->has_widget("filter_open")) {
            const auto *live = empire_city_get(city);
            const auto *archive = period_index ? route(city) : nullptr;
            if (period_index && !archive) return false;
            const bool open = archive ? archive->open : live->is_open;
            const bool sea = archive ? archive->sea : live->is_sea_trade;
            if ((open_filter == 1 && !open) || (open_filter == 2 && open) || (route_filter == 1 && sea) || (route_filter == 2 && !sea)) return false;
            if (resource_filter == RESOURCE_NONE) return true;
            bool sells = live->sells_resource[resource_filter], buys = live->buys_resource[resource_filter];
            if (archive) {
                const auto found = archive->resources.find(resource_text_id(resource_filter));
                sells = found != archive->resources.end() && found->second.import_limit > 0;
                buys = found != archive->resources.end() && found->second.export_limit > 0;
            }
            return direction_filter == 1 ? buys : direction_filter == 2 ? sells : buys || sells;
        }
        if (!period_index) return window_empire_sidebar_sort_city_matches_current_filter(empire_city_get(city));
        const auto filter = window_empire_sidebar_sort_get_current_filtering();
        if (filter == FILTER_NONE) return true;
        const auto *entry = route(city);
        if (!entry) return false;
        if (filter == FILTER_BY_OPEN) return entry->open;
        if (filter == FILTER_BY_CLOSED) return !entry->open;
        if (filter == FILTER_BY_LAND) return !entry->sea;
        if (filter == FILTER_BY_SEA) return entry->sea;
        const char *key = resource_text_id(window_empire_sidebar_sort_get_selected_filter_resource());
        const auto found = entry->resources.find(key ? key : "");
        if (found == entry->resources.end()) return false;
        return filter == FILTER_BY_RESOURCE ? found->second.import_limit || found->second.export_limit :
            filter == FILTER_BY_RESOURCE_SELL ? found->second.import_limit != 0 : found->second.export_limit != 0;
    }
    int64_t sort_value(int city) const
    {
        const auto sorting = window_empire_sidebar_sort_get_current_sorting();
        if (sorting == SORT_BY_PROFIT) {
            const auto found = trade_balances.find(city);
            return found == trade_balances.end() ? 0 : found->second;
        }
        const auto *entry = route(city);
        if (!entry) return -1;
        if (sorting == SORT_BY_ROUTE_COST) return entry->cost;
        int64_t traded = 0, limit = 0;
        for (const auto &[key, amounts] : entry->resources) {
            const bool imports = sorting == SORT_BY_QUOTA_FILL_IMPORT;
            traded += imports ? amounts.imported : amounts.exported;
            limit += imports ? amounts.import_limit : amounts.export_limit;
        }
        return limit ? 100 * traded / limit : 0;
    }
    void refresh()
    {
        std::vector<sidebar_city_entry> entries;
        for (int i = 1; i < empire_city_get_array_size(); ++i) {
            const auto *city = empire_city_get(i);
            if (city->in_use && city->type == EMPIRE_CITY_TRADE && matches_filter(i)) entries.push_back({0, city->empire_object_id, i});
        }
        std::sort(entries.begin(), entries.end(), [this](const auto &a, const auto &b) {
            if (!period_index && window_empire_sidebar_sort_get_current_sorting() != SORT_BY_PROFIT) return window_empire_sidebar_sort_sidebar_city_sorter(&a, &b) < 0;
            if (window_empire_sidebar_sort_get_current_sorting() != SORT_BY_NAME) {
                const int64_t left = sort_value(a.city_id), right = sort_value(b.city_id);
                if (left != right) return window_empire_sidebar_sort_get_sorting_reversed() ? left > right : left < right;
                return a.city_id < b.city_id;
            }
            return window_empire_sidebar_sort_sidebar_city_sorter(&a, &b) < 0;
        });
        cities.clear();
        for (const auto &entry : entries) cities.push_back(entry.city_id);
        page = std::min(page, std::max(0, (static_cast<int>(cities.size()) - 1) / capacity));
        resources.clear();
        const auto *selected_route = route(data.selected_city);
        if (selected_route) for (int i = 0; i < resource_loaded_count(); ++i) {
            const auto resource = resource_get_loaded(i);
            const char *key = resource_text_id(resource);
            if (key && selected_route->resources.count(key)) resources.push_back(resource);
        }
        resource_page = std::min(resource_page, std::max(0, (static_cast<int>(resources.size()) - 1) / 4));
    }
    const empire_city *selected() const { return data.selected_city > 0 && data.selected_city < empire_city_get_array_size() ? empire_city_get(data.selected_city) : nullptr; }
    int city_id(int index) const { const int row = page * capacity + index; return row >= 0 && row < cities.size() ? cities[row] : 0; }
    int repeat_count(std::string_view source) const override
    {
        if (source == "cities") return std::max(0, std::min(capacity, static_cast<int>(cities.size()) - page * capacity));
        if (source == "resources") return std::max(0, std::min(4, static_cast<int>(resources.size()) - resource_page * 4));
        return 0;
    }
    std::vector<std::string> choices(std::string_view binding) const override
    {
        auto tr = [](const char *key) { return std::string(reinterpret_cast<const char *>(translation_for_key(key))); };
        if (binding == "classic.sort") return {tr("TR_EMPIRE_SIDE_BAR_SORT_BY_NAME"), tr("TR_EMPIRE_SIDE_BAR_SORT_BY_QUOTA_FILL_EXPORT"), tr("TR_EMPIRE_SIDE_BAR_SORT_BY_QUOTA_FILL_IMPORT"), tr("TR_EMPIRE_SIDE_BAR_SORT_BY_ROUTE_COST"), tr("TR_EMPIRE_SIDE_BAR_SORT_BY_PROFIT")};
        if (binding == "filter.direction") return {tr("TR_UI_TRADE_LEDGER_TRADES"), tr("TR_UI_TRADE_LEDGER_BUYS"), tr("TR_UI_TRADE_LEDGER_SELLS")};
        if (binding == "filter.picker") { std::vector<std::string> result; for (const auto resource : picker_resources()) result.push_back(resource_text_id(resource)); result.push_back("Clear selection"); return result; }
        if (binding == "filter") return {tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE_SELL"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE_BUY"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_OPEN"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_CLOSED"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_LAND"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_SEA"), tr("TR_EMPIRE_SIDE_BAR_FILTER_BY_NONE")};
        std::vector<std::string> result;
        if (binding == "filter.resource") for (int i = 0; i < resource_loaded_count(); ++i) result.push_back(reinterpret_cast<const char *>(resource_get_data(resource_get_loaded(i))->text));
        if (binding == "history.year") { for (const auto &year : city_trade_ledger_periods()) result.push_back(std::to_string(year.year)); if (!result.empty()) result[0] = tr("TR_UI_CURRENT_YEAR"); }
        return result;
    }
    std::string text(std::string_view binding, int index) const override
    {
        auto tr = [](const char *key) { return std::string(reinterpret_cast<const char *>(translation_for_key(key))); };
        if (binding == "history.year") return period_index ? std::to_string(period.year) : tr("TR_UI_CURRENT_YEAR");
        if (binding == "filter.direction") return choices(binding)[direction_filter];
        if (binding == "treasury") return std::string("Dn ") + std::to_string(city_finance_treasury());
        if (binding == "period") return std::to_string(period.year) + (period.partial ? " - " + tr("TR_LEDGER_PARTIAL") : "");
        if (binding == "classic.sort") {
            const char *labels[] = {"TR_EMPIRE_SIDE_BAR_SORT_BY_NAME", "TR_EMPIRE_SIDE_BAR_SORT_BY_QUOTA_FILL_EXPORT", "TR_EMPIRE_SIDE_BAR_SORT_BY_QUOTA_FILL_IMPORT", "TR_EMPIRE_SIDE_BAR_SORT_BY_ROUTE_COST", "TR_EMPIRE_SIDE_BAR_SORT_BY_PROFIT"};
            return tr(labels[window_empire_sidebar_sort_get_current_sorting()]);
        }
        if (binding == "sort") {
            const char *labels[] = {"main_strings.44.9", "TR_LEDGER_EXPORTED", "TR_LEDGER_IMPORTED", "TR_LEDGER_ROUTE_COST", "main_strings.60.19"};
            return tr(labels[window_empire_sidebar_sort_get_current_sorting()]) + (window_empire_sidebar_sort_get_sorting_reversed() ? " (Z-A)" : " (A-Z)");
        }
        if (binding == "selected.name") return selected() ? reinterpret_cast<const char *>(empire_city_get_name(selected())) : tr("main_strings.47.8");
        if (binding == "filter") {
            const char *keys[] = {"TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE", "TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE_SELL", "TR_EMPIRE_SIDE_BAR_FILTER_BY_RESOURCE_BUY", "TR_EMPIRE_SIDE_BAR_FILTER_BY_OPEN", "TR_EMPIRE_SIDE_BAR_FILTER_BY_CLOSED", "TR_EMPIRE_SIDE_BAR_FILTER_BY_LAND", "TR_EMPIRE_SIDE_BAR_FILTER_BY_SEA", "TR_EMPIRE_SIDE_BAR_FILTER_BY_NONE"};
            return tr(keys[window_empire_sidebar_sort_get_current_filtering()]);
        }
        if (binding == "filter.resource") {
            const auto resource = window_empire_sidebar_sort_get_selected_filter_resource();
            return resource == RESOURCE_NONE ? tr("main_strings.52.19") : reinterpret_cast<const char *>(resource_get_data(resource)->text);
        }
        if (binding == "selected.info") {
            const int selection = empire_selected_object();
            const auto *object = selection ? empire_object_get(selection - 1) : nullptr;
            if (!object) return {};
            if (object->type == EMPIRE_OBJECT_ROMAN_ARMY && city_military_distant_battle_roman_army_is_traveling() && city_military_distant_battle_roman_months_traveled() == object->distant_battle_travel_months) return tr(city_military_distant_battle_roman_army_is_traveling_forth() ? "main_strings.47.15" : "main_strings.47.16");
            if (object->type == EMPIRE_OBJECT_ENEMY_ARMY && city_military_months_until_distant_battle() > 0 && city_military_distant_battle_enemy_months_traveled() == object->distant_battle_travel_months) return tr("main_strings.47.14");
            if (!selected()) return {};
            switch (selected()->type) {
                case EMPIRE_CITY_DISTANT_ROMAN: return tr("main_strings.47.12");
                case EMPIRE_CITY_VULNERABLE_ROMAN: return tr(city_military_distant_battle_city_is_roman() ? "main_strings.47.12" : "main_strings.47.13");
                case EMPIRE_CITY_FUTURE_TRADE: case EMPIRE_CITY_DISTANT_FOREIGN: case EMPIRE_CITY_FUTURE_ROMAN: return tr("main_strings.47.0");
                case EMPIRE_CITY_OURS: return tr("main_strings.47.1");
                default: return {};
            }
        }
        if (binding == "selected.open") return selected() ? tr("TR_LEDGER_OPEN_ROUTE") + " " + std::to_string(selected()->cost_to_open) : "";
        if (binding == "selected.cost") {
            if (!selected()) return {};
            std::string cost = std::to_string(selected()->cost_to_open) + " " + tr("main_strings.8.1");
            for (int resource = 1; resource < RESOURCE_SLOT_COUNT; ++resource) {
                const int loads = empire_city_trade_resource_cost(selected()->route_id, static_cast<resource_type>(resource));
                if (loads) cost += ", " + std::to_string(loads) + " " + reinterpret_cast<const char *>(resource_get_data(resource)->text);
            }
            return cost;
        }
        if (binding.substr(0, 9) == "resource.") {
            const int row = resource_page * 4 + index;
            if (!selected() || row < 0 || row >= resources.size()) return {};
            const auto resource = resources[row];
            if (binding == "resource.name") return reinterpret_cast<const char *>(resource_get_data(resource)->text);
            const bool imports = binding == "resource.import";
            const auto *entry = route(data.selected_city);
            if (!entry) return tr("TR_LEDGER_UNKNOWN");
            const auto &amounts = entry->resources.at(resource_text_id(resource));
            const int limit = imports ? amounts.import_limit : amounts.export_limit;
            if (!limit) return "-";
            return (entry->open ? std::to_string(imports ? amounts.imported : amounts.exported) + " / " : "") + std::to_string(limit);
        }
        const int id = city_id(index);
        const auto *city = id ? empire_city_get(id) : nullptr;
        if (!city) return {};
        if (binding == "city.name") return reinterpret_cast<const char *>(empire_city_get_name(city));
        if (binding == "city.status") {
            const auto *entry = route(id);
            return !entry ? tr("TR_LEDGER_UNKNOWN") : entry->open ? tr("TR_LEDGER_OPEN") : tr("TR_LEDGER_ROUTE_COST") + " " + std::to_string(entry->cost);
        }
        if (binding == "city.balance") {
            if (unknown_trade_balances.count(id)) return tr("main_strings.60.19") + ": —";
            const auto found = trade_balances.find(id);
            return tr("main_strings.60.19") + ": " + std::to_string(found != trade_balances.end() ? found->second : 0);
        }
        return {};
    }
    ImageGroupEntryRef image(std::string_view binding, int index) const override
    {
        if (binding == "filter.picker") {
            const auto items = picker_resources();
            const auto selected = index < 0 ? resource_filter : index < items.size() ? items[index] : RESOURCE_NONE;
            return selected == RESOURCE_NONE ? ImageGroupEntryRef::from_group("UI\\resource_picker", "resource_picker") : resource_graphics(selected).empire_icon();
        }
        if (binding == "filter.open") return open_filter == 0 ? ImageGroupEntryRef{} : ImageGroupEntryRef::from_group(open_filter == 1 ? "UI\\Selection_Checkmark" : "UI\\Denied_Walker_Checkmark", open_filter == 1 ? "Selection_Checkmark" : "Denied_Walker_Checkmark");
        if (binding == "filter.route") return ImageGroupEntryRef::from_group("UI\\Both_Trade_Icons", route_filter == 0 ? "Both_Trade_Icons" : route_filter == 1 ? "Land_Trade_Icon_Centered" : "Sea_Trade_Icon_Centered");
        if (binding == "sort.direction") return ImageGroupEntryRef::from_group("UI\\Arrow_Button", window_empire_sidebar_sort_get_sorting_reversed() ? "Increase" : "Decrease");
        return EmpireTradeController::image(binding, index);
    }
    int condition(std::string_view binding, int index) const override
    {
        if (binding == "sidebar.collapsed") return data.sidebar.border_btn.is_collapsed;
        if (binding == "period.older") return period_index + 1 < period_count;
        if (binding == "period.newer") return period_index > 0;
        if (binding == "selected.open_trade") return display_city() && display_city()->type == EMPIRE_CITY_TRADE && display_city()->is_open;
        if (binding == "selected.trade") return selected() && selected()->type == EMPIRE_CITY_TRADE;
        if (binding == "filter.resource") return window_empire_sidebar_sort_get_current_filtering() <= FILTER_BY_RESOURCE_BUY;
        if (binding == "filter.general") return window_empire_sidebar_sort_get_current_filtering() > FILTER_BY_RESOURCE_BUY;
        if (binding == "city.selected") return city_id(index) == data.selected_city;
        if (binding == "previous") return page > 0;
        if (binding == "next") return (page + 1) * capacity < cities.size();
        if (binding == "resources.previous") return resource_page > 0;
        if (binding == "resources.next") return (resource_page + 1) * 4 < resources.size();
        if (binding == "selected.city") return selected() != nullptr;
        if (binding == "selected.closed") return !period_index && selected() && !selected()->is_open && selected()->type == EMPIRE_CITY_TRADE;
        return 0;
    }
    const char *tooltip(std::string_view binding, int) const override
    {
        if (binding == "history.disabled") return "TR_UI_LEDGER_DISABLED_1";
        if (binding == "ledger") return "TR_UI_TOOLTIP_OPEN_TRADE_LEDGER";
        if (binding == "sort.reset") return "TR_UI_TOOLTIP_RESET_SORTING";
        if (binding == "filter.reset") return "TR_UI_TOOLTIP_RESET_FILTERS";
        if (binding == "filter.open") return open_filter == 0 ? "TR_UI_TOOLTIP_SHOW_OPEN_AND_CLOSED_ROUTES" : open_filter == 1 ? "TR_UI_TOOLTIP_SHOW_OPEN_ROUTES" : "TR_UI_TOOLTIP_SHOW_CLOSED_ROUTES";
        if (binding == "filter.route") return route_filter == 0 ? "TR_UI_TOOLTIP_SHOW_ALL_ROUTE_TYPES" : route_filter == 1 ? "TR_UI_TOOLTIP_SHOW_LAND_ROUTES" : "TR_UI_TOOLTIP_SHOW_SEA_ROUTES";
        return nullptr;
    }
    void action(std::string_view action, int index) override
    {
        if (action == "sort.reset") { window_empire_sidebar_sort_set_current_sorting(SORT_BY_NAME); window_empire_sidebar_sort_set_sorting_reversed(0); }
        if (action == "filter.reset") { open_filter = route_filter = direction_filter = 0; resource_filter = RESOURCE_NONE; }
        if (action == "filter.open") open_filter = (open_filter + 1) % 3;
        if (action == "filter.route") route_filter = (route_filter + 1) % 3;
        if (action == "filter.direction" && index >= 0 && index < 3) direction_filter = index;
        if (action == "filter.picker") { const auto items = picker_resources(); resource_filter = index >= 0 && index < items.size() ? items[index] : RESOURCE_NONE; }
        if (action == "city.select" && city_id(index)) empire_select_object_by_id(empire_city_get(city_id(index))->empire_object_id);
        else if (action == "city.ledger") window_trade_ledger_show(city_id(index), -1, static_cast<int>(period_index));
        else if (action == "ledger") window_trade_ledger_show(selected() ? data.selected_city : -1, -1, static_cast<int>(period_index));
        else if (action == "period.older" && condition(action, 0)) select_period(period_index + 1);
        else if (action == "period.newer" && period_index) select_period(period_index - 1);
        else if (action == "prices") button_show_prices(0, 0);
        else if (action == "advisor") button_advisor(ADVISOR_TRADE, 0);
        else if (action == "close") button_return_to_city(0, 0);
        else if (action == "help") button_help(0, 0);
        else if (action == "open" && condition("selected.closed", 0)) button_open_trade_by_route(selected()->route_id);
        else if (action == "previous" && page) --page;
        else if (action == "next" && condition("next", 0)) ++page;
        else if (action == "resources.previous" && resource_page) --resource_page;
        else if (action == "resources.next" && condition(action, 0)) ++resource_page;
        else if (action == "resource.settings" && !period_index && selected() && selected()->is_open && index >= 0 && resource_page * 4 + index < resources.size()) window_resource_settings_show(resources[resource_page * 4 + index]);
        else if (action == "filter") { window_empire_sidebar_sort_set_current_filtering(index >= 0 ? index : (window_empire_sidebar_sort_get_current_filtering() + 1) % MAX_FILTER_KEY); page = 0; }
        else if (action == "filter.resource") {
            if (index >= 0 && index < resource_loaded_count()) { window_empire_sidebar_sort_set_selected_filter_resource(resource_get_loaded(index)); refresh(); empire_city_list.initialize(&cities); window_invalidate(); return; }
            const auto previous = window_empire_sidebar_sort_get_selected_filter_resource();
            resource_type next = RESOURCE_NONE;
            if (previous == RESOURCE_NONE && resource_loaded_count()) next = resource_get_loaded(0);
            else for (int i = 0; i + 1 < resource_loaded_count(); ++i) if (resource_get_loaded(i) == previous) next = resource_get_loaded(i + 1);
            window_empire_sidebar_sort_set_selected_filter_resource(next); page = 0;
        }
        else if (action == "sort") window_empire_sidebar_sort_set_current_sorting(index >= 0 ? index : (window_empire_sidebar_sort_get_current_sorting() + 1) % MAX_SORTING_KEY);
        else if (action == "reverse") window_empire_sidebar_sort_set_sorting_reversed(!window_empire_sidebar_sort_get_sorting_reversed());
        if (action == "history.year" && index >= 0 && index < period_count) select_period(index);
        if (action == "sort" || action == "reverse" || action == "filter" || action == "filter.resource" || action == "history.year" || action == "sort.reset" || action.substr(0, 7) == "filter.") { refresh(); empire_city_list.initialize(&cities); }
        process_selection(); refresh(); window_invalidate();
    }
};
EmpireController empire_controller;
std::unique_ptr<DeclarativeWindowRuntime> empire_sidebar_ui, empire_details_ui, empire_hud_ui;
const DeclarativeWindowDefinition *sidebar_definition, *details_definition;
void initialize_empire_ui()
{
    for (const char *name : {"empire_sidebar", "empire_details", "empire_city_card", "empire_map"}) {
        const auto *definition = declarative_window_definition(name);
        if (!definition) { Logger::error("Missing required empire window: ", name); continue; }
        for (const auto &widget : definition->widgets()) {
            if (widget.type != DeclarativeWidgetType::Image && widget.type != DeclarativeWidgetType::ImageButton) continue;
            if (!widget.binding.empty()) continue;
            for (const auto &image : {widget.image_name, widget.pressed_image_name}) {
                if (image.empty()) continue;
                const auto ref = ImageGroupEntryRef::from_group(widget.assetlist_name, image);
                if (!ref.is_bound() || ref.width() <= 0 || ref.height() <= 0) Logger::errorf("Window %s widget %s has an unresolved image: %s/%s", name, widget.id.c_str(), widget.assetlist_name.c_str(), image.c_str());
            }
        }
    }
    const auto *hud = declarative_window_definition("empire_hud");
    empire_hud_ui = hud ? std::make_unique<DeclarativeWindowRuntime>(*hud, empire_controller) : nullptr;
    empire_controller.select_period(0);
    empire_city_list.initialize();
    sidebar_definition = declarative_window_definition("empire_sidebar");
    details_definition = declarative_window_definition("empire_details");
    empire_sidebar_ui = sidebar_definition ? std::make_unique<DeclarativeWindowRuntime>(*sidebar_definition, empire_controller) : nullptr;
    empire_details_ui = details_definition ? std::make_unique<DeclarativeWindowRuntime>(*details_definition, empire_controller) : nullptr;
}
void draw_empire_ui()
{
    process_selection();
    if (sidebar_definition && sidebar_definition->widget("cities")) {
        const auto &row = *sidebar_definition->widget("cities");
        empire_controller.capacity = std::max(1, (data.sidebar.height - row.y - 36) / std::max(1, row.repeat_spacing_y));
    }
    empire_controller.refresh();
    for (auto phase : {DeclarativeDrawPhase::Background, DeclarativeDrawPhase::Foreground}) {
        if (empire_hud_ui) empire_hud_ui->draw(phase, data.x_max - data.x_min, data.y_max - data.y_min, data.x_min, data.y_min);
        if (empire_sidebar_ui && !data.sidebar.border_btn.is_collapsed) empire_sidebar_ui->draw(phase, data.sidebar.width, data.sidebar.height, data.sidebar.x_min, data.sidebar.y_min);
        if (empire_details_ui) empire_details_ui->draw(phase, data.panel.x_max - data.panel.x_min, BOTTOM_PANEL_HEIGHT, data.panel.x_min, data.y_max - BOTTOM_PANEL_HEIGHT);
    }
}
int handle_empire_ui(const mouse *m)
{
    empire_city_list.hovered = false;
    mouse local = *m;
    if (m->y >= data.y_max - BOTTOM_PANEL_HEIGHT) {
        local.x -= data.panel.x_min; local.y -= data.y_max - BOTTOM_PANEL_HEIGHT;
        return empire_details_ui ? empire_details_ui->handle_mouse(local, data.panel.x_max - data.panel.x_min, BOTTOM_PANEL_HEIGHT) : 0;
    }
    if (!data.sidebar.border_btn.is_collapsed && m->x >= data.sidebar.x_min) {
        local.x -= data.sidebar.x_min; local.y -= data.sidebar.y_min;
        if (m->scrolled != SCROLL_NONE && !sidebar_definition->widget("city_list")) { empire_controller.action(m->scrolled == SCROLL_UP ? "previous" : "next", 0); return 1; }
        return empire_sidebar_ui ? empire_sidebar_ui->handle_mouse(local, data.sidebar.width, data.sidebar.height) : 0;
    }
    return 0;
}
}

static void draw_foreground(void)
{
    draw_map();
    draw_paneling();
    draw_empire_ui();
}

static void determine_selected_object(const mouse *m)
{
    if (is_map(m)) {
        if (!m->left.went_up || data.finished_scroll) {
            int hovered_obj_id = empire_get_hovered_object(m->x - data.x_min - 16, m->y - data.y_min - 16);
            data.hovered_object = hovered_obj_id;
            return;
        } else {
            empire_select_object(m->x - data.x_min - 16, m->y - data.y_min - 16);
            window_invalidate();
        }
    } else if (is_sidebar(m)) {
        data.hovered_object = NO_POSITION;
    } else {
        data.finished_scroll = 0;
        data.hovered_object = NO_POSITION;
        return;
    }
}

static void process_selection(void)
{
    int selected_object = empire_selected_object();
    if (selected_object) {
        data.selected_city = empire_city_get_for_object(selected_object - 1);
        //data.selected_city is array index of the empire object from the array of cities 
    } else {
        data.selected_city = 0;
    }
}

// -------------------------------------------------------------------------------------------------------
//                                              HANDLE INPUT
// -------------------------------------------------------------------------------------------------------

// Mouse position helper functions
static int is_sidebar(const mouse *m)
{
    if (m->x >= data.sidebar.x_min &&
        m->x < data.sidebar.x_max &&
        m->y >= data.sidebar.y_min &&
        m->y < data.sidebar.y_max) {
        return 1;
    }
    return 0;
}

static int is_sidebar_border(const mouse *m)
{
    if (!declarative_window_definition("empire_sidebar")) return 0;
    if (m->x >= data.sidebar.border_btn.x_min &&
        m->x <= data.sidebar.border_btn.x_max &&
        m->y >= data.sidebar.border_btn.y_min &&
        m->y <= data.sidebar.border_btn.y_max) {
        return 1;
    }
    return 0;
}

static int is_map(const mouse *m)
{
    if (m->x >= data.x_min + WIDTH_BORDER &&
        m->x < data.sidebar.x_min &&
        m->y >= data.y_min + WIDTH_BORDER &&
        m->y < data.y_max - BOTTOM_PANEL_HEIGHT - WIDTH_BORDER) {
        return 1;
    }
    return 0;
}

static int is_outside_map(int x, int y)
{
    return (x < data.x_min + 16 || x >= data.sidebar.x_min ||
        y < data.y_min + 16 || y >= data.y_max - BOTTOM_PANEL_HEIGHT);
}

static void handle_sidebar_border(const mouse *m)
{
    // Set hover state
    data.sidebar.border_btn.is_hovered = is_sidebar_border(m);

    // Early exit if mouse not on sidebar border
    if (!data.sidebar.border_btn.is_hovered) {
        return;
    }

    data.hovered_object = 0; //clear hovers from sidebar

    // Handle expand/collapse toggle on left mouse release
    if (m->left.went_up) {
        if (data.sidebar.border_btn.is_collapsed) {
            sidebar_expand();
        } else {
            data.sidebar.previous_width = data.sidebar.width_percent;
            data.sidebar.dragging_width = data.sidebar.width_percent;
            data.sidebar.dragging = 1;
        }
    }
}

void handle_sidebar_dragging(const mouse *m)
{
    if (m->left.went_up) {
        data.sidebar.dragging = 0; // stopped dragging
        if (data.sidebar.dragging_width <= 5) {
            sidebar_collapse();
        } else {
            data.sidebar.width_percent = data.sidebar.dragging_width; // save the width percent
            config_set(CONFIG_UI_EMPIRE_SIDEBAR_WIDTH, data.sidebar.width_percent);
        }
        return;
    }
    if (m->right.went_up) {
        data.sidebar.dragging = 0;
        data.sidebar.width_percent = data.sidebar.previous_width;
        window_invalidate(); // reset to previous width
        return;
    }
    const int map_draw_x_max = data.x_max - WIDTH_BORDER;                 // right edge of usable map
    const int map_draw_x_min = data.x_min + WIDTH_BORDER;                 // left edge of usable map

    // Ignore if mouse isn't over the map horizontally
    if (m->x < map_draw_x_min || m->x > map_draw_x_max || data.usable_map_width <= 0) {
        return;
    }

    // Percent position measured FROM THE RIGHT EDGE (0 at right edge, 100 at left edge)
    const int dist_from_right_px = map_draw_x_max - m->x;
    int mouse_percent_from_right = (dist_from_right_px * 100) / data.usable_map_width;

    // Snap to 2% strips (each strip = 2% of map width)
    const int strip = 2;
    int strip_index = mouse_percent_from_right / strip;
    if (strip_index < 0) strip_index = 0;
    int new_width = (strip_index * strip) + 1; // +1 to center in strip
    if (new_width < 10) {
        data.sidebar.dragging_width = 0;      // collapse preview
    } else if (new_width < 20) {
        data.sidebar.dragging_width = 20;     // minimum visible width (20%)
    } else if (new_width > 70) {
        data.sidebar.dragging_width = 70;     // maximum width (70%)
    } else {
        data.sidebar.dragging_width = static_cast<uint8_t>(new_width);
    }

    // Immediate layout update for live feedback
    int raw = (data.usable_map_width * data.sidebar.dragging_width) / 100;
    data.sidebar.width =
        ((raw + (BLOCK_SIZE / 2)) / BLOCK_SIZE) * BLOCK_SIZE
        + data.sidebar.margin_left + data.sidebar.margin_right;

    data.sidebar.x_max = map_draw_x_max;
    data.sidebar.x_min = data.sidebar.x_max - data.sidebar.width;
}


static void handle_input(const mouse *m, const hotkeys *h)
{
    pixel_offset position;
    if (data.sidebar.dragging) {
        handle_sidebar_dragging(m);
        return; //block other input handling if the sidebar is being dragged
    }
    if (scroll_get_delta(m, &position, SCROLL_TYPE_EMPIRE)) {
        empire_scroll_map(position.x, position.y);
    }

    data.focus_resource = RESOURCE_NONE;
    if (handle_empire_ui(m)) return;

    if (m->is_touch) {
        const touch *t = touch_get_earliest();
        if (!is_outside_map(t->current_point.x, t->current_point.y) && !is_sidebar(m)) { // disable dragging on sidebar
            if (t->has_started) {
                data.is_scrolling = 1;
                scroll_drag_start(1);
            }
        }
        if (t->has_ended) {
            data.is_scrolling = 0;
            data.finished_scroll = !touch_was_click(t);
            scroll_drag_end();
        }
    }
    data.focus_button_id = 0;
    data.focus_resource = RESOURCE_NONE;
    determine_selected_object(m);
    handle_sidebar_border(m);
    process_selection();
    int selected_object = empire_selected_object();
    if (selected_object) {

        const empire_object *obj = empire_object_get(selected_object - 1);
        // allow de-selection only for objects that are currently selected/drawn, otherwise exit empire map
        if (input_go_back_requested(m, h)) {

            switch (obj->type) {
                case EMPIRE_OBJECT_CITY:

                    empire_clear_selected_object();
                    window_invalidate();
                    break;
                case EMPIRE_OBJECT_ROMAN_ARMY:

                    if (city_military_distant_battle_roman_army_is_traveling()) {
                        if (city_military_distant_battle_roman_months_traveled() == obj->distant_battle_travel_months) {
                            empire_clear_selected_object();
                            window_invalidate();
                        }
                    }
                    break;
                case EMPIRE_OBJECT_ENEMY_ARMY:
                    if (city_military_months_until_distant_battle() > 0) {
                        if (city_military_distant_battle_enemy_months_traveled() == obj->distant_battle_travel_months) {
                            empire_clear_selected_object();
                            window_invalidate();
                        }
                    }
                    break;
                default:
                    window_city_show();
                    break;
            }
        }

    } else {
        if (m->right.went_down) {
            scroll_drag_start(0);
        }
        if (m->right.went_up) {
            int has_scrolled = scroll_drag_end();
            if (!has_scrolled && input_go_back_requested(m, h)) {
                window_city_show();
            }
        }
        if (h->escape_pressed) { // handle escape
            window_city_show();
        }
    }
}

static void get_tooltip(tooltip_context *c)
{
    if (data.sidebar.border_btn.is_hovered) {
        c->type = TOOLTIP_BUTTON;
        c->translation_key = "TR_TOOLTIP_CHANGE_SIDEBAR_WIDTH";
    } else {
        if (empire_sidebar_ui) empire_sidebar_ui->tooltip(*c);
        empire_city_list.tooltip(*c);
        if (empire_details_ui) empire_details_ui->tooltip(*c);
    }
}
// -------------------------------------------------------------------------------------------------------
//                                              BUTTON HANDLERS     
// -------------------------------------------------------------------------------------------------------

static void button_help(int param1, int param2)
{
    (void) param1;
    (void) param2;
    window_message_dialog_show(MESSAGE_DIALOG_EMPIRE_MAP, 0);
}

static void button_return_to_city(int param1, int param2)
{
    (void) param1;
    (void) param2;
    window_city_show();
}

static void button_advisor(int advisor, int param2)
{
    (void) param2;
    window_advisors_show_advisor(static_cast<advisor_type>(advisor));
}

static void button_show_prices(int param1, int param2)
{
    (void) param1;
    (void) param2;
    window_trade_prices_show(0, 0, screen_width(), screen_height());
}

static void confirmed_open_trade_by_route(int accepted, int checked)
{
    (void) checked;
    if (accepted) {
        int city_id = empire_city_get_for_trade_route(data.selected_trade_route);
        empire_city_open_trade(city_id, 1);
        if (!empire_city_is_trade_route_open(data.selected_trade_route)) return;
        building_menu_update();
        window_trade_opened_show(city_id);
    }
}

static void button_open_trade_by_route(int route_id)
{
    data.selected_trade_route = route_id;
    const int city_id = empire_city_get_for_trade_route(route_id);
    if (!empire_city_can_pay_trade_resources(city_id)) { city_warning_show_translated(WARNING_RESOURCES_NOT_AVAILABLE); return; }
    const auto *city = empire_city_get(city_id);
    static std::string cost;
    bool has_resource_cost = false;
    cost = std::to_string(city->cost_to_open) + " " + reinterpret_cast<const char *>(lang_get_string("main_strings.8.1"));
    for (int resource = 1; resource < RESOURCE_SLOT_COUNT; ++resource) {
        const int loads = empire_city_trade_resource_cost(route_id, static_cast<resource_type>(resource));
        if (loads) { has_resource_cost = true; cost += ", " + std::to_string(loads) + " " + reinterpret_cast<const char *>(resource_get_data(resource)->text); }
    }
    if (has_resource_cost) {
        window_popup_dialog_show_confirmation(lang_get_string("main_strings.47.6"), reinterpret_cast<const uint8_t *>(cost.c_str()), nullptr, confirmed_open_trade_by_route);
        return;
    }
    window_popup_dialog_show(POPUP_DIALOG_OPEN_TRADE, confirmed_open_trade_by_route, 2);
}

// -------------------------------------------------------------------------------------------------------
//                                              WINDOW SHOW 
// -------------------------------------------------------------------------------------------------------

void window_empire_show(void)
{

    init();
    setup_sidebar();
    initialize_empire_ui();
    window_type window = {
        WINDOW_EMPIRE,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    window_show(&window);
}

int window_empire_is_dragging_sidebar(void)
{
    return data.sidebar.dragging;
}

void window_empire_show_checked(void)
{
    tutorial_availability avail = tutorial_advisor_empire_availability();
    if (avail == AVAILABLE) {
        window_empire_show();
    } else if (avail == NOT_AVAILABLE) {
        city_warning_show(WARNING_NOT_AVAILABLE, translation_for_key("TR_CITY_WARNING_NOT_AVAILABLE"));
    } else {
        city_warning_show(WARNING_NOT_AVAILABLE_YET, translation_for_key("TR_CITY_WARNING_NOT_AVAILABLE_YET"));
    }
}

#include "../../tools/catch_up_test/empire_ui.h"
