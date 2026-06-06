#include "roadblock.h"

#include "building/building.h"
#include "building/building_record.h"
#include "building/building_type.h"

void Roadblock::toggle_permission(roadblock_permission permission)
{
    int permission_bit = 1 << permission;
    set_exceptions(exceptions() ^ permission_bit);
}

int Roadblock::has_permission(roadblock_permission permission) const
{
    int permission_bit = 1 << permission;
    return exceptions() & permission_bit;
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
    if (const building_type_registry_impl::BuildingType *definition = type_definition()) {
        return roadblock_kind_from_definition(definition->roadblock().kind());
    }
    return ROADBLOCK_NONE;
}

int Roadblock::exceptions() const
{
    return legacy_record() ? legacy_record()->data.roadblock.exceptions : 0;
}

void Roadblock::set_exceptions(int exceptions)
{
    if (legacy_record()) {
        legacy_record()->data.roadblock.exceptions = exceptions;
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
