#include "plague_runtime_contract_test.h"

#include "building/building_record.h"
#include "building/plague_state.h"

#include <ostream>

bool validate_plague_runtime_contract(std::ostream &errors)
{
    ::building record = {};
    record.state = BUILDING_STATE_IN_USE;
    record.has_plague = 1;
    record.sickness_duration = 10;
    record.sickness_level = 7;
    record.figure_id4 = 42;
    record.fumigation_frame = 3;
    record.fumigation_direction = 5;
    building_plague_apply_treatment(record);
    if (record.sickness_duration != 95 || record.sickness_doctor_cure != 99) {
        errors << "Building plague treatment did not enter the legacy-compatible cure state.\n";
        return false;
    }

    record.sickness_duration = 99;
    building_plague_advance_day(record);
    if (record.has_plague || record.sickness_duration || record.sickness_level ||
        record.sickness_doctor_cure || record.figure_id4 || record.fumigation_frame ||
        record.fumigation_direction) {
        errors << "Building plague completion left stale treatment state.\n";
        return false;
    }

    record.has_plague = 1;
    record.sickness_duration = 12;
    record.state = BUILDING_STATE_MOTHBALLED;
    building_plague_advance_day(record);
    if (record.sickness_duration != 12) {
        errors << "Inactive Building advanced plague state.\n";
        return false;
    }
    return true;
}
