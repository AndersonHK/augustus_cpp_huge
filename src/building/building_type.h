#pragma once

extern "C" {
#include "building/building.h"
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
class StorageType;

enum class GraphicComparison {
    None,
    LessThan,
    LessThanOrEqual,
    Equal,
    GreaterThan,
    GreaterThanOrEqual
};

enum class WaterAccessType {
    None,
    Well,
    Fountain,
    Reservoir
};

enum class WaterAccessRequirement {
    None,
    ReservoirNetwork,
    WaterSourceAny,
    WaterSourceFreshOnly
};

enum class WaterAccessNodeKind {
    None,
    AqueductConnection
};

enum class WaterAccessMode {
    None,
    ReservoirRange
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

enum class FigureSlot {
    None,
    Primary,
    Secondary,
    Quaternary
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

enum class GraphicsConditionType {
    None,
    HasWorkers,
    Working,
    WaterAccess,
    FigureSlotOccupied,
    ResourcePositive,
    Climate,
    MonumentUpgrade,
    FestivalGames,
    Desirability,
    Days1Positive,
    Days1NotPositive,
    Days2Positive,
    Days1OrDays2Positive
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
    void set_text_key(std::string key);

    int has_group() const;
    const char *group() const;
    int has_order() const;
    int order() const;
    int has_icon() const;
    const char *icon() const;
    int has_text_key() const;
    const char *text_key() const;
    int has_any() const;

private:
    std::string group_;
    int has_order_ = 0;
    int order_ = 0;
    std::string icon_;
    std::string text_key_;
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

struct GraphicsTarget {
    void set_path(std::string path);
    void set_image(std::string image);

    int has_path() const;
    const char *path() const;

    int has_image() const;
    const char *image() const;

private:
    std::string path_;
    std::string image_;
};

struct GraphicsCondition {
    GraphicsConditionType type = GraphicsConditionType::None;
    GraphicComparison comparison = GraphicComparison::None;
    FigureSlot figure_slot = FigureSlot::None;
    int threshold = 0;
    resource_type resource = RESOURCE_NONE;
    int climate = 0;
    int monument_upgrade = 0;
    int festival_games = 0;

    int matches(const ::building &building) const;
};

struct WaterAccessNode {
    WaterAccessNodeKind kind = WaterAccessNodeKind::None;
    int x = 0;
    int y = 0;
};

class WaterAccessDefinition {
public:
    void set_type(WaterAccessType type);
    void set_range(int range);
    void set_requirement(WaterAccessRequirement requirement);
    void add_node(WaterAccessNode node);

    int has_provider() const;
    WaterAccessType type() const;
    int range() const;
    WaterAccessRequirement requirement() const;
    const std::vector<WaterAccessNode> &nodes() const;

private:
    int has_type_ = 0;
    int has_range_ = 0;
    WaterAccessType type_ = WaterAccessType::None;
    int range_ = 0;
    WaterAccessRequirement requirement_ = WaterAccessRequirement::None;
    std::vector<WaterAccessNode> nodes_;
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

struct GraphicsVariant {
    GraphicsTarget target;
    std::vector<GraphicsCondition> conditions;

    int matches(const ::building &building) const;
};

class StateDefinition {
public:
    void set_water_access_mode(WaterAccessMode mode);

    WaterAccessMode water_access_mode() const;

private:
    int has_water_access_rule_ = 0;
    WaterAccessMode water_access_mode_ = WaterAccessMode::None;
};

class GraphicsDefinition {
public:
    void mark_default_node();
    GraphicsTarget &default_target();
    const GraphicsTarget &default_target() const;
    GraphicsVariant &add_variant();
    GraphicsVariant *last_variant();
    const GraphicsVariant *last_variant() const;

    int has_path() const;
    int has_default_node() const;
    int has_variants() const;
    const std::vector<GraphicsVariant> &variants() const;
    const GraphicsTarget *resolve_target(const ::building &building) const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    GraphicsTarget default_target_;
    std::vector<GraphicsVariant> variants_;
    int has_default_node_ = 0;
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

    void set_state_water_access_mode(WaterAccessMode mode);
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
    void set_button_text_key(std::string key);
    void set_sound_id(int sound);
    void set_sound_mute_on_enemies(int value);
    void set_sound_always_play(int value);
    void set_event_attr(std::string attr);
    void set_fire_proof(int value);
    void set_draw_desirability_range(int value);
    void set_venus_gt_bonus(int value);
    void set_water_access_type(WaterAccessType type);
    void set_water_access_range(int range);
    void set_water_access_requirement(WaterAccessRequirement requirement);
    void add_water_access_node(WaterAccessNode node);
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
    void add_storage_type(const StorageType *storage_type);
    void add_production_method(const ProductionMethod *production_method);

    building_type type() const;
    const char *attr() const;
    const IdentityDefinition &identity() const;
    const BuildModelDefinition &model() const;
    const FoundationDefinition &foundation() const;
    const BuildButtonDefinition &button() const;
    const SoundDefinition &sound() const;
    const EventDataDefinition &event_data() const;
    const BuildingFlagsDefinition &flags() const;
    const StateDefinition &state() const;
    const WaterAccessDefinition &water_access() const;
    const GraphicsDefinition &graphics() const;
    const ConstructionDefinition &construction() const;
    const GraphicsTarget *resolve_graphics_target(const ::building &building) const;
    const GraphicsTarget *resolve_construction_graphics_target(int phase) const;
    static const GraphicsTarget *resolve_graphics_target_for_image(const BuildingType *definition, const ::building &building);
    WaterAccessMode water_access_mode() const;
    int has_identity() const;
    int has_model() const;
    int has_foundation() const;
    int foundation_required_terrain() const;
    int has_button() const;
    int has_sound() const;
    int has_event_data() const;
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
    const std::vector<const StorageType *> &storage_types() const;
    const std::vector<const ProductionMethod *> &production_methods() const;
    int has_native_storage() const;
    int has_native_production() const;
    unsigned char upgrade_level_for(const ::building &building) const;

private:
    building_type type_;
    std::string attr_;
    IdentityDefinition identity_;
    BuildModelDefinition model_;
    FoundationDefinition foundation_;
    BuildButtonDefinition button_;
    SoundDefinition sound_;
    EventDataDefinition event_data_;
    BuildingFlagsDefinition flags_;
    StateDefinition state_;
    WaterAccessDefinition water_access_;
    GraphicsDefinition graphics_;
    ConstructionDefinition construction_;
    bool has_construction_ = false;
    LaborDefinition labor_;
    std::vector<SpawnDelayGroup> spawn_groups_;
    std::vector<std::string> storage_reference_paths_;
    std::vector<std::string> production_method_reference_paths_;
    std::vector<const StorageType *> storage_types_;
    std::vector<const ProductionMethod *> production_methods_;
};

} // namespace building_type_registry_impl
