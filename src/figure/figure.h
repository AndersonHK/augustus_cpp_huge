#pragma once

#include "building/building.h"
#include "core/buffer.h"
#include "core/direction.h"
#include "figure/action.h"
#include "figure/properties.h"
#include "figure/runtime_profile_identity.h"
#include "figure/type.h"
#include "translation/translation.h"

#include <cstdio>
#include <array>
#include <cstdint>
#include <vector>

constexpr int FIGURE_FACTION_ROAMER_PREVIEW = 2;
class Figure;
struct FigureGraphicDrawRequest;
struct building_info_context;

class FigureRelation {
public:
    Figure &get();
    const Figure &get() const;
    void retarget(Figure &figure);
    void clear();
    unsigned int save_id() const;
    unsigned int debug_known_id() const;

private:
    Figure *figure_ = nullptr;
};

class Figure {
public:
    Figure() = default;
    explicit Figure(unsigned int slot);

    unsigned int id() const;
    void reset(unsigned int slot);

    static Figure *get(unsigned int id);
    static unsigned int count();
    static Figure *create(figure_type type, int x, int y, direction_type dir);
    static void init_scenario();
    static void kill_all();
    static void save_state(buffer *list, buffer *seq);
    static void load_state(buffer *list, buffer *seq, int version);
    static bool resolve_loaded_building_references(int save_version);
    static std::vector<unsigned int> ids_referencing_building(const Building &building);
    static std::vector<unsigned int> ids_directly_referencing_building(const Building &building);

    void remove();
    void release_destination_reservations();
    int retarget_building(Building &from, Building &to);
    bool set_home_building(Building *building);
    bool set_immigrant_building(Building *building);
    bool set_destination_building(Building *building);
    bool set_last_destination_building(Building *building);
    void set_last_destination_figure_id(unsigned int figure_id);
    bool clear_last_destination_building_if_matches(const Building &building);
    void clear_building_references();
    unsigned int home_building_id() const;
    unsigned int immigrant_building_id() const;
    unsigned int destination_building_id() const;
    const char *runtime_profile_id() const;
    bool set_runtime_profile_id(const char *profile_id);
    bool references_building(const Building &building) const;
    int is_dead() const;
    int is_enemy() const;
    int is_melee_enemy() const;
    int is_ranged_enemy() const;
    int is_mounted_enemy() const;
    int is_caesar_enemy() const;
    int is_legion() const;
    int is_herd() const;
    int is_category(figure_category_mask category_mask) const;
    int uses_tall_info_panel() const;
    int has_info_action_button() const;
    void handle_info_action_button();
    void draw_figure_info(building_info_context *c);
    void draw(building_info_context *c);
    int graphic_draw_request(FigureGraphicDrawRequest &request) const;
    static int big_people_image_id(figure_type type);
    static void draw_big_people_image(figure_type type, int x, int y);
    void draw_big_people_image(int draw_x, int draw_y) const;
    static translation_key new_type_translation_key(figure_type type);
    translation_key type_translation_key() const;
    int target_is_alive() const;
    int legacy_corpse_image_id(int base_image_id) const;
    int legacy_frame_image_id(int base_image_id, int frame_offset) const;
    int legacy_static_frame_image_id(int base_image_id, int frame_count) const;
    int legacy_directional_frame_image_id(
        int base_image_id,
        int frame_direction,
        int frame_offset,
        int frame_stride = 8) const;
    int legacy_image_id_for_direction_major_frame(
        int base_image_id,
        int frame_direction,
        int frame_offset,
        int direction_stride) const;
    void select_legacy_corpse_image(int base_image_id);
    void select_legacy_frame_image(int base_image_id, int frame_offset);
    void select_legacy_static_frame_image(int base_image_id, int frame_count);
    void select_legacy_directional_frame_image(
        int base_image_id,
        int frame_direction,
        int frame_offset,
        int frame_stride = 8);
    void select_legacy_default_or_corpse_image(int base_image_id);
    void clear_legacy_image();
    void adjust_legacy_gladiator_attack_image_row();
    void clear_legacy_cart_overlay_image();

    unsigned int image_id;
    unsigned int cart_image_id;
    unsigned char image_offset;
    unsigned char is_enemy_image;

    unsigned char alternative_location_index;
    unsigned char flotsam_visible;
    short next_figure_id_on_same_tile;
    unsigned char type;
    unsigned char resource_id;
    unsigned char use_cross_country;
    unsigned char is_friendly;
    unsigned char state;
    unsigned char faction_id; // 2 = roamer preview, 1 = city, 0 = enemy
    unsigned char action_state_before_attack;
    signed char direction;
    signed char previous_tile_direction;
    signed char attack_direction;
    unsigned char x;
    unsigned char y;
    unsigned char previous_tile_x;
    unsigned char previous_tile_y;
    unsigned char missile_height;
    unsigned char damage;
    short grid_offset;
    unsigned char destination_x;
    unsigned char destination_y;
    short destination_grid_offset; // only used for soldiers
    unsigned char source_x;
    unsigned char source_y;
    union {
        unsigned char soldier;
        signed char enemy;
    } formation_position_x;
    union {
        unsigned char soldier;
        signed char enemy;
    } formation_position_y;
    short disallow_diagonal;
    short wait_ticks;
    unsigned char action_state;
    unsigned char progress_on_tile;
    unsigned int routing_path_id;
    unsigned int routing_path_current_tile;
    unsigned int routing_path_length;
    unsigned char in_building_wait_ticks;
    unsigned char is_on_road;
    short max_roam_length;
    short roam_length;
    unsigned char roam_choose_destination;
    unsigned char roam_random_counter;
    signed char roam_turn_direction;
    signed char roam_ticks_until_next_turn;
    int32_t cross_country_x; // position = FIGURE_CROSS_COUNTRY_TILE_UNITS * x + tile offset
    int32_t cross_country_y; // position = FIGURE_CROSS_COUNTRY_TILE_UNITS * y + tile offset
    int32_t cc_destination_x;
    int32_t cc_destination_y;
    int32_t cc_delta_x;
    int32_t cc_delta_y;
    int32_t cc_delta_xy;
    unsigned char cc_direction; // 1 = x, 2 = y
    unsigned char speed_multiplier;
    Building *building = nullptr;
    Building *immigrant_building = nullptr;
    Building *destination_building = nullptr;
    unsigned int formation_id;
    unsigned char index_in_formation;
    unsigned char formation_at_rest;
    unsigned char migrant_num_people;
    unsigned char is_ghost;
    unsigned char min_max_seen;
    char progress_to_next_tick;
    short leading_figure_id;
    unsigned char attack_image_offset;
    unsigned char wait_ticks_missile;
    signed char x_offset_cart;
    signed char y_offset_cart;
    unsigned char empire_city_id;
    unsigned char trader_amount_bought;
    short name;
    unsigned char terrain_usage;
    unsigned char loads_sold_or_carrying;
    unsigned char is_boat; // 1 for boat, 2 for flotsam
    unsigned char height_adjusted_ticks;
    unsigned char current_height;
    unsigned char target_height;
    unsigned char collecting_item_id; // NOT a resource ID for cartpushers! IS a resource ID for warehousemen or lighthouse supplier
    unsigned char trade_ship_failed_dock_attempts;
    unsigned char phrase_sequence_exact;
    signed char phrase_id;
    unsigned char phrase_sequence_city;
    unsigned char trader_id;
    unsigned char wait_ticks_next_target; // used for retargeting fighting figures and destinations for pushers
    unsigned char dont_draw_elevated;
    FigureRelation target_figure;
    FigureRelation targeted_by_figure;
    unsigned short created_sequence;
    unsigned short target_figure_created_sequence;
    unsigned char figures_on_same_tile_index;
    unsigned char num_attackers;
    FigureRelation attacker1;
    FigureRelation attacker2;
    FigureRelation opponent;
    short last_visited_index; // can only be used if figure goes through initialization process
    int last_destination_id; // legacy save/debug slot until each meaning is split
    unsigned int legacy_visited_dock_mask;
    struct {
        unsigned short tourist_money_spent;
        unsigned short ticks_since_last_visited_id[12];
        unsigned short visited_building_type_ids[12];
        unsigned char tourist_rank;
    } tourist;

private:
    bool set_building_reference(Building *&relation, unsigned int &relation_id, Building *building);
    bool set_indexed_building_id(unsigned int &relation_id, unsigned int building_id);

    unsigned int slot_ = 0;
    unsigned int home_building_id_ = 0;
    unsigned int immigrant_building_id_ = 0;
    unsigned int destination_building_id_ = 0;
    unsigned int last_destination_building_id_ = 0;
    std::array<char, FIGURE_RUNTIME_PROFILE_ID_CAPACITY> runtime_profile_id_ = {};
};

void figure_debug_dump(FILE *file);
