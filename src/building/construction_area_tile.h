#pragma once

#include "building/building_type.h"

#include <vector>

class ConstructionAreaTilePlacement {
public:
    ConstructionAreaTilePlacement(
        int x_start,
        int y_start,
        int x_end,
        int y_end,
        building_type type,
        bool preview);

    int place();
    int support_cost() const { return support_cost_; }

    static bool is_area_tile_type(building_type type);
    static bool updates_land_routing(building_type type);
    static void restore_preview_map(building_type type);

private:
    struct Region {
        int x_min = 0;
        int y_min = 0;
        int x_max = 0;
        int y_max = 0;
    };

    class GardenTileSet {
    public:
        int placed_count() const;
        void add(int grid_offset);
        void refresh_preview_tiles() const;

    private:
        std::vector<int> grid_offsets_;
    };

    int x_min_ = 0;
    int y_min_ = 0;
    int x_max_ = 0;
    int y_max_ = 0;
    building_type type_ = BUILDING_NONE;
    bool preview_ = false;
    int support_cost_ = 0;

    Region region() const;

    static const building_type_registry_impl::TileDefinition *tile_definition_for_type(building_type type);
    static bool is_garden_area_tile(const building_type_registry_impl::TileDefinition &tile);
    static void refresh_area_tile_after_restore(const building_type_registry_impl::TileDefinition &tile);
    static void refresh_garden_region(
        const Region &region,
        const building_type_registry_impl::TileDefinition &tile);
    static int place_garden_area(
        const Region &region,
        const building_type_registry_impl::TileDefinition &tile,
        bool preview);
    int place_plaza_area(const Region &region);
};
