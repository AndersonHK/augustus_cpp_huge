#pragma once

#include "core/image.h"
#include "graphics/renderer.h"

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
#include <string>
#include <utility>

namespace vespasian::graphics::extraction {

class GroupImageKey {
public:
    GroupImageKey() = default;
    GroupImageKey(std::string group_key, std::string image_id)
        : group_key_(std::move(group_key))
        , image_id_(std::move(image_id))
    {
    }

    const std::string &group_key() const { return group_key_; }
    const std::string &image_id() const { return image_id_; }
    bool valid() const { return !group_key_.empty() && !image_id_.empty(); }

private:
    std::string group_key_;
    std::string image_id_;
};

class LegacyClimateAtlas {
public:
    LegacyClimateAtlas(
        const image *images,
        int image_count,
        const uint16_t *group_image_ids,
        int group_count,
        const char *source_name,
        const image_atlas_data *atlas_data)
        : images_(images)
        , image_count_(image_count)
        , group_image_ids_(group_image_ids)
        , group_count_(group_count)
        , source_name_(source_name)
        , atlas_data_(atlas_data)
    {
    }

    bool valid() const
    {
        return images_ && image_count_ > 0 && group_image_ids_ && group_count_ > 0 &&
            source_name_ && *source_name_ && atlas_data_;
    }
    const image *images() const { return images_; }
    int image_count() const { return image_count_; }
    const uint16_t *group_image_ids() const { return group_image_ids_; }
    int group_count() const { return group_count_; }
    const char *source_name() const { return source_name_; }
    const image_atlas_data *atlas_data() const { return atlas_data_; }

private:
    const image *images_ = nullptr;
    int image_count_ = 0;
    const uint16_t *group_image_ids_ = nullptr;
    int group_count_ = 0;
    const char *source_name_ = nullptr;
    const image_atlas_data *atlas_data_ = nullptr;
};

class JuliusExtractionReport {
public:
    JuliusExtractionReport() = default;
    JuliusExtractionReport(bool succeeded, int groups_exported, int images_exported, int pngs_written)
        : succeeded_(succeeded)
        , groups_exported_(groups_exported)
        , images_exported_(images_exported)
        , pngs_written_(pngs_written)
    {
    }

    bool succeeded() const { return succeeded_; }
    int groups_exported() const { return groups_exported_; }
    int images_exported() const { return images_exported_; }
    int pngs_written() const { return pngs_written_; }

private:
    bool succeeded_ = false;
    int groups_exported_ = 0;
    int images_exported_ = 0;
    int pngs_written_ = 0;
};

class JuliusExtractor {
public:
    JuliusExtractionReport extract(const LegacyClimateAtlas &climate);
    std::string resolveLegacyGroup(int group_id) const;
    GroupImageKey resolveLegacyImage(int group_id, int image_offset) const;
};

} // namespace vespasian::graphics::extraction
#endif
