#include "figure/figure.h"
#include "building/building_record.h"
#include "building/building.h"
#include "building/building_type.h"

#include "building/building_type_registry_internal.h"

#include "building/connectable.h"
#include "building/industry.h"
#include "building/production_method.h"
#include "building/religion.h"
#include "figure/formation_type.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/constants.h"

#include <algorithm>
#include <initializer_list>
#include <utility>

namespace building_type_registry_impl {

void IdentityDefinition::set_name_key(std::string key)
{
    name_key_ = std::move(key);
}

bool IdentityDefinition::add_alias(std::string alias)
{
    if (alias.empty() || has_alias(alias)) {
        return false;
    }
    aliases_.push_back(std::move(alias));
    return true;
}

int IdentityDefinition::has_name_key() const
{
    return !name_key_.empty();
}

const char *IdentityDefinition::name_key() const
{
    return name_key_.c_str();
}

bool IdentityDefinition::has_alias(std::string_view alias) const
{
    return std::find(aliases_.begin(), aliases_.end(), alias) != aliases_.end();
}

const std::vector<std::string> &IdentityDefinition::aliases() const
{
    return aliases_;
}

void BuildModelDefinition::set_cost(int value)
{
    has_cost_ = 1;
    cost_ = value;
}

void BuildModelDefinition::set_hit_points(int value)
{
    has_hit_points_ = 1;
    hit_points_ = value;
}

void BuildModelDefinition::set_desirability_value(int value)
{
    has_desirability_value_ = 1;
    desirability_value_ = value;
}

void BuildModelDefinition::set_desirability_step(int value)
{
    has_desirability_step_ = 1;
    desirability_step_ = value;
}

void BuildModelDefinition::set_desirability_step_size(int value)
{
    has_desirability_step_size_ = 1;
    desirability_step_size_ = value;
}

void BuildModelDefinition::set_desirability_range(int value)
{
    has_desirability_range_ = 1;
    desirability_range_ = value;
}

int BuildModelDefinition::has_cost() const
{
    return has_cost_;
}

int BuildModelDefinition::cost() const
{
    return cost_;
}

int BuildModelDefinition::has_hit_points() const
{
    return has_hit_points_;
}

int BuildModelDefinition::hit_points() const
{
    return hit_points_;
}

int BuildModelDefinition::has_desirability_value() const
{
    return has_desirability_value_;
}

int BuildModelDefinition::desirability_value() const
{
    return desirability_value_;
}

int BuildModelDefinition::has_desirability_step() const
{
    return has_desirability_step_;
}

int BuildModelDefinition::desirability_step() const
{
    return desirability_step_;
}

int BuildModelDefinition::has_desirability_step_size() const
{
    return has_desirability_step_size_;
}

int BuildModelDefinition::desirability_step_size() const
{
    return desirability_step_size_;
}

int BuildModelDefinition::has_desirability_range() const
{
    return has_desirability_range_;
}

int BuildModelDefinition::desirability_range() const
{
    return desirability_range_;
}

int BuildModelDefinition::has_any() const
{
    return has_cost_ || has_hit_points_ || has_desirability_value_ || has_desirability_step_ ||
        has_desirability_step_size_ || has_desirability_range_;
}

void BuildButtonDefinition::set_group(std::string group)
{
    group_ = std::move(group);
}

void BuildButtonDefinition::set_order(int order)
{
    has_order_ = 1;
    order_ = order;
}

void BuildButtonDefinition::set_icon(std::string icon)
{
    icon_ = std::move(icon);
}

void BuildButtonDefinition::set_icon_image(std::string image)
{
    icon_image_ = std::move(image);
}

void BuildButtonDefinition::set_text_key(std::string key)
{
    text_key_ = std::move(key);
}

int BuildButtonDefinition::has_group() const
{
    return !group_.empty();
}

const char *BuildButtonDefinition::group() const
{
    return group_.c_str();
}

int BuildButtonDefinition::has_order() const
{
    return has_order_;
}

int BuildButtonDefinition::order() const
{
    return order_;
}

int BuildButtonDefinition::has_icon() const
{
    return !icon_.empty();
}

const char *BuildButtonDefinition::icon() const
{
    return icon_.c_str();
}

int BuildButtonDefinition::has_icon_image() const
{
    return !icon_image_.empty();
}

const char *BuildButtonDefinition::icon_image() const
{
    return icon_image_.c_str();
}

ImageGroupEntryRef BuildButtonDefinition::icon_ref() const
{
    if (!has_icon()) {
        return ImageGroupEntryRef();
    }
    return ImageGroupEntryRef::from_group(icon(), has_icon_image() ? icon_image() : "");
}

int BuildButtonDefinition::has_text_key() const
{
    return !text_key_.empty();
}

const char *BuildButtonDefinition::text_key() const
{
    return text_key_.c_str();
}

int BuildButtonDefinition::has_any() const
{
    return has_group() || has_order() || has_icon() || has_icon_image() || has_text_key();
}

void BridgeDefinition::set_type(BridgeType type)
{
    type_ = type;
}

BridgeType BridgeDefinition::type() const
{
    return type_;
}

int BridgeDefinition::is_bridge() const
{
    return type_ != BridgeType::None;
}

int BridgeDefinition::is_ship_bridge() const
{
    return type_ == BridgeType::Ship;
}

void TileDefinition::set_kind(TileKind kind)
{
    kind_ = kind;
}

void TileDefinition::set_kind_key(std::string key)
{
    kind_key_ = std::move(key);
}

void TileDefinition::set_refresh_behavior(TileRefreshBehavior behavior)
{
    refresh_behavior_ = behavior;
}

void TileDefinition::set_placement_behavior(TilePlacementBehavior behavior)
{
    placement_behavior_ = behavior;
}

void TileDefinition::set_overgrown(int value)
{
    overgrown_ = value ? 1 : 0;
}

TileKind TileDefinition::kind() const
{
    return kind_;
}

const char *TileDefinition::kind_key() const
{
    return kind_key_.c_str();
}

TileRefreshBehavior TileDefinition::refresh_behavior() const
{
    return refresh_behavior_;
}

TilePlacementBehavior TileDefinition::placement_behavior() const
{
    return placement_behavior_;
}

int TileDefinition::is_area_placement() const
{
    return placement_behavior_ != TilePlacementBehavior::None;
}

int TileDefinition::updates_land_routing() const
{
    return placement_behavior_ == TilePlacementBehavior::Garden;
}

int TileDefinition::overgrown() const
{
    return overgrown_;
}

int TileDefinition::has_any() const
{
    return kind_ != TileKind::None || !kind_key_.empty() ||
        refresh_behavior_ != TileRefreshBehavior::None ||
        placement_behavior_ != TilePlacementBehavior::None || overgrown_;
}

void ConstructionToolDefinition::set_kind(ConstructionToolKind kind)
{
    kind_ = kind;
}

void ConstructionToolDefinition::set_drag_terrain(ConstructionDragTerrain terrain)
{
    drag_terrain_ = terrain;
}

void ConstructionToolDefinition::set_drag_rotation(ConstructionDragRotation rotation)
{
    drag_rotation_ = rotation;
}

void SmartToolDef::add_mode(SmartToolModeDefinition mode)
{
    modes_.push_back(std::move(mode));
}

void SmartToolDef::resolve_mode_type(std::size_t index, const BuildingType *type)
{
    if (index < modes_.size()) {
        modes_[index].type = type;
    }
}

const std::vector<SmartToolModeDefinition> &SmartToolDef::modes() const
{
    return modes_;
}

const SmartToolModeDefinition *SmartToolDef::resolve_mode(
    int control,
    int shift,
    SmartToolContext context) const
{
    // Specific modifiers retain authored priority when both keys are held.
    // An unconditional mode is a fallback, never an order-dependent shadow.
    for (const SmartToolModeDefinition &mode : modes_) {
        if (mode.context == context &&
            ((mode.modifier == SmartToolModifier::Control && control) ||
            (mode.modifier == SmartToolModifier::Shift && shift))) {
            return &mode;
        }
    }
    for (const SmartToolModeDefinition &mode : modes_) {
        if (mode.context == context && mode.modifier == SmartToolModifier::Any) {
            return &mode;
        }
    }
    return nullptr;
}

const SmartToolModeDefinition *SmartToolDef::mode_for_type(
    const BuildingType *type,
    SmartToolContext context) const
{
    if (!type) {
        return nullptr;
    }
    for (const SmartToolModeDefinition &mode : modes_) {
        if (mode.context == context && mode.type == type) {
            return &mode;
        }
    }
    return nullptr;
}

int SmartToolDef::has_any() const
{
    return !modes_.empty();
}

void ConstructionToolDefinition::add_smart_mode(SmartToolModeDefinition mode)
{
    smart_tool_.add_mode(std::move(mode));
}

void ConstructionToolDefinition::resolve_smart_mode_type(std::size_t index, const BuildingType *type)
{
    smart_tool_.resolve_mode_type(index, type);
}

ConstructionToolKind ConstructionToolDefinition::kind() const
{
    return kind_;
}

ConstructionDragTerrain ConstructionToolDefinition::drag_terrain() const
{
    return drag_terrain_;
}

ConstructionDragRotation ConstructionToolDefinition::drag_rotation() const
{
    return drag_rotation_;
}

int ConstructionToolDefinition::has_any() const
{
    return kind_ != ConstructionToolKind::None;
}

int ConstructionToolDefinition::is_clear_land() const
{
    return kind_ == ConstructionToolKind::ClearLand;
}

int ConstructionToolDefinition::is_clear_trees() const
{
    return kind_ == ConstructionToolKind::ClearTrees;
}

int ConstructionToolDefinition::is_repair_land() const
{
    return kind_ == ConstructionToolKind::RepairLand;
}

int ConstructionToolDefinition::is_land_work() const
{
    return is_clear_land() || is_clear_trees() || is_repair_land();
}

int ConstructionToolDefinition::is_road() const
{
    return kind_ == ConstructionToolKind::Road;
}

int ConstructionToolDefinition::is_highway() const
{
    return kind_ == ConstructionToolKind::Highway;
}

int ConstructionToolDefinition::is_wall() const
{
    return kind_ == ConstructionToolKind::Wall;
}

int ConstructionToolDefinition::is_roadblock() const
{
    return kind_ == ConstructionToolKind::Roadblock;
}

int ConstructionToolDefinition::is_aqueduct() const
{
    return kind_ == ConstructionToolKind::Aqueduct;
}

int ConstructionToolDefinition::is_draggable_reservoir() const
{
    return kind_ == ConstructionToolKind::DraggableReservoir;
}

int ConstructionToolDefinition::is_draggable_building() const
{
    return kind_ == ConstructionToolKind::DraggableBuilding;
}

int ConstructionToolDefinition::draggable_allows_roads() const
{
    return drag_terrain_ == ConstructionDragTerrain::LandOrRoad;
}

const SmartToolDef &ConstructionToolDefinition::smart_tool() const
{
    return smart_tool_;
}

void ConstructionCycleDefinition::set_group(std::string group)
{
    group_ = std::move(group);
}

void ConstructionCycleDefinition::set_order(int order)
{
    order_ = order;
}

void ConstructionCycleDefinition::set_steps(int steps)
{
    steps_ = steps;
}

int ConstructionCycleDefinition::has_group() const
{
    return !group_.empty();
}

const char *ConstructionCycleDefinition::group() const
{
    return group_.c_str();
}

int ConstructionCycleDefinition::order() const
{
    return order_;
}

int ConstructionCycleDefinition::steps() const
{
    return steps_;
}

int ConstructionCycleDefinition::has_any() const
{
    return has_group();
}

void SoundDefinition::set_city_sound(int sound)
{
    has_city_sound_ = 1;
    city_sound_ = sound;
}

void SoundDefinition::set_mute_on_enemies(int value)
{
    mute_on_enemies_ = value ? 1 : 0;
}

void SoundDefinition::set_always_play(int value)
{
    always_play_ = value ? 1 : 0;
}

int SoundDefinition::has_city_sound() const
{
    return has_city_sound_;
}

int SoundDefinition::city_sound() const
{
    return city_sound_;
}

int SoundDefinition::mute_on_enemies() const
{
    return mute_on_enemies_;
}

int SoundDefinition::always_play() const
{
    return always_play_;
}

int SoundDefinition::has_any() const
{
    return has_city_sound_ || mute_on_enemies_ || always_play_;
}

void MarketDefinition::set_max_distance(int value)
{
    max_distance_ = value;
}

void MarketDefinition::set_max_food_stock(int value)
{
    max_food_stock_ = value;
}

int MarketDefinition::max_distance() const
{
    return max_distance_;
}

int MarketDefinition::max_food_stock() const
{
    return max_food_stock_;
}

int MarketDefinition::has_any() const
{
    return max_distance_ > 0 && max_food_stock_ > 0;
}

void BuildingFlagsDefinition::set_fire_proof(int value)
{
    has_fire_proof_ = 1;
    fire_proof_ = value;
}

void BuildingFlagsDefinition::set_draw_desirability_range(int value)
{
    has_draw_desirability_range_ = 1;
    draw_desirability_range_ = value;
}

void BuildingFlagsDefinition::set_venus_gt_bonus(int value)
{
    has_venus_gt_bonus_ = 1;
    venus_gt_bonus_ = value;
}

int BuildingFlagsDefinition::has_fire_proof() const
{
    return has_fire_proof_;
}

int BuildingFlagsDefinition::fire_proof() const
{
    return fire_proof_;
}

int BuildingFlagsDefinition::has_draw_desirability_range() const
{
    return has_draw_desirability_range_;
}

int BuildingFlagsDefinition::draw_desirability_range() const
{
    return draw_desirability_range_;
}

int BuildingFlagsDefinition::has_venus_gt_bonus() const
{
    return has_venus_gt_bonus_;
}

int BuildingFlagsDefinition::venus_gt_bonus() const
{
    return venus_gt_bonus_;
}

int BuildingFlagsDefinition::has_any() const
{
    return has_fire_proof_ || has_draw_desirability_range_ || has_venus_gt_bonus_;
}

void MilitaryDefinition::set_formation_reference(std::string key)
{
    formation_reference_ = std::move(key);
}

void MilitaryDefinition::set_formation_type(const FormationType *formation)
{
    formation_type_ = formation;
}

const std::string &MilitaryDefinition::formation_reference() const
{
    return formation_reference_;
}

const FormationType *MilitaryDefinition::formation_type() const
{
    return formation_type_;
}

figure_type MilitaryDefinition::primary_figure_type() const
{
    return formation_type_ ? formation_type_->primary_figure_type() : FIGURE_NONE;
}

int MilitaryDefinition::has_any() const
{
    return !formation_reference_.empty();
}

void WaterAccessDefinition::set_requires_open_water(int required)
{
    requires_open_water_ = required ? 1 : 0;
}

void WaterAccessDefinition::add_provide_rule(WaterAccessProvideRule rule)
{
    provide_rules_.push_back(std::move(rule));
}

void WaterAccessDefinition::add_requirement_rule(WaterAccessRequirementRule rule)
{
    requirement_rules_.push_back(std::move(rule));
}

void WaterAccessDefinition::add_node(WaterAccessNode node)
{
    nodes_.push_back(std::move(node));
    provider_nodes_.push_back(nodes_.back());
    requirement_nodes_.push_back(nodes_.back());
}

void WaterAccessDefinition::add_provider_node(WaterAccessNode node)
{
    provider_nodes_.push_back(std::move(node));
}

void WaterAccessDefinition::add_requirement_node(WaterAccessNode node)
{
    requirement_nodes_.push_back(std::move(node));
}

int WaterAccessDefinition::has_provider() const
{
    return !provide_rules_.empty();
}

int WaterAccessDefinition::has_requirements() const
{
    return !requirement_rules_.empty();
}

int WaterAccessDefinition::requires_open_water() const
{
    return requires_open_water_;
}

const std::vector<WaterAccessProvideRule> &WaterAccessDefinition::provide_rules() const
{
    return provide_rules_;
}

const std::vector<WaterAccessRequirementRule> &WaterAccessDefinition::requirement_rules() const
{
    return requirement_rules_;
}

const std::vector<WaterAccessNode> &WaterAccessDefinition::nodes() const
{
    return nodes_;
}

const std::vector<WaterAccessNode> &WaterAccessDefinition::provider_nodes() const
{
    return provider_nodes_;
}

const std::vector<WaterAccessNode> &WaterAccessDefinition::requirement_nodes() const
{
    return requirement_nodes_;
}

void LaborDefinition::set_employee_count(int count)
{
    has_employee_count_ = 1;
    employee_count_ = count;
}

void LaborDefinition::set_seeker_policy(LaborSeekerPolicy policy)
{
    has_seeker_policy_ = 1;
    seeker_policy_ = policy;
}

int LaborDefinition::has_employee_count() const
{
    return has_employee_count_;
}

int LaborDefinition::employee_count() const
{
    return employee_count_;
}

int LaborDefinition::has_seeker_policy() const
{
    return has_seeker_policy_;
}

const LaborSeekerPolicy &LaborDefinition::seeker_policy() const
{
    return seeker_policy_;
}

int LaborDefinition::has_any() const
{
    return has_employee_count_ || has_seeker_policy_;
}

void ConstructionDefinition::set_mode(ConstructionMode mode)
{
    mode_ = mode;
}

void ConstructionDefinition::set_road_update_radius(int radius)
{
    road_update_radius_ = radius;
}

void ConstructionDefinition::set_free_when_broke_limit(int limit)
{
    free_when_broke_limit_ = limit;
}

void ConstructionDefinition::set_max_count(int limit)
{
    max_count_ = limit;
}

void ConstructionDefinition::set_max_count_unless_config(ConstructionConfigFlag flag)
{
    max_count_unless_config_ = flag;
}

void ConstructionDefinition::set_required_building_reference(std::string type_attr)
{
    required_building_reference_ = std::move(type_attr);
}

void ConstructionDefinition::set_required_building_type(building_type type)
{
    required_building_type_ = type;
}

ConstructionPhase &ConstructionDefinition::add_phase(int index)
{
    phases_.push_back(ConstructionPhase());
    phases_.back().index = index;
    return phases_.back();
}

ConstructionPhase *ConstructionDefinition::last_phase()
{
    return phases_.empty() ? nullptr : &phases_.back();
}

const ConstructionPhase *ConstructionDefinition::phase(int index) const
{
    for (const ConstructionPhase &phase : phases_) {
        if (phase.index == index) {
            return &phase;
        }
    }
    return nullptr;
}

void ConstructionDefinition::add_instant_requirement(resource_type resource, int amount)
{
    instant_requirements_.push_back({ resource, amount });
}

void ConstructionDefinition::add_requirement(resource_type resource, int amount)
{
    ConstructionPhase *current_phase = last_phase();
    if (current_phase) {
        current_phase->requirements.push_back({ resource, amount });
    }
}

ConstructionMode ConstructionDefinition::mode() const
{
    return mode_;
}

int ConstructionDefinition::is_phased() const
{
    return mode_ == ConstructionMode::Phased;
}

int ConstructionDefinition::road_update_radius() const
{
    return road_update_radius_;
}

int ConstructionDefinition::free_when_broke_limit() const
{
    return free_when_broke_limit_;
}

int ConstructionDefinition::max_count() const
{
    return max_count_;
}

ConstructionConfigFlag ConstructionDefinition::max_count_unless_config() const
{
    return max_count_unless_config_;
}

const std::string &ConstructionDefinition::required_building_reference() const
{
    return required_building_reference_;
}

building_type ConstructionDefinition::required_building_type() const
{
    return required_building_type_;
}

int ConstructionDefinition::phase_count() const
{
    return static_cast<int>(phases_.size());
}

int ConstructionDefinition::instant_requirement_amount(resource_type resource) const
{
    if (mode_ != ConstructionMode::Instant) {
        return 0;
    }
    for (const ConstructionRequirement &requirement : instant_requirements_) {
        if (requirement.resource == resource) {
            return requirement.amount;
        }
    }
    return 0;
}

int ConstructionDefinition::requirement_amount(resource_type resource, int phase_index) const
{
    const ConstructionPhase *current_phase = phase(phase_index);
    if (!current_phase) {
        return 0;
    }
    for (const ConstructionRequirement &requirement : current_phase->requirements) {
        if (requirement.resource == resource) {
            return requirement.amount;
        }
    }
    return 0;
}

const std::vector<ConstructionPhase> &ConstructionDefinition::phases() const
{
    return phases_;
}

static bool attr_is(const std::string &attr, const char *text_id)
{
    return attr == text_id;
}

static bool attr_is_any(const std::string &attr, std::initializer_list<const char *> text_ids)
{
    for (const char *text_id : text_ids) {
        if (attr_is(attr, text_id)) {
            return true;
        }
    }
    return false;
}

static LaborCategory determine_labor_category(const std::string &attr)
{
    if (attr_is(attr, "theater") || attr_is_any(attr, {
        "amphitheater", "hippodrome", "colosseum", "gladiator_school", "lion_house",
        "actor_colony", "chariot_maker", "tavern", "arena"
    })) {
        return LaborCategory::Entertainment;
    }
    if (attr_is_any(attr, {
        "gatehouse", "tower", "military_academy", "barracks", "mess_hall",
        "watchtower", "armoury"
    })) {
        return LaborCategory::Military;
    }
    if (attr_is_any(attr, {
        "small_temple_ceres", "large_temple_ceres", "grand_temple_ceres",
        "small_temple_neptune", "large_temple_neptune", "grand_temple_neptune",
        "small_temple_mercury", "large_temple_mercury", "grand_temple_mercury",
        "small_temple_mars", "large_temple_mars", "grand_temple_mars",
        "small_temple_venus", "large_temple_venus", "grand_temple_venus",
        "pantheon", "senate", "forum", "oracle", "lighthouse", "caravanserai"
    })) {
        return LaborCategory::GovernanceReligion;
    }
    if (attr_is_any(attr, {
        "market", "warehouse", "dock", "city_mint", "concrete_maker",
        "brickworks", "cart_depot", "distribution_center_unused"
    })) {
        return LaborCategory::IndustryCommerce;
    }
    if (attr_is_any(attr, {"granary", "shipyard", "wharf"})) {
        return LaborCategory::FoodProduction;
    }
    if (attr_is_any(attr, {"engineers_post", "workcamp", "architect_guild"})) {
        return LaborCategory::Engineering;
    }
    if (attr_is(attr, "fountain")) {
        return LaborCategory::Water;
    }
    if (attr_is(attr, "prefecture")) {
        return LaborCategory::Prefectures;
    }
    if (attr_is_any(attr, {
        "doctor", "hospital", "bathhouse", "barber", "school", "academy",
        "library", "mission_post", "latrines"
    })) {
        return LaborCategory::HealthEducation;
    }

    return LaborCategory::None;
}

BuildingType::BuildingType(building_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
    , labor_category_(determine_labor_category(attr_))
{
}

void BuildingType::assign_runtime_type(building_type type)
{
    type_ = type;
}

void BuildingType::set_identity_name_key(std::string key)
{
    identity_.set_name_key(std::move(key));
}

bool BuildingType::add_identity_alias(std::string alias)
{
    return identity_.add_alias(std::move(alias));
}

void BuildingType::set_model_cost(int value)
{
    model_.set_cost(value);
}

void BuildingType::set_model_hit_points(int value)
{
    model_.set_hit_points(value);
}

void BuildingType::set_model_desirability_value(int value)
{
    model_.set_desirability_value(value);
}

void BuildingType::set_model_desirability_step(int value)
{
    model_.set_desirability_step(value);
}

void BuildingType::set_model_desirability_step_size(int value)
{
    model_.set_desirability_step_size(value);
}

void BuildingType::set_model_desirability_range(int value)
{
    model_.set_desirability_range(value);
}

void BuildingType::set_foundation_reference(std::string path)
{
    foundation_reference_path_ = std::move(path);
}

void BuildingType::set_foundation_definition(const FoundationDef *foundation)
{
    foundation_def_ = foundation;
}

int BuildingType::add_foundation_replacement_reference(std::string type)
{
    if (type.empty() || std::find(
            foundation_replacement_reference_paths_.begin(),
            foundation_replacement_reference_paths_.end(),
            type) != foundation_replacement_reference_paths_.end()) {
        return 0;
    }
    foundation_replacement_reference_paths_.push_back(std::move(type));
    return 1;
}

void BuildingType::add_foundation_replacement_type(const BuildingType *type)
{
    if (type && std::find(
            foundation_replacement_types_.begin(),
            foundation_replacement_types_.end(),
            type) == foundation_replacement_types_.end()) {
        foundation_replacement_types_.push_back(type);
    }
}

void BuildingType::set_button_group(std::string group)
{
    if (buttons_.empty()) {
        buttons_.emplace_back();
    }
    buttons_.front().set_group(std::move(group));
}

void BuildingType::set_button_order(int order)
{
    if (buttons_.empty()) {
        buttons_.emplace_back();
    }
    buttons_.front().set_order(order);
}

void BuildingType::set_button_icon(std::string icon)
{
    if (buttons_.empty()) {
        buttons_.emplace_back();
    }
    buttons_.front().set_icon(std::move(icon));
}

void BuildingType::set_button_icon_image(std::string image)
{
    if (buttons_.empty()) {
        buttons_.emplace_back();
    }
    buttons_.front().set_icon_image(std::move(image));
}

void BuildingType::set_button_text_key(std::string key)
{
    if (buttons_.empty()) {
        buttons_.emplace_back();
    }
    buttons_.front().set_text_key(std::move(key));
}

void BuildingType::add_button(BuildButtonDefinition button)
{
    buttons_.push_back(std::move(button));
}

void BuildingType::set_bridge_type(BridgeType type)
{
    bridge_.set_type(type);
}

void BuildingType::set_rubble(RubbleType type)
{
    rubble_.set(type, this);
}

void BuildingType::set_rubble_burn_days(int days)
{
    rubble_.burn_days = days;
}

void BuildingType::set_rubble_decay_reference(std::string attr)
{
    rubble_.decays_to = std::move(attr);
}

void BuildingType::set_rubble_decay_type(const BuildingType *type)
{
    rubble_.decay_type = type;
}

void BuildingType::set_tile_kind(TileKind kind)
{
    tile_.set_kind(kind);
}

void BuildingType::set_tile_kind_key(std::string key)
{
    tile_.set_kind_key(std::move(key));
}

void BuildingType::set_tile_refresh_behavior(TileRefreshBehavior behavior)
{
    tile_.set_refresh_behavior(behavior);
}

void BuildingType::set_tile_placement_behavior(TilePlacementBehavior behavior)
{
    tile_.set_placement_behavior(behavior);
}

void BuildingType::set_tile_overgrown(int value)
{
    tile_.set_overgrown(value);
}

void BuildingType::set_tool_kind(ConstructionToolKind kind)
{
    tool_.set_kind(kind);
}

void BuildingType::set_tool_drag_terrain(ConstructionDragTerrain terrain)
{
    tool_.set_drag_terrain(terrain);
}

void BuildingType::set_tool_drag_rotation(ConstructionDragRotation rotation)
{
    tool_.set_drag_rotation(rotation);
}

void BuildingType::add_tool_smart_mode(SmartToolModeDefinition mode)
{
    tool_.add_smart_mode(std::move(mode));
}

void BuildingType::resolve_tool_smart_mode_type(std::size_t index, const BuildingType *type)
{
    tool_.resolve_smart_mode_type(index, type);
}

void BuildingType::set_cycle_group(std::string group)
{
    cycle_.set_group(std::move(group));
}

void BuildingType::set_cycle_order(int order)
{
    cycle_.set_order(order);
}

void BuildingType::set_cycle_steps(int steps)
{
    cycle_.set_steps(steps);
}

void BuildingType::set_temple_religion_reference(std::string path)
{
    religion_reference_path_ = std::move(path);
}

void BuildingType::set_sound_id(int sound)
{
    sound_.set_city_sound(sound);
}

void BuildingType::set_sound_mute_on_enemies(int value)
{
    sound_.set_mute_on_enemies(value);
}

void BuildingType::set_sound_always_play(int value)
{
    sound_.set_always_play(value);
}

void BuildingType::set_market_max_distance(int value)
{
    market_.set_max_distance(value);
}

void BuildingType::set_market_max_food_stock(int value)
{
    market_.set_max_food_stock(value);
}

void BuildingType::set_fire_proof(int value)
{
    flags_.set_fire_proof(value);
}

void BuildingType::set_draw_desirability_range(int value)
{
    flags_.set_draw_desirability_range(value);
}

void BuildingType::set_venus_gt_bonus(int value)
{
    flags_.set_venus_gt_bonus(value);
}

void BuildingType::set_military_formation_reference(std::string key)
{
    military_.set_formation_reference(std::move(key));
}

void BuildingType::set_military_formation_type(const FormationType *formation)
{
    military_.set_formation_type(formation);
}

void BuildingType::add_water_access_provide_rule(WaterAccessProvideRule rule)
{
    water_access_.add_provide_rule(std::move(rule));
}

void BuildingType::add_water_access_requirement_rule(WaterAccessRequirementRule rule)
{
    water_access_.add_requirement_rule(std::move(rule));
}

void BuildingType::set_water_access_requires_open_water(int required)
{
    water_access_.set_requires_open_water(required);
}

void BuildingType::add_water_access_node(WaterAccessNode node)
{
    water_access_.add_node(std::move(node));
}

void BuildingType::add_water_access_provider_node(WaterAccessNode node)
{
    water_access_.add_provider_node(std::move(node));
}

void BuildingType::add_water_access_requirement_node(WaterAccessNode node)
{
    water_access_.add_requirement_node(std::move(node));
}

void BuildingType::set_graphics_status_icon_anchor(int x, int y)
{
    graphics_.set_status_icon_anchor(x, y);
}

void BuildingType::set_graphics_overlay_summary_policy(GraphicsOverlaySummaryPolicy policy)
{
    graphics_.set_overlay_summary_policy(policy);
}

void BuildingType::mark_graphics_default_node()
{
    graphics_.mark_default_node();
}

void BuildingType::clear_graphics()
{
    graphics_ = BuildingGraphicsDef();
}

GraphicsTarget &BuildingType::default_graphics_target()
{
    return graphics_.default_target();
}

GraphicsVariant &BuildingType::add_graphics_variant()
{
    return graphics_.add_variant();
}

GraphicsVariant *BuildingType::last_graphics_variant()
{
    return graphics_.last_variant();
}

void BuildingType::add_graphics_variant_condition(GraphicsCondition condition)
{
    GraphicsVariant *variant = graphics_.last_variant();
    if (variant) {
        variant->conditions.push_back(std::move(condition));
    }
}

void BuildingType::set_construction_mode(ConstructionMode mode)
{
    has_construction_ = true;
    construction_.set_mode(mode);
}

void BuildingType::set_construction_road_update_radius(int radius)
{
    has_construction_ = true;
    construction_.set_road_update_radius(radius);
}

void BuildingType::set_construction_free_when_broke_limit(int limit)
{
    has_construction_ = true;
    construction_.set_free_when_broke_limit(limit);
}

void BuildingType::set_construction_max_count(int limit)
{
    has_construction_ = true;
    construction_.set_max_count(limit);
}

void BuildingType::set_construction_max_count_unless_config(ConstructionConfigFlag flag)
{
    has_construction_ = true;
    construction_.set_max_count_unless_config(flag);
}

void BuildingType::set_construction_required_building_reference(std::string type_attr)
{
    has_construction_ = true;
    construction_.set_required_building_reference(std::move(type_attr));
}

void BuildingType::set_construction_required_building_type(building_type type)
{
    has_construction_ = true;
    construction_.set_required_building_type(type);
}

void BuildingType::clear_construction()
{
    construction_ = ConstructionDefinition();
    has_construction_ = false;
}

ConstructionPhase &BuildingType::add_construction_phase(int index)
{
    has_construction_ = true;
    return construction_.add_phase(index);
}

ConstructionPhase *BuildingType::last_construction_phase()
{
    return construction_.last_phase();
}

void BuildingType::add_instant_construction_requirement(resource_type resource, int amount)
{
    has_construction_ = true;
    construction_.add_instant_requirement(resource, amount);
}

void BuildingType::add_construction_requirement(resource_type resource, int amount)
{
    has_construction_ = true;
    construction_.add_requirement(resource, amount);
}

void BuildingType::add_spawn_policy(SpawnPolicy policy)
{
    if (spawn_groups_.empty()) {
        spawn_groups_.emplace_back();
    }
    spawn_groups_.back().policies.push_back(std::move(policy));
}

void BuildingType::set_labor_employee_count(int count)
{
    labor_.set_employee_count(count);
}

void BuildingType::set_labor_seeker_policy(LaborSeekerPolicy policy)
{
    labor_.set_seeker_policy(policy);
}

void BuildingType::add_spawn_group(SpawnDelayGroup group)
{
    spawn_groups_.push_back(std::move(group));
}

void BuildingType::add_culture_module_reference(std::string path, int capacity, int upgrade_bonus_capacity, CultureModuleCountMode count_mode)
{
    BuildingCultureModule module;
    module.reference_path = std::move(path);
    module.capacity = capacity;
    module.upgrade_bonus_capacity = upgrade_bonus_capacity;
    module.count_mode = count_mode;
    culture_modules_.push_back(std::move(module));
}

void BuildingType::add_storage_reference(std::string path)
{
    storage_reference_paths_.push_back(std::move(path));
}

void BuildingType::add_production_method_reference(std::string path)
{
    production_method_reference_paths_.push_back(std::move(path));
}

void BuildingType::set_distribution_reference(std::string path)
{
    distribution_reference_path_ = std::move(path);
}

void BuildingType::resolve_culture_module(const std::string &path, const CultureModule *culture_module)
{
    for (BuildingCultureModule &module : culture_modules_) {
        if (module.reference_path == path) {
            module.module = culture_module;
            return;
        }
    }
}

void BuildingType::add_storage_type(const StorageType *storage_type)
{
    storage_types_.push_back(storage_type);
}

void BuildingType::add_production_method(ProductionMethod *production_method)
{
    production_methods_.push_back(production_method);
    const resource_type output = production_method ? production_method->output_resource() : RESOURCE_NONE;
    if (labor_category_ == LaborCategory::None && output != RESOURCE_NONE) {
        labor_category_ = resource_is_food(output) ? LaborCategory::FoodProduction : LaborCategory::IndustryCommerce;
    }
}

void BuildingType::inherit_labor_category(LaborCategory category)
{
    if (labor_category_ == LaborCategory::None && category != LaborCategory::None) {
        labor_category_ = category;
    }
}

void BuildingType::set_distribution(const Distribution *distribution)
{
    distribution_ = distribution;
}

void BuildingType::set_temple_religion(const Religion *religion)
{
    religion_ = religion;
}

SpawnDelayGroup *BuildingType::last_spawn_group()
{
    return spawn_groups_.empty() ? nullptr : &spawn_groups_.back();
}

building_type BuildingType::type() const
{
    return type_;
}

const char *BuildingType::attr() const
{
    return attr_.c_str();
}

bool BuildingType::attr_is(std::string_view attr) const
{
    return attr_ == attr;
}

bool BuildingType::matches_identity(std::string_view identity) const
{
    return attr_is(identity) || identity_.has_alias(identity);
}

const IdentityDefinition &BuildingType::identity() const
{
    return identity_;
}

const BuildModelDefinition &BuildingType::model() const
{
    return model_;
}

const FoundationDef *BuildingType::foundation_def() const
{
    return foundation_def_;
}

const std::string &BuildingType::foundation_reference_path() const
{
    return foundation_reference_path_;
}

const std::vector<std::string> &BuildingType::foundation_replacement_reference_paths() const
{
    return foundation_replacement_reference_paths_;
}

const std::vector<const BuildingType *> &BuildingType::foundation_replacement_types() const
{
    return foundation_replacement_types_;
}

int BuildingType::foundation_may_replace(const BuildingType *type) const
{
    return type && std::find(
        foundation_replacement_types_.begin(),
        foundation_replacement_types_.end(),
        type) != foundation_replacement_types_.end();
}

const BuildButtonDefinition &BuildingType::button() const
{
    static const BuildButtonDefinition empty_button;
    return buttons_.empty() ? empty_button : buttons_.front();
}

const std::vector<BuildButtonDefinition> &BuildingType::buttons() const
{
    return buttons_;
}

const BridgeDefinition &BuildingType::bridge() const
{
    return bridge_;
}

const RubbleDef &BuildingType::rubble() const
{
    return rubble_;
}

LaborCategory BuildingType::labor_category() const
{
    return labor_category_;
}

const TileDefinition &BuildingType::tile() const
{
    return tile_;
}

const ConstructionToolDefinition &BuildingType::tool() const
{
    return tool_;
}

const ConstructionCycleDefinition &BuildingType::cycle() const
{
    return cycle_;
}

const Religion *BuildingType::religion() const
{
    return religion_;
}

const SoundDefinition &BuildingType::sound() const
{
    return sound_;
}

const MarketDefinition &BuildingType::market() const
{
    return market_;
}

const BuildingFlagsDefinition &BuildingType::flags() const
{
    return flags_;
}

const MilitaryDefinition &BuildingType::military() const
{
    return military_;
}

const WaterAccessDefinition &BuildingType::water_access() const
{
    return water_access_;
}

const BuildingGraphicsDef &BuildingType::graphics() const
{
    return graphics_;
}

const ConstructionDefinition &BuildingType::construction() const
{
    return construction_;
}

CompositionDef &BuildingType::composition()
{
    return composition_;
}

const CompositionDef &BuildingType::composition() const
{
    return composition_;
}

ImageGroupEntryRef BuildingType::button_icon_ref() const
{
    return has_button() ? button().icon_ref() : ImageGroupEntryRef();
}

const char *BuildingType::button_text_key() const
{
    const BuildButtonDefinition &primary_button = button();
    return has_button() && primary_button.has_text_key() ? primary_button.text_key() : nullptr;
}

int BuildingType::placement_width(int orientation) const
{
    if (has_composition()) {
        const CompositionLayoutResult layout = build_composition_layout(
            this, composition_, 0, 0, orientation);
        return layout.valid() ? layout.bounds.width() : 0;
    }
    return foundation_def_ ? foundation_def_->rotated_width(orientation) : 0;
}

int BuildingType::placement_height(int orientation) const
{
    if (has_composition()) {
        const CompositionLayoutResult layout = build_composition_layout(
            this, composition_, 0, 0, orientation);
        return layout.valid() ? layout.bounds.height() : 0;
    }
    return foundation_def_ ? foundation_def_->rotated_height(orientation) : 0;
}

figure_type BuildingType::preview_figure_type() const
{
    if (!has_housing()) {
        for (const SpawnDelayGroup &group : spawn_groups()) {
            for (const SpawnPolicy &policy : group.policies) {
                if (policy.spawn_figure != FIGURE_NONE) {
                    return policy.spawn_figure;
                }
            }
        }
    }
    if (has_temple()) {
        return FIGURE_PRIEST;
    }
    if (has_labor() && labor().has_seeker_policy() &&
        labor().seeker_policy().method != LaborSeekerMethod::None) {
        return FIGURE_LABOR_SEEKER;
    }
    return FIGURE_NONE;
}

int BuildingType::required_workers() const
{
    const model_building *model = model_get_building(type_);
    return model ? model->laborers : 0;
}

int BuildingType::is_temple(std::optional<god_type> god, ReligionTier tier) const
{
    const Religion *religion = religion_;
    if (!religion) {
        return 0;
    }
    if (god.has_value() && !religion->has_god(god.value())) {
        return 0;
    }
    return tier == ReligionTier::None || religion->is_tier(tier);
}

int BuildingType::is_theater() const
{
    return attr_ == "theater";
}

int BuildingType::is_hippodrome() const
{
    return attr_ == "hippodrome";
}

int BuildingType::is_well() const
{
    return attr_ == "well";
}

int BuildingType::is_fountain() const
{
    return attr_ == "fountain";
}

int BuildingType::is_latrines() const
{
    return attr_ == "latrines";
}

int BuildingType::is_warehouse() const
{
    return attr_ == "warehouse";
}

int BuildingType::is_granary() const
{
    return attr_ == "granary";
}

int BuildingType::is_storage() const
{
    return is_granary() || is_warehouse();
}

int BuildingType::is_plague_treatment_target() const
{
    return has_housing() || attr_is("dock") || is_storage();
}

int BuildingType::is_mess_hall() const
{
    return attr_ == "mess_hall";
}

int BuildingType::is_architect_guild() const
{
    return attr_ == "architect_guild";
}

int BuildingType::is_caravanserai() const
{
    return attr_ == "caravanserai";
}

int BuildingType::is_lighthouse() const
{
    return attr_ == "lighthouse";
}

int BuildingType::is_watchtower() const
{
    return attr_ == "watchtower";
}

int BuildingType::is_armoury() const
{
    return attr_ == "armoury";
}

const GraphicsTarget *BuildingType::resolve_graphics_target(const BuildingType *definition, const Building &building)
{
    if (!definition || !definition->has_graphic()) {
        return nullptr;
    }

#ifndef STARTUP_PARSER_TEST
    if (definition->has_phased_construction() &&
        building.monument_phase() != MONUMENT_FINISHED &&
        building.monument_phase() >= MONUMENT_START) {
        const ConstructionPhase *construction_phase = definition->construction_.phase(building.monument_phase());
        if (construction_phase && (
                construction_phase->graphics.has_path() ||
                construction_phase->graphics.has_options() ||
                construction_phase->graphics.is_resource_storage())) {
            return &construction_phase->graphics;
        }
    }
#endif

    return definition->graphics_.resolve_target(building);
}

int BuildingType::has_identity() const
{
    return identity_.has_name_key();
}

int BuildingType::has_model() const
{
    return model_.has_any();
}

int BuildingType::has_button() const
{
    for (const BuildButtonDefinition &button : buttons_) {
        if (button.has_any()) {
            return 1;
        }
    }
    return 0;
}

int BuildingType::has_rubble() const
{
    return rubble_.has_any();
}

int BuildingType::has_tile() const
{
    return tile_.has_any();
}

int BuildingType::has_cycle() const
{
    return cycle_.has_any();
}

int BuildingType::has_temple() const
{
    return !religion_reference_path_.empty();
}

int BuildingType::has_sound() const
{
    return sound_.has_any();
}

int BuildingType::has_market() const
{
    return market_.has_any();
}

int BuildingType::has_flags() const
{
    return flags_.has_any();
}

int BuildingType::has_military() const
{
    return military_.has_any();
}

int BuildingType::has_water_access_provider() const
{
    return water_access_.has_provider();
}

int BuildingType::has_graphic() const
{
    return graphics_.has_path();
}

int BuildingType::has_construction() const
{
    return has_construction_;
}

int BuildingType::has_composition() const
{
    return composition_.has_any();
}

int BuildingType::has_rotated_placement_geometry() const
{
    return (foundation_def_ && foundation_def_->rotates()) || has_composition();
}

int BuildingType::has_phased_construction() const
{
    return construction_.is_phased() && construction_.phase_count() > 0;
}

int BuildingType::has_labor() const
{
    return labor_.has_any();
}

const LaborDefinition &BuildingType::labor() const
{
    return labor_;
}

const std::vector<SpawnDelayGroup> &BuildingType::spawn_groups() const
{
    return spawn_groups_;
}

const std::vector<std::string> &BuildingType::storage_reference_paths() const
{
    return storage_reference_paths_;
}

const std::vector<std::string> &BuildingType::production_method_reference_paths() const
{
    return production_method_reference_paths_;
}

const std::string &BuildingType::distribution_reference_path() const
{
    return distribution_reference_path_;
}

const std::string &BuildingType::temple_religion_reference_path() const
{
    return religion_reference_path_;
}

const std::vector<BuildingCultureModule> &BuildingType::culture_modules() const
{
    return culture_modules_;
}

const std::vector<const StorageType *> &BuildingType::storage_types() const
{
    return storage_types_;
}

const std::vector<ProductionMethod *> &BuildingType::production_methods() const
{
    return production_methods_;
}

resource_type BuildingType::output_resource() const
{
    for (const ProductionMethod *method : production_methods_) {
        if (method && method->has_resource_output()) {
            return method->output_resource();
        }
    }
    if (!has_composition()) {
        return RESOURCE_NONE;
    }
    for (const CompositionChildDef &part : composition().children()) {
        const BuildingType *part_type = part.type;
        if (part_type) {
            resource_type resource = part_type->output_resource();
            if (resource != RESOURCE_NONE) {
                return resource;
            }
        }
    }
    return RESOURCE_NONE;
}

const Distribution *BuildingType::distribution() const
{
    return distribution_;
}

HousingDef &BuildingType::housing_def()
{
    return housing_def_;
}

const HousingDef &BuildingType::housing_def() const
{
    return housing_def_;
}

building_type BuildingType::vacant_lot_fill_type() const
{
    const BuildingType *target = housing_def_.transition(HousingTransitionKind::VacantLot).type;
    return target ? target->type() : BUILDING_NONE;
}

int BuildingType::has_native_storage() const
{
    return !storage_types_.empty();
}

int BuildingType::has_native_production() const
{
    return !production_methods_.empty();
}

int BuildingType::is_farm() const
{
    return farm_panel_production_method() ? 1 : 0;
}

const ProductionMethod *BuildingType::farm_production_method() const
{
    for (const ProductionMethod *method : production_methods_) {
        if (method && method->is_farm()) {
            return method;
        }
    }
    return nullptr;
}

const ProductionMethod *BuildingType::farm_panel_production_method() const
{
    if (const ProductionMethod *method = farm_production_method()) {
        return method;
    }
    if (!has_composition()) {
        return nullptr;
    }
    for (const CompositionChildDef &part : composition().children()) {
        const BuildingType *part_type = part.type;
        if (!part_type) {
            continue;
        }
        if (const ProductionMethod *method = part_type->farm_production_method()) {
            return method;
        }
    }
    return nullptr;
}

int BuildingType::has_farm_panel() const
{
    return farm_panel_production_method() ? 1 : 0;
}

int BuildingType::has_culture_modules() const
{
    for (const BuildingCultureModule &module : culture_modules_) {
        if (module.module) {
            return 1;
        }
    }
    return 0;
}

int BuildingType::has_distribution() const
{
    return distribution_ ? 1 : 0;
}

int BuildingType::has_housing() const
{
    return housing_def_.profile ? 1 : 0;
}

int BuildingType::is_vacant_lot() const
{
    return !housing_def_.transition(HousingTransitionKind::VacantLot).type_path.empty();
}

unsigned char BuildingType::upgrade_level_for(const Building &building) const
{
    return graphics_.upgrade_level_for(building);
}

} // namespace building_type_registry_impl
