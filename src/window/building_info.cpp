#include "window/advisors.h"
#include "window/building/figures.h"
#include "window/building/military.h"
#include "building/barracks.h"
#include "building/roadblock.h"

#include "building_info.h"

#include "assets/assets.h"
#include "building/building_type_api.h"
#include "building/culture.h"
#include "../building/distribution.h"
#include "building/house_evolution.h"
#include "../building/industry.h"
#include "building/monument.h"
#include "building/properties.h"
#include "building/religion.h"
#include "building/temple.h"
#include "building/warehouse.h"
#include "city/map.h"
#include "city/view.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image_group.h"
#include "figure/figure.h"
#include "figure/formation_legion.h"
#include "figure/roamer_preview.h"
#include "figure/phrase.h"
#include "game/state.h"
#include "graphics/ui_runtime_api.h"
#include "graphics/generic_button.h"
#include "graphics/image_button.h"
#include "graphics/lang_text.h"
#include "graphics/screen.h"
#include "graphics/text.h"
#include "graphics/window.h"
#include "input/input.h"
#include "map/aqueduct.h"
#include "map/bridge.h"
#include "map/building.h"
#include "map/figure.h"
#include "map/grid.h"
#include "map/image.h"
#include "map/property.h"
#include "map/sprite.h"
#include "map/terrain.h"
#include "window/city.h"
#include "window/message_dialog.h"
#include "window/building/depot.h"
#include "window/building/distribution.h"
#include "window/building/government.h"
#include "window/building/industry.h"
#include "window/building/house.h"
#include "window/building/terrain.h"
#include "window/building/utility.h"
#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "graphics/image.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

using building_type_registry_impl::BuildingType;

enum {
    HEIGHT_0_22_BLOCKS = 0,
    HEIGHT_1_16_BLOCKS = 1,
    HEIGHT_2_18_BLOCKS = 2,
    HEIGHT_3_20_BLOCKS = 3,
    HEIGHT_4_14_BLOCKS = 4,
    HEIGHT_5_24_BLOCKS = 5,
    HEIGHT_6_38_BLOCKS = 6,
    HEIGHT_7_26_BLOCKS = 7,
    HEIGHT_8_40_BLOCKS = 8,
    HEIGHT_10_46_BLOCKS = 10,
    HEIGHT_11_28_BLOCKS = 11,
    HEIGHT_13_15_BLOCKS = 13,
};

#define OFFSET(x,y) (x + GRID_SIZE * y)
#define SMALL_ICON_SIDE 24

static void button_help(int param1, int param2);
static void button_close(int param1, int param2);
static void button_advisor(int advisor, int param2);
static void button_mothball(int mothball, int param2);
static void button_monument_construction(const generic_button *button);

static image_button image_buttons_help_advisor_close[] = {
    {0, 0, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 0, button_help, button_none, 0, 0, 1},
    {0, 0, 24, 24, IB_NORMAL, GROUP_CONTEXT_ICONS, 4, button_close, button_none, 0, 0, 1},
    {0, 0, 24, 24, IB_NORMAL, 0, 0, button_advisor, button_none, 0, 0, 1}
};

static image_button image_button_mothball[] = {
    {0, 0, 24, 24, IB_NORMAL, 0, 0, button_mothball, button_none, 0, 0, 1, "UI", "Mothball_1"},
    {0, 0, 24, 24, IB_NORMAL, 0, 0, button_mothball, button_none, 0, 0, 1, "UI", "Unmothball_1"}
}; //0d to adjust dynamically via init

static generic_button generic_button_monument_construction[] = {
    {80, 3, 304, 24, button_monument_construction}
};

static building_info_context context;
static building_info_context previous_context; //allow layering contexts
static unsigned int focus_image_button_id;
static unsigned int focus_mothball_image_button_id;
static unsigned int focus_monument_construction_button_id;
static int original_overlay;
static int previous_overlay;

static const char *advisor_button_image_base(int advisor)
{
    switch (advisor) {
        case ADVISOR_LABOR:
            return "Advisor_Building_Window_Labor";
        case ADVISOR_MILITARY:
            return "Advisor_Building_Window_Military";
        case ADVISOR_IMPERIAL:
            return "Advisor_Building_Window_Imperial";
        case ADVISOR_RATINGS:
            return "Advisor_Building_Window_Ratings";
        case ADVISOR_TRADE:
            return "Advisor_Building_Window_Trade";
        case ADVISOR_POPULATION:
            return "Advisor_Building_Window_Population";
        case ADVISOR_HOUSING:
            return "Advisor_Building_Window_Housing";
        case ADVISOR_HEALTH:
            return "Advisor_Building_Window_Health";
        case ADVISOR_EDUCATION:
            return "Advisor_Building_Window_Education";
        case ADVISOR_ENTERTAINMENT:
            return "Advisor_Building_Window_Entertainment";
        case ADVISOR_RELIGION:
            return "Advisor_Building_Window_Religion";
        case ADVISOR_FINANCIAL:
            return "Advisor_Building_Window_Financial";
        case ADVISOR_CHIEF:
            return "Advisor_Building_Window_Chief";
        default:
            return "Advisor_Building_Window_Border";
    }
}

static int image_button_state_suffix(const image_button &button)
{
    if (!button.enabled) {
        return 4;
    }
    if (button.pressed) {
        return 3;
    }
    if (button.focused) {
        return 2;
    }
    return 1;
}

static void update_advisor_button_image(int advisor)
{
    static char image_name[64];
    std::snprintf(
        image_name,
        sizeof(image_name),
        "%s_%d",
        advisor_button_image_base(advisor),
        image_button_state_suffix(image_buttons_help_advisor_close[2]));

    image_buttons_help_advisor_close[2].assetlist_name = "UI\\Advisor_Building_Window_Border_1";
    image_buttons_help_advisor_close[2].image_name = image_name;
    image_buttons_help_advisor_close[2].image_offset = 0;
    image_buttons_help_advisor_close[2].parameter1 = advisor;
}

static int building_type_attr_is(building_type type, const char *text_id)
{
    const BuildingType *definition = building_type_registry_impl::definition_for_type(type);
    return definition && std::strcmp(definition->attr(), text_id) == 0;
}

static int building_type_attr_is_any(building_type type, std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (building_type_attr_is(type, text_id)) {
            return 1;
        }
    }
    return 0;
}

static int type_is_storage(const BuildingType &type_definition)
{
    return type_definition.is_granary() || type_definition.is_warehouse();
}

static int grand_temple_panel_height_blocks(const BuildingType &type_definition)
{
    const building_type_registry_impl::Religion *religion = type_definition.religion();
    if (!religion || !religion->presentation().is_complete()) {
        return 0;
    }
    return religion->presentation().height_blocks();
}

static void init_context_buttons(building_info_context *c)
{
    int y_offset = c->y_offset + c->height_blocks * BLOCK_SIZE - 13 - SMALL_ICON_SIDE; // 13px from bottom edge
    //help button
    image_buttons_help_advisor_close[0].y_offset = y_offset;
    image_buttons_help_advisor_close[0].x_offset = c->x_offset + 14;
    //close button
    image_buttons_help_advisor_close[1].y_offset = y_offset;
    image_buttons_help_advisor_close[1].x_offset = c->x_offset + c->width_blocks * BLOCK_SIZE - 40; // 40px from right edge
    //advisor button
    image_buttons_help_advisor_close[2].y_offset = y_offset;
    image_buttons_help_advisor_close[2].x_offset = c->x_offset + 38;
    for (int i = 0; i < 2; i++) {
        image_button_mothball[i].y_offset = y_offset;
        image_button_mothball[i].x_offset = c->x_offset + c->width_blocks * BLOCK_SIZE - 64; // 64px from right edge
    }
}

static void update_context_buttons_vertical_offset(int new_context_y_offset, int new_context_block_height)
{
    int y_offset = new_context_y_offset + new_context_block_height * BLOCK_SIZE - SMALL_ICON_SIDE - 18; // 18px from bottom edge
    for (int i = 0; i < 3; i++) {
        image_buttons_help_advisor_close[i].y_offset = y_offset;
    }
}

static int get_height_id(void)
{
    if (context.type == BUILDING_INFO_TERRAIN) {
        switch (context.terrain_type) {
            case TERRAIN_INFO_AQUEDUCT:
                return HEIGHT_4_14_BLOCKS;
            case TERRAIN_INFO_RUBBLE:
            case TERRAIN_INFO_WALL:
            case TERRAIN_INFO_GARDEN:
                return HEIGHT_1_16_BLOCKS;
            default:
                return HEIGHT_5_24_BLOCKS;
        }
    } else if (context.type == BUILDING_INFO_LEGION) {
        return HEIGHT_7_26_BLOCKS;
    } else if (context.type == BUILDING_INFO_BUILDING) {
        const Building current_building = context.building;
        building *b = current_building.id() ? building_get(current_building.id()) : nullptr;
        if (!b || !current_building.type) {
            return HEIGHT_0_22_BLOCKS;
        }
        const BuildingType &type_definition = *current_building.type;
        const building_type type = static_cast<building_type>(b->type);
        const int is_dock = std::strcmp(type_definition.attr(), "dock") == 0;
        if (type_definition.is_well()) {
            return HEIGHT_4_14_BLOCKS;
        }
        if (building_is_house(type)) {
            return HEIGHT_5_24_BLOCKS;
        }

        if (building_type_attr_is_any(type, {
            "small_pond", "large_pond", "pine_tree", "fir_tree", "oak_tree", "elm_tree", "fig_tree",
            "plum_tree", "palm_tree", "date_tree", "pine_path", "fir_path", "oak_path", "elm_path",
            "fig_path", "plum_path", "palm_path", "date_path", "garden_path", "pavilion", "goddess_statue",
            "senator_statue"
        })) {
            return HEIGHT_1_16_BLOCKS;
        }

        if (building_type_attr_is_any(type, {
            "small_statue", "medium_statue", "large_statue", "legion_statue", "decorative_column",
            "horse_statue", "burning_ruin", "reservoir", "native_hut", "native_hut_alt",
            "native_meeting", "native_crops", "native_decor", "native_monument", "native_watchtower",
            "mission_post", "gatehouse", "tower", "military_academy", "governors_house",
            "governors_villa", "governors_palace", "workcamp", "architect_guild", "obelisk",
            "hedge_dark", "hedge_light", "colonnade", "watchtower", "looped_garden_wall",
            "roofed_garden_wall", "panelled_garden_wall", "palisade", "gladiator_statue"
        })) {
            return HEIGHT_1_16_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "fountain", "gladiator_school", "lion_house", "actor_colony", "chariot_maker"
        })) {
            return HEIGHT_2_18_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "prefecture", "engineers_post", "barber", "bathhouse", "doctor", "hospital",
            "market", "latrines", "wharf", "shipyard"
        })) {
            return HEIGHT_3_20_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "roadblock", "hedge_gate_dark", "hedge_gate_light", "palisade_gate",
            "looped_garden_gate", "roofed_garden_wall_gate", "panelled_garden_gate"
        })) {
            return HEIGHT_4_14_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "tavern", "amphitheater", "arena", "oracle", "nymphaeum", "small_mausoleum",
            "large_mausoleum", "triumphal_arch", "ship_bridge", "low_bridge", "depot"
        })) {
            return HEIGHT_5_24_BLOCKS;
        }
        if (is_dock || building_type_attr_is_any(type, {"lighthouse", "caravanserai"})) {
            return HEIGHT_6_38_BLOCKS;
        }
        if (type_definition.is_temple_tier(building_type_registry_impl::ReligionTier::Grand)) {
            const int panel_height = grand_temple_panel_height_blocks(type_definition);
            return panel_height > 0 ? panel_height : HEIGHT_8_40_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "hippodrome", "colosseum"
        })) {
            return HEIGHT_8_40_BLOCKS;
        }
        if (building_type_attr_is_any(type, {
            "fort_legionaries", "fort_javelin", "fort_mounted", "fort_swords", "fort_archers",
            "mess_hall", "city_mint", "barracks", "granary", "warehouse", "warehouse_space"
        })) {
            return HEIGHT_11_28_BLOCKS;
        }
        if (building_type_attr_is_any(type, {"lararium", "armoury"})) {
            return HEIGHT_13_15_BLOCKS;
        }
    }
    return HEIGHT_0_22_BLOCKS;
}

static void adjust_height_for_storage_buildings(building_info_context *c)
{
    building *b = c->building.id() ? building_get(c->building.id()) : nullptr;
    if (!b || !b->storage_id) {
        return;
    }
    int stored_types = building_storage_count_stored_resource_types(c->building.id());
    int y_offset_blocks = 0;

    if (!b->has_plague && c->has_road_access) {
        y_offset_blocks = ((stored_types - 1) / 2 - 3) * 2 + 2;
    }
    c->height_blocks += y_offset_blocks;
}

static int has_mothball_button(const Building &building)
{
    return building.type ? building.type->required_workers() : 0;
}

static void draw_mothball_button(void)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (!b) {
        return;
    }
    if (b->state == BUILDING_STATE_MOTHBALLED) {
        image_buttons_draw(0, 0, image_button_mothball, 2); //  x and y offsets adjusted dynamically in init()
    } else {
        image_buttons_draw(0, 0, image_button_mothball, 1);
    }
}

static void draw_halt_monument_construction_button(int x, int y, int focused, building *monument)
{
    int width = BLOCK_SIZE * (context.width_blocks - 10);
    button_border_draw(x, y, width, 20, focused ? 1 : 0);
    if (monument->state != BUILDING_STATE_MOTHBALLED) {
        text_draw_centered(translation_for_key("TR_BUTTON_HALT_MONUMENT_CONSTRUCTION"), x, y + 4, width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    } else {
        text_draw_centered(translation_for_key("TR_BUTTON_RESUME_MONUMENT_CONSTRUCTION"), x, y + 4, width, FONT_NORMAL_BLACK, screen_ui_to_pixel(font_definition_for(FONT_NORMAL_BLACK)->line_height), 0);
    }
}

static int center_in_city(int element_width_pixels)
{
    int x, y, width, height;
    city_view_get_viewport(&x, &y, &width, &height);
    x = screen_pixel_to_ui(x);
    width = screen_pixel_to_ui(width);
    int margin = (width - element_width_pixels) / 2;
    return x + margin;
}

static void init(int grid_offset)
{
    original_overlay = game_state_overlay();
    context.can_play_sound = 1;
    context.show_special_orders = 0;
    context.depot_selection = 0;
    context.advisor_button = ADVISOR_NONE;
    context.building = Building(nullptr);
    context.rubble_building_id = map_building_rubble_building_id(grid_offset);
    context.has_reservoir_pipes = map_terrain_is(grid_offset, TERRAIN_RESERVOIR_RANGE);
    context.aqueduct_has_water = map_aqueduct_has_water_access_at(grid_offset);
    context.has_road_access = 0;
    context.worker_percentage = 0;
    context.formation_id = 0;
    context.formation_types = 0;
    context.barracks_soldiers_requested = 0;
    context.worst_desirability_building_type = BUILDING_NONE;
    context.warehouse_space_text = 0;

    city_resource_determine_available(1);
    context.type = BUILDING_INFO_TERRAIN;
    context.figure.drawn = 0;

    const unsigned int selected_building_id = map_building_at(grid_offset);
    building *selected_building = selected_building_id ? building_get(selected_building_id) : nullptr;

    if (map_is_bridge(grid_offset)) {
        if (selected_building) {
            context.building = Building(selected_building);
        }
        if (map_terrain_is(grid_offset, TERRAIN_WATER)) {
            context.terrain_type = TERRAIN_INFO_BRIDGE;
        } else {
            context.terrain_type = TERRAIN_INFO_EMPTY;
        }
    } else if (map_property_is_plaza_earthquake_or_overgrown_garden(grid_offset)) {
        if (map_terrain_is(grid_offset, TERRAIN_ROAD)) {
            context.terrain_type = TERRAIN_INFO_PLAZA;
        }
        if (map_terrain_is(grid_offset, TERRAIN_ROCK)) {
            context.terrain_type = TERRAIN_INFO_EARTHQUAKE;
        }
        if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
            context.terrain_type = TERRAIN_INFO_GARDEN;
        }
    } else if (map_terrain_is(grid_offset, TERRAIN_TREE)) {
        context.terrain_type = TERRAIN_INFO_TREE;
    } else if (map_terrain_is(grid_offset, TERRAIN_ROCK)) {
        if (grid_offset == city_map_entry_flag()->grid_offset) {
            context.terrain_type = TERRAIN_INFO_ENTRY_FLAG;
        } else if (grid_offset == city_map_exit_flag()->grid_offset) {
            context.terrain_type = TERRAIN_INFO_EXIT_FLAG;
        } else {
            context.terrain_type = TERRAIN_INFO_ROCK;
        }
    } else if ((map_terrain_get(grid_offset) & (TERRAIN_WATER | TERRAIN_BUILDING)) == TERRAIN_WATER) {
        context.terrain_type = TERRAIN_INFO_WATER;
    } else if (map_terrain_is(grid_offset, TERRAIN_SHRUB)) {
        context.terrain_type = TERRAIN_INFO_SHRUB;
    } else if (map_terrain_is(grid_offset, TERRAIN_GARDEN)) {
        context.terrain_type = TERRAIN_INFO_GARDEN;
    } else if ((map_terrain_get(grid_offset) & (TERRAIN_ROAD | TERRAIN_BUILDING)) == TERRAIN_ROAD) {
        context.terrain_type = TERRAIN_INFO_ROAD;
    } else if (map_terrain_is(grid_offset, TERRAIN_AQUEDUCT)) {
        context.terrain_type = TERRAIN_INFO_AQUEDUCT;
    } else if (map_terrain_is(grid_offset, TERRAIN_RUBBLE)) {
        context.terrain_type = TERRAIN_INFO_RUBBLE;
    } else if (map_terrain_is(grid_offset, TERRAIN_WALL)) {
        context.terrain_type = TERRAIN_INFO_WALL;
    } else if (map_terrain_is(grid_offset, TERRAIN_HIGHWAY)) {
        context.terrain_type = TERRAIN_INFO_HIGHWAY;
    } else if (!selected_building) {
        context.terrain_type = TERRAIN_INFO_EMPTY;
    } else {
        context.type = BUILDING_INFO_BUILDING;
        building *b = selected_building;
        building_type btype = static_cast<building_type>(b->type);
        if (building_type_attr_is(btype, "fort_ground")) {
            context.formation_id = b->formation_id;
            if (b->prev_part_building_id) {
                b = building_get(b->prev_part_building_id);
                btype = static_cast<building_type>(b->type);
            }
        } else if (building_is_fort(btype)) {
            context.formation_id = b->formation_id;
        } else if (building_type_attr_is_any(btype, {"warehouse_space", "hippodrome"})) {
            b = building_main(b);
            btype = static_cast<building_type>(b->type);
        }

        context.building = Building(b);
        const Building current_building = context.building;
        const BuildingType *type_definition = current_building.type;
        context.worker_percentage = calc_percentage(
            b->num_workers, type_definition ? type_definition->required_workers() : 0);

        if (building_type_attr_is(btype, "barracks")) {
            context.barracks_soldiers_requested = formation_legion_recruits_needed();
            if (Barracks(b).unmanned_tower(nullptr).id()) {
                context.barracks_soldiers_requested++;
            }
        } else if (b->house_size) {
            Building house(b);
            context.worst_desirability_building_type =
                building_house_determine_worst_desirability_building_type(house);
            building_house_determine_evolve_text(house, context.worst_desirability_building_type);
        }
        // TODO: this information should be derived from b->has_road_access.
        // Context information should not differ from building properties.
        if (type_definition && type_definition->is_granary()) {
            context.has_road_access = map_has_road_access_granary(b->x, b->y, 0);
        } else if (building_type_attr_is(btype, "hippodrome")) {
            context.has_road_access = map_has_road_access_hippodrome_rotation(b->x, b->y, 0, b->subtype.orientation);
        } else if (type_definition && type_definition->is_warehouse()) {
            context.has_road_access = map_has_road_access_warehouse(b->x, b->y, 0);
            const Building warehouse(b);
            context.warehouse_space_text = building_warehouse_get_space_info(warehouse);
        } else if (building_type_attr_is(btype, "depot")) {
            context.has_road_access = map_has_road_access(b->x, b->y, b->size, 0);
            game_state_set_overlay(OVERLAY_STORAGES);
            window_building_depot_init_main(b->id);
        } else if (building_monument_is_unfinished_monument(b)) {
            context.has_road_access = map_has_road_access_monument_construction(b->x, b->y, b->size);
        } else {
            context.has_road_access = map_has_road_access(b->x, b->y, b->size, 0);
        }
        figure_roamer_preview_reset(b->type);
        figure_roamer_preview_create(b->type, b->x, b->y);
    }
    // figures
    context.figure.selected_index = 0;
    context.figure.count = 0;
    for (int i = 0; i < 7; i++) {
        context.figure.figure_ids[i] = 0;
    }
    static const int FIGURE_OFFSETS[] = {
        OFFSET(0,0), OFFSET(0,-1), OFFSET(0,1), OFFSET(1,0), OFFSET(-1,0),
        OFFSET(-1,-1), OFFSET(1,-1), OFFSET(-1,1), OFFSET(1,1)
    };
    for (int i = 0; i < 9 && context.figure.count < 7; i++) {
        int figure_id = map_figure_at(grid_offset + FIGURE_OFFSETS[i]);
        while (figure_id > 0 && context.figure.count < 7) {
            Figure *f = Figure::get(figure_id);
            if (f->state != FIGURE_STATE_DEAD &&
                f->action_state != FIGURE_ACTION_149_CORPSE) {
                switch (f->type) {
                    case FIGURE_NONE:
                    case FIGURE_EXPLOSION:
                    case FIGURE_MAP_FLAG:
                    case FIGURE_FLOTSAM:
                    case FIGURE_ARROW:
                    case FIGURE_JAVELIN:
                    case FIGURE_BOLT:
                    case FIGURE_BALLISTA:
                    case FIGURE_CATAPULT_MISSILE:
                    case FIGURE_CREATURE:
                    case FIGURE_FISH_GULLS:
                    case FIGURE_SPEAR:
                    case FIGURE_HIPPODROME_HORSES:
                    case FIGURE_FRIENDLY_ARROW:
                    case FIGURE_WATCHTOWER_ARCHER:
                        break;
                    default:
                        context.figure.figure_ids[context.figure.count++] = figure_id;
                        figure_phrase_determine(f);
                        break;
                }
            }
            figure_id = f->next_figure_id_on_same_tile;
        }
    }
    // check for legion figures
    for (int i = 0; i < 7; i++) {
        int figure_id = context.figure.figure_ids[i];
        if (figure_id <= 0) {
            continue;
        }
        Figure *f = Figure::get(figure_id);
        if (f->type == FIGURE_FORT_STANDARD || f->is_legion()) {
            context.type = BUILDING_INFO_LEGION;
            context.formation_id = f->formation_id;
            const formation *m = formation_get(context.formation_id);
            if (m->figure_type != FIGURE_FORT_LEGIONARY && m->figure_type != FIGURE_FORT_INFANTRY) {
                context.formation_types = 5;
            } else if (m->has_military_training) {
                context.formation_types = 4;
            } else {
                context.formation_types = 3;
            }
            break;
        }
    }
    // dialog size
    context.width_blocks = 29;

    int height_id = get_height_id();
    if (height_id > HEIGHT_13_15_BLOCKS) {
        context.height_blocks = height_id;
    } else {
        switch (height_id) {
            case 1: context.height_blocks = 16; break;
            case 2: context.height_blocks = 18; break;
            case 3: context.height_blocks = 20; break;
            case 4: context.height_blocks = 14; break;
            case 5: context.height_blocks = 24; break;
            case 6: context.height_blocks = 38; break;
            case 7: context.height_blocks = 26; break;
            case 8: context.height_blocks = 40; break;

            case 10: context.height_blocks = 46; break;
            case 11: context.height_blocks = 28; break;

            case 13: context.height_blocks = 15; break;
            default: context.height_blocks = 22; break;
        }
    }
    adjust_height_for_storage_buildings(&context);
    const Building current_dialog_building = context.building;
    if ((screen_height() <= 600) ||
        (screen_height() <= 720 && current_dialog_building.type && current_dialog_building.type->is_grand_temple_mars())) {
        context.height_blocks = calc_bound(context.height_blocks, 0, 26);
    }
    // dialog placement
    int s_width = screen_width();
    int s_height = screen_height();
    context.x_offset = center_in_city(BLOCK_SIZE * context.width_blocks);
    if (s_width >= 1024 && s_height >= 768) {
        context.x_offset = mouse_get()->x;
        context.y_offset = mouse_get()->y;
        window_building_set_possible_position(&context.x_offset, &context.y_offset,
            context.width_blocks, context.height_blocks);
    } else if (s_height >= 600 && mouse_get()->y <= (s_height - 24) / 2 + 24) {
        context.y_offset = s_height - BLOCK_SIZE * context.height_blocks - MARGIN_POSITION;
    } else {
        context.y_offset = MIN_Y_POSITION;
    }
}

static void draw_background(void)
{
    window_city_draw_panels();
    window_city_draw();
    context.risk_icons.active = 0;
    if (context.type == BUILDING_INFO_NONE) {
        window_building_draw_no_people(&context);
    } else if (context.type == BUILDING_INFO_TERRAIN) {
        window_building_draw_terrain(&context);
    } else if (context.type == BUILDING_INFO_BUILDING) {
        const Building current_building = context.building;
        building *b = current_building.id() ? building_get(current_building.id()) : nullptr;
        if (!b || !current_building.type) {
            return;
        }
        const BuildingType &type_definition = *current_building.type;
        building_type btype = static_cast<building_type>(b->type);
        const int is_dock = std::strcmp(type_definition.attr(), "dock") == 0;
        if (building_is_house(btype)) {
            window_building_draw_house(&context);
        } else if (type_definition.is_farm()) {
            window_building_draw_farm(&context, building_output_resource(btype));
        } else if (building_type_attr_is(btype, "marble_quarry")) {
            window_building_draw_marble_quarry(&context);
        } else if (building_type_attr_is(btype, "iron_mine")) {
            window_building_draw_iron_mine(&context);
        } else if (building_type_attr_is(btype, "timber_yard")) {
            window_building_draw_timber_yard(&context);
        } else if (building_type_attr_is(btype, "clay_pit")) {
            window_building_draw_clay_pit(&context);
        } else if (building_type_attr_is(btype, "gold_mine")) {
            window_building_draw_gold_mine(&context);
        } else if (building_type_attr_is(btype, "stone_quarry")) {
            window_building_draw_stone_quarry(&context);
        } else if (building_type_attr_is(btype, "sand_pit")) {
            window_building_draw_sand_pit(&context);
        } else if (building_type_attr_is(btype, "wine_workshop")) {
            window_building_draw_wine_workshop(&context);
        } else if (building_type_attr_is(btype, "oil_workshop")) {
            window_building_draw_oil_workshop(&context);
        } else if (building_type_attr_is(btype, "weapons_workshop")) {
            window_building_draw_weapons_workshop(&context);
        } else if (building_type_attr_is(btype, "furniture_workshop")) {
            window_building_draw_furniture_workshop(&context);
        } else if (building_type_attr_is(btype, "pottery_workshop")) {
            window_building_draw_pottery_workshop(&context);
        } else if (building_type_attr_is(btype, "brickworks")) {
            window_building_draw_brickworks(&context);
        } else if (building_type_attr_is(btype, "concrete_maker")) {
            window_building_draw_concrete_maker(&context);
        } else if (building_type_attr_is(btype, "city_mint")) {
            window_building_draw_city_mint(&context);
        } else if (building_type_attr_is(btype, "market")) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_MARKET_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_market(&context);
            }
        } else if (type_definition.is_mess_hall()) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_MESS_HALL_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_mess_hall(&context);
            }
        } else if (type_is_storage(type_definition)) {
            if (context.show_special_orders == SPECIAL_ORDERS_ROADBLOCK) {
                window_building_draw_roadblock_orders(&context);
            } else if (context.show_special_orders == SPECIAL_ORDERS_STORAGE ||
                     context.show_special_orders == SPECIAL_ORDERS_GENERIC) {
                window_building_draw_storage_orders(&context);
            } else {
                window_building_draw_storage(&context);
            }
        } else if (building_type_attr_is(btype, "depot")) {
            if (context.depot_selection == 2) {
                window_building_draw_depot_order_source_destination_background(&context, 0);
            } else if (context.depot_selection == 3) {
                window_building_draw_depot_order_source_destination_background(&context, 1);
            } else if (context.depot_selection == 1) {
                window_building_draw_depot_select_resource(&context);
            } else {
                window_building_draw_depot(&context);
            }
        } else if (building_type_attr_is(btype, "amphitheater")) {
            window_building_draw_amphitheater(&context);
        } else if (type_definition.is_theater()) {
            window_building_draw_theater(&context);
        } else if (building_type_attr_is(btype, "hippodrome")) {
            window_building_draw_hippodrome_background(&context);
        } else if (building_type_attr_is(btype, "colosseum")) {
            window_building_draw_colosseum_background(&context);
        } else if (building_type_attr_is(btype, "arena")) {
            window_building_draw_arena(&context);
        } else if (building_type_attr_is(btype, "gladiator_school")) {
            window_building_draw_gladiator_school(&context);
        } else if (building_type_attr_is(btype, "lion_house")) {
            window_building_draw_lion_house(&context);
        } else if (building_type_attr_is(btype, "actor_colony")) {
            window_building_draw_actor_colony(&context);
        } else if (building_type_attr_is(btype, "chariot_maker")) {
            window_building_draw_chariot_maker(&context);
        } else if (building_type_attr_is(btype, "doctor")) {
            window_building_draw_clinic(&context);
        } else if (building_type_attr_is(btype, "hospital")) {
            window_building_draw_hospital(&context);
        } else if (building_type_attr_is(btype, "bathhouse")) {
            window_building_draw_bathhouse(&context);
        } else if (building_type_attr_is(btype, "barber")) {
            window_building_draw_barber(&context);
        } else if (building_type_attr_is(btype, "school")) {
            window_building_draw_school(&context);
        } else if (building_type_attr_is(btype, "academy")) {
            window_building_draw_academy(&context);
        } else if (building_type_attr_is(btype, "library")) {
            window_building_draw_library(&context);
        } else if (building_type_attr_is_any(btype, {"small_temple_ceres", "large_temple_ceres"})) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_TEMPLE_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_temple_ceres(&context);
            }
        } else if (building_type_attr_is_any(btype, {"small_temple_neptune", "large_temple_neptune"})) {
            window_building_draw_temple_neptune(&context);
        } else if (building_type_attr_is_any(btype, {"small_temple_mercury", "large_temple_mercury"})) {
            window_building_draw_temple_mercury(&context);
        } else if (building_type_attr_is_any(btype, {"small_temple_mars", "large_temple_mars"})) {
            window_building_draw_temple_mars(&context);
        } else if (building_type_attr_is_any(btype, {"small_temple_venus", "large_temple_venus"})) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_TEMPLE_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_temple_venus(&context);
            }
        } else if (building_type_attr_is(btype, "oracle")) {
            window_building_draw_oracle(&context);
        } else if (building_type_attr_is(btype, "lararium")) {
            window_building_draw_lararium(&context);
        } else if (building_type_attr_is(btype, "nymphaeum")) {
            window_building_draw_nymphaeum(&context);
        } else if (building_type_attr_is(btype, "small_mausoleum")) {
            window_building_draw_small_mausoleum(&context);
        } else if (building_type_attr_is(btype, "large_mausoleum")) {
            window_building_draw_large_mausoleum(&context);
        } else if (building_type_attr_is(btype, "workcamp")) {
            window_building_draw_work_camp(&context);
        } else if (type_definition.is_architect_guild()) {
            window_building_draw_architect_guild(&context);
        } else if (building_type_attr_is(btype, "tavern")) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_TAVERN_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_tavern(&context);
            }
        } else if (type_definition.is_temple_tier(building_type_registry_impl::ReligionTier::Grand)) {
            Temple temple(b, &type_definition);
            window_building_draw_grand_temple(&context, temple);
        } else if (type_definition.is_lighthouse()) {
            window_building_draw_lighthouse(&context);
        } else if (building_type_attr_is_any(btype, {"governors_house", "governors_villa", "governors_palace"})) {
            window_building_draw_governor_home(&context);
        } else if (building_type_attr_is_any(btype, {"forum", "forum_2_unused"})) {
            window_building_draw_forum(&context);
        } else if (building_type_attr_is_any(btype, {"senate_1_unused", "senate"})) {
            window_building_draw_senate(&context);
        } else if (building_type_attr_is(btype, "engineers_post")) {
            window_building_draw_engineers_post(&context);
        } else if (building_type_attr_is(btype, "shipyard")) {
            window_building_draw_shipyard(&context);
        } else if (is_dock) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context, translation_for_key("TR_DOCK_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_dock(&context);
            }
        } else if (building_type_attr_is(btype, "wharf")) {
            window_building_draw_wharf(&context);
        } else if (building_type_attr_is(btype, "reservoir")) {
            window_building_draw_reservoir(&context);
        } else if (building_type_attr_is(btype, "fountain")) {
            window_building_draw_fountain(&context);
        } else if (type_definition.is_well()) {
            window_building_draw_well(&context);
        } else if (building_type_attr_is_any(btype, {
            "small_statue", "medium_statue", "goddess_statue", "senator_statue",
            "legion_statue", "decorative_column", "horse_statue", "gladiator_statue"
        })) {
            window_building_draw_statue(&context);
        } else if (building_type_attr_is(btype, "large_statue")) {
            window_building_draw_large_statue(&context);
        } else if (building_type_attr_is_any(btype, {"small_pond", "large_pond"})) {
            window_building_draw_pond(&context);
        } else if (building_type_attr_is_any(btype, {
            "pine_tree", "fir_tree", "oak_tree", "elm_tree", "fig_tree", "plum_tree", "palm_tree", "date_tree",
            "pine_path", "fir_path", "oak_path", "elm_path", "fig_path", "plum_path", "palm_path", "date_path",
            "pavilion", "hedge_dark", "hedge_light", "colonnade", "garden_path", "looped_garden_wall",
            "roofed_garden_wall", "panelled_garden_wall"
        })) {
            window_building_draw_garden(&context);
        } else if (building_type_attr_is(btype, "prefecture")) {
            window_building_draw_prefect(&context);
        } else if (building_type_attr_is(btype, "obelisk")) {
            window_building_draw_obelisk(&context);
        } else if (Roadblock(b).kind() != ROADBLOCK_NONE && context.show_special_orders) {
            window_building_draw_roadblock_orders(&context);
        } else if (building_type_attr_is(btype, "roadblock")) {
            window_building_draw_roadblock(&context);
        } else if (building_type_attr_is(btype, "triumphal_arch")) {
            window_building_draw_triumphal_arch(&context);
        } else if (building_type_attr_is(btype, "gatehouse")) {
            window_building_draw_gatehouse(&context);
        } else if (building_type_attr_is(btype, "tower")) {
            window_building_draw_tower(&context);
        } else if (building_type_attr_is(btype, "military_academy")) {
            window_building_draw_military_academy(&context);
        } else if (building_type_attr_is(btype, "barracks")) {
            window_building_draw_barracks(&context);
        } else if (building_is_fort(btype)) {
            window_building_draw_fort(&context);
        } else if (building_type_attr_is(btype, "burning_ruin")) {
            window_building_draw_burning_ruin(&context);
        } else if (building_type_attr_is_any(btype, {"native_hut", "native_hut_alt"})) {
            window_building_draw_native_hut(&context);
        } else if (building_type_attr_is(btype, "native_meeting")) {
            window_building_draw_native_meeting(&context);
        } else if (building_type_attr_is(btype, "native_crops")) {
            window_building_draw_native_crops(&context);
        } else if (building_type_attr_is(btype, "native_decor")) {
            window_building_draw_native_decoration(&context);
        } else if (building_type_attr_is(btype, "native_monument")) {
            window_building_draw_native_monument(&context);
        } else if (building_type_attr_is(btype, "native_watchtower")) {
            window_building_draw_native_watchtower(&context);
        } else if (building_type_attr_is(btype, "mission_post")) {
            window_building_draw_mission_post(&context);
        } else if (building_type_attr_is(btype, "watchtower")) {
            window_building_draw_watchtower(&context);
        } else if (type_definition.is_caravanserai()) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders(&context,
                    translation_for_key("TR_CARAVANSERAI_SPECIAL_ORDERS_HEADER"));
            } else {
                window_building_draw_caravanserai(&context);
            }
        } else if (building_type_attr_is_any(btype, {
            "roofed_garden_wall_gate", "hedge_gate_dark", "hedge_gate_light",
            "looped_garden_gate", "panelled_garden_gate"
        })) {
            window_building_draw_garden_gate(&context);
        } else if (building_type_attr_is(btype, "palisade")) {
            window_building_draw_palisade(&context);
        } else if (building_type_attr_is(btype, "palisade_gate")) {
            window_building_draw_palisade_gate(&context);
        } else if (building_type_attr_is(btype, "shrine_ceres")) {
            window_building_draw_shrine_ceres(&context);
        } else if (building_type_attr_is(btype, "shrine_neptune")) {
            window_building_draw_shrine_neptune(&context);
        } else if (building_type_attr_is(btype, "shrine_mercury")) {
            window_building_draw_shrine_mercury(&context);
        } else if (building_type_attr_is(btype, "shrine_mars")) {
            window_building_draw_shrine_mars(&context);
        } else if (building_type_attr_is(btype, "shrine_venus")) {
            window_building_draw_shrine_venus(&context);
        } else if (type_definition.is_armoury()) {
            window_building_draw_armoury(&context);
        } else if (building_type_attr_is(btype, "latrines")) {
            window_building_draw_latrines(&context);
        }
    } else if (context.type == BUILDING_INFO_LEGION) {
        window_building_draw_legion_info(&context);
    }
}

static void draw_foreground(void)
{
    const Building current_building = context.building;
    building *b = current_building.id() ? building_get(current_building.id()) : nullptr;
    // building-specific buttons
    init_context_buttons(&context);
    if (context.type == BUILDING_INFO_BUILDING && b && current_building.type) {
        const BuildingType &type_definition = *current_building.type;
        building_type btype = static_cast<building_type>(b->type);
        const int is_dock = std::strcmp(type_definition.attr(), "dock") == 0;

        if (building_is_primary_product_producer(btype)) {
            window_building_draw_primary_product_stockpiling(&context);
        }

        if (type_definition.is_lighthouse() && b->monument.phase == MONUMENT_FINISHED) {
            window_building_draw_lighthouse_foreground(&context);
        } else if (type_is_storage(type_definition)) {
            if (context.show_special_orders == SPECIAL_ORDERS_ROADBLOCK) {
                window_building_draw_roadblock_orders_foreground(&context);
            } else if (context.show_special_orders == SPECIAL_ORDERS_STORAGE ||
                     context.show_special_orders == SPECIAL_ORDERS_GENERIC) {
                window_building_draw_storage_orders_foreground(&context);
            } else {
                window_building_draw_storage_foreground(&context);
            }
        } else if (building_type_attr_is(btype, "depot")) {
            if (context.depot_selection == 2) {
                window_building_draw_depot_select_source_destination(&context);
            } else if (context.depot_selection == 3) {
                window_building_draw_depot_select_source_destination(&context);
            } else if (context.depot_selection == 1) {
                window_building_draw_depot_select_resource_foreground(&context);
            } else {
                window_building_draw_depot_foreground(&context);
            }
        } else if (building_type_attr_is(btype, "market")) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
            }
        } else if (building_type_attr_is(btype, "tavern")) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
            }
        } else if (type_definition.is_mess_hall()) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
            }
        } else if (building_is_venus_temple(btype) && b->monument.phase <= 0 &&
            building_monument_gt_module_is_active(VENUS_MODULE_1_DISTRIBUTE_WINE)) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
            }
        } else if (building_is_ceres_temple(btype) && b->monument.phase <= 0 &&
            building_monument_gt_module_is_active(CERES_MODULE_2_DISTRIBUTE_FOOD)) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
            }
        } else if (Roadblock(b).kind() != ROADBLOCK_NONE) {
            if (context.show_special_orders) {
                window_building_draw_roadblock_orders_foreground(&context);
            } else {
                window_building_draw_roadblock_button(&context);
            }
        } else if (is_dock) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_draw_dock_foreground(&context);
            }
        } else if (building_type_attr_is(btype, "barracks")) {
            window_building_draw_barracks_foreground(&context);
        } else if (type_definition.is_temple_tier(building_type_registry_impl::ReligionTier::Grand)) {
            Temple temple(b, &type_definition);
            window_building_draw_grand_temple_foreground(&context, temple);
        } else if (type_definition.is_caravanserai() &&
            b->monument.phase == MONUMENT_FINISHED) {
            if (context.show_special_orders) {
                window_building_draw_distributor_orders_foreground(&context);
            } else {
                window_building_distributor_draw_foreground(&context);
                window_building_draw_caravanserai_foreground(&context);
            }
        } else if (building_type_attr_is(btype, "colosseum")) {
            window_building_draw_colosseum_foreground(&context);
        } else if (building_type_attr_is(btype, "hippodrome")) {
            window_building_draw_hippodrome_foreground(&context);
        } else if (building_type_attr_is(btype, "city_mint")) {
            window_building_draw_city_mint_foreground(&context);
        }

        if (building_monument_is_unfinished_monument(b)) {
            draw_halt_monument_construction_button(context.x_offset + 80,
                context.y_offset + 3 + BLOCK_SIZE * context.height_blocks - 40,
                focus_monument_construction_button_id, b);
        }
    } else if (context.type == BUILDING_INFO_LEGION) {
        window_building_draw_legion_info_foreground(&context);
    } else if (context.terrain_type == TERRAIN_INFO_BRIDGE) {
        if (context.show_special_orders) {
            window_building_draw_roadblock_orders_foreground(&context);
        } else {
            window_building_draw_roadblock_button(&context);
        }
    }

    // general buttons
    if (context.show_special_orders ||
        context.depot_selection != 0) {
        int new_context_y = window_building_get_vertical_offset(&context, 28);
        update_context_buttons_vertical_offset(new_context_y, 28);
        image_buttons_draw(0, 0, image_buttons_help_advisor_close, 2);
    } else {
        if (context.advisor_button) {
            update_advisor_button_image(context.advisor_button);
        }

        image_buttons_draw(0, 0, image_buttons_help_advisor_close, context.advisor_button ? 3 : 2);
    }

    if (!context.show_special_orders &&
        context.type == BUILDING_INFO_BUILDING &&
        context.depot_selection == 0 &&
        b &&
        !building_monument_is_unfinished_monument(b) &&
        has_mothball_button(context.building)) {
        draw_mothball_button();
    }
}

static int handle_specific_building_info_mouse(const mouse *m)
{
    // building-specific buttons
    if (context.type == BUILDING_INFO_NONE) {
        return 0;
    }

    if (context.type == BUILDING_INFO_LEGION) {
        return window_building_handle_mouse_legion_info(m, &context);
    } else if (context.figure.drawn) {
        return window_building_handle_mouse_figure_list(m, &context);
    } else if (context.type == BUILDING_INFO_BUILDING) {
        const Building current_building = context.building;
        building *b = current_building.id() ? building_get(current_building.id()) : nullptr;
        if (!b || !current_building.type) {
            return 0;
        }
        const BuildingType &type_definition = *current_building.type;
        building_type btype = static_cast<building_type>(b->type);
        const int is_dock = std::strcmp(type_definition.attr(), "dock") == 0;

        if (building_has_supplier_inventory(btype)) {
            if (context.show_special_orders) {
                return window_building_handle_mouse_distributor_orders(m, &context);
            } else {
                if (type_definition.is_caravanserai()) {
                    if (window_building_handle_mouse_caravanserai(m, &context)) {
                        return 1;
                    }
                }
                return window_building_handle_mouse_distributor(m, &context);
            }
        } else if (Roadblock(b).kind() == ROADBLOCK_STANDARD) {
            if (context.show_special_orders) {
                return window_building_handle_mouse_roadblock_orders(m, &context);
            } else {
                return window_building_handle_mouse_roadblock_button(m, &context);
            }
        } else if (is_dock) {
            if (context.show_special_orders) {
                return window_building_handle_mouse_distributor_orders(m, &context);
            } else {
                return window_building_handle_mouse_dock(m, &context);
            }
        } else if (building_type_attr_is(btype, "barracks")) {
            return window_building_handle_mouse_barracks(m, &context);
        } else if (type_definition.is_temple_tier(building_type_registry_impl::ReligionTier::Grand)) {
            Temple temple(b, &type_definition);
            if (type_definition.is_grand_temple_mars()) {
                return window_building_handle_mouse_grand_temple_mars(m, &context, temple);
            }
            return window_building_handle_mouse_grand_temple(m, &context, temple);
        } else if (type_is_storage(type_definition)) {
            if (context.show_special_orders == SPECIAL_ORDERS_GENERIC ||
                context.show_special_orders == SPECIAL_ORDERS_STORAGE) {
                return window_building_handle_mouse_storage_orders(m, &context);
            } else if (context.show_special_orders == SPECIAL_ORDERS_ROADBLOCK) {
                return window_building_handle_mouse_roadblock_orders(m, &context);
            } else {
                return window_building_handle_mouse_storage(m, &context);
            }
        } else if (building_type_attr_is(btype, "depot")) {
            if (context.depot_selection == 2) {
                window_building_handle_mouse_depot_select_source(m, &context);
            } else if (context.depot_selection == 3) {
                window_building_handle_mouse_depot_select_destination(m, &context);
            } else if (context.depot_selection == 1) {
                window_building_handle_mouse_depot_select_resource(m, &context);
            } else {
                return window_building_handle_mouse_depot(m, &context);
            }
        } else if (type_definition.is_lighthouse()) {
            return window_building_handle_mouse_lighthouse(m, &context);
        } else if (building_type_attr_is(btype, "colosseum")) {
            return window_building_handle_mouse_colosseum(m, &context);
        } else if (building_type_attr_is(btype, "hippodrome")) {
            return window_building_handle_mouse_hippodrome(m, &context);
        } else if (building_type_attr_is(btype, "city_mint")) {
            return window_building_handle_mouse_city_mint(m, &context);
        } else if (building_is_primary_product_producer(btype)) {
            return window_building_handle_mouse_primary_product_producer(m, &context);
        }
    }
    return 0;
}

static void handle_input(const mouse *m, const hotkeys *h)
{
    int handled = 0;
    // general buttons
    if (context.terrain_type == TERRAIN_INFO_RUBBLE && context.rubble_building_id) {
        handled |= window_building_handle_rubble_button(m, &context);
    }
    if (context.show_special_orders ||
        context.depot_selection != 0) {
        handled |= image_buttons_handle_mouse(m, 0, 0,
            image_buttons_help_advisor_close, 2, &focus_image_button_id);
    } else {
        handled |= image_buttons_handle_mouse(
            m, 0, 0, image_buttons_help_advisor_close, context.advisor_button ? 3 : 2, &focus_image_button_id);
        building *b = (context.type == BUILDING_INFO_BUILDING && context.building.id()) ?
            building_get(context.building.id()) : nullptr;
        if (b && building_monument_is_unfinished_monument(b)) {
            handled = GenericButtonList(generic_button_monument_construction, 1).handle_mouse(
                *m,
                context.x_offset,
                context.y_offset + BLOCK_SIZE * context.height_blocks - 40,
                &focus_monument_construction_button_id
            );
        } else if (b) {
            if (has_mothball_button(context.building)) {
                if (b->state == BUILDING_STATE_MOTHBALLED) {
                    handled |= image_buttons_handle_mouse(m, 0, 0,
                        image_button_mothball, 2, &focus_mothball_image_button_id);
                } else {
                    handled |= image_buttons_handle_mouse(m, 0, 0,
                         image_button_mothball, 1, &focus_mothball_image_button_id);
                }
            }
        }
    }

    if (!handled) {
        handled |= handle_specific_building_info_mouse(m);
    }
    if (!handled && input_go_back_requested(m, h)) {
        if (previous_context.type != BUILDING_INFO_NONE) {
            window_building_info_restore_previous_context();
            return;
        }
        game_state_set_overlay(original_overlay);
        window_city_show();
    }
}

static void get_tooltip(tooltip_context *c)
{
    int text_id = 0, group_id = 0;
    translation_key translation;
    const uint8_t *precomposed_text = 0;
    if (focus_image_button_id) {
        if (focus_image_button_id == 3) {
            int advisor = image_buttons_help_advisor_close[2].parameter1;
            if (advisor == ADVISOR_HOUSING) {
                translation = "TR_TOOLTIP_ADVISOR_POPULATION_HOUSING_BUTTON";
            } else {
                text_id = 69 + advisor - (advisor >= ADVISOR_HOUSING ? 1 : 0);
            }
        } else {
            text_id = focus_image_button_id;
        }
    } else if (context.type == BUILDING_INFO_LEGION) {
        text_id = window_building_get_legion_info_tooltip_text(&context);
    }

    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (!text_id && !group_id && !translation && !precomposed_text &&
        context.type == BUILDING_INFO_BUILDING && b &&
        focus_mothball_image_button_id && has_mothball_button(context.building)) {
        if (!building_monument_is_unfinished_monument(b)) {
            if (b->state == BUILDING_STATE_MOTHBALLED) {
                translation = "TR_TOOLTIP_BUTTON_MOTHBALL_OFF";
            } else {
                translation = "TR_TOOLTIP_BUTTON_MOTHBALL_ON";
            }
        }
    }

    const int has_building_tooltip_context =
        context.type == BUILDING_INFO_BUILDING || (context.terrain_type == TERRAIN_INFO_BRIDGE && b);
    building_type btype = (has_building_tooltip_context && b) ? static_cast<building_type>(b->type) : BUILDING_NONE;
    const Building current_building = context.building;
    const BuildingType *type_definition = has_building_tooltip_context ? current_building.type : nullptr;
    const int is_dock = type_definition && std::strcmp(type_definition->attr(), "dock") == 0;

    if (!text_id && !group_id && !translation && !precomposed_text && type_definition) {
        if (building_is_primary_product_producer(btype)) {
            window_building_primary_product_producer_stockpiling_tooltip(&translation);
        } else if ((context.type == BUILDING_INFO_BUILDING && context.show_special_orders) || building_type_is_bridge(btype)) {
            //bridges are technically terrain, but they have special orders
            if (type_is_storage(*type_definition)) {
                if (context.show_special_orders == SPECIAL_ORDERS_ROADBLOCK) {
                    window_building_roadblock_get_tooltip_walker_permissions(&translation);
                } else {
                    window_building_get_tooltip_storage_orders(&group_id, &text_id, &translation);
                }
            } else if (Roadblock(b).kind() != ROADBLOCK_NONE) {
                window_building_roadblock_get_tooltip_walker_permissions(&translation);
            } else if (type_definition->has_distribution()) {
                window_building_get_tooltip_distribution_orders(&group_id, &text_id, &translation);
            }
        } else if (building_is_house(btype)) {
            precomposed_text = window_building_house_get_tooltip(&context);
        } else if (type_is_storage(*type_definition)) {
            window_building_storage_get_tooltip_distribution_permissions(&translation);
        } else if (is_dock) {
            precomposed_text = window_building_dock_get_tooltip(&context);
        } else if (context.type == BUILDING_INFO_BUILDING && building_type_attr_is(btype, "depot")) {
            if (context.depot_selection == 2 || context.depot_selection == 3) {
                precomposed_text = window_building_depot_get_tooltip_source_destination(&translation, &group_id);
            } else if (context.depot_selection == 0) {
                window_building_depot_get_tooltip_main(&translation);
            }
        } else if (building_type_attr_is(btype, "barracks") || building_type_attr_is(btype, "grand_temple_mars")) {
            window_building_barracks_get_tooltip_priority(&translation);
        }
    }

    if (!text_id && !group_id && !translation && !precomposed_text && has_building_tooltip_context) {
        if (building_is_farm(btype) || building_is_raw_resource_producer(btype) || building_is_workshop(btype)) {
            window_building_industry_get_tooltip(&context, &translation);
        }
        if (!translation) {
            window_building_get_risks_tooltip(&context, &group_id, &text_id, &translation);
        }
    }
    if (text_id || group_id || translation || precomposed_text) {
        c->type = TOOLTIP_BUTTON;
        c->text_id = text_id;
        c->translation_key = translation;
        if (group_id) {
            c->text_group = group_id;
        }
        c->precomposed_text = precomposed_text;
    }
}

static void button_help(int param1, int param2)
{
    if (context.help_id > 0) {
        window_message_dialog_show(context.help_id, window_city_draw_all);
    } else {
        window_message_dialog_show(MESSAGE_DIALOG_HELP, window_city_draw_all);
    }
    window_invalidate();
}

static void button_close(int param1, int param2)
{
    if (context.depot_selection != 0) {
        window_building_info_depot_return_to_main_window();
        return;
    }
    if (context.show_special_orders) {
        if (previous_context.type != BUILDING_INFO_NONE) {
            window_building_info_restore_previous_context();
            return;
        }
        context.show_special_orders = 0;
        window_invalidate();
    } else {
        game_state_set_overlay(original_overlay);
        window_city_show();
    }
}

static void button_advisor(int advisor, int param2)
{
    window_advisors_show_advisor(static_cast<advisor_type>(advisor));
}

static void button_mothball(int param1, int param2)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    int workers_needed = context.building.type ? context.building.type->required_workers() : 0;
    if (b && workers_needed) {
        building_mothball_toggle(b);
        window_invalidate();
    }
}

static void button_monument_construction(const generic_button *button)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (b) {
        building_monument_toggle_construction_halted(b);
        window_invalidate();
    }
}

void window_building_info_show(int grid_offset)
{
    window_type window = {
        WINDOW_BUILDING_INFO,
        draw_background,
        draw_foreground,
        handle_input,
        get_tooltip
    };
    init(grid_offset);
    window_show(&window);
}

int window_building_info_get_building_type(void)
{
    if (context.type == BUILDING_INFO_BUILDING) {
        return context.building.type ? context.building.type->type() : BUILDING_NONE;
    }
    return BUILDING_NONE;
}

Building window_building_info_current_building()
{
    return context.type == BUILDING_INFO_BUILDING ? context.building : Building(nullptr);
}

void window_building_info_show_storage_orders(void)
{
    context.show_special_orders = SPECIAL_ORDERS_GENERIC;
    window_invalidate();
}

void window_building_info_show_storage_special_orders(void)
{
    context.show_special_orders = SPECIAL_ORDERS_STORAGE;
    window_invalidate();
}

void window_building_info_show_storage_special_orders_on_top(Building building)
{
    previous_context = context;
    previous_overlay = original_overlay;
    window_building_info_show(building.grid_offset());
    context.show_special_orders = SPECIAL_ORDERS_STORAGE;
    window_invalidate();
}

void window_building_info_restore_previous_context(void)
{
    context = previous_context;
    original_overlay = previous_overlay;
    window_building_info_reset_previous_context();
    window_invalidate();
}

void window_building_info_reset_previous_context(void)
{
    previous_context = {};
    previous_overlay = OVERLAY_NONE;
}

void window_building_info_show_roadblock_orders(void)
{
    context.show_special_orders = SPECIAL_ORDERS_ROADBLOCK;
    window_invalidate();
}

void window_building_info_depot_select_source(void)
{
    window_building_depot_init_storage_selection(&context);
    context.depot_selection = 2;
    window_invalidate();
}

void window_building_info_depot_select_destination(void)
{
    window_building_depot_init_storage_selection(&context);
    context.depot_selection = 3;
    window_invalidate();
}

void window_building_info_depot_select_resource(void)
{
    window_building_depot_init_resource_selection();
    context.depot_selection = 1;
    window_invalidate();
}

void window_building_info_depot_toggle_condition_type(void)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (!b) {
        return;
    }
    b->data.depot.current_order.condition.condition_type = static_cast<order_condition_type>(
        (b->data.depot.current_order.condition.condition_type + 1) % 4);
    window_invalidate();
}

void window_building_info_depot_toggle_condition_threshold(void)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (!b) {
        return;
    }
    int step = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 4 : 8;
    int step_max = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 36 : 40;
    b->data.depot.current_order.condition.threshold = (b->data.depot.current_order.condition.threshold + step) % step_max;
    window_invalidate();
}

void window_building_info_depot_toggle_condition_threshold_reverse(void)
{
    building *b = context.building.id() ? building_get(context.building.id()) : nullptr;
    if (!b) {
        return;
    }
    int step = config_get(CONFIG_GP_STORAGE_INCREMENT_4) ? 4 : 8;
    int new_threshold = (b->data.depot.current_order.condition.threshold - step);
    new_threshold = new_threshold < 0 ? 32 : new_threshold;
    b->data.depot.current_order.condition.threshold = new_threshold;
    window_invalidate();
}

void window_building_info_depot_return_to_main_window(void)
{
    context.depot_selection = 0;
    window_invalidate();
}
