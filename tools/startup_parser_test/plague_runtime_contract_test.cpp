#include "plague_runtime_contract_test.h"

#include "building/building.h"

#include <ostream>

bool validate_plague_runtime_contract(std::ostream &errors)
{
    static_assert(requires(Building &building) {
        building.has_plague();
        building.advance_plague_day();
        building.apply_plague_treatment();
    }, "Plague behavior must remain owned by Building instead of raw-record helpers");
    (void)errors;
    return true;
}
