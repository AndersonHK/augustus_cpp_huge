#include "building/HousingProfileDef.h"

#include <utility>
#include <algorithm>
#include <cmath>
#include <numeric>
#include <limits>
#include "core/xml_value.h"

namespace building_type_registry_impl {

bool HousingGoodsRate::parse(const char *text, HousingGoodsRate &result)
{
    std::string value = xml_value::trim_copy(text ? text : "");
    const size_t slash = value.find('/');
    const size_t decimal = value.find('.');
    int numerator = 0;
    int denominator = 1;
    if (slash != std::string::npos) {
        if (!xml_value::parse_int_strict(value.substr(0, slash).c_str(), &numerator) ||
            !xml_value::parse_int_strict(value.substr(slash + 1).c_str(), &denominator)) return false;
    } else if (decimal != std::string::npos) {
        const size_t digits = value.size() - decimal - 1;
        if (!digits || digits > 6 || value.find('.', decimal + 1) != std::string::npos) return false;
        value.erase(decimal, 1);
        if (!xml_value::parse_int_strict(value.c_str(), &numerator)) return false;
        for (size_t i = 0; i < digits; ++i) denominator *= 10;
    } else if (!xml_value::parse_int_strict(value.c_str(), &numerator)) return false;
    if (numerator < 0 || denominator <= 0 || static_cast<int64_t>(numerator) > 10000LL * denominator) return false;
    const int divisor = std::gcd(numerator, denominator);
    result.numerator = numerator / divisor;
    result.denominator = denominator / divisor;
    return true;
}

double HousingGoodsRate::annual_amount(int population) const
{
    return static_cast<double>(static_cast<int64_t>(std::max(0, population)) * numerator) / denominator;
}

int HousingGoodsRate::consume(int population, int events_per_month, double &remainder) const
{
    if (!numerator || population <= 0 || events_per_month <= 0) return 0;
    const double due = remainder + annual_amount(population) / (12 * events_per_month);
    const int whole = static_cast<int>(std::floor(due + 1e-9));
    remainder = std::max(0.0, due - whole);
    return whole;
}

int HousingGoodsRate::stock_target(int population, int events_per_month, int buffered_events) const
{
    if (!numerator || events_per_month <= 0) return 0;
    const double amount = annual_amount(std::max(1, population)) * buffered_events / (12 * events_per_month);
    return static_cast<int>(std::clamp(std::ceil(amount), 1.0, static_cast<double>(std::numeric_limits<int16_t>::max())));
}

HousingProfileDef::HousingProfileDef(std::string path_value)
    : path_id(std::move(path_value))
{
}

} // namespace building_type_registry_impl
