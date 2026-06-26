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
    int permission_bit = 1 << permission;
    set_exceptions(exceptions() ^ permission_bit);
}

int Roadblock::has_permission(roadblock_permission permission) const
{
    if (!permission_is_roadblock_managed(permission)) {
        return 1;
    }
    int permission_bit = 1 << permission;
    return exceptions() & permission_bit;
}

roadblock_permission Roadblock::permission_for(const Figure &figure)
{
    switch (figure.type) {
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

static roadblock_type roadblock_kind_from_definition(building_type_registry_impl::RoadblockKind kind)
{
    switch (kind) {
        case building_type_registry_impl::RoadblockKind::Standard:
            return ROADBLOCK_STANDARD;
        case building_type_registry_impl::RoadblockKind::Storage:
            return ROADBLOCK_STORAGE;
        case building_type_registry_impl::RoadblockKind::Bridge:
            return ROADBLOCK_BRIDGE;
        default:
            return ROADBLOCK_NONE;
    }
}

roadblock_type Roadblock::kind() const
{
    if (const building_type_registry_impl::BuildingType *definition = type) {
        return roadblock_kind_from_definition(definition->roadblock().kind());
    }
    return ROADBLOCK_NONE;
}

int Roadblock::exceptions() const
{
    building *record = id() ? building_get(id()) : nullptr;
    return record ? record->data.roadblock.exceptions : 0;
}

void Roadblock::set_exceptions(int exceptions)
{
    if (building *record = id() ? building_get(id()) : nullptr) {
        record->data.roadblock.exceptions = exceptions;
    }
}

void Roadblock::accept_none()
{
    set_exceptions(0);
}

void Roadblock::accept_all()
{
    set_exceptions(ROADBLOCK_PERMISSION_ALL);
}
