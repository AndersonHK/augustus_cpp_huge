#pragma once

#include <stdint.h>


const char *building_type_legacy_migration_text_id_for_enum(uint16_t legacy_type);
uint16_t building_type_legacy_migration_enum_for_text_id(const char *text_id);
int building_type_legacy_migration_text_id_is_xml_owned(const char *text_id);

