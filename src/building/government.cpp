#include "building/building_record.h"
#include "building/building_type_registry_internal.h"
#include "government.h"

#include "building/building.h"
#include "city/finance.h"

void building_government_distribute_treasury(void)
{
    int units = 0;
    building *senate = nullptr;
    Building::for_each([&](Building *building) {
        if (!building->is_in_use() ||
            (building->Composition && building->Composition->is_child()) ||
            building->is_dynamic_bridge_segment()) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (building->type && building->type->attr_is("forum")) {
            units++;
        } else if (building->type && building->type->attr_is("senate")) {
            units += 8;
            senate = b;
        }
    });

    int amount_per_unit = 0;
    int remainder = 0;
    int treasury = city_finance_treasury();

    if (treasury > 0 && units > 0) {
        amount_per_unit = treasury / units;
        remainder = treasury - units * amount_per_unit;
    }

    if (senate && senate->state == BUILDING_STATE_IN_USE &&
        !building_type_registry_impl::type_has_housing(senate->type) && senate->num_workers > 0) {
        senate->collected_tax_income = 8 * amount_per_unit + remainder;
        remainder = 0;
    }

    Building::for_each([&](Building *building) {
        if (!building->type || !building->type->attr_is("forum")) {
            return;
        }
        ::building *b = const_cast<::building *>(building->record());
        if (b->state != BUILDING_STATE_IN_USE || building->Housing || b->num_workers <= 0) {
            return;
        }
        b->collected_tax_income = amount_per_unit + remainder;
        remainder = 0;
    });
}
