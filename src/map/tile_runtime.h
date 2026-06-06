#pragma once

#include "building/building_type.h"
#include "graphics/runtime_texture.h"

#ifdef __cplusplus

#include <string>
#include <utility>

class tile_runtime {
public:
    tile_runtime(
        int grid_offset,
        const building_type_registry_impl::BuildingType *definition,
        std::string graphics_path)
        : grid_offset_(grid_offset)
        , definition_(definition)
        , graphics_path_(std::move(graphics_path))
    {
    }

    int grid_offset() const
    {
        return grid_offset_;
    }

    const building_type_registry_impl::BuildingType *definition() const
    {
        return definition_;
    }

    const char *graphics_path() const
    {
        return graphics_path_.c_str();
    }

    void set_plaza_image_id(const char *image_id)
    {
        plaza_image_id_ = image_id ? image_id : "";
    }

    const char *plaza_image_id() const
    {
        return plaza_image_id_.c_str();
    }

    const RuntimeDrawSlice *resolve_graphic_slice() const;

private:
    int grid_offset_ = -1;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    std::string graphics_path_;
    std::string plaza_image_id_;
};

namespace tile_runtime_impl {

tile_runtime *get_or_create_instance(int grid_offset, building_type_registry_impl::TileKind kind, const char *image_id);
tile_runtime *get_instance(int grid_offset);

}

#endif
