#pragma once

#include "building/building_fwd.h"
#include "building/production_method.h"

#include <cstddef>

namespace building_type_registry_impl {
class BuildingType;
}

class Production {
public:
    Production(const Building &building, const building_type_registry_impl::ProductionMethod *method, size_t method_index);

    Building building() const;

    const building_type_registry_impl::ProductionMethod *method() const
    {
        return method_;
    }

    size_t method_index() const
    {
        return method_index_;
    }

    int update_daily(int new_day, int *out_is_striking);
    int has_raw_materials() const;
    int max_progress() const;
    int efficiency() const;
    int has_completed_effect() const;
    void start_new_production();
    void advance_stats();
    void bless_farm();
    void curse_farm(int big_curse);
    void bless_industry();

private:
    Building context_building() const;
    ::building *context_record() const;
    int decrement_strike_if_needed(int new_day, int *out_is_striking);
    int pending_production_for_stats() const;
    void refresh_images() const;

    ::building *record_ = nullptr;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    const building_type_registry_impl::ProductionMethod *method_ = nullptr;
    size_t method_index_ = 0;
};
