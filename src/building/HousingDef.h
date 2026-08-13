#pragma once

#include <string>

namespace building_type_registry_impl {

class BuildingType;
class HousingProfileDef;

enum class HousingTransitionKind {
    EvolveTo,
    DevolveTo,
    MergeTo,
    SplitTo,
    VacantLot
};

struct HousingTransitionDef {
    std::string type_path;
    const BuildingType *type = nullptr;

    explicit operator bool() const
    {
        return type != nullptr;
    }
};

class HousingDef {
public:
    const HousingTransitionDef &transition(HousingTransitionKind kind) const;
    HousingTransitionDef &transition(HousingTransitionKind kind);
    bool consumes_goods_on_day(int day_of_month) const;

    std::string profile_path;
    const HousingProfileDef *profile = nullptr;
    int capacity = 0;
    int goods_consumption_events_per_month = 0;
    int mars_offering_amount = 0;

private:
    HousingTransitionDef evolve_to_;
    HousingTransitionDef devolve_to_;
    HousingTransitionDef merge_to_;
    HousingTransitionDef split_to_;
    HousingTransitionDef vacant_lot_;
};

} // namespace building_type_registry_impl
