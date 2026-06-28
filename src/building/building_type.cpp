#include "figure/figure.h"
#include "building/building_record.h"
#include "building/building.h"
#include "building/building_type.h"
#include "building/building_type_registry_internal.h"

#include "building/connectable.h"
#include "building/housing_type.h"
#include "building/industry.h"
#include "building/production_method.h"
#include "building/religion.h"
#include "figure/formation_type.h"

#include "building/monument.h"
#include "building/properties.h"
#include "city/constants.h"

#include <initializer_list>
#include <utility>

namespace building_type_registry_impl {

void IdentityDefinition::set_name_key(std::string key)
{
    name_key_ = std::move(key);
}

int IdentityDefinition::has_name_key() const
{
    return !name_key_.empty();
}

const char *IdentityDefinition::name_key() const
{
    return name_key_.c_str();
}

void BuildModelDefinition::set_size(int value)
{
    has_size_ = 1;
    size_ = value;
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

int BuildModelDefinition::has_size() const
{
    return has_size_;
}

int BuildModelDefinition::size() const
{
    return size_;
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
    return has_size_ || has_cost_ || has_hit_points_ || has_desirability_value_ || has_desirability_step_ ||
        has_desirability_step_size_ || has_desirability_range_;
}

void FoundationDefinition::set_policy(
    std::string policy,
    FoundationPolicy type,
    FoundationCellRequirement requirement)
{
    policy_ = std::move(policy);
    policy_type_ = type;
    policy_requirement_ = requirement;
}

void FoundationDefinition::set_requires_open_water(int value)
{
    requires_open_water_ = value ? 1 : 0;
}

void FoundationDefinition::add_required_terrain(int flags)
{
    required_terrain_ |= flags;
}

void FoundationDefinition::add_cell(int x, int y, int rotation, FoundationCellRequirement requirement)
{
    FoundationCellDefinition cell;
    cell.x = x;
    cell.y = y;
    cell.rotation = rotation;
    cell.requirement = requirement;
    cells_.push_back(cell);
}

int FoundationDefinition::has_policy() const
{
    return !policy_.empty();
}

const char *FoundationDefinition::policy() const
{
    return policy_.c_str();
}

FoundationPolicy FoundationDefinition::policy_type() const
{
    return policy_type_;
}

FoundationCellRequirement FoundationDefinition::policy_requirement() const
{
    return policy_requirement_;
}

int FoundationDefinition::requires_open_water() const
{
    return requires_open_water_;
}

int FoundationDefinition::required_terrain() const
{
    return required_terrain_;
}

int FoundationDefinition::has_cells() const
{
    return !cells_.empty();
}

int FoundationDefinition::has_water_requirement() const
{
    if (policy_type_ == FoundationPolicy::Shoreline || policy_type_ == FoundationPolicy::Water ||
        policy_requirement_ == FoundationCellRequirement::Water) {
        return 1;
    }
    for (const FoundationCellDefinition &cell : cells_) {
        if (cell.requirement == FoundationCellRequirement::Water) {
            return 1;
        }
    }
    return 0;
}

const std::vector<FoundationCellDefinition> &FoundationDefinition::cells() const
{
    return cells_;
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

void RoadblockDefinition::set_kind(RoadblockKind kind)
{
    kind_ = kind;
    if (kind != RoadblockKind::Bridge) {
        bridge_type_ = RoadblockBridgeType::None;
    }
}

void RoadblockDefinition::set_bridge_type(RoadblockBridgeType type)
{
    bridge_type_ = type;
}

void RoadblockDefinition::set_passage_type(RoadblockPassageType type)
{
    passage_type_ = type;
}

RoadblockKind RoadblockDefinition::kind() const
{
    return kind_;
}

RoadblockBridgeType RoadblockDefinition::bridge_type() const
{
    return bridge_type_;
}

RoadblockPassageType RoadblockDefinition::passage_type() const
{
    return passage_type_;
}

int RoadblockDefinition::is_bridge() const
{
    return kind_ == RoadblockKind::Bridge;
}

int RoadblockDefinition::is_ship_bridge() const
{
    return bridge_type_ == RoadblockBridgeType::Ship;
}

int RoadblockDefinition::is_wall_gate() const
{
    return passage_type_ == RoadblockPassageType::WallGate;
}

int RoadblockDefinition::has_center_road_passage() const
{
    return passage_type_ == RoadblockPassageType::CenterRoad;
}

int RoadblockDefinition::has_any() const
{
    return kind_ != RoadblockKind::None;
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

void EventDataDefinition::set_attr(std::string attr)
{
    attr_ = std::move(attr);
}

int EventDataDefinition::has_attr() const
{
    return !attr_.empty();
}

const char *EventDataDefinition::attr() const
{
    return attr_.c_str();
}

int EventDataDefinition::has_any() const
{
    return has_attr();
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

ComposedPartOffset ComposedPartDefinition::offset_for_rotation(int rotation) const
{
    int normalized_rotation = rotation % 4;
    if (normalized_rotation < 0) {
        normalized_rotation += 4;
    }
    if (offsets[normalized_rotation].has_value) {
        return offsets[normalized_rotation];
    }
    for (const ComposedPartOffset &offset : offsets) {
        if (offset.has_value) {
            return offset;
        }
    }
    return ComposedPartOffset();
}

void ComposedBuildingDefinition::set_footprint(int width, int height)
{
    footprint_width_ = width;
    footprint_height_ = height;
}

void ComposedBuildingDefinition::set_child_inherits_orientation(int value)
{
    child_inherits_orientation_ = value ? 1 : 0;
}

void ComposedBuildingDefinition::set_main_offset(int rotation, int x, int y)
{
    int normalized_rotation = rotation % 4;
    if (normalized_rotation < 0) {
        normalized_rotation += 4;
    }
    main_offsets_[normalized_rotation] = { x, y, 1 };
}

ComposedPartDefinition &ComposedBuildingDefinition::add_part(std::string type_attr, std::string role)
{
    parts_.push_back(ComposedPartDefinition());
    parts_.back().type_attr = std::move(type_attr);
    parts_.back().role = std::move(role);
    return parts_.back();
}

int ComposedBuildingDefinition::footprint_width() const
{
    return footprint_width_;
}

int ComposedBuildingDefinition::footprint_height() const
{
    return footprint_height_;
}

int ComposedBuildingDefinition::child_inherits_orientation() const
{
    return child_inherits_orientation_;
}

ComposedPartOffset ComposedBuildingDefinition::main_offset_for_rotation(int rotation) const
{
    int normalized_rotation = rotation % 4;
    if (normalized_rotation < 0) {
        normalized_rotation += 4;
    }
    if (main_offsets_[normalized_rotation].has_value) {
        return main_offsets_[normalized_rotation];
    }
    for (const ComposedPartOffset &offset : main_offsets_) {
        if (offset.has_value) {
            return offset;
        }
    }
    return ComposedPartOffset{ 0, 0, 1 };
}

const std::vector<ComposedPartDefinition> &ComposedBuildingDefinition::parts() const
{
    return parts_;
}

std::vector<ComposedPartDefinition> &ComposedBuildingDefinition::parts()
{
    return parts_;
}

int ComposedBuildingDefinition::has_any() const
{
    return footprint_width_ > 0 && footprint_height_ > 0 && !parts_.empty();
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

void BuildingType::set_identity_name_key(std::string key)
{
    identity_.set_name_key(std::move(key));
}

void BuildingType::set_model_size(int value)
{
    model_.set_size(value);
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

void BuildingType::set_foundation_policy(
    std::string policy,
    FoundationPolicy type,
    FoundationCellRequirement requirement)
{
    foundation_.set_policy(std::move(policy), type, requirement);
}

void BuildingType::set_foundation_requires_open_water(int value)
{
    foundation_.set_requires_open_water(value);
}

void BuildingType::add_foundation_required_terrain(int flags)
{
    foundation_.add_required_terrain(flags);
}

void BuildingType::add_foundation_cell(int x, int y, int rotation, FoundationCellRequirement requirement)
{
    foundation_.add_cell(x, y, rotation, requirement);
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

void BuildingType::set_roadblock_kind(RoadblockKind kind)
{
    roadblock_.set_kind(kind);
}

void BuildingType::set_roadblock_bridge_type(RoadblockBridgeType type)
{
    roadblock_.set_bridge_type(type);
}

void BuildingType::set_roadblock_passage_type(RoadblockPassageType type)
{
    roadblock_.set_passage_type(type);
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

void BuildingType::set_event_attr(std::string attr)
{
    event_data_.set_attr(std::move(attr));
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

void BuildingType::set_composed_footprint(int width, int height)
{
    composition_.set_footprint(width, height);
}

void BuildingType::set_composed_child_inherits_orientation(int value)
{
    composition_.set_child_inherits_orientation(value);
}

void BuildingType::set_composed_main_offset(int rotation, int x, int y)
{
    composition_.set_main_offset(rotation, x, y);
}

ComposedPartDefinition &BuildingType::add_composed_part(std::string type_attr, std::string role)
{
    return composition_.add_part(std::move(type_attr), std::move(role));
}

void BuildingType::set_composed_part_type(size_t index, building_type type)
{
    std::vector<ComposedPartDefinition> &parts = composition_.parts();
    if (index < parts.size()) {
        parts[index].type = type;
    }
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

void BuildingType::set_housing_reference(std::string path)
{
    housing_reference_path_ = std::move(path);
}

void BuildingType::set_housing_capacity(int capacity)
{
    housing_capacity_ = capacity;
}

void BuildingType::set_housing_transition(HousingTransitionKind kind, std::string text_id)
{
    switch (kind) {
        case HousingTransitionKind::EvolveTo:
            housing_evolve_to_ = std::move(text_id);
            break;
        case HousingTransitionKind::DevolveTo:
            housing_devolve_to_ = std::move(text_id);
            break;
        case HousingTransitionKind::MergeTo:
            housing_merge_to_ = std::move(text_id);
            break;
        case HousingTransitionKind::SplitTo:
            housing_split_to_ = std::move(text_id);
            break;
    }
}

void BuildingType::set_vacant_lot_fill_reference(std::string text_id)
{
    vacant_lot_fill_to_ = std::move(text_id);
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

void BuildingType::set_housing_type(const HousingType *housing_type)
{
    housing_type_ = housing_type;
}

void BuildingType::set_temple_religion(const Religion *religion)
{
    religion_ = religion;
}

void BuildingType::set_housing_transition_type(HousingTransitionKind kind, building_type type)
{
    switch (kind) {
        case HousingTransitionKind::EvolveTo:
            housing_evolve_to_type_ = type;
            break;
        case HousingTransitionKind::DevolveTo:
            housing_devolve_to_type_ = type;
            break;
        case HousingTransitionKind::MergeTo:
            housing_merge_to_type_ = type;
            break;
        case HousingTransitionKind::SplitTo:
            housing_split_to_type_ = type;
            break;
    }
}

void BuildingType::set_vacant_lot_fill_type(building_type type)
{
    vacant_lot_fill_type_ = type;
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

const IdentityDefinition &BuildingType::identity() const
{
    return identity_;
}

const BuildModelDefinition &BuildingType::model() const
{
    return model_;
}

const FoundationDefinition &BuildingType::foundation() const
{
    return foundation_;
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

const RoadblockDefinition &BuildingType::roadblock() const
{
    return roadblock_;
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

const EventDataDefinition &BuildingType::event_data() const
{
    return event_data_;
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

const ComposedBuildingDefinition &BuildingType::composition() const
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

int BuildingType::declared_model_size() const
{
    return has_model() && model().has_size() ? model().size() : 0;
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

int BuildingType::has_data_only_graphics() const
{
    return 0;
}

int BuildingType::is_temple() const
{
    return religion_ ? 1 : 0;
}

int BuildingType::is_temple(god_type god, ReligionTier tier) const
{
    const Religion *religion = religion_;
    if (!religion) {
        return 0;
    }
    if (god != GOD_ALL && !religion->has_god(god)) {
        return 0;
    }
    return tier == ReligionTier::None || religion->is_tier(tier);
}

int BuildingType::is_temple_for_god(god_type god) const
{
    return is_temple(god, ReligionTier::None);
}

int BuildingType::is_temple_tier(ReligionTier tier) const
{
    return is_temple(GOD_ALL, tier);
}

int BuildingType::is_ceres_temple() const
{
    return is_temple_for_god(GOD_CERES);
}

int BuildingType::is_venus_temple() const
{
    return is_temple_for_god(GOD_VENUS);
}

int BuildingType::is_mars_temple() const
{
    return is_temple_for_god(GOD_MARS);
}

int BuildingType::is_mercury_temple() const
{
    return is_temple_for_god(GOD_MERCURY);
}

int BuildingType::is_neptune_temple() const
{
    return is_temple_for_god(GOD_NEPTUNE);
}

int BuildingType::is_pantheon() const
{
    return religion_ && religion_->is_tier(ReligionTier::Grand) && religion_->has_all_gods();
}

int BuildingType::is_oracle() const
{
    return is_temple_tier(ReligionTier::Oracle);
}

int BuildingType::is_grand_temple_mars() const
{
    return is_temple(GOD_MARS, ReligionTier::Grand);
}

int BuildingType::is_grand_temple_venus() const
{
    return is_temple(GOD_VENUS, ReligionTier::Grand);
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

const GraphicsTarget *BuildingType::resolve_graphics_target_for_image(const BuildingType *definition, const Building &building)
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

int BuildingType::has_foundation() const
{
    return foundation_.has_policy() || foundation_.requires_open_water() ||
        foundation_.required_terrain() != 0 || foundation_.has_cells();
}

int BuildingType::foundation_required_terrain() const
{
    return foundation_.required_terrain();
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

int BuildingType::has_roadblock() const
{
    return roadblock_.has_any();
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

int BuildingType::has_event_data() const
{
    return event_data_.has_any();
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

const std::string &BuildingType::housing_reference_path() const
{
    return housing_reference_path_;
}

const std::string &BuildingType::vacant_lot_fill_reference() const
{
    return vacant_lot_fill_to_;
}

const std::string &BuildingType::temple_religion_reference_path() const
{
    return religion_reference_path_;
}

int BuildingType::housing_capacity() const
{
    return housing_capacity_;
}

const std::string &BuildingType::housing_transition_reference(HousingTransitionKind kind) const
{
    switch (kind) {
        case HousingTransitionKind::EvolveTo:
            return housing_evolve_to_;
        case HousingTransitionKind::DevolveTo:
            return housing_devolve_to_;
        case HousingTransitionKind::MergeTo:
            return housing_merge_to_;
        case HousingTransitionKind::SplitTo:
            return housing_split_to_;
    }
    return housing_evolve_to_;
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

const Distribution *BuildingType::distribution() const
{
    return distribution_;
}

const HousingType *BuildingType::housing_type() const
{
    return housing_type_;
}

building_type BuildingType::housing_transition_type(HousingTransitionKind kind) const
{
    switch (kind) {
        case HousingTransitionKind::EvolveTo:
            return housing_evolve_to_type_;
        case HousingTransitionKind::DevolveTo:
            return housing_devolve_to_type_;
        case HousingTransitionKind::MergeTo:
            return housing_merge_to_type_;
        case HousingTransitionKind::SplitTo:
            return housing_split_to_type_;
    }
    return BUILDING_NONE;
}

building_type BuildingType::vacant_lot_fill_type() const
{
    return vacant_lot_fill_type_;
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
    return farm_production_method() ? 1 : 0;
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
    for (const ComposedPartDefinition &part : composition().parts()) {
        const BuildingType *part_type = building_type_registry_impl::definition_for_type(part.type);
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
    return housing_type_ ? 1 : 0;
}

int BuildingType::housing_level() const
{
    return housing_type_ ? housing_type_->level() : -1;
}

int BuildingType::is_vacant_lot() const
{
    return !vacant_lot_fill_to_.empty();
}

unsigned char BuildingType::upgrade_level_for(const Building &building) const
{
    return graphics_.upgrade_level_for(building);
}

} // namespace building_type_registry_impl
