#pragma once

#ifdef __cplusplus
extern "C" {
#endif

typedef struct building building;
// Lifecycle hooks that remain callable from legacy C systems while runtime graphics move through C++-only APIs.
void building_runtime_reset(void);
// After save load/new city init, prime runtime wrappers for all live building instances and rebuild native graphics/storage/production state.
void building_runtime_initialize_city_graphics_cache(void);
void building_runtime_apply_graphic(building *b);
// Returns true when XML runtime graphics own tile image bookkeeping; draw code then reads payload slices instead of map image ids.
int building_runtime_apply_graphic_if_native(building *b);
// Clamp or reseed the saved variant byte for native graphics options.
// force_reseed is for new buildings and old saves whose variant byte predates native option selection.
void building_runtime_assign_graphic_variant(building *b, int force_reseed);
void building_runtime_spawn_figure(building *b);

#ifdef __cplusplus
}
#endif
