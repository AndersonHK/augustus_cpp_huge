#include "building/building_record.h"
#include "government.h"

#include "building/building.h"
#include "city/finance.h"

void building_government_distribute_treasury(void)
{
    int units = 0;
    building *senate = nullptr;
    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        Building building(b);
        if (!building.is_in_use() || building_main(b) != b) {
            continue;
        }
        if (building.type && building.type->attr_is("forum")) {
            units++;
        } else if (building.type && building.type->attr_is("senate")) {
            units += 8;
            senate = b;
        }
    }

    int amount_per_unit = 0;
    int remainder = 0;
    int treasury = city_finance_treasury();

    if (treasury > 0 && units > 0) {
        amount_per_unit = treasury / units;
        remainder = treasury - units * amount_per_unit;
    }

    if (senate && senate->state == BUILDING_STATE_IN_USE && !senate->house_size && senate->num_workers > 0) {
        senate->tax_income_or_storage = 8 * amount_per_unit + remainder;
        remainder = 0;
    }

    for (int id = 1; id < building_count(); id++) {
        building *b = building_get(id);
        Building building(b);
        if (!building.type || !building.type->attr_is("forum")) {
            continue;
        }
        if (b->state != BUILDING_STATE_IN_USE || b->house_size || b->num_workers <= 0) {
            continue;
        }
        b->tax_income_or_storage = amount_per_unit + remainder;
        remainder = 0;
    }
}
