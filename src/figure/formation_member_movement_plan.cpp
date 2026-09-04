#include "figure/formation_member_movement_plan.h"

#include <algorithm>
#include <cstdlib>
#include <limits>

bool FormationMemberMovementPlan::matches(
    FormationMemberDestination kind,
    const FormationLayoutDef &layout,
    int origin_x,
    int origin_y,
    int area_width,
    int area_height,
    int station_count) const
{
    return valid_ && kind_ == kind && layout_ == &layout && origin_x_ == origin_x && origin_y_ == origin_y &&
        area_width_ == area_width && area_height_ == area_height && static_cast<int>(stations_.size()) == station_count;
}

bool FormationMemberMovementPlan::configure(
    FormationMemberDestination kind,
    const FormationLayoutDef &layout,
    int origin_x,
    int origin_y,
    int area_width,
    int area_height,
    const FormationMovementBounds &bounds,
    const std::vector<FigureMovementDestination> &ideals)
{
    if (ideals.empty() || area_width <= 0 || area_height <= 0 ||
        bounds.min_x > bounds.max_x || bounds.min_y > bounds.max_y) {
        return false;
    }

    kind_ = kind;
    layout_ = &layout;
    origin_x_ = origin_x;
    origin_y_ = origin_y;
    area_width_ = area_width;
    area_height_ = area_height;
    bounds_ = bounds;
    stations_.assign(ideals.size(), {});
    for (size_t slot = 0; slot < ideals.size(); slot++) {
        Station &station = stations_[slot];
        station.ideal = ideals[slot];
        station.destination = station.ideal;
    }
    fallback_step_x_ = calculate_fallback_step(true);
    fallback_step_y_ = calculate_fallback_step(false);
    valid_ = true;
    return true;
}

bool FormationMemberMovementPlan::point_is_reserved(int slot, const FigureMovementDestination &candidate) const
{
    const bool candidate_is_own_ideal = same_point(candidate, stations_[static_cast<size_t>(slot)].ideal);
    for (int candidate_slot = 0; candidate_slot < static_cast<int>(stations_.size()); candidate_slot++) {
        const Station &station = stations_[static_cast<size_t>(candidate_slot)];
        if (candidate_slot != slot && same_point(candidate, station.ideal) &&
            (!candidate_is_own_ideal || candidate_slot < slot)) {
            return true;
        }
        if (candidate_slot != slot && station.occupant && station.state != FormationStationState::Unplaced &&
            same_point(candidate, station.destination)) {
            return true;
        }
    }
    return false;
}

int FormationMemberMovementPlan::calculate_fallback_step(bool horizontal) const
{
    int smallest_pitch = std::numeric_limits<int>::max();
    for (size_t first = 0; first < stations_.size(); first++) {
        const int first_coordinate = horizontal ? stations_[first].ideal.x : stations_[first].ideal.y;
        for (size_t second = first + 1; second < stations_.size(); second++) {
            const int second_coordinate = horizontal ? stations_[second].ideal.x : stations_[second].ideal.y;
            const int distance = std::abs(first_coordinate - second_coordinate);
            if (distance > 0) smallest_pitch = std::min(smallest_pitch, distance);
        }
    }
    if (smallest_pitch == std::numeric_limits<int>::max()) smallest_pitch = FIGURE_CROSS_COUNTRY_TILE_UNITS;
    return std::max(1, smallest_pitch / 2);
}

void FormationMemberMovementPlan::find_nearest_fallback_candidates(
    int slot,
    std::vector<Candidate> *candidates,
    const Reachability &reachable) const
{
    const FigureMovementDestination ideal = stations_[static_cast<size_t>(slot)].ideal;
    const auto append_candidate = [this, slot, &ideal, candidates, &reachable](
        const FigureMovementDestination &candidate) {
        if (candidate.x < bounds_.min_x || candidate.x > bounds_.max_x ||
            candidate.y < bounds_.min_y || candidate.y > bounds_.max_y ||
            same_point(candidate, ideal) || point_is_reserved(slot, candidate)) {
            return;
        }
        if (std::any_of(candidates->begin(), candidates->end(), [&candidate](const Candidate &other) {
            return same_point(candidate, other.destination);
        })) {
            return;
        }
        const int route_distance = reachable(slot, candidate);
        if (route_distance <= 0) return;
        const long long exact_dx = static_cast<long long>(candidate.x) - ideal.x;
        const long long exact_dy = static_cast<long long>(candidate.y) - ideal.y;
        candidates->push_back({candidate, exact_dx * exact_dx + exact_dy * exact_dy, route_distance});
    };

    const int horizontal_rings = std::max(
        std::abs(bounds_.min_x - ideal.x), std::abs(bounds_.max_x - ideal.x)) / fallback_step_x_ + 2;
    const int vertical_rings = std::max(
        std::abs(bounds_.min_y - ideal.y), std::abs(bounds_.max_y - ideal.y)) / fallback_step_y_ + 2;
    const int maximum_ring = std::max(horizontal_rings, vertical_rings);
    for (int ring = 1; ring <= maximum_ring; ring++) {
        for (int dx = -ring; dx <= ring; dx++) {
            append_candidate({ideal.x + dx * fallback_step_x_, ideal.y - ring * fallback_step_y_, ideal.plane});
            append_candidate({ideal.x + dx * fallback_step_x_, ideal.y + ring * fallback_step_y_, ideal.plane});
        }
        for (int dy = -ring + 1; dy < ring; dy++) {
            append_candidate({ideal.x - ring * fallback_step_x_, ideal.y + dy * fallback_step_y_, ideal.plane});
            append_candidate({ideal.x + ring * fallback_step_x_, ideal.y + dy * fallback_step_y_, ideal.plane});
        }
        if (!candidates->empty()) {
            const auto best = std::min_element(candidates->begin(), candidates->end(),
                [](const Candidate &left, const Candidate &right) {
                    return left.displacement_squared < right.displacement_squared;
                });
            const long long next_x = static_cast<long long>(ring + 1) * fallback_step_x_;
            const long long next_y = static_cast<long long>(ring + 1) * fallback_step_y_;
            const long long next_ring_minimum = std::min(next_x * next_x, next_y * next_y);
            if (next_ring_minimum > best->displacement_squared) return;
        }
    }
}

bool FormationMemberMovementPlan::assign_members(
    const std::vector<FormationMemberStationRequest> &requests,
    const Reachability &reachable)
{
    if (!valid_) return false;
    for (Station &station : stations_) {
        station.destination = station.ideal;
        station.state = FormationStationState::Unplaced;
        station.occupant = nullptr;
    }
    std::vector<FormationMemberStationRequest> ordered = requests;
    std::sort(ordered.begin(), ordered.end(), [](const auto &left, const auto &right) {
        return left.slot < right.slot;
    });
    int previous_slot = -1;
    for (const FormationMemberStationRequest &request : ordered) {
        if (request.slot == previous_slot || !request.member || !assign_member(request.slot, *request.member, reachable)) {
            return false;
        }
        previous_slot = request.slot;
    }
    return true;
}

bool FormationMemberMovementPlan::assign_member(
    int slot,
    RelationshipEndpoint &member,
    const Reachability &reachable)
{
    if (!valid_ || slot < 0 || slot >= static_cast<int>(stations_.size())) return false;
    Station &station = stations_[static_cast<size_t>(slot)];
    if (station.occupant == &member) return true;
    if (station.occupant) return false;

    station.occupant = &member;
    station.destination = station.ideal;
    station.state = FormationStationState::Unplaced;
    if (!point_is_reserved(slot, station.ideal) && reachable(slot, station.ideal) > 0) {
        station.state = FormationStationState::Ideal;
        return true;
    }

    std::vector<Candidate> candidates;
    find_nearest_fallback_candidates(slot, &candidates, reachable);
    std::sort(candidates.begin(), candidates.end(), [](const Candidate &left, const Candidate &right) {
        if (left.displacement_squared != right.displacement_squared) {
            return left.displacement_squared < right.displacement_squared;
        }
        if (left.route_distance != right.route_distance) return left.route_distance < right.route_distance;
        if (left.destination.y != right.destination.y) return left.destination.y < right.destination.y;
        return left.destination.x < right.destination.x;
    });
    if (!candidates.empty()) {
        station.destination = candidates.front().destination;
        station.state = FormationStationState::Fallback;
    }
    return true;
}

void FormationMemberMovementPlan::release_member(int slot, const RelationshipEndpoint &member)
{
    if (slot < 0 || slot >= static_cast<int>(stations_.size())) return;
    Station &station = stations_[static_cast<size_t>(slot)];
    if (station.occupant != &member) return;
    release_slot(slot);
}

void FormationMemberMovementPlan::release_slot(int slot)
{
    if (slot < 0 || slot >= static_cast<int>(stations_.size())) return;
    Station &station = stations_[static_cast<size_t>(slot)];
    station.destination = station.ideal;
    station.state = FormationStationState::Unplaced;
    station.occupant = nullptr;
}

void FormationMemberMovementPlan::mark_unplaced(int slot, const RelationshipEndpoint &member)
{
    if (slot < 0 || slot >= static_cast<int>(stations_.size())) return;
    Station &station = stations_[static_cast<size_t>(slot)];
    if (station.occupant != &member) return;
    station.destination = station.ideal;
    station.state = FormationStationState::Unplaced;
}

bool FormationMemberMovementPlan::resolve_member(
    int slot,
    const RelationshipEndpoint &member,
    FigureMovementDestination *destination,
    FormationStationState *state) const
{
    if (slot < 0 || slot >= static_cast<int>(stations_.size()) || !destination || !state) return false;
    const Station &station = stations_[static_cast<size_t>(slot)];
    if (station.occupant != &member) return false;
    *destination = station.destination;
    *state = station.state;
    return true;
}
