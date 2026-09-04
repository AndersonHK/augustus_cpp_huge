#pragma once

#include "core/relationship.h"
#include "figure/formation_layout.h"
#include "figure/movement.h"

#include <functional>
#include <vector>

enum class FormationMemberDestination {
    MusteringGround,
    Standard
};

enum class FormationStationState {
    Ideal,
    Fallback,
    Unplaced
};

enum class FormationMemberMovementResult {
    Moving,
    Stationed,
    SettledUnplaced
};

struct FormationMemberStationRequest {
    int slot = -1;
    RelationshipEndpoint *member = nullptr;
};

struct FormationMovementBounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
};

class FormationMemberMovementPlan {
public:
    using Reachability = std::function<int(int, const FigureMovementDestination &)>;

    void invalidate() { valid_ = false; }
    bool matches(FormationMemberDestination kind, const FormationLayoutDef &layout,
        int origin_x, int origin_y, int area_width, int area_height, int station_count) const;
    bool configure(FormationMemberDestination kind, const FormationLayoutDef &layout,
        int origin_x, int origin_y, int area_width, int area_height,
        const FormationMovementBounds &bounds, const std::vector<FigureMovementDestination> &ideals);
    bool assign_members(const std::vector<FormationMemberStationRequest> &requests, const Reachability &reachable);
    bool assign_member(int slot, RelationshipEndpoint &member, const Reachability &reachable);
    void release_member(int slot, const RelationshipEndpoint &member);
    void release_slot(int slot);
    void mark_unplaced(int slot, const RelationshipEndpoint &member);
    bool resolve_member(int slot, const RelationshipEndpoint &member, FigureMovementDestination *destination,
        FormationStationState *state) const;

private:
    struct Station {
        FigureMovementDestination ideal;
        FigureMovementDestination destination;
        FormationStationState state = FormationStationState::Unplaced;
        RelationshipEndpoint *occupant = nullptr;
    };

    struct Candidate {
        FigureMovementDestination destination;
        long long displacement_squared = 0;
        int route_distance = 0;
    };

    static bool same_point(const FigureMovementDestination &left, const FigureMovementDestination &right)
    {
        return left.x == right.x && left.y == right.y && left.plane == right.plane;
    }
    bool point_is_reserved(int slot, const FigureMovementDestination &candidate) const;
    int calculate_fallback_step(bool horizontal) const;
    void find_nearest_fallback_candidates(int slot, std::vector<Candidate> *candidates, const Reachability &reachable) const;

    FormationMemberDestination kind_ = FormationMemberDestination::MusteringGround;
    const FormationLayoutDef *layout_ = nullptr;
    int origin_x_ = 0;
    int origin_y_ = 0;
    int area_width_ = 0;
    int area_height_ = 0;
    FormationMovementBounds bounds_;
    int fallback_step_x_ = 1;
    int fallback_step_y_ = 1;
    bool valid_ = false;
    std::vector<Station> stations_;
};
