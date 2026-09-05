#include "roadblock.h"

#include "building/building.h"
#include "figure/figure.h"
#include "building/building_record.h"
#include "building/building_type.h"

static int permission_is_roadblock_managed(roadblock_permission permission)
{
    return permission != PERMISSION_NONE;
}

void Roadblock::toggle_permission(roadblock_permission permission)
{
    if (!permission_is_roadblock_managed(permission)) {
        return;
    }
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        Foundation->roadblock_state().toggle_permission(static_cast<int>(permission));
        return;
    }
    int permission_bit = 1 << permission;
    set_exceptions(exceptions() ^ permission_bit);
}

int Roadblock::has_permission(roadblock_permission permission) const
{
    if (!permission_is_roadblock_managed(permission)) {
        return 1;
    }
    if (Foundation && Foundation->has_unrestricted_road_crossing()) {
        return 1;
    }
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        return Foundation->roadblock_state().has_permission(static_cast<int>(permission));
    }
    int permission_bit = 1 << permission;
    return exceptions() & permission_bit;
}

int Roadblock::can_configure(roadblock_permission permission) const
{
    if (!permission_is_roadblock_managed(permission)) {
        return 0;
    }
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        return Foundation->roadblock_state().can_configure(static_cast<int>(permission));
    }
    return kind() == ROADBLOCK_BRIDGE;
}

int Roadblock::allows_at(int grid_offset, roadblock_permission permission) const
{
    if (kind() == ROADBLOCK_BRIDGE) {
        return has_permission(permission);
    }
    if (type && type->is_warehouse()) {
        if (permission == PERMISSION_FREIGHT) {
            return Foundation ? Foundation->allows_passage(grid_offset, PERMISSION_NONE) : 0;
        }
        if (permission == PERMISSION_NONE) {
            return 0;
        }
    } else if (permission == PERMISSION_FREIGHT) {
        // Freight used to be part of the unmanaged bypass category and must
        // continue to ignore ordinary roadblocks and granary gates.
        permission = PERMISSION_NONE;
    }
    return Foundation ? Foundation->allows_passage(grid_offset, static_cast<int>(permission)) : 0;
}

roadblock_permission Roadblock::permission_for(const Figure &figure)
{
    switch (figure.type) {
        case FIGURE_CART_PUSHER:
        case FIGURE_WAREHOUSEMAN:
        case FIGURE_DEPOT_CART_PUSHER:
            return PERMISSION_FREIGHT;
        case FIGURE_ENGINEER:
        case FIGURE_PREFECT:
            return PERMISSION_MAINTENANCE;
        case FIGURE_PRIEST:
            return PERMISSION_PRIEST;
        case FIGURE_MARKET_TRADER:
            return PERMISSION_MARKET;
        case FIGURE_GLADIATOR:
        case FIGURE_CHARIOTEER:
        case FIGURE_ACTOR:
        case FIGURE_LION_TAMER:
        case FIGURE_BARKEEP:
            return PERMISSION_ENTERTAINER;
        case FIGURE_SURGEON:
        case FIGURE_DOCTOR:
        case FIGURE_BARBER:
        case FIGURE_BATHHOUSE_WORKER:
            return PERMISSION_MEDICINE;
        case FIGURE_SCHOOL_CHILD:
        case FIGURE_TEACHER:
        case FIGURE_LIBRARIAN:
            return PERMISSION_EDUCATION;
        case FIGURE_TAX_COLLECTOR:
            return PERMISSION_TAX_COLLECTOR;
        case FIGURE_LABOR_SEEKER:
            return PERMISSION_LABOR_SEEKER;
        case FIGURE_MISSIONARY:
            return PERMISSION_MISSIONARY;
        case FIGURE_WATCHMAN:
            return PERMISSION_WATCHMAN;
        default:
            return PERMISSION_NONE;
    }
}

int Roadblock::allows(const Figure &figure) const
{
    return has_permission(permission_for(figure));
}

roadblock_type Roadblock::kind() const
{
    if (type && type->bridge().is_bridge()) {
        return ROADBLOCK_BRIDGE;
    }
    if (Foundation && (Foundation->has_owner_controlled_passage() ||
            Foundation->has_unrestricted_road_crossing())) {
        return ROADBLOCK_FOUNDATION;
    }
    return ROADBLOCK_NONE;
}

int Roadblock::exceptions() const
{
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        return Foundation->roadblock_state().permissions();
    }
    const building *record = this->record();
    return record ? record->data.roadblock.exceptions : 0;
}

void Roadblock::set_exceptions(int exceptions)
{
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        Foundation->roadblock_state().set_permissions(static_cast<unsigned short>(exceptions));
        return;
    }
    if (building *record = const_cast<building *>(this->record())) {
        record->data.roadblock.exceptions = static_cast<unsigned short>(exceptions);
    }
}

void Roadblock::accept_none()
{
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        Foundation->roadblock_state().accept_none();
    } else {
        set_exceptions(0);
    }
}

void Roadblock::accept_all()
{
    if (Foundation && kind() != ROADBLOCK_BRIDGE) {
        Foundation->roadblock_state().accept_all();
    } else {
        set_exceptions(ROADBLOCK_PERMISSION_ALL);
    }
}
