#pragma once

#include <stdint.h>

#ifdef __cplusplus
#include <array>
#include <memory>
#include <string>
#include <vector>
#endif

#ifdef __cplusplus
namespace building_type_registry_impl {

class WaterAccessType {
public:
    WaterAccessType(std::string text_id, uint8_t number_id);

    const char *text_id() const;
    uint8_t number_id() const;
    uint8_t mask() const;

private:
    std::string text_id_;
    uint8_t number_id_ = 0;
    uint8_t mask_ = 0;
};

const WaterAccessType *find_water_access_type(const char *text_id);
const WaterAccessType *water_access_type_from_number_id(uint8_t number_id);
uint8_t water_access_mask_from_text(const char *text_id);
uint8_t water_access_defined_mask(void);
const std::vector<std::unique_ptr<WaterAccessType>> &water_access_types(void);

} // namespace building_type_registry_impl
extern "C" {
#endif

int water_access_type_registry_load(void);
const char *water_access_type_text_from_number_id(uint8_t number_id);
uint8_t water_access_type_mask_from_text_id(const char *text_id);
uint8_t water_access_type_defined_mask_c(void);

#ifdef __cplusplus
}
#endif
