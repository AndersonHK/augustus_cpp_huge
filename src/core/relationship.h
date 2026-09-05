#ifndef CORE_RELATIONSHIP_H
#define CORE_RELATIONSHIP_H

#include <algorithm>
#include <vector>

class Relationship;
class RelationshipEndpoint;

enum class RelationshipEventType {
    Connected,
    Disconnected,
};

enum class RelationshipDisconnectReason {
    Explicit,
    Retargeted,
    EndpointRemoved,
    EndpointReassigned,
    EndpointDestroyed,
};

struct RelationshipEvent {
    RelationshipEventType type;
    RelationshipDisconnectReason reason;
    Relationship &relationship;
    RelationshipEndpoint *other;
};

class Relationship {
public:
    virtual ~Relationship() = default;
    virtual RelationshipEndpoint *other_endpoint(const RelationshipEndpoint &endpoint) const = 0;
    virtual const char *role() const = 0;

private:
    friend class RelationshipEndpoint;
    virtual void disconnect_endpoint(RelationshipEndpoint &endpoint, RelationshipDisconnectReason reason) = 0;
};

class RelationshipEndpoint {
public:
    RelationshipEndpoint() = default;
    RelationshipEndpoint(const RelationshipEndpoint &) {}
    RelationshipEndpoint &operator=(const RelationshipEndpoint &)
    {
        disconnect_relationships(RelationshipDisconnectReason::EndpointReassigned);
        return *this;
    }
    virtual ~RelationshipEndpoint() { disconnect_relationships(RelationshipDisconnectReason::EndpointDestroyed); }

    const std::vector<Relationship *> &relationships() const { return relationships_; }

protected:
    void disconnect_relationships(RelationshipDisconnectReason reason)
    {
        while (!relationships_.empty()) {
            relationships_.back()->disconnect_endpoint(*this, reason);
        }
    }

    virtual void on_relationship_event(const RelationshipEvent &) {}

private:
    template<typename Left, typename Right>
    friend class ObjectRelationship;

    void attach_relationship(Relationship &relationship)
    {
        if (std::find(relationships_.begin(), relationships_.end(), &relationship) == relationships_.end()) {
            relationships_.push_back(&relationship);
        }
    }

    void detach_relationship(Relationship &relationship)
    {
        relationships_.erase(std::remove(relationships_.begin(), relationships_.end(), &relationship), relationships_.end());
    }

    void send_relationship_event(const RelationshipEvent &event) { on_relationship_event(event); }

    std::vector<Relationship *> relationships_;
};

template<typename Left, typename Right>
class ObjectRelationship final : public Relationship {
    friend Left;

public:
    explicit ObjectRelationship(const char *role) : role_(role) {}
    ObjectRelationship(const ObjectRelationship &other) : role_(other.role_) {}
    ObjectRelationship &operator=(const ObjectRelationship &other)
    {
        if (this != &other) {
            disconnect(RelationshipDisconnectReason::EndpointReassigned, nullptr);
            role_ = other.role_;
        }
        return *this;
    }
    ~ObjectRelationship() override { disconnect(RelationshipDisconnectReason::EndpointDestroyed, left_); }

    Right *get_ptr() { return right_; }
    Right *get_ptr() const { return right_; }
    Right &get() { return *right_; }
    Right &get() const { return *right_; }
    Right *operator->() { return right_; }
    Right *operator->() const { return right_; }
    Right &operator*() { return *right_; }
    Right &operator*() const { return *right_; }
    operator Right *() { return right_; }
    operator Right *() const { return right_; }
    explicit operator bool() const { return right_ != nullptr; }

    RelationshipEndpoint *other_endpoint(const RelationshipEndpoint &endpoint) const override
    {
        if (&endpoint == left_) {
            return right_;
        }
        return &endpoint == right_ ? left_ : nullptr;
    }

    const char *role() const override { return role_; }

public:
    void retarget(Left &left, Right *right)
    {
        if (left_ == &left && right_ == right) {
            return;
        }
        disconnect(RelationshipDisconnectReason::Retargeted, &left);
        if (!right) {
            return;
        }
        left_ = &left;
        right_ = right;
        left_->attach_relationship(*this);
        right_->attach_relationship(*this);
        left_->send_relationship_event({ RelationshipEventType::Connected, RelationshipDisconnectReason::Explicit, *this, right_ });
        right_->send_relationship_event({ RelationshipEventType::Connected, RelationshipDisconnectReason::Explicit, *this, left_ });
    }

    void clear(Left &left, RelationshipDisconnectReason reason = RelationshipDisconnectReason::Explicit)
    {
        if (left_ && left_ != &left) {
            return;
        }
        disconnect(reason, &left);
    }

private:
    void disconnect_endpoint(RelationshipEndpoint &endpoint, RelationshipDisconnectReason reason) override
    {
        if (&endpoint != left_ && &endpoint != right_) {
            return;
        }
        disconnect(reason, &endpoint);
    }

    void disconnect(RelationshipDisconnectReason reason, RelationshipEndpoint *initiator)
    {
        Left *left = left_;
        Right *right = right_;
        if (!left && !right) {
            return;
        }
        if (left) {
            left->detach_relationship(*this);
        }
        if (right) {
            right->detach_relationship(*this);
        }
        left_ = nullptr;
        right_ = nullptr;
        if (left && left != initiator) {
            left->send_relationship_event({ RelationshipEventType::Disconnected, reason, *this, right });
        }
        if (right && right != initiator) {
            right->send_relationship_event({ RelationshipEventType::Disconnected, reason, *this, left });
        }
    }

    const char *role_;
    Left *left_ = nullptr;
    Right *right_ = nullptr;
};

#endif
