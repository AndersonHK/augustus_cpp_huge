#pragma once

#include "building/building_record.h"
#include "building/building_type.h"

inline void building_plague_apply_treatment(::building &record)
{
    if (record.state != BUILDING_STATE_IN_USE || !record.has_plague) {
        return;
    }
    if (record.sickness_duration < 95) {
        record.sickness_duration = 95;
    }
    // Value 99 is part of the legacy save/display contract for active fumigation.
    record.sickness_doctor_cure = 99;
}

inline void building_plague_advance_day(::building &record)
{
    if (record.state != BUILDING_STATE_IN_USE || !record.has_plague) {
        return;
    }
    if (record.sickness_duration != 99) {
        ++record.sickness_duration;
        return;
    }
    record.sickness_duration = 0;
    record.has_plague = 0;
    record.sickness_level = 0;
    record.sickness_doctor_cure = 0;
    record.figure_id4 = 0;
    record.fumigation_frame = 0;
    record.fumigation_direction = 0;
}
