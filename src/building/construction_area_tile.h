#pragma once

#include "building/building_type.h"

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

    static bool is_area_tile_type(building_type type);
    static bool updates_land_routing(building_type type);
    static void restore_preview_map(building_type type);

private:
    struct Region {
        bool active = false;
        int x_min = 0;
        int y_min = 0;
        int x_max = 0;
        int y_max = 0;

        void include(const Region &other);
    };

    class GardenPreviewState {
    public:
        Region consume();
        void remember(const Region &region);

    private:
        Region region_;
    };

    int x_min_ = 0;
    int y_min_ = 0;
    int x_max_ = 0;
    int y_max_ = 0;
    building_type type_ = BUILDING_NONE;
    bool preview_ = false;

    Region region() const;

    static GardenPreviewState &garden_preview();
    static const building_type_registry_impl::TileDefinition *tile_definition_for_type(building_type type);
    static const building_type_registry_impl::TileDefinition *tile_definition_for_kind(
        building_type_registry_impl::TileKind kind);
    static const building_type_registry_impl::TileDefinition *garden_tile_definition();
    static bool is_garden_area_tile(const building_type_registry_impl::TileDefinition &tile);
    static void refresh_area_tile_after_restore(const building_type_registry_impl::TileDefinition &tile);
    static void refresh_garden_region(
        const Region &region,
        const building_type_registry_impl::TileDefinition &tile);
    static void refresh_garden_preview_region(const Region &region);
    static int place_garden_area(
        const Region &region,
        const building_type_registry_impl::TileDefinition &tile,
        bool preview);
    static int place_plaza_area(const Region &region);
};
