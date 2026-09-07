#pragma once

#include "map/Terrain.h"
#include "core/buffer.h"
#include "map/grid.h"
#include <array>
#include <map>
#include <set>

class Building;

class TerrainMap {
public:
    TerrainMap(const TerrainMap &) = delete;
    TerrainMap &operator=(const TerrainMap &) = delete;
    int count(const TerrainSet &terrain);
    int contains(int grid_offset, const TerrainSet &terrain);

    int is_roadblock(int grid_offset);

    int contains_all(int grid_offset, const TerrainSet &terrains);

    const TerrainSet &at(int grid_offset) const;

    void set(int grid_offset, const TerrainSet &terrain);

    void add(int grid_offset, const TerrainSet &terrain);

    void remove(int grid_offset, const TerrainSet &terrain);

    void add_with_radius(int x, int y, int size, int radius, const TerrainSet &terrain);

    void remove_with_radius(int x, int y, int size, int radius, const TerrainSet &terrain);

    void remove_all(const TerrainSet &terrain);

    /**
    * Check orthogonal neighbours of a tile if they contain a terrain.
    * @param grid_offset Tile which neighbours will be checked.
    * @param terrain Terrain references to be checked for.
    * @return 1 if any orthogonal tiles matches at least one terrain from the set, 0 otherwise.
    */
    int count_directly_adjacent_with_type(int grid_offset, const TerrainSet &terrain);

    /**
    * Check orthogonal neighbours of a tile if they contain a terrain.
    * @param grid_offset Tile which neighbours will be checked.
    * @param terrains Terrain references to be checked for.
    * @return 1 if any orthogonal tiles matches all terrains from the set, 0 otherwise.
    */
    int count_directly_adjacent_with_types(int grid_offset, const TerrainSet &terrains);

    int count_diagonally_adjacent_with_type(int grid_offset, const TerrainSet &terrain);

    int has_adjacent_x_with_type(int grid_offset, const TerrainSet &terrain);

    int has_adjacent_y_with_type(int grid_offset, const TerrainSet &terrain);

    int exists_tile_in_area_with_type(int x, int y, int size, const TerrainSet &terrain);

    int exists_tile_in_radius_with_type(int x, int y, int size, int radius, const TerrainSet &terrain);

    // Chebyshev distance, or max_distance + 1 when no matching terrain is nearby.
    int distance_to_nearest(int grid_offset, const TerrainSet &terrain, int max_distance) const;

    int exists_rock_in_radius(int x, int y, int size, int radius);

    int exists_clear_tile_in_radius(int x, int y, int size, int radius, int except_grid_offset,
    int *x_tile, int *y_tile);

    int all_tiles_in_radius_are(int x, int y, int size, int radius, const TerrainSet &terrain);

    int has_only_rocks_trees_in_ring(int x, int y, int distance);

    int has_only_meadow_in_ring(int x, int y, int distance);

    int is_adjacent_to_wall(int x, int y, int size);

    int is_adjacent_to_water(int x, int y, int size);

    int get_adjacent_road_or_clear_land(
    const Building &building,
    int *x_tile,
    int *y_tile);

    void add_roadblock_road(int x, int y);
    void add_gatehouse_roads(int x, int y, int orientation);
    void add_triumphal_arch_roads(int x, int y, int orientation);

    void backup(void);

    void restore(void);

    void clear(void);

    void init_outside_map(void);

    void save_state(buffer *buf);
    void save_state_legacy(buffer *buf);

    void migrate_old_bridges(void);

    void migrate_old_walls(void);
    int validate_loaded_walls(void);

    void load_state(buffer *buf, int expanded_terrain_data, buffer *images, int legacy_image_buffer);

private:
    friend TerrainMap &terrain_map();
    TerrainMap() = default;
    const TerrainSet &intern(TerrainSet terrain);
    void count_changed(int offset, const TerrainSet &before, const TerrainSet &after);
    const TerrainSet empty_terrain;
    std::set<TerrainSet> compositions;
    std::array<const TerrainSet *, GRID_SIZE * GRID_SIZE> tiles{}, backup_tiles{};
    std::map<TerrainSet, int> terrain_counts;
};

TerrainMap &terrain_map();
