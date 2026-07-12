#pragma once

#include "building/building_type.h"
#include "graphics/runtime_texture.h"


#include <string>
#include <utility>

class ImageGroupEntry;

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

    void set_image_id(const char *image_id)
    {
        if (image_id_ != (image_id ? image_id : "")) {
            cached_entry_ = nullptr;
        }
        image_id_ = image_id ? image_id : "";
    }

    const char *image_id() const
    {
        return image_id_.c_str();
    }

    const ImageGroupEntry *cached_graphic_entry() const;
    const ImageGroupEntry *resolve_graphic_entry() const;
    const RuntimeDrawSlice *resolve_graphic_slice() const;
    const RuntimeDrawSlice *resolve_graphic_top_slice() const;

private:
    int grid_offset_ = -1;
    const building_type_registry_impl::BuildingType *definition_ = nullptr;
    std::string graphics_path_;
    std::string image_id_;
    mutable const ImageGroupEntry *cached_entry_ = nullptr;
};

namespace tile_runtime_impl {

tile_runtime *get_or_create_instance(int grid_offset, building_type_registry_impl::TileKind kind, const char *image_id);
tile_runtime *get_instance(int grid_offset);

}

