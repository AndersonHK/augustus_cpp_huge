#include "houses.h"

#include "city/data_private.h"
#include <algorithm>
#include <array>

static std::array<int *, 5> missing_religions()
{
    auto &missing = city_data.houses.missing;
    return {&missing.religion, &missing.second_religion, &missing.third_religion, &missing.fourth_religion, &missing.fifth_religion};
}

bool city_houses_check_religion_requirement(int required, int available)
{
    if (available < required) {
        const auto counters = missing_religions();
        ++*counters[std::clamp(required, 1, static_cast<int>(counters.size())) - 1];
        return false;
    }
    if (required > 0) ++city_data.houses.requiring.religion;
    return true;
}

void city_houses_reset_demands(void)
{
    city_data.houses.missing.fountain = 0;
    city_data.houses.missing.well = 0;
    city_data.houses.missing.entertainment = 0;
    city_data.houses.missing.more_entertainment = 0;
    city_data.houses.missing.education = 0;
    city_data.houses.missing.more_education = 0;
    for (auto *count : missing_religions()) *count = 0;
    city_data.houses.missing.barber = 0;
    city_data.houses.missing.bathhouse = 0;
    city_data.houses.missing.clinic = 0;
    city_data.houses.missing.hospital = 0;
    city_data.houses.missing.food = 0;
    // NB: second_wine purposely not cleared

    city_data.houses.requiring.school = 0;
    city_data.houses.requiring.library = 0;
    city_data.houses.requiring.barber = 0;
    city_data.houses.requiring.bathhouse = 0;
    city_data.houses.requiring.clinic = 0;
    city_data.houses.requiring.religion = 0;
}

house_demands *city_houses_demands(void)
{
    return &city_data.houses;
}

void city_houses_calculate_culture_demands(void)
{
    // health
    city_data.houses.health = 0;
    int max = 0;
    if (city_data.houses.missing.bathhouse > max) {
        city_data.houses.health = 1;
        max = city_data.houses.missing.bathhouse;
    }
    if (city_data.houses.missing.barber > max) {
        city_data.houses.health = 2;
        max = city_data.houses.missing.barber;
    }
    if (city_data.houses.missing.clinic > max) {
        city_data.houses.health = 3;
        max = city_data.houses.missing.clinic;
    }
    if (city_data.houses.missing.hospital > max) {
        city_data.houses.health = 4;
    }
    // education
    city_data.houses.education = 0;
    if (city_data.houses.missing.more_education > city_data.houses.missing.education) {
        city_data.houses.education = 1; // schools(academies?)
    } else if (city_data.houses.missing.more_education < city_data.houses.missing.education) {
        city_data.houses.education = 2; // libraries
    } else if (city_data.houses.missing.more_education || city_data.houses.missing.education) {
        city_data.houses.education = 3; // more education
    }
    // entertainment
    city_data.houses.entertainment = 0;
    if (city_data.houses.missing.entertainment > city_data.houses.missing.more_entertainment) {
        city_data.houses.entertainment = 1;
    } else if (city_data.houses.missing.more_entertainment) {
        city_data.houses.entertainment = 2;
    }
    // religion
    city_data.houses.religion = 0;
    max = 0;
    const auto counts = missing_religions();
    for (size_t i = 0; i < counts.size(); ++i) {
        if (*counts[i] > max) {
            max = *counts[i];
            city_data.houses.religion = static_cast<int>(i + 1);
        }
    }
}
