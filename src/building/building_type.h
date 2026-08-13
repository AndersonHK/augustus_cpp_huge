#pragma once

#include <stdint.h>

typedef uint16_t building_type;

enum {
    BUILDING_NONE = 0,
    BUILDING_DYNAMIC_TYPE_FIRST = 212,
    BUILDING_TYPE_MAX = 512
};

enum {
    BUILDING_TYPE_TERRAIN_MEADOW = 1 << 0,
    BUILDING_TYPE_TERRAIN_ROCK = 1 << 1,
    BUILDING_TYPE_TERRAIN_TREE = 1 << 2,
    BUILDING_TYPE_TERRAIN_WATER = 1 << 3,
    BUILDING_TYPE_TERRAIN_WALL = 1 << 4,
    BUILDING_TYPE_TERRAIN_DISTANT_WATER = 1 << 5
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



#include "building/BuildingGraphicsDef.h"
#include "building/RubbleDef.h"
#include "building/religion.h"
#include "building/water_access_type.h"
#include "graphics/image.h"

#include "city/constants.h"
#include "building/FoundationDef.h"
#include "building/HousingDef.h"
#include "building/CompositionDef.h"
#include "core/direction.h"
#include "figure/type.h"
#include "game/resource.h"
#include "map/point.h"

#include <string>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

class FormationType;

namespace building_type_registry_impl {

class BuildingType;
class ProductionMethod;
class CultureModule;
class StorageType;
class Distribution;
class Religion;
enum class CultureModuleCountMode;

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

enum class SpecialSpawnMode {
    None,
    TempleSupplier,
    TempleDestinationPriest,
    TempleMarsMessHallPriest,
    TempleNeptuneChariot,
    GrandTempleMarsRecruit,
    FishingBoat
};

enum class SpawnSource {
    None,
    Self,
    Shipyard
};

enum class RoadAccessMode {
    None,
    Normal
};

enum class BridgeType {
    None,
    Low,
    Ship
};

enum class TileKind {
    None,
    Garden,
    Plaza,
    Roadblock
};

enum class TileRefreshBehavior {
    None,
    Garden,
    Plaza
};

enum class TilePlacementBehavior {
    None,
    Garden,
    Plaza
};

enum class ConstructionConfigFlag {
    None,
    MultipleBarracks
};

enum class ConstructionToolKind {
    None,
    ClearLand,
    ClearTrees,
    RepairLand,
    Road,
    Highway,
    Wall,
    Roadblock,
    Aqueduct,
    DraggableReservoir,
    DraggableBuilding
};

enum class ConstructionDragTerrain {
    Land,
    LandOrRoad
};

enum class ConstructionDragRotation {
    None,
    Path,
    Hedge,
    Variant,
    Pair
};

enum class LaborSeekerMethod {
    None,
    HousesSpawnIfBelow,
    HousesGenerateIfBelow,
    Workforce
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

enum class LaborCategory {
    None = 0,
    IndustryCommerce,
    FoodProduction,
    Engineering,
    Water,
    Prefectures,
    Military,
    Entertainment,
    HealthEducation,
    GovernanceReligion,
    Max
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

struct LaborSeekerPolicy {
    LaborSeekerMethod method = LaborSeekerMethod::None;
    int amount = 0;
};

struct BuildingCultureModule {
    std::string reference_path;
    const CultureModule *module = nullptr;
    int capacity = 0;
    int upgrade_bonus_capacity = 0;
    CultureModuleCountMode count_mode;
};

struct DelayBand {
    int min_worker_percentage = 0;
    int delay = 0;
};

struct SpawnPolicy {
    SpecialSpawnMode special_mode = SpecialSpawnMode::None;
    GraphicTiming graphic_timing = GraphicTiming::None;
    FigureSlot figure_slot = FigureSlot::Primary;
    figure_type spawn_figure = FIGURE_NONE;
    int spawn_direction = 0;
    int spawn_count = 1;
    int require_water_access = 0;
    int mark_problem_if_no_water = 0;
    int block_on_success = 0;
    SpawnSource spawn_source = SpawnSource::None;
    int capacity = 0;
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
    bool add_alias(std::string alias);

    int has_name_key() const;
    const char *name_key() const;
    bool has_alias(std::string_view alias) const;
    const std::vector<std::string> &aliases() const;

private:
    std::string name_key_;
    std::vector<std::string> aliases_;
};

enum class SmartToolModifier {
    Any,
    Control,
    Shift
};

enum class SmartToolTarget {
    DragArea,
    SingleTarget
};

enum class SmartToolContext {
    Default,
    HoverWater,
    RoutedSegment
};

struct SmartToolModeDefinition {
    SmartToolModifier modifier = SmartToolModifier::Any;
    SmartToolTarget target = SmartToolTarget::DragArea;
    SmartToolContext context = SmartToolContext::Default;
    std::string type_reference;
    const BuildingType *type = nullptr;
    int footprint_size = 1;
};

class SmartToolDef {
public:
    void add_mode(SmartToolModeDefinition mode);
    void resolve_mode_type(std::size_t index, const BuildingType *type);

    const std::vector<SmartToolModeDefinition> &modes() const;
    const SmartToolModeDefinition *resolve_mode(
        int control,
        int shift,
        SmartToolContext context = SmartToolContext::Default) const;
    const SmartToolModeDefinition *mode_for_type(
        const BuildingType *type,
        SmartToolContext context = SmartToolContext::Default) const;
    int has_any() const;

private:
    std::vector<SmartToolModeDefinition> modes_;
};

class BuildModelDefinition {
public:
    void set_cost(int value);
    void set_hit_points(int value);
    void set_desirability_value(int value);
    void set_desirability_step(int value);
    void set_desirability_step_size(int value);
    void set_desirability_range(int value);

    int has_cost() const;
    int cost() const;
    int has_hit_points() const;
    int hit_points() const;
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
    int has_cost_ = 0;
    int cost_ = 0;
    int has_hit_points_ = 0;
    int hit_points_ = 0;
    int has_desirability_value_ = 0;
    int desirability_value_ = 0;
    int has_desirability_step_ = 0;
    int desirability_step_ = 0;
    int has_desirability_step_size_ = 0;
    int desirability_step_size_ = 0;
    int has_desirability_range_ = 0;
    int desirability_range_ = 0;
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

class BridgeDefinition {
public:
    void set_type(BridgeType type);

    BridgeType type() const;
    int is_bridge() const;
    int is_ship_bridge() const;

private:
    BridgeType type_ = BridgeType::None;
};

class TileDefinition {
public:
    void set_kind(TileKind kind);
    void set_kind_key(std::string key);
    void set_refresh_behavior(TileRefreshBehavior behavior);
    void set_placement_behavior(TilePlacementBehavior behavior);
    void set_overgrown(int value);

    TileKind kind() const;
    const char *kind_key() const;
    TileRefreshBehavior refresh_behavior() const;
    TilePlacementBehavior placement_behavior() const;
    int is_area_placement() const;
    int updates_land_routing() const;
    int overgrown() const;
    int has_any() const;

private:
    TileKind kind_ = TileKind::None;
    std::string kind_key_;
    TileRefreshBehavior refresh_behavior_ = TileRefreshBehavior::None;
    TilePlacementBehavior placement_behavior_ = TilePlacementBehavior::None;
    int overgrown_ = 0;
};

class ConstructionToolDefinition {
public:
    void set_kind(ConstructionToolKind kind);
    void set_drag_terrain(ConstructionDragTerrain terrain);
    void set_drag_rotation(ConstructionDragRotation rotation);
    void add_smart_mode(SmartToolModeDefinition mode);
    void resolve_smart_mode_type(std::size_t index, const BuildingType *type);

    ConstructionToolKind kind() const;
    ConstructionDragTerrain drag_terrain() const;
    ConstructionDragRotation drag_rotation() const;
    int has_any() const;
    int is_clear_land() const;
    int is_clear_trees() const;
    int is_repair_land() const;
    int is_land_work() const;
    int is_road() const;
    int is_highway() const;
    int is_wall() const;
    int is_roadblock() const;
    int is_aqueduct() const;
    int is_draggable_reservoir() const;
    int is_draggable_building() const;
    int draggable_allows_roads() const;
    const SmartToolDef &smart_tool() const;

private:
    ConstructionToolKind kind_ = ConstructionToolKind::None;
    ConstructionDragTerrain drag_terrain_ = ConstructionDragTerrain::Land;
    ConstructionDragRotation drag_rotation_ = ConstructionDragRotation::None;
    SmartToolDef smart_tool_;
};

class ConstructionCycleDefinition {
public:
    void set_group(std::string group);
    void set_order(int order);
    void set_steps(int steps);

    int has_group() const;
    const char *group() const;
    int order() const;
    int steps() const;
    int has_any() const;

private:
    std::string group_;
    int order_ = 0;
    int steps_ = 1;
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

class MilitaryDefinition {
public:
    void set_formation_reference(std::string key);
    void set_formation_type(const FormationType *formation);

    const std::string &formation_reference() const;
    const FormationType *formation_type() const;
    figure_type primary_figure_type() const;
    int has_any() const;

private:
    std::string formation_reference_;
    const FormationType *formation_type_ = nullptr;
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
    void set_requires_open_water(int required);
    void add_provide_rule(WaterAccessProvideRule rule);
    void add_requirement_rule(WaterAccessRequirementRule rule);
    void add_node(WaterAccessNode node);
    void add_provider_node(WaterAccessNode node);
    void add_requirement_node(WaterAccessNode node);

    int has_provider() const;
    int has_requirements() const;
    int requires_open_water() const;
    const std::vector<WaterAccessProvideRule> &provide_rules() const;
    const std::vector<WaterAccessRequirementRule> &requirement_rules() const;
    const std::vector<WaterAccessNode> &nodes() const;
    const std::vector<WaterAccessNode> &provider_nodes() const;
    const std::vector<WaterAccessNode> &requirement_nodes() const;

private:
    int requires_open_water_ = 0;
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
    void set_free_when_broke_limit(int limit);
    void set_max_count(int limit);
    void set_max_count_unless_config(ConstructionConfigFlag flag);
    void set_required_building_reference(std::string type_attr);
    void set_required_building_type(building_type type);
    ConstructionPhase &add_phase(int index);
    ConstructionPhase *last_phase();
    const ConstructionPhase *phase(int index) const;
    void add_instant_requirement(resource_type resource, int amount);
    void add_requirement(resource_type resource, int amount);

    ConstructionMode mode() const;
    int is_phased() const;
    int road_update_radius() const;
    int free_when_broke_limit() const;
    int max_count() const;
    ConstructionConfigFlag max_count_unless_config() const;
    const std::string &required_building_reference() const;
    building_type required_building_type() const;
    int phase_count() const;
    int instant_requirement_amount(resource_type resource) const;
    int requirement_amount(resource_type resource, int phase) const;
    const std::vector<ConstructionPhase> &phases() const;

private:
    ConstructionMode mode_ = ConstructionMode::Instant;
    int road_update_radius_ = 0;
    int free_when_broke_limit_ = 0;
    int max_count_ = 0;
    ConstructionConfigFlag max_count_unless_config_ = ConstructionConfigFlag::None;
    std::string required_building_reference_;
    building_type required_building_type_ = BUILDING_NONE;
    std::vector<ConstructionRequirement> instant_requirements_;
    std::vector<ConstructionPhase> phases_;
};

class BuildingType {
public:
    BuildingType(building_type type, std::string attr);
    // Startup registries parse identity before assigning the deterministic
    // effective-stack runtime id.
    void assign_runtime_type(building_type type);

    void set_identity_name_key(std::string key);
    bool add_identity_alias(std::string alias);
    void set_model_cost(int value);
    void set_model_hit_points(int value);
    void set_model_desirability_value(int value);
    void set_model_desirability_step(int value);
    void set_model_desirability_step_size(int value);
    void set_model_desirability_range(int value);
    void set_foundation_reference(std::string path);
    void set_foundation_definition(const FoundationDef *foundation);
    int add_foundation_replacement_reference(std::string type);
    void add_foundation_replacement_type(const BuildingType *type);
    void set_button_group(std::string group);
    void set_button_order(int order);
    void set_button_icon(std::string icon);
    void set_button_icon_image(std::string image);
    void set_button_text_key(std::string key);
    void add_button(BuildButtonDefinition button);
    void set_bridge_type(BridgeType type);
    void set_rubble(RubbleType type);
    void set_rubble_burn_days(int days);
    void set_rubble_decay_reference(std::string attr);
    void set_rubble_decay_type(const BuildingType *type);
    void set_tile_kind(TileKind kind);
    void set_tile_kind_key(std::string key);
    void set_tile_refresh_behavior(TileRefreshBehavior behavior);
    void set_tile_placement_behavior(TilePlacementBehavior behavior);
    void set_tile_overgrown(int value);
    void set_tool_kind(ConstructionToolKind kind);
    void set_tool_drag_terrain(ConstructionDragTerrain terrain);
    void set_tool_drag_rotation(ConstructionDragRotation rotation);
    void add_tool_smart_mode(SmartToolModeDefinition mode);
    void resolve_tool_smart_mode_type(std::size_t index, const BuildingType *type);
    void set_cycle_group(std::string group);
    void set_cycle_order(int order);
    void set_cycle_steps(int steps);
    void set_temple_religion_reference(std::string path);
    void set_sound_id(int sound);
    void set_sound_mute_on_enemies(int value);
    void set_sound_always_play(int value);
    void set_market_max_distance(int value);
    void set_market_max_food_stock(int value);
    void set_fire_proof(int value);
    void set_draw_desirability_range(int value);
    void set_venus_gt_bonus(int value);
    void set_military_formation_reference(std::string key);
    void set_military_formation_type(const FormationType *formation);
    void add_water_access_provide_rule(WaterAccessProvideRule rule);
    void add_water_access_requirement_rule(WaterAccessRequirementRule rule);
    void set_water_access_requires_open_water(int required);
    void add_water_access_node(WaterAccessNode node);
    void add_water_access_provider_node(WaterAccessNode node);
    void add_water_access_requirement_node(WaterAccessNode node);
    void set_graphics_status_icon_anchor(int x, int y);
    void set_graphics_overlay_summary_policy(GraphicsOverlaySummaryPolicy policy);
    void mark_graphics_default_node();
    void clear_graphics();
    GraphicsTarget &default_graphics_target();
    GraphicsVariant &add_graphics_variant();
    GraphicsVariant *last_graphics_variant();
    void add_graphics_variant_condition(GraphicsCondition condition);
    void set_construction_mode(ConstructionMode mode);
    void set_construction_road_update_radius(int radius);
    void set_construction_free_when_broke_limit(int limit);
    void set_construction_max_count(int limit);
    void set_construction_max_count_unless_config(ConstructionConfigFlag flag);
    void set_construction_required_building_reference(std::string type_attr);
    void set_construction_required_building_type(building_type type);
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
    void add_culture_module_reference(std::string path, int capacity, int upgrade_bonus_capacity, CultureModuleCountMode count_mode);
    void add_storage_reference(std::string path);
    void add_production_method_reference(std::string path);
    void set_distribution_reference(std::string path);
    void resolve_culture_module(const std::string &path, const CultureModule *culture_module);
    void add_storage_type(const StorageType *storage_type);
    void add_production_method(ProductionMethod *production_method);
    void inherit_labor_category(LaborCategory category);
    void set_distribution(const Distribution *distribution);
    void set_temple_religion(const Religion *religion);

    building_type type() const;
    const char *attr() const;
    bool attr_is(std::string_view attr) const;
    bool matches_identity(std::string_view identity) const;
    const IdentityDefinition &identity() const;
    const BuildModelDefinition &model() const;
    const FoundationDef *foundation_def() const;
    const std::string &foundation_reference_path() const;
    const std::vector<std::string> &foundation_replacement_reference_paths() const;
    const std::vector<const BuildingType *> &foundation_replacement_types() const;
    int foundation_may_replace(const BuildingType *type) const;
    const BuildButtonDefinition &button() const;
    const std::vector<BuildButtonDefinition> &buttons() const;
    const BridgeDefinition &bridge() const;
    const RubbleDef &rubble() const;
    LaborCategory labor_category() const;
    const TileDefinition &tile() const;
    const ConstructionToolDefinition &tool() const;
    const ConstructionCycleDefinition &cycle() const;
    const Religion *religion() const;
    const SoundDefinition &sound() const;
    const MarketDefinition &market() const;
    const BuildingFlagsDefinition &flags() const;
    const MilitaryDefinition &military() const;
    const WaterAccessDefinition &water_access() const;
    const BuildingGraphicsDef &graphics() const;
    const ConstructionDefinition &construction() const;
    CompositionDef &composition();
    const CompositionDef &composition() const;
    ImageGroupEntryRef button_icon_ref() const;
    const char *button_text_key() const;
    int placement_width(int orientation) const;
    int placement_height(int orientation) const;
    figure_type preview_figure_type() const;
    int required_workers() const;
    int is_temple(
        std::optional<god_type> god = std::nullopt,
        ReligionTier tier = ReligionTier::None) const;
    int is_theater() const;
    int is_hippodrome() const;
    int is_well() const;
    int is_fountain() const;
    int is_latrines() const;
    int is_warehouse() const;
    int is_granary() const;
    int is_mess_hall() const;
    int is_architect_guild() const;
    int is_caravanserai() const;
    int is_lighthouse() const;
    int is_watchtower() const;
    int is_armoury() const;
    int is_farm() const;
    int is_storage() const;
    int is_plague_treatment_target() const;
    resource_type output_resource() const;
    const ProductionMethod *farm_production_method() const;
    const ProductionMethod *farm_panel_production_method() const;
    int has_farm_panel() const;
    int production_is_enabled() const;
    static const GraphicsTarget *resolve_graphics_target(const BuildingType *definition, const Building &building);
    int has_identity() const;
    int has_model() const;
    int has_button() const;
    int has_rubble() const;
    int has_tile() const;
    int has_cycle() const;
    int has_temple() const;
    int has_sound() const;
    int has_market() const;
    int has_flags() const;
    int has_military() const;
    int has_water_access_provider() const;
    int has_graphic() const;
    int has_construction() const;
    int has_composition() const;
    int has_rotated_placement_geometry() const;
    int has_phased_construction() const;
    int has_labor() const;
    const LaborDefinition &labor() const;
    const std::vector<SpawnDelayGroup> &spawn_groups() const;
    const std::vector<BuildingCultureModule> &culture_modules() const;
    const std::vector<std::string> &storage_reference_paths() const;
    const std::vector<std::string> &production_method_reference_paths() const;
    const std::string &distribution_reference_path() const;
    const std::string &temple_religion_reference_path() const;
    const std::vector<const StorageType *> &storage_types() const;
    const std::vector<ProductionMethod *> &production_methods() const;
    const Distribution *distribution() const;
    HousingDef &housing_def();
    const HousingDef &housing_def() const;
    building_type vacant_lot_fill_type() const;
    int has_native_storage() const;
    int has_native_production() const;
    int has_culture_modules() const;
    int has_distribution() const;
    int has_housing() const;
    int is_vacant_lot() const;
    unsigned char upgrade_level_for(const Building &building) const;

private:
    building_type type_;
    std::string attr_;
    IdentityDefinition identity_;
    BuildModelDefinition model_;
    std::string foundation_reference_path_;
    const FoundationDef *foundation_def_ = nullptr;
    std::vector<std::string> foundation_replacement_reference_paths_;
    std::vector<const BuildingType *> foundation_replacement_types_;
    std::vector<BuildButtonDefinition> buttons_;
    BridgeDefinition bridge_;
    RubbleDef rubble_;
    LaborCategory labor_category_ = LaborCategory::None;
    TileDefinition tile_;
    ConstructionToolDefinition tool_;
    ConstructionCycleDefinition cycle_;
    std::string religion_reference_path_;
    const Religion *religion_ = nullptr;
    SoundDefinition sound_;
    MarketDefinition market_;
    BuildingFlagsDefinition flags_;
    MilitaryDefinition military_;
    WaterAccessDefinition water_access_;
    BuildingGraphicsDef graphics_;
    ConstructionDefinition construction_;
    bool has_construction_ = false;
    CompositionDef composition_;
    LaborDefinition labor_;
    std::vector<SpawnDelayGroup> spawn_groups_;
    std::vector<BuildingCultureModule> culture_modules_;
    std::vector<std::string> storage_reference_paths_;
    std::vector<std::string> production_method_reference_paths_;
    std::string distribution_reference_path_;
    HousingDef housing_def_;
    std::vector<const StorageType *> storage_types_;
    std::vector<ProductionMethod *> production_methods_;
    const Distribution *distribution_ = nullptr;
};

} // namespace building_type_registry_impl


