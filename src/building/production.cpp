#include "building/production.h"

extern "C" {
#include "building/image.h"
#include "building/industry.h"
#include "city/data_private.h"
#include "core/calc.h"
#include "figure/figure.h"
#include "game/resource.h"
#include "map/building_tiles.h"
}

namespace {

constexpr int kRecordProductionMonths = 12;
constexpr int kMercuryBlessingLoads = 3;

int get_resource_slot_index(resource_type resource)
{
    if (resource <= RESOURCE_NONE || resource >= RESOURCE_MAX) {
        return -1;
    }
    return static_cast<int>(resource);
}

void update_farm_image(const building *b, int max_progress)
{
    map_building_tiles_add_farm(
        b->id,
        b->x,
        b->y,
        building_image_get_base_farm_crop(b->type),
        calc_percentage(b->data.industry.progress, max_progress));
}

} // namespace

int Production::decrement_strike_if_needed(int new_day, int *out_is_striking)
{
    if (out_is_striking) {
        *out_is_striking = 0;
    }

    if (building_->strike_duration_days <= 0) {
        return 0;
    }

    if (out_is_striking) {
        *out_is_striking = 1;
    }

    if (new_day) {
        building_->strike_duration_days--;
        if (city_data.sentiment.value > 50) {
            building_->strike_duration_days -= 3;
        }
        if (city_data.sentiment.value > 65) {
            building_->strike_duration_days = 0;
        }
    }

    if (building_->strike_duration_days == 0) {
        city_data.building.num_striking_industries--;
        figure_delete(figure_get(building_->figure_id4));
        if (out_is_striking) {
            *out_is_striking = 0;
        }
    }
    return 1;
}

void Production::refresh_images() const
{
    if (building_ && method_ && method_->refreshes_farm_image()) {
        update_farm_image(building_, max_progress());
    }
}

int Production::has_raw_materials() const
{
    return building_ && method_ ? method_->has_required_inputs(*building_) : 0;
}

int Production::max_progress() const
{
    return building_ && method_ ? method_->max_progress_for(*building_) : 0;
}

int Production::efficiency() const
{
    if (building_->state == BUILDING_STATE_MOTHBALLED) {
        return -1;
    }
    if (building_->data.industry.age_months == 0 || method_->output_resource() == RESOURCE_NONE) {
        return -1;
    }

    const int production_for_resource = method_->effective_monthly_production();
    if (production_for_resource <= 0) {
        return -1;
    }
    const int percentage =
        calc_percentage(building_->data.industry.average_production_per_month, production_for_resource);
    return calc_bound(percentage, 0, 100);
}

int Production::update_daily(int new_day, int *out_is_striking)
{
    if (!building_ || !method_) {
        return 0;
    }

    decrement_strike_if_needed(new_day, out_is_striking);

    building_->data.industry.has_raw_materials = 0;
    if (!method_->can_start_cycle(*building_)) {
        return 1;
    }

    if (building_->data.industry.curse_days_left) {
        if (new_day) {
            building_->data.industry.curse_days_left--;
        }
        return 1;
    }
    if (building_->data.industry.blessing_days_left && new_day) {
        building_->data.industry.blessing_days_left--;
    }

    int progress = building_->num_workers;
    if (building_->data.industry.blessing_days_left && method_->uses_blessing_multiplier()) {
        progress += building_->num_workers;
    }

    building_->data.industry.has_raw_materials = 1;
    building_->data.industry.progress += progress;

    const int max_value = max_progress();
    if (building_->data.industry.progress > max_value) {
        building_->data.industry.progress = max_value;
    }

    refresh_images();
    return 1;
}

int Production::has_produced_resource() const
{
    const int max_value = max_progress();
    return building_ && method_ && max_value > 0 && building_->data.industry.progress >= max_value;
}

int Production::output_cart_loads() const
{
    return method_ ? method_->batch_size() : 0;
}

int Production::pending_production_for_stats() const
{
    const int max_value = max_progress();
    if (max_value <= 0) {
        return 0;
    }

    int pending_production_percentage = calc_percentage(building_->data.industry.progress, max_value);
    pending_production_percentage = calc_bound(pending_production_percentage, 0, 100);
    return pending_production_percentage * output_cart_loads();
}

void Production::start_new_production()
{
    if (!building_ || !method_) {
        return;
    }

    const int max_value = max_progress();
    if (max_value > 0 && building_->data.industry.progress >= max_value) {
        building_->data.industry.production_current_month += RESOURCE_ONE_LOAD * output_cart_loads();
        building_->data.industry.progress = 0;
    }

    const int raw_materials_available = has_raw_materials();
    if (raw_materials_available) {
        for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
            const int resource_slot_index = get_resource_slot_index(input.resource);
            if (resource_slot_index >= 0) {
                building_->resources[resource_slot_index] -= method_->scaled_input_amount(input);
            }
        }
    }
    building_->data.industry.has_raw_materials = raw_materials_available;
    refresh_images();
}

void Production::advance_stats()
{
    if (!building_ || !method_) {
        return;
    }
    if (building_->state != BUILDING_STATE_IN_USE && building_->state != BUILDING_STATE_MOTHBALLED) {
        return;
    }

    if (building_->data.industry.age_months < kRecordProductionMonths) {
        building_->data.industry.age_months++;
    }

    int sum_months = building_->data.industry.average_production_per_month * (building_->data.industry.age_months - 1);
    const int pending_production_percentage = pending_production_for_stats();
    sum_months += building_->data.industry.production_current_month + pending_production_percentage;
    building_->data.industry.average_production_per_month = sum_months / building_->data.industry.age_months;
    const int leftover_from_average = sum_months % building_->data.industry.age_months;
    building_->data.industry.production_current_month = leftover_from_average - pending_production_percentage;
}

void Production::bless_farm()
{
    if (!building_ || !method_ || !method_->is_farm()) {
        return;
    }

    building_->data.industry.progress = max_progress();
    building_->data.industry.curse_days_left = 0;
    building_->data.industry.blessing_days_left = 16;
    refresh_images();
}

void Production::curse_farm(int big_curse)
{
    if (!building_ || !method_ || !method_->is_farm()) {
        return;
    }

    building_->data.industry.progress = 0;
    building_->data.industry.blessing_days_left = 0;
    building_->data.industry.curse_days_left = big_curse ? 48 : 4;
    refresh_images();
}

void Production::bless_industry()
{
    if (!building_ || !method_ || !method_->is_workshop()) {
        return;
    }
    if (building_->state != BUILDING_STATE_IN_USE || building_->output_resource_id != method_->output_resource()) {
        return;
    }
    if (building_->num_workers <= 0) {
        return;
    }

    for (const building_type_registry_impl::ProductionResourceAmount &input : method_->inputs()) {
        const int resource_slot_index = get_resource_slot_index(input.resource);
        if (resource_slot_index < 0) {
            continue;
        }
        short &resource_slot = building_->resources[resource_slot_index];
        if (resource_slot > 0 &&
            resource_slot < kMercuryBlessingLoads * method_->scaled_input_amount(input)) {
            resource_slot = static_cast<short>(kMercuryBlessingLoads * method_->scaled_input_amount(input));
        }
    }
    if (building_->data.industry.progress) {
        building_->data.industry.progress = max_progress();
    }
}
