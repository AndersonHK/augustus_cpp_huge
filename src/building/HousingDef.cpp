#include "building/HousingDef.h"

#include <cassert>

namespace building_type_registry_impl {

const HousingTransitionDef &HousingDef::transition(HousingTransitionKind kind) const
{
    return const_cast<HousingDef *>(this)->transition(kind);
}

HousingTransitionDef &HousingDef::transition(HousingTransitionKind kind)
{
    switch (kind) {
        case HousingTransitionKind::EvolveTo: return evolve_to_;
        case HousingTransitionKind::DevolveTo: return devolve_to_;
        case HousingTransitionKind::MergeTo: return merge_to_;
        case HousingTransitionKind::SplitTo: return split_to_;
        case HousingTransitionKind::VacantLot: return vacant_lot_;
    }

    assert(false && "Unsupported housing transition kind");
    return vacant_lot_;
}

bool HousingDef::consumes_goods_on_day(int day_of_month) const
{
    if (goods_consumption_events_per_month == 1) {
        return day_of_month == 0;
    }
    if (goods_consumption_events_per_month == 2) {
        return day_of_month == 0 || day_of_month == 7;
    }
    return false;
}

} // namespace building_type_registry_impl
