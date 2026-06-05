#include "building/building_type.h"

#include "building/housing_type.h"

extern "C" {
#include "building/monument.h"
}

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
    return has_size_ || has_cost_ || has_desirability_value_ || has_desirability_step_ ||
        has_desirability_step_size_ || has_desirability_range_;
}

void FoundationDefinition::set_policy(std::string policy)
{
    policy_ = std::move(policy);
}

void FoundationDefinition::add_required_terrain(int flags)
{
    required_terrain_ |= flags;
}

int FoundationDefinition::has_policy() const
{
    return !policy_.empty();
}

const char *FoundationDefinition::policy() const
{
    return policy_.c_str();
}

int FoundationDefinition::required_terrain() const
{
    return required_terrain_;
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
    return has_group() || has_order() || has_icon() || has_text_key();
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

int ConstructionDefinition::phase_count() const
{
    return static_cast<int>(phases_.size());
}

int ConstructionDefinition::instant_requirement_amount(resource_type resource) const
{
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

BuildingType::BuildingType(building_type type, std::string attr)
    : type_(type)
    , attr_(std::move(attr))
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

void BuildingType::set_foundation_policy(std::string policy)
{
    foundation_.set_policy(std::move(policy));
}

void BuildingType::add_foundation_required_terrain(int flags)
{
    foundation_.add_required_terrain(flags);
}

void BuildingType::set_button_group(std::string group)
{
    button_.set_group(std::move(group));
}

void BuildingType::set_button_order(int order)
{
    button_.set_order(order);
}

void BuildingType::set_button_icon(std::string icon)
{
    button_.set_icon(std::move(icon));
}

void BuildingType::set_button_text_key(std::string key)
{
    button_.set_text_key(std::move(key));
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
    graphics_ = GraphicsDefinition();
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

void BuildingType::add_storage_reference(std::string path)
{
    storage_reference_paths_.push_back(std::move(path));
}

void BuildingType::add_production_method_reference(std::string path)
{
    production_method_reference_paths_.push_back(std::move(path));
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

void BuildingType::add_storage_type(const StorageType *storage_type)
{
    storage_types_.push_back(storage_type);
}

void BuildingType::add_production_method(const ProductionMethod *production_method)
{
    production_methods_.push_back(production_method);
}

void BuildingType::set_housing_type(const HousingType *housing_type)
{
    housing_type_ = housing_type;
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
    return button_;
}

const SoundDefinition &BuildingType::sound() const
{
    return sound_;
}

const EventDataDefinition &BuildingType::event_data() const
{
    return event_data_;
}

const BuildingFlagsDefinition &BuildingType::flags() const
{
    return flags_;
}

const WaterAccessDefinition &BuildingType::water_access() const
{
    return water_access_;
}

const GraphicsDefinition &BuildingType::graphics() const
{
    return graphics_;
}

const ConstructionDefinition &BuildingType::construction() const
{
    return construction_;
}

const GraphicsTarget *BuildingType::resolve_graphics_target(const ::building &building) const
{
    return graphics_.resolve_target(building);
}

const GraphicsTarget *BuildingType::resolve_construction_graphics_target(int phase) const
{
    const ConstructionPhase *construction_phase = construction_.phase(phase);
    return construction_phase && construction_phase->graphics.has_path() ? &construction_phase->graphics : nullptr;
}

const GraphicsTarget *BuildingType::resolve_graphics_target_for_image(const BuildingType *definition, const ::building &building)
{
    if (!definition || !definition->has_graphic()) {
        return nullptr;
    }

    if (definition->has_phased_construction() &&
        building.monument.phase != MONUMENT_FINISHED &&
        building.monument.phase >= MONUMENT_START) {
        if (const GraphicsTarget *target = definition->resolve_construction_graphics_target(building.monument.phase)) {
            return target;
        }
    }

    return definition->resolve_graphics_target(building);
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
    return foundation_.has_policy() || foundation_.required_terrain() != 0;
}

int BuildingType::foundation_required_terrain() const
{
    return foundation_.required_terrain();
}

int BuildingType::has_button() const
{
    return button_.has_any();
}

int BuildingType::has_sound() const
{
    return sound_.has_any();
}

int BuildingType::has_event_data() const
{
    return event_data_.has_any();
}

int BuildingType::has_flags() const
{
    return flags_.has_any();
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

const std::string &BuildingType::housing_reference_path() const
{
    return housing_reference_path_;
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

const std::vector<const StorageType *> &BuildingType::storage_types() const
{
    return storage_types_;
}

const std::vector<const ProductionMethod *> &BuildingType::production_methods() const
{
    return production_methods_;
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

int BuildingType::has_native_storage() const
{
    return !storage_types_.empty();
}

int BuildingType::has_native_production() const
{
    return !production_methods_.empty();
}

int BuildingType::has_housing() const
{
    return housing_type_ ? 1 : 0;
}

unsigned char BuildingType::upgrade_level_for(const ::building &building) const
{
    return graphics_.upgrade_level_for(building);
}

} // namespace building_type_registry_impl
