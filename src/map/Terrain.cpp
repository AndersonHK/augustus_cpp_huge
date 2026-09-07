#include "map/Terrain.h"
#include "map/TerrainMap.h"

bool Terrain::exists_at(int grid_offset) const { return terrain_map().at(grid_offset).contains(*this); }
void Terrain::add_at(int grid_offset) const { terrain_map().add(grid_offset, *this); }
void Terrain::remove_at(int grid_offset) const { terrain_map().remove(grid_offset, *this); }

#include "map/TerrainRegistry.h"
#include "assets/image_group_payload.h"
#include "core/log.h"

const ImageGroupEntry *Terrain::water_image(WaterShoreShape shape, unsigned variation) const
{
    const auto &images = water_images_[static_cast<size_t>(shape)].bound_images;
    return images.empty() ? nullptr : images[variation % images.size()];
}

bool TerrainRegistry::bind_graphics()
{
    for (auto &[name, terrain] : definitions_) for (auto &rule : terrain->water_images_) {
        rule.bound_images.clear();
        for (const auto &image : rule.images) {
            const ImageGroupPayload *payload = image_group_payload_load(rule.group.c_str()) ? image_group_payload_get(rule.group.c_str()) : nullptr;
            const ImageGroupEntry *entry = payload ? payload->entry_for(image.c_str()) : nullptr;
            if (!entry) {
                const std::string detail = name + ": " + rule.group + "/" + image;
                log_error("Unable to bind terrain image", detail.c_str(), 0);
                return false;
            }
            rule.bound_images.push_back(entry);
        }
    }
    return true;
}
