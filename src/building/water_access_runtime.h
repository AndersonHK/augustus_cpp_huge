#pragma once


class Building;

namespace building_type_registry_impl {
class BuildingType;
}

void water_access_runtime_reset(void);
void water_access_runtime_refresh(void);
void water_access_runtime_refresh_building(Building *building);

int water_access_runtime_range_for_building(const building_type_registry_impl::BuildingType *definition);
const char *water_access_runtime_primary_provider_access_text(const building_type_registry_impl::BuildingType *definition);
int water_access_runtime_building_type_provides_access(const building_type_registry_impl::BuildingType *definition);
int water_access_runtime_building_type_provides_access_text(
    const building_type_registry_impl::BuildingType *definition,
    const char *text_id);
int water_access_runtime_building_type_requires_access_text(
    const building_type_registry_impl::BuildingType *definition,
    const char *text_id);
int water_access_runtime_tile_has_access(int grid_offset, const char *text_id);
int water_access_runtime_building_area_has_access(const Building *building, const char *text_id);
int water_access_runtime_building_has_required_access(const Building *building);
int water_access_runtime_building_type_has_required_access_at(
    const building_type_registry_impl::BuildingType *definition,
    int x,
    int y,
    int size);
int water_access_runtime_reservoir_has_network_access(int grid_offset);

void water_access_runtime_begin_preview(
    const building_type_registry_impl::BuildingType *definition,
    int primary_grid_offset,
    int secondary_grid_offset);
void water_access_runtime_end_preview(void);
int water_access_runtime_tile_has_preview_highlight(int grid_offset);
int water_access_runtime_should_draw_overlay_at(int grid_offset);

