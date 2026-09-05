#include "building/building_record.h"
#include "house_service.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "building/monument.h"
#include "city/culture.h"

static int is_tower_coverage_type(building_type type)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    return definition && (definition->attr_is("tower") || definition->is_watchtower());
}

static void decay(uint8_t &value)
{
    if (value > 0) {
        --value;
    }
}

void house_service_decay_culture(void)
{
    Building::for_each(BuildingRuntimeList::Housing, [](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        HousingState &state = building->Housing->state();
        HousingServiceState &services = state.services;
        decay(services.theater);
        decay(services.amphitheater_actor);
        decay(services.amphitheater_gladiator);
        decay(services.colosseum_gladiator);
        decay(services.colosseum_lion);
        decay(state.arena_gladiator);
        decay(state.arena_lion);
        decay(state.tavern_food_access);
        decay(state.tavern_wine_access);
        decay(services.hippodrome);
        decay(services.school);
        decay(services.library);
        decay(services.academy);
        decay(services.barber);
        decay(services.clinic);
        decay(services.bathhouse);
        decay(services.hospital);
        decay(services.temple_ceres);
        decay(services.temple_neptune);
        decay(services.temple_mercury);
        decay(services.temple_mars);
        decay(services.temple_venus);
        decay(state.pantheon_access);
        ::building *b = const_cast<::building *>(building->record());
        if (b->days_since_offering < 125) {
            ++b->days_since_offering;
        }
    });
}

void house_service_decay_tax_collector(void)
{
    Building::for_each(BuildingRuntimeList::Housing, [](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        decay(building->Housing->state().tax_coverage);
    });
}

void house_service_decay_houses_covered(void)
{
    Building::for_each([](Building *building) {
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_UNUSED && !is_tower_coverage_type(b->type)) {
            if (b->houses_covered <= 1) {
                b->houses_covered = 0;
            } else {
                b->houses_covered--;
            }
        }
    });
}

void house_service_calculate_culture_aggregates(void)
{
    int venus_module2 = building_monument_gt_module_is_active(VENUS_MODULE_2_DESIRABILITY_ENTERTAINMENT);
    int completed_colosseum = building_monument_working(building_type_registry_impl::type_from_attr("colosseum"));
    int completed_hippodrome = building_monument_working(building_type_registry_impl::type_from_attr("hippodrome"));

    Building::for_each(BuildingRuntimeList::Housing, [&](Building *building) {
        if (!building->is_in_use()) {
            return;
        }
        HousingState &state = building->Housing->state();
        HousingServiceState &services = state.services;
        int arena_total = 0;
        int colosseum_total = 0;

        // Entertainment
        services.entertainment = 0;

        if (services.theater) {
            services.entertainment += 10;
        }

        if (state.tavern_wine_access) {
            services.entertainment += 10;
            if (state.tavern_food_access) {
                services.entertainment += 5;
            }
        }

        if (services.amphitheater_actor) {
            if (services.amphitheater_gladiator) {
                services.entertainment += 15;
            } else {
                services.entertainment += 10;
            }
        }

        if (state.arena_gladiator) {
            arena_total = state.arena_lion ? 20 : 10;
        }

        if (services.colosseum_gladiator) {
            colosseum_total = services.colosseum_lion ? 25 : 15;
        }

        services.entertainment = static_cast<uint8_t>(
            services.entertainment + (arena_total > colosseum_total ? arena_total : colosseum_total));

        if (services.hippodrome) {
            services.entertainment += 30;
        }

        if (completed_hippodrome) {
            services.entertainment += 5;
        }

        if (completed_colosseum) {
            services.entertainment += 5;
        }

        // Venus Module 2 Entertainment Bonus
        if (venus_module2 && services.temple_venus) {
            services.entertainment += 10;
        }

        // Education
        services.education = 0;
        if (services.school || services.library) {
            services.education = 1;
            if (services.school && services.library) {
                services.education = 2;
                if (services.academy) {
                    services.education = 3;
                }
            }
        }

        // religion
        services.num_gods = 0;
        if (services.temple_ceres) {
            ++services.num_gods;
        }
        if (services.temple_neptune) {
            ++services.num_gods;
        }
        if (services.temple_mercury) {
            ++services.num_gods;
        }
        if (services.temple_mars) {
            ++services.num_gods;
        }
        if (services.temple_venus) {
            ++services.num_gods;
        }

        // health
        services.health = 0;
        if (services.clinic) {
            ++services.health;
        }
        if (services.hospital) {
            ++services.health;
        }
    });
}
