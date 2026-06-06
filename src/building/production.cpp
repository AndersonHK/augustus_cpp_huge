#include "building/image.h"
#include "building/industry.h"
#include "map/building_tiles.h"

#include "building/building_record.h"
#include "building/building.h"
#include "building/production.h"

extern "C" {
#include "city/finance.h"
#include "city/data_private.h"
#include "core/calc.h"
#include "figure/figure.h"
#include "game/resource.h"
}

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
    return building.legacy_record();
}

void update_farm_image(const Building &building, int max_progress)
{
    const ::building *record = building.legacy_record();
    if (!record) {
        return;
    }
    map_building_tiles_add_farm(
        building.id(),
        building.x(),
        building.y(),
        building_image_get_base_farm_crop(building.type_id()),
        calc_percentage(record->data.industry.progress, max_progress));
}

} // namespace

Production::Production(
    const Building &building,
    const building_type_registry_impl::ProductionMethod *method,
    size_t method_index)
    : record_(const_cast<::building *>(building.legacy_record()))
    , definition_(building.type_definition())
    , method_(method)
    , method_index_(method_index)
{
}

Building Production::building() const
{
    return Building(record_, definition_);
}

unsigned int Production::building_id() const
{
    return building().id();
}

int Production::decrement_strike_if_needed(int new_day, int *out_is_striking)
{
    Building building = this->building();
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
        figure_delete(figure_get(legacy->figure_id4));
        if (out_is_striking) {
            *out_is_striking = 0;
        }
    }
    return 1;
}

void Production::refresh_images() const
{
    if (production_record(building()) && method_ && method_->refreshes_farm_image()) {
        update_farm_image(building(), max_progress());
    }
}

int Production::has_raw_materials() const
{
    return production_record(building()) && method_ ? method_->has_required_inputs(building()) : 0;
}

int Production::max_progress() const
{
    return production_record(building()) && method_ ? method_->max_progress_for(building()) : 0;
}

int Production::efficiency() const
{
    const ::building *legacy = production_record(building());
    if (!legacy || !method_) {
        return -1;
    }
    if (building().is_mothballed()) {
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
    if (!legacy || !method_) {
        return 0;
    }

    decrement_strike_if_needed(new_day, out_is_striking);

    legacy->data.industry.has_raw_materials = 0;
    if (!method_->can_start_cycle(building())) {
        return 1;
    }

    if (legacy->data.industry.curse_days_left) {
        if (new_day) {
            legacy->data.industry.curse_days_left--;
        }
        return 1;
    }
    if (legacy->data.industry.blessing_days_left && new_day) {
        legacy->data.industry.blessing_days_left--;
    }

    int progress = building().worker_count();
    if (legacy->data.industry.blessing_days_left && method_->uses_blessing_multiplier()) {
        progress += building().worker_count();
    }

    if (legacy->data.industry.progress == 0 && method_->treasury_cost_per_cycle() > 0) {
        city_finance_process_sundry(method_->treasury_cost_per_cycle());
    }

    legacy->data.industry.has_raw_materials = 1;
    legacy->data.industry.progress += progress;

    const int max_value = max_progress();
    if (legacy->data.industry.progress > max_value) {
        legacy->data.industry.progress = max_value;
    }

    refresh_images();
    return 1;
}

int Production::has_produced_resource() const
{
    const ::building *legacy = production_record(building());
    const int max_value = max_progress();
    return legacy && method_ && max_value > 0 && legacy->data.industry.progress >= max_value;
}

int Production::output_cart_loads() const
{
    return method_ ? method_->batch_size() : 0;
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
    return pending_production_percentage * output_cart_loads();
}

void Production::start_new_production()
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_) {
        return;
    }

    const int max_value = max_progress();
    if (max_value > 0 && legacy->data.industry.progress >= max_value) {
        legacy->data.industry.production_current_month += resource_units_per_load() * output_cart_loads();
        legacy->data.industry.progress = 0;
    }

    const int raw_materials_available = has_raw_materials();
    if (raw_materials_available) {
        for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
            const int resource_slot_index = get_resource_slot_index(input.resource);
            if (resource_slot_index >= 0) {
                building().add_resource(input.resource, -method_->scaled_input_amount(input));
            }
        }
    }
    legacy->data.industry.has_raw_materials = raw_materials_available;
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

    legacy->data.industry.progress = max_progress();
    legacy->data.industry.curse_days_left = 0;
    legacy->data.industry.blessing_days_left = 16;
    refresh_images();
}

void Production::curse_farm(int big_curse)
{
    ::building *legacy = production_record(building());
    if (!legacy || !method_ || !method_->is_farm()) {
        return;
    }

    legacy->data.industry.progress = 0;
    legacy->data.industry.blessing_days_left = 0;
    legacy->data.industry.curse_days_left = big_curse ? 48 : 4;
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
    if (!building().has_workers()) {
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
