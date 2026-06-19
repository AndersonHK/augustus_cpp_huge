#include "graphics/image_group_reference.h"

#include "assets/image_group_payload.h"

#include "assets/assets.h"

int graphics_image_id_for_group_reference(const char *path, const char *image_name)
{
    if (!path || !*path) {
        return 0;
    }
    if (!image_name || !*image_name) {
        if (!image_group_payload_load(path)) {
            return 0;
        }
        const ImageGroupPayload *payload = image_group_payload_get(path);
        image_name = payload ? payload->default_image_id() : nullptr;
    }
    return assets_get_image_id_from_path_or_name(path, image_name);
}
