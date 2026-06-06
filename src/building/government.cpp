#include "building/building_record.h"
#include "government.h"

#include "building/building.h"
#include "building/building_type_api.h"
#include "building/count.h"
#include "city/finance.h"

static building_type xml_type(const char *text_id)
{
    return building_type_registry_runtime_id_from_text(text_id);
}

void building_government_distribute_treasury(void)
{
    building_type forum_type = xml_type("forum");
    building_type senate_type = xml_type("senate");
    int units =
        (forum_type != BUILDING_NONE ? building_count_active(forum_type) : 0) +
        8 * (senate_type != BUILDING_NONE ? building_count_active(senate_type) : 0);
    int amount_per_unit = 0;
    int remainder = 0;
    int treasury = city_finance_treasury();

    if (treasury > 0 && units > 0) {
        amount_per_unit = treasury / units;
        remainder = treasury - units * amount_per_unit;
    }

    building *senate = senate_type != BUILDING_NONE ? building_first_of_type(senate_type) : 0;
    if (senate && senate->state == BUILDING_STATE_IN_USE && !senate->house_size && senate->num_workers > 0) {
        senate->tax_income_or_storage = 8 * amount_per_unit + remainder;
        remainder = 0;
    }

    for (building *b = forum_type != BUILDING_NONE ? building_first_of_type(forum_type) : 0; b; b = b->next_of_type) {
        if (b->state != BUILDING_STATE_IN_USE || b->house_size || b->num_workers <= 0) {
            continue;
        }
        b->tax_income_or_storage = amount_per_unit + remainder;
        remainder = 0;
    }
}
