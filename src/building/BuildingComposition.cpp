#include "building/BuildingComposition.h"

#include "building/building.h"
#include "core/log.h"

#include <set>

void BuildingComposition::bind_standalone(Building *building)
{
    clear();
    building_ = building;
}

void BuildingComposition::bind_owner(
    Building *building,
    const building_type_registry_impl::CompositionDef *definition)
{
    clear();
    building_ = building;
    definition_ = definition;
    if (definition_) {
        children_.resize(definition_->children().size(), nullptr);
    }
}

void BuildingComposition::clear()
{
    if (owner_ && definition_index_ < owner_->children_.size() &&
        owner_->children_[definition_index_] == this) {
        owner_->children_[definition_index_] = nullptr;
    }
    for (BuildingComposition *child : children_) {
        if (child && child->owner_ == this) {
            child->owner_ = nullptr;
            child->definition_index_ = no_definition_index;
        }
    }
    building_ = nullptr;
    owner_ = nullptr;
    definition_ = nullptr;
    definition_index_ = no_definition_index;
    children_.clear();
}

bool BuildingComposition::attach_children(
    const std::vector<BuildingComposition *> &children,
    std::string *error)
{
    if (!is_owner() || children.size() != children_.size()) {
        if (error) {
            *error = "composition child count does not match its definition";
        }
        return false;
    }

    std::set<const BuildingComposition *> unique_children;
    for (std::size_t index = 0; index < children.size(); ++index) {
        BuildingComposition *child = children[index];
        const building_type_registry_impl::CompositionChildDef &expected =
            definition_->children()[index];
        if (!child || !child->building_) {
            if (error) {
                *error = "composition child for role '" + expected.role + "' is missing";
            }
            return false;
        }
        if (child == this || child->building_ == building_) {
            if (error) {
                *error = "composition cannot contain its owner";
            }
            return false;
        }
        if (!unique_children.insert(child).second) {
            if (error) {
                *error = "composition child is assigned to more than one role";
            }
            return false;
        }
        if (child->is_owner()) {
            if (error) {
                *error = "nested compositions are not supported";
            }
            return false;
        }
        if (child->owner_ && child->owner_ != this) {
            if (error) {
                *error = "composition child already belongs to another owner";
            }
            return false;
        }
        if (!expected.type || child->building_->type != expected.type) {
            if (error) {
                *error = "composition child's building type does not match role '" + expected.role + "'";
            }
            return false;
        }
    }

    for (BuildingComposition *old_child : children_) {
        if (old_child && old_child->owner_ == this) {
            old_child->owner_ = nullptr;
            old_child->definition_index_ = no_definition_index;
        }
    }
    children_ = children;
    for (std::size_t index = 0; index < children_.size(); ++index) {
        children_[index]->owner_ = this;
        children_[index]->definition_index_ = index;
    }
    return true;
}

bool BuildingComposition::complete(std::string *error) const
{
    if (!is_owner()) {
        if (error) {
            *error = "building is not a composition owner";
        }
        return false;
    }
    for (std::size_t index = 0; index < children_.size(); ++index) {
        const BuildingComposition *child = children_[index];
        if (!child || child->owner_ != this || child->definition_index_ != index) {
            if (error) {
                *error = "composition is missing child role '" + definition_->children()[index].role + "'";
            }
            return false;
        }
        const building_type_registry_impl::CompositionChildDef &expected = definition_->children()[index];
        if (!child->building_ || !expected.type || child->building_->type != expected.type) {
            if (error) {
                *error = "composition child type no longer matches role '" + expected.role + "'";
            }
            return false;
        }
        if (child == this || child->building_ == building_ || child->definition_) {
            if (error) {
                *error = "composition contains its owner or a nested composition";
            }
            return false;
        }
    }
    return true;
}

bool BuildingComposition::is_composed() const
{
    return is_owner() || is_child();
}

bool BuildingComposition::is_owner() const
{
    return building_ && definition_ && definition_->has_any() && !owner_;
}

bool BuildingComposition::is_child() const
{
    return building_ && owner_;
}

Building *BuildingComposition::building() const
{
    return building_;
}

Building *BuildingComposition::owner() const
{
    return owner_ ? owner_->building_ : building_;
}

BuildingComposition *BuildingComposition::owner_module() const
{
    return owner_ ? owner_ : const_cast<BuildingComposition *>(this);
}

const building_type_registry_impl::CompositionDef *BuildingComposition::definition() const
{
    return owner_ ? owner_->definition_ : definition_;
}

const building_type_registry_impl::CompositionChildDef *BuildingComposition::child_definition() const
{
    const building_type_registry_impl::CompositionDef *composition = definition();
    if (!is_child() || !composition || definition_index_ >= composition->children().size()) {
        return nullptr;
    }
    return &composition->children()[definition_index_];
}

std::size_t BuildingComposition::definition_index() const
{
    return definition_index_;
}

const std::vector<BuildingComposition *> &BuildingComposition::children() const
{
    return owner_ ? owner_->children_ : children_;
}

void BuildingComposition::for_each_member(const std::function<void(Building &)> &visitor) const
{
    const BuildingComposition *composition = owner_ ? owner_ : this;
    if (!visitor || !composition->building_) {
        return;
    }
    if (composition->is_owner()) {
        std::string error;
        if (!composition->complete(&error)) {
            log_error("Cannot iterate incomplete BuildingComposition", error.c_str(), composition->building_->id);
            return;
        }
    }
    visitor(*composition->building_);
    for (BuildingComposition *child : composition->children_) {
        if (child && child->building_) {
            visitor(*child->building_);
        }
    }
}
