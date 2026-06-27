#pragma once


const char *figure_type_registry_get_failure_reason(void);
int figure_type_registry_load(void);
int figure_type_registry_resolve_building_references(void);
void figure_type_registry_reset(void);

