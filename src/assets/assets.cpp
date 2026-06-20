#include "assets/image.h"
#include "assets/group.h"
#include "assets/xml.h"

#include "assets.h"

#include "core/file.h"
#include "core/dir.h"
#include "core/log.h"
#include "core/png_read.h"
#include "graphics/renderer.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static struct {
    int roadblock_image_id;
    asset_image *roadblock_image;
    int asset_lookup[ASSET_MAX_KEY];
    int font_lookup[ASSET_FONT_MAX_KEY];
    char failure_reason[512];
} data;

static void set_failure_reason(const char *reason, const char *detail)
{
    if (detail && *detail) {
        snprintf(data.failure_reason, sizeof(data.failure_reason), "%s\n\n%s", reason, detail);
    } else {
        snprintf(data.failure_reason, sizeof(data.failure_reason), "%s", reason);
    }
}

int assets_init(int force_reload, color_t **main_images, int *main_image_widths)
{
    data.failure_reason[0] = '\0';

    if (graphics_renderer()->has_image_atlas(ATLAS_EXTRA_ASSET) && !force_reload) {
        asset_image_reload_climate();
        return 1;
    }

    graphics_renderer()->free_image_atlas(ATLAS_EXTRA_ASSET);
    memset(data.asset_lookup, 0, sizeof(data.asset_lookup));
    memset(data.font_lookup, 0, sizeof(data.font_lookup));
    data.roadblock_image_id = 0;
    data.roadblock_image = nullptr;

    if (!group_create_all(0) || !asset_image_init_array()) {
        log_error("Not enough memory to initialize extra assets. The game will probably crash.", 0, 0);
        set_failure_reason("Not enough memory to initialize extra assets.", 0);
        return 0;
    }

    if (!asset_image_load_all(main_images, main_image_widths)) {
        set_failure_reason("Failed to build or upload extra asset graphics.", 0);
        return 0;
    }

    group_set_for_external_files();
    return 1;
}

int assets_load_single_group(const char *file_name, color_t **main_images, int *main_image_widths)
{
    data.failure_reason[0] = '\0';
    if (!group_create_all(1) || !asset_image_init_array()) {
        log_error("Not enough memory to initialize extra assets. The game will probably crash.", 0, 0);
        set_failure_reason("Not enough memory to initialize extra assets.", 0);
        return 0;
    }
    xml_init();
    graphics_renderer()->free_image_atlas(ATLAS_EXTRA_ASSET);
    if (!xml_process_assetlist_file(file_name)) {
        char detail[FILE_NAME_MAX + 64];
        snprintf(detail, sizeof(detail), "Asset list: %s", file_name);
        set_failure_reason("Failed to parse or load an asset list.", detail);
        return 0;
    }
    if (!asset_image_load_all(main_images, main_image_widths)) {
        set_failure_reason("Failed to build or upload extra asset graphics.", 0);
        return 0;
    }
    return 1;
}

int assets_get_group_id(const char *assetlist_name)
{
    image_groups *group = group_get_from_name(assetlist_name);
    if (group) {
        return group->first_image_index + IMAGE_MAIN_ENTRIES;
    }
    log_info("Asset group not found: ", assetlist_name, 0);
    return data.roadblock_image_id;
}

static int image_id_from_group(const image_groups *group, const char *image_name)
{
    if (!group || !image_name || !*image_name ||
        group->first_image_index < 0 || group->last_image_index < group->first_image_index) {
        return 0;
    }

    const asset_image *img = asset_image_get_from_id(group->first_image_index);
    while (img && img->index <= (unsigned int) group->last_image_index) {
        if (img->id && strcmp(img->id, image_name) == 0) {
            return img->index + IMAGE_MAIN_ENTRIES;
        }
        img = asset_image_get_from_id(img->index + 1);
    }
    return 0;
}

int assets_get_image_id(const char *assetlist_name, const char *image_name)
{
    if (!image_name || !*image_name) {
        return data.roadblock_image_id;
    }
    image_groups *group = group_get_from_name(assetlist_name);
    if (!group) {
        char detail[256];
        snprintf(detail, sizeof(detail), "%s image=%s", assetlist_name ? assetlist_name : "", image_name);
        log_info("Asset group not found: ", detail, 0);
        return data.roadblock_image_id;
    }
    int image_id = image_id_from_group(group, image_name);
    if (image_id) {
        return image_id;
    }
    char detail[256];
    snprintf(detail, sizeof(detail), "%s image=%s", assetlist_name ? assetlist_name : "", image_name);
    log_info("Asset image not found: ", detail, 0);
    return data.roadblock_image_id;
}

int assets_get_image_id_by_name(const char *image_name)
{
    if (!image_name || !*image_name) {
        return 0;
    }
    for (int group_id = 0; group_id < group_get_total(); group_id++) {
        int image_id = image_id_from_group(group_get_from_id(group_id), image_name);
        if (image_id) {
            return image_id;
        }
    }
    return 0;
}

int assets_get_external_image(const char *path, int force_reload)
{
    if (!path || !*path) {
        return 0;
    }
    image_groups *group = group_get_from_name(ASSET_EXTERNAL_FILE_LIST);
    asset_image *img = asset_image_get_from_id(group->first_image_index);
    int was_found = 0;
    while (img && img->index <= (unsigned int) group->last_image_index) {
        if (img->id && strcmp(img->id, path) == 0) {
            if (!force_reload) {
                return img->index + IMAGE_MAIN_ENTRIES;
            } else {
                was_found = 1;
                break;
            }
        }
        img = asset_image_get_from_id(img->index + 1);
    }
    if (was_found) {
        graphics_renderer()->free_unpacked_image(&img->img);
        asset_image_unload(img);
    }
    const asset_image *new_img = asset_image_create_external(path);
    if (!new_img) {
        return 0;
    }
    if (group->first_image_index == -1) {
        group->first_image_index = new_img->index;
    }
    group->last_image_index = new_img->index;
    return new_img->index + IMAGE_MAIN_ENTRIES;
}

int assets_lookup_image_id(asset_id id)
{
    return data.asset_lookup[id];
}

const image *assets_get_image(int image_id)
{
    asset_image *img = asset_image_get_from_id(image_id - IMAGE_MAIN_ENTRIES);
    if (!img) {
        img = data.roadblock_image;
    }
    if (!img) {
        return image_get(0);
    }
    return &img->img;
}

const image *assets_get_font_image(int letter_id)
{
    const image *img = image_get(data.font_lookup[letter_id - IMAGE_FONT_CUSTOM_OFFSET]);
    return img; // offset used to differentiate from normal letters
}

void assets_load_unpacked_asset(int image_id)
{
    asset_image *img = asset_image_get_from_id(image_id - IMAGE_MAIN_ENTRIES);
    if (!img || img->img.resource_handle) {
        return;
    }
    const color_t *pixels;
    if (img->is_reference) {
        asset_image *referenced_asset =
            asset_image_get_from_id(img->first_layer.calculated_image_id - IMAGE_MAIN_ENTRIES);
        pixels = referenced_asset->data;
    } else {
        pixels = img->data;
    }
    graphics_renderer()->load_unpacked_image(&img->img, pixels);
}

const char *assets_get_failure_reason(void)
{
    return data.failure_reason;
}
