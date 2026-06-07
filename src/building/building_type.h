#pragma once

#include <stdint.h>

typedef uint16_t building_type;

enum {
    BUILDING_NONE = 0,
    BUILDING_DYNAMIC_TYPE_FIRST = 212,
    BUILDING_TYPE_MAX = 512
};

/**
 * House levels
 */
typedef enum {
    HOUSE_MIN = 0,
    HOUSE_SMALL_TENT = 0,
    HOUSE_LARGE_TENT = 1,
    HOUSE_SMALL_SHACK = 2,
    HOUSE_LARGE_SHACK = 3,
    HOUSE_SMALL_HOVEL = 4,
    HOUSE_LARGE_HOVEL = 5,
    HOUSE_SMALL_CASA = 6,
    HOUSE_LARGE_CASA = 7,
    HOUSE_SMALL_INSULA = 8,
    HOUSE_MEDIUM_INSULA = 9,
    HOUSE_LARGE_INSULA = 10,
    HOUSE_GRAND_INSULA = 11,
    HOUSE_SMALL_VILLA = 12,
    HOUSE_MEDIUM_VILLA = 13,
    HOUSE_LARGE_VILLA = 14,
    HOUSE_GRAND_VILLA = 15,
    HOUSE_SMALL_PALACE = 16,
    HOUSE_MEDIUM_PALACE = 17,
    HOUSE_LARGE_PALACE = 18,
    HOUSE_LUXURY_PALACE = 19,
    HOUSE_MAX = 19,
} house_level;

typedef enum {
    HOUSE_GROUP_ALL = 0,
    HOUSE_GROUP_TENT = 10000,
    HOUSE_GROUP_SHACK = 20000,
    HOUSE_GROUP_HOVEL = 30000,
    HOUSE_GROUP_CASA = 40000,
    HOUSE_GROUP_INSULA = 50000,
    HOUSE_GROUP_VILLA = 60000,
    HOUSE_GROUP_PALACE = 70000
} house_groups;

enum {
    BUILDING_STATE_UNUSED = 0,
    BUILDING_STATE_IN_USE = 1,
    BUILDING_STATE_UNDO = 2,
    BUILDING_STATE_CREATED = 3,
    BUILDING_STATE_RUBBLE = 4,
    BUILDING_STATE_DELETED_BY_GAME = 5, // used for earthquakes, fires, house mergers
    BUILDING_STATE_DELETED_BY_PLAYER = 6,
    BUILDING_STATE_MOTHBALLED = 7
};

#ifdef __cplusplus
extern "C++" {


#include "building/animations.h"
#include "building/water_access_type.h"
#include "graphics/image.h"

extern "C" {
#include "city/constants.h"
#include "core/direction.h"
#include "figure/type.h"
#include "game/resource.h"
#include "game/mod_manager.h"
#include "map/point.h"
}

#include <string>
#include <cstdint>
#include <vector>

namespace building_type_registry_impl {

class ProductionMethod;
class HousingType;
class StorageType;
class Distribution;
class Religion;
enum class ReligionTier;

enum class WaterAccessNodeKind {
    None,
    AqueductConnection
};

enum class WaterAccessOrigin {
    Footprint,
    Nodes
};

enum class WaterAccessRequirementMode {
    Any,
    All
};

enum class WaterAccessRequirementWhere {
    Footprint,
    Nodes
};

enum class WaterAccessRequirementTermKind {
    Access,
    WaterSourceAny,
    WaterSourceFreshOnly
};

enum class SpawnMode {
    None,
    ServiceRoamer,
    TempleSupplier,
    TempleDestinationPriest,
    TempleMarsMessHallPriest,
    TempleNeptuneChariot,
    GrandTempleMarsRecruit
};

enum class RoadAccessMode {
    None,
    Normal
};

enum class RoadblockKind {
    None,
    Standard,
    Storage,
    Bridge
};

enum class TileKind {
    None,
    Plaza
};

enum class LaborSeekerMethod {
    None,
    HousesSpawnIfBelow,
    HousesGenerateIfBelow,
    Workforce
};

enum class HousingTransitionKind {
    EvolveTo,
    DevolveTo,
    MergeTo,
    SplitTo
};

enum class GraphicTiming {
    None,
    OnSpawnEntry,
    BeforeDelayCheck,
    BeforeSuccessfulSpawn
};

enum class GuardTiming {
    BeforeRoadAccess,
    AfterLaborSeeker
};

enum class SpawnCondition {
    Always,
    Days1Positive,
    Days1NotPositive,
    Days2Positive,
    Days1OrDays2Positive
};

enum class SpawnChanceSource {
    None,
    CityUnemploymentPercent,
    HouseUnemployedWorkers
};

struct ChanceBand {
    int min_value = 0;
    int chance_per_million = 0;
};

enum class ConstructionMode {
    Instant,
    Phased
};

enum FoundationTerrainRequirement {
    FoundationTerrainMeadow = 1 << 0,
    FoundationTerrainRock = 1 << 1,
    FoundationTerrainTree = 1 << 2,
    FoundationTerrainWater = 1 << 3,
    FoundationTerrainWall = 1 << 4,
    FoundationTerrainDistantWater = 1 << 5
};

struct LaborSeekerPolicy {
    LaborSeekerMethod method = LaborSeekerMethod::None;
    int amount = 0;
};

struct DelayBand {
    int min_worker_percentage = 0;
    int delay = 0;
};

struct SpawnPolicy {
    SpawnMode mode = SpawnMode::None;
    GraphicTiming graphic_timing = GraphicTiming::None;
    FigureSlot figure_slot = FigureSlot::Primary;
    figure_type spawn_figure = FIGURE_NONE;
    int action_state = 0;
    int spawn_direction = 0;
    int spawn_count = 1;
    int init_roaming = 0;
    int require_water_access = 0;
    int mark_problem_if_no_water = 0;
    int block_on_success = 0;
    SpawnChanceSource chance_source = SpawnChanceSource::None;
    int chance_per_million = -1;
    int chance_divisor = 0;
    std::vector<ChanceBand> chance_bands;
    SpawnCondition condition = SpawnCondition::Always;
    std::string profile;
};

struct SpawnDelayGroup {
    RoadAccessMode road_access_mode = RoadAccessMode::None;
    std::vector<DelayBand> delay_bands;
    GuardTiming guard_timing = GuardTiming::BeforeRoadAccess;
    std::vector<figure_type> existing_figures;
    std::vector<SpawnPolicy> policies;
};

class IdentityDefinition {
public:
    void set_name_key(std::string key);

    int has_name_key() const;
    const char *name_key() const;

private:
    std::string name_key_;
};

class BuildModelDefinition {
public:
    void set_size(int value);
    void set_cost(int value);
    void set_desirability_value(int value);
    void set_desirability_step(int value);
    void set_desirability_step_size(int value);
    void set_desirability_range(int value);

    int has_size() const;
    int size() const;
    int has_cost() const;
    int cost() const;
    int has_desirability_value() const;
    int desirability_value() const;
    int has_desirability_step() const;
    int desirability_step() const;
    int has_desirability_step_size() const;
    int desirability_step_size() const;
    int has_desirability_range() const;
    int desirability_range() const;
    int has_any() const;

private:
    int has_size_ = 0;
    int size_ = 0;
    int has_cost_ = 0;
    int cost_ = 0;
    int has_desirability_value_ = 0;
    int desirability_value_ = 0;
    int has_desirability_step_ = 0;
    int desirability_step_ = 0;
    int has_desirability_step_size_ = 0;
    int desirability_step_size_ = 0;
    int has_desirability_range_ = 0;
    int desirability_range_ = 0;
};

class FoundationDefinition {
public:
    void set_policy(std::string policy);
    void add_required_terrain(int flags);

    int has_policy() const;
    const char *policy() const;
    int required_terrain() const;

private:
    std::string policy_;
    int required_terrain_ = 0;
};

class BuildButtonDefinition {
public:
    void set_group(std::string group);
    void set_order(int order);
    void set_icon(std::string icon);
    void set_icon_image(std::string image);
    void set_text_key(std::string key);

    int has_group() const;
    const char *group() const;
    int has_order() const;
    int order() const;
    int has_icon() const;
    const char *icon() const;
    int has_icon_image() const;
    const char *icon_image() const;
    ImageGroupEntryRef icon_ref() const;
    int has_text_key() const;
    const char *text_key() const;
    int has_any() const;

private:
    std::string group_;
    int has_order_ = 0;
    int order_ = 0;
    std::string icon_;
    std::string icon_image_;
    std::string text_key_;
};

class RoadblockDefinition {
public:
    void set_kind(RoadblockKind kind);

    RoadblockKind kind() const;
    int has_any() const;

private:
    RoadblockKind kind_ = RoadblockKind::None;
};

class TileDefinition {
public:
    void set_kind(TileKind kind);

    TileKind kind() const;
    int has_any() const;

private:
    TileKind kind_ = TileKind::None;
};

class TempleDefinition {
public:
    void set_religion_reference(std::string path);
    void set_religion(const Religion *religion);

    const std::string &religion_reference_path() const;
    const Religion *religion() const;
    int has_any() const;

private:
    std::string religion_reference_path_;
    const Religion *religion_ = nullptr;
};

class SoundDefinition {
public:
    void set_city_sound(int sound);
    void set_mute_on_enemies(int value);
    void set_always_play(int value);

    int has_city_sound() const;
    int city_sound() const;
    int mute_on_enemies() const;
    int always_play() const;
    int has_any() const;

private:
    int has_city_sound_ = 0;
    int city_sound_ = 0;
    int mute_on_enemies_ = 0;
    int always_play_ = 0;
};

class EventDataDefinition {
public:
    void set_attr(std::string attr);

    int has_attr() const;
    const char *attr() const;
    int has_any() const;

private:
    std::string attr_;
};

class MarketDefinition {
public:
    void set_max_distance(int value);
    void set_max_food_stock(int value);

    int max_distance() const;
    int max_food_stock() const;
    int has_any() const;

private:
    int max_distance_ = 0;
    int max_food_stock_ = 0;
};

class BuildingFlagsDefinition {
public:
    void set_fire_proof(int value);
    void set_draw_desirability_range(int value);
    void set_venus_gt_bonus(int value);

    int has_fire_proof() const;
    int fire_proof() const;
    int has_draw_desirability_range() const;
    int draw_desirability_range() const;
    int has_venus_gt_bonus() const;
    int venus_gt_bonus() const;
    int has_any() const;

private:
    int has_fire_proof_ = 0;
    int fire_proof_ = 0;
    int has_draw_desirability_range_ = 0;
    int draw_desirability_range_ = 0;
    int has_venus_gt_bonus_ = 0;
    int venus_gt_bonus_ = 0;
};

struct WaterAccessNode {
    WaterAccessNodeKind kind = WaterAccessNodeKind::None;
    int x = 0;
    int y = 0;
};

struct WaterAccessProvideRule {
    uint8_t mask = 0;
    int range = 0;
    WaterAccessOrigin origin = WaterAccessOrigin::Footprint;
};

struct WaterAccessRequirementTerm {
    WaterAccessRequirementTermKind kind = WaterAccessRequirementTermKind::Access;
    uint8_t mask = 0;
    WaterAccessRequirementWhere where = WaterAccessRequirementWhere::Footprint;
};

struct WaterAccessRequirementRule {
    WaterAccessRequirementMode mode = WaterAccessRequirementMode::All;
    std::vector<WaterAccessRequirementTerm> terms;
};

class WaterAccessDefinition {
public:
    void add_provide_rule(WaterAccessProvideRule rule);
    void add_requirement_rule(WaterAccessRequirementRule rule);
    void add_node(WaterAccessNode node);
    void add_provider_node(WaterAccessNode node);
    void add_requirement_node(WaterAccessNode node);

    int has_provider() const;
    int has_requirements() const;
    const std::vector<WaterAccessProvideRule> &provide_rules() const;
    const std::vector<WaterAccessRequirementRule> &requirement_rules() const;
    const std::vector<WaterAccessNode> &nodes() const;
    const std::vector<WaterAccessNode> &provider_nodes() const;
    const std::vector<WaterAccessNode> &requirement_nodes() const;

private:
    std::vector<WaterAccessProvideRule> provide_rules_;
    std::vector<WaterAccessRequirementRule> requirement_rules_;
    std::vector<WaterAccessNode> nodes_;
    std::vector<WaterAccessNode> provider_nodes_;
    std::vector<WaterAccessNode> requirement_nodes_;
};

class LaborDefinition {
public:
    void set_employee_count(int count);
    void set_seeker_policy(LaborSeekerPolicy policy);

    int has_employee_count() const;
    int employee_count() const;
    int has_seeker_policy() const;
    const LaborSeekerPolicy &seeker_policy() const;
    int has_any() const;

private:
    int has_employee_count_ = 0;
    int employee_count_ = 0;
    int has_seeker_policy_ = 0;
    LaborSeekerPolicy seeker_policy_;
};

struct ConstructionRequirement {
    resource_type resource = RESOURCE_NONE;
    int amount = 0;
};

struct ConstructionPhase {
    int index = 0;
    GraphicsTarget graphics;
    std::vector<ConstructionRequirement> requirements;
};

class ConstructionDefinition {
public:
    void set_mode(ConstructionMode mode);
    void set_road_update_radius(int radius);
    ConstructionPhase &add_phase(int index);
    ConstructionPhase *last_phase();
    const ConstructionPhase *phase(int index) const;
    void add_instant_requirement(resource_type resource, int amount);
    void add_requirement(resource_type resource, int amount);

    ConstructionMode mode() const;
    int is_phased() const;
    int road_update_radius() const;
    int phase_count() const;
    int instant_requirement_amount(resource_type resource) const;
    int requirement_amount(resource_type resource, int phase) const;
    const std::vector<ConstructionPhase> &phases() const;

private:
    ConstructionMode mode_ = ConstructionMode::Instant;
    int road_update_radius_ = 0;
    std::vector<ConstructionRequirement> instant_requirements_;
    std::vector<ConstructionPhase> phases_;
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr);

    void set_identity_name_key(std::string key);
    void set_model_size(int value);
    void set_model_cost(int value);
    void set_model_desirability_value(int value);
    void set_model_desirability_step(int value);
    void set_model_desirability_step_size(int value);
    void set_model_desirability_range(int value);
    void set_foundation_policy(std::string policy);
    void add_foundation_required_terrain(int flags);
    void set_button_group(std::string group);
    void set_button_order(int order);
    void set_button_icon(std::string icon);
    void set_button_icon_image(std::string image);
    void set_button_text_key(std::string key);
    void add_button(BuildButtonDefinition button);
    void set_roadblock_kind(RoadblockKind kind);
    void set_tile_kind(TileKind kind);
    void set_temple_religion_reference(std::string path);
    void set_sound_id(int sound);
    void set_sound_mute_on_enemies(int value);
    void set_sound_always_play(int value);
    void set_event_attr(std::string attr);
    void set_market_max_distance(int value);
    void set_market_max_food_stock(int value);
    void set_fire_proof(int value);
    void set_draw_desirability_range(int value);
    void set_venus_gt_bonus(int value);
    void add_water_access_provide_rule(WaterAccessProvideRule rule);
    void add_water_access_requirement_rule(WaterAccessRequirementRule rule);
    void add_water_access_node(WaterAccessNode node);
    void add_water_access_provider_node(WaterAccessNode node);
    void add_water_access_requirement_node(WaterAccessNode node);
    void mark_graphics_default_node();
    void clear_graphics();
    GraphicsTarget &default_graphics_target();
    GraphicsVariant &add_graphics_variant();
    GraphicsVariant *last_graphics_variant();
    void add_graphics_variant_condition(GraphicsCondition condition);
    void set_construction_mode(ConstructionMode mode);
    void set_construction_road_update_radius(int radius);
    void clear_construction();
    ConstructionPhase &add_construction_phase(int index);
    ConstructionPhase *last_construction_phase();
    void add_instant_construction_requirement(resource_type resource, int amount);
    void add_construction_requirement(resource_type resource, int amount);

    void add_spawn_policy(SpawnPolicy policy);
    void set_labor_employee_count(int count);
    void set_labor_seeker_policy(LaborSeekerPolicy policy);
    void add_spawn_group(SpawnDelayGroup group);
    SpawnDelayGroup *last_spawn_group();
    void add_storage_reference(std::string path);
    void add_production_method_reference(std::string path);
    void set_distribution_reference(std::string path);
    void set_housing_reference(std::string path);
    void set_housing_capacity(int capacity);
    void set_housing_transition(HousingTransitionKind kind, std::string text_id);
    void set_vacant_lot_fill_reference(std::string text_id);
    void add_storage_type(const StorageType *storage_type);
    void add_production_method(ProductionMethod *production_method);
    void set_distribution(const Distribution *distribution);
    void set_housing_type(const HousingType *housing_type);
    void set_temple_religion(const Religion *religion);
    void set_housing_transition_type(HousingTransitionKind kind, building_type type);
    void set_vacant_lot_fill_type(building_type type);

    building_type type() const;
    const char *attr() const;
    const IdentityDefinition &identity() const;
    const BuildModelDefinition &model() const;
    const FoundationDefinition &foundation() const;
    const BuildButtonDefinition &button() const;
    const std::vector<BuildButtonDefinition> &buttons() const;
    const RoadblockDefinition &roadblock() const;
    const TileDefinition &tile() const;
    const TempleDefinition &temple() const;
    const SoundDefinition &sound() const;
    const EventDataDefinition &event_data() const;
    const MarketDefinition &market() const;
    const BuildingFlagsDefinition &flags() const;
    const WaterAccessDefinition &water_access() const;
    const GraphicsDefinition &graphics() const;
    const ConstructionDefinition &construction() const;
    ImageGroupEntryRef button_icon_ref() const;
    const char *button_text_key() const;
    int required_workers() const;
    int has_data_only_graphics() const;
    int is_temple() const;
    int is_temple(god_type god, ReligionTier tier) const;
    int is_temple_for_god(god_type god) const;
    int is_temple_tier(ReligionTier tier) const;
    int is_ceres_temple() const;
    int is_venus_temple() const;
    int is_mars_temple() const;
    int is_mercury_temple() const;
    int is_neptune_temple() const;
    int is_pantheon() const;
    int is_oracle() const;
    int is_grand_temple_mars() const;
    int is_grand_temple_venus() const;
    int is_warehouse() const;
    int is_granary() const;
    int is_mess_hall() const;
    int is_architect_guild() const;
    int is_caravanserai() const;
    int is_lighthouse() const;
    int is_watchtower() const;
    int is_armoury() const;
    const GraphicsTarget *resolve_graphics_target(const Building &building) const;
    const GraphicsTarget *resolve_construction_graphics_target(int phase) const;
    static const GraphicsTarget *resolve_graphics_target_for_image(const BuildingType *definition, const Building &building);
    int has_identity() const;
    int has_model() const;
    int has_foundation() const;
    int foundation_required_terrain() const;
    int has_button() const;
    int has_roadblock() const;
    int has_tile() const;
    int has_temple() const;
    int has_sound() const;
    int has_event_data() const;
    int has_market() const;
    int has_flags() const;
    int has_water_access_provider() const;
    int has_graphic() const;
    int has_construction() const;
    int has_phased_construction() const;
    int has_labor() const;
    const LaborDefinition &labor() const;
    const std::vector<SpawnDelayGroup> &spawn_groups() const;
    const std::vector<std::string> &storage_reference_paths() const;
    const std::vector<std::string> &production_method_reference_paths() const;
    const std::string &distribution_reference_path() const;
    const std::string &housing_reference_path() const;
    const std::string &vacant_lot_fill_reference() const;
    const std::string &temple_religion_reference_path() const;
    int housing_capacity() const;
    const std::string &housing_transition_reference(HousingTransitionKind kind) const;
    const std::vector<const StorageType *> &storage_types() const;
    const std::vector<ProductionMethod *> &production_methods() const;
    const Distribution *distribution() const;
    const HousingType *housing_type() const;
    building_type housing_transition_type(HousingTransitionKind kind) const;
    building_type vacant_lot_fill_type() const;
    int has_native_storage() const;
    int has_native_production() const;
    int has_distribution() const;
    int has_housing() const;
    int is_vacant_lot() const;
    unsigned char upgrade_level_for(const Building &building) const;

private:
    building_type type_;
    std::string attr_;
    IdentityDefinition identity_;
    BuildModelDefinition model_;
    FoundationDefinition foundation_;
    std::vector<BuildButtonDefinition> buttons_;
    RoadblockDefinition roadblock_;
    TileDefinition tile_;
    TempleDefinition temple_;
    SoundDefinition sound_;
    EventDataDefinition event_data_;
    MarketDefinition market_;
    BuildingFlagsDefinition flags_;
    WaterAccessDefinition water_access_;
    GraphicsDefinition graphics_;
    ConstructionDefinition construction_;
    bool has_construction_ = false;
    LaborDefinition labor_;
    std::vector<SpawnDelayGroup> spawn_groups_;
    std::vector<std::string> storage_reference_paths_;
    std::vector<std::string> production_method_reference_paths_;
    std::string distribution_reference_path_;
    std::string housing_reference_path_;
    int housing_capacity_ = 0;
    std::string housing_evolve_to_;
    std::string housing_devolve_to_;
    std::string housing_merge_to_;
    std::string housing_split_to_;
    std::string vacant_lot_fill_to_;
    std::vector<const StorageType *> storage_types_;
    std::vector<ProductionMethod *> production_methods_;
    const Distribution *distribution_ = nullptr;
    const HousingType *housing_type_ = nullptr;
    building_type housing_evolve_to_type_ = BUILDING_NONE;
    building_type housing_devolve_to_type_ = BUILDING_NONE;
    building_type housing_merge_to_type_ = BUILDING_NONE;
    building_type housing_split_to_type_ = BUILDING_NONE;
    building_type vacant_lot_fill_type_ = BUILDING_NONE;
};

} // namespace building_type_registry_impl

} // extern "C++"

#endif
