#include "building/industry.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/production.h"
#include "figure/figure.h"

#include "city/finance.h"
#include "city/data_private.h"
#include "city/buildings.h"
#include "city/emperor.h"
#include "core/calc.h"
#include "game/time.h"
#include "game/resource.h"
#include "map/water.h"

namespace {

constexpr int kRecordProductionMonths = 12;
constexpr int kMercuryBlessingLoads = 3;

int get_resource_slot_index(resource_type resource)
{
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_SLOT_COUNT) {
        return -1;
    }
    return static_cast<int>(resource);
}

::building *production_record(Building building)
{
    return building_get(building.id);
}

int output_amount(const building_type_registry_impl::ProductionMethod &method)
{
    return (resource_units_per_load() * method.cart_load_numerator()) / method.cart_load_denominator();
}

void add_treasury_output(resource_type resource, int amount)
{
    if (resource != resource_denarii()) {
        city_finance_treasury_add_miscellaneous(amount);
        return;
    }

    int personal_funds = 0;
    if (city_buildings_has_governor_house()) {
        const int personal_salary = city_emperor_salary_amount();
        if (personal_salary > 5) {
            personal_funds = amount / 10;
            personal_funds = calc_adjust_with_percentage(personal_funds, personal_salary);
            if (personal_funds == 0) {
                personal_funds = 1;
            }
        }
    }
    city_data.emperor.personal_savings += personal_funds;
    city_finance_treasury_add_miscellaneous(amount - personal_funds);
}

} // namespace

Production::Production(
    const Building &building,
    const building_type_registry_impl::ProductionMethod *method,
    size_t method_index)
    : record_(building_get(building.id))
    , definition_(building.type)
    , method_(method)
    , method_index_(method_index)
{
}

Building Production::building() const
{
    return Building(record_, definition_);
}

Building Production::context_building() const
{
    Building owner = building();
    Building main = owner.main();
    return main.id ? main : owner;
}

::building *Production::context_record() const
{
    return building_get(context_building().id);
}

int Production::decrement_strike_if_needed(int new_day, int *out_is_striking)
{
    Building building = context_building();
    ::building *legacy = production_record(building);
    if (!legacy) {
        if (out_is_striking) {
            *out_is_striking = 0;
        }
        return 0;
    }
    if (out_is_striking) {
        *out_is_striking = 0;
    }

    if (legacy->strike_duration_days <= 0) {
        return 0;
    }

    if (out_is_striking) {
        *out_is_striking = 1;
    }

    if (new_day) {
        legacy->strike_duration_days--;
        if (city_data.sentiment.value > 50) {
            legacy->strike_duration_days -= 3;
        }
        if (city_data.sentiment.value > 65) {
            legacy->strike_duration_days = 0;
        }
    }

    if (legacy->strike_duration_days == 0) {
        city_data.building.num_striking_industries--;
        Figure::get(legacy->figure_id4)->remove();
        if (out_is_striking) {
            *out_is_striking = 0;
        }
    }
    return 1;
}

void Production::refresh_images() const
{
    if (production_record(building()) && method_ && method_->is_farm()) {
        building().refresh_graphic_if_native();
    }
}

int Production::has_raw_materials() const
{
    return production_record(building()) && method_ ? method_->has_required_inputs(context_building()) : 0;
}

int Production::max_progress() const
{
    return production_record(building()) && method_ ? method_->max_progress_for(context_building()) : 0;
}

int Production::efficiency() const
{
    const ::building *legacy = production_record(building());
    if (!legacy || !method_) {
        return -1;
    }
    Building context = context_building();
    if (context.is_mothballed()) {
        return -1;
    }
    if (legacy->data.industry.age_months == 0 || method_->output_resource() == RESOURCE_NONE) {
        return -1;
    }

    const int production_for_resource = method_->effective_monthly_production();
    if (production_for_resource <= 0) {
        return -1;
    }
    const int percentage =
        calc_percentage(legacy->data.industry.average_production_per_month, production_for_resource);
    return calc_bound(percentage, 0, 100);
}

int Production::update_daily(int new_day, int *out_is_striking)
{
    ::building *legacy = production_record(building());
    ::building *context = context_record();
    if (!legacy || !method_) {
        return 0;
    }

    decrement_strike_if_needed(new_day, out_is_striking);

    legacy->data.industry.has_raw_materials = 0;
    if (context && context != legacy) {
        context->data.industry.has_raw_materials = 0;
    }
    if (method_->is_figure_delivery_output()) {
        legacy->data.industry.has_raw_materials = 1;
        if (context && context != legacy) {
            context->data.industry.has_raw_materials = 1;
        }
        return 1;
    }
    Building context_building = this->context_building();
    if (!method_->can_start_cycle(context_building)) {
        return 1;
    }

    ::building *state_record = context ? context : legacy;
    if (state_record->data.industry.curse_days_left) {
        if (new_day) {
            state_record->data.industry.curse_days_left--;
        }
        return 1;
    }
    if (state_record->data.industry.blessing_days_left && new_day) {
        state_record->data.industry.blessing_days_left--;
    }

    int progress = context_building.employment_worker_count();
    if (state_record->data.industry.blessing_days_left && method_->uses_blessing_multiplier()) {
        progress += context_building.employment_worker_count();
    }

    if (legacy->data.industry.progress == 0 && method_->treasury_cost_per_cycle() > 0) {
        city_finance_process_sundry(method_->treasury_cost_per_cycle());
    }

    legacy->data.industry.has_raw_materials = 1;
    if (context && context != legacy) {
        context->data.industry.has_raw_materials = 1;
    }
    legacy->data.industry.progress += progress;

    const int max_value = max_progress();
    if (legacy->data.industry.progress > max_value) {
        legacy->data.industry.progress = max_value;
    }
    if (method_->has_resource_output() && method_->outputs_to_building_storage() &&
        legacy->data.industry.progress >= max_value) {
        start_new_production();
    }

    refresh_images();
    return 1;
}

int Production::has_completed_effect() const
{
    const ::building *legacy = production_record(building());
    const int max_value = max_progress();
    return legacy && method_ && method_->has_effect_output() && max_value > 0 &&
        legacy->data.industry.progress >= max_value;
}

int Production::pending_production_for_stats() const
{
    const ::building *legacy = production_record(building());
    if (!legacy) {
        return 0;
    }
    const int max_value = max_progress();
    if (max_value <= 0) {
        return 0;
    }

    int pending_production_percentage = calc_percentage(legacy->data.industry.progress, max_value);
    pending_production_percentage = calc_bound(pending_production_percentage, 0, 100);
    return method_->has_resource_output() ?
        (pending_production_percentage * output_amount(*method_)) / 100 : 0;
}

void Production::start_new_production()
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_) {
        return;
    }

    const int max_value = max_progress();
    const int raw_materials_available = has_raw_materials();
    ::building *context = context_record();
    if (max_value > 0 && legacy->data.industry.progress >= max_value) {
        if (!raw_materials_available) {
            legacy->data.industry.has_raw_materials = 0;
            if (context && context != legacy) {
                context->data.industry.has_raw_materials = 0;
            }
            return;
        }
        if (method_->spawns_fishing_boat()) {
            if (!map_water_spawn_fishing_boat_from_shipyard(context_building())) {
                return;
            }
        } else if (method_->outputs_to_building_storage()) {
            if (!context_building().add_storage_resource(method_->output_resource(), output_amount(*method_),
                building_type_registry_impl::StorageRole::Output)) {
                return;
            }
            ::building *output_record = context_record();
            if (!output_record) {
                output_record = legacy;
            }
            output_record->data.industry.production_current_month += output_amount(*method_);
        } else if (method_->outputs_to_treasury()) {
            add_treasury_output(method_->output_resource(), output_amount(*method_));
            ::building *output_record = context_record();
            if (!output_record) {
                output_record = legacy;
            }
            output_record->data.industry.production_current_month += output_amount(*method_);
        } else {
            ::building *output_record = context_record();
            if (!output_record) {
                output_record = legacy;
            }
            output_record->data.industry.production_current_month += output_amount(*method_);
        }
        legacy->data.industry.progress = 0;
    }

    if (raw_materials_available) {
        for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
            const int resource_slot_index = get_resource_slot_index(input.resource);
            if (resource_slot_index >= 0) {
                context_building().add_storage_resource(input.resource, -method_->scaled_input_amount(input),
                    building_type_registry_impl::StorageRole::Input);
            }
        }
    }
    legacy->data.industry.has_raw_materials = raw_materials_available;
    if (context && context != legacy) {
        context->data.industry.has_raw_materials = raw_materials_available;
    }
    refresh_images();
}

void Production::advance_stats()
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_) {
        return;
    }
    if (!building().is_in_use() && !building().is_mothballed()) {
        return;
    }

    if (legacy->data.industry.age_months < kRecordProductionMonths) {
        legacy->data.industry.age_months++;
    }

    int sum_months = legacy->data.industry.average_production_per_month * (legacy->data.industry.age_months - 1);
    const int pending_production_percentage = pending_production_for_stats();
    sum_months += legacy->data.industry.production_current_month + pending_production_percentage;
    legacy->data.industry.average_production_per_month = sum_months / legacy->data.industry.age_months;
    const int leftover_from_average = sum_months % legacy->data.industry.age_months;
    legacy->data.industry.production_current_month = leftover_from_average - pending_production_percentage;
}

void Production::bless_farm()
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_ || !method_->is_farm()) {
        return;
    }

    ::building *state_record = context_record();
    if (!state_record) {
        state_record = legacy;
    }
    legacy->data.industry.progress = max_progress();
    state_record->data.industry.curse_days_left = 0;
    state_record->data.industry.blessing_days_left = 16;
    refresh_images();
}

void Production::curse_farm(int big_curse)
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_ || !method_->is_farm()) {
        return;
    }

    ::building *state_record = context_record();
    if (!state_record) {
        state_record = legacy;
    }
    legacy->data.industry.progress = 0;
    state_record->data.industry.blessing_days_left = 0;
    state_record->data.industry.curse_days_left = big_curse ? 48 : 4;
    refresh_images();
}

void Production::bless_industry()
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_ || !method_->is_workshop()) {
        return;
    }
    if (!building().is_in_use() || legacy->output_resource_id != method_->output_resource()) {
        return;
    }
    if (!building().worker_count()) {
        return;
    }

    for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
        if (get_resource_slot_index(input.resource) < 0) {
            continue;
        }
        const int resource_slot = building().resource_amount(input.resource);
        const int blessed_amount = kMercuryBlessingLoads * method_->scaled_input_amount(input);
        if (resource_slot > 0 && resource_slot < blessed_amount) {
            building().add_resource(input.resource, blessed_amount - resource_slot);
        }
    }
    if (legacy->data.industry.progress) {
        legacy->data.industry.progress = max_progress();
    }
}
