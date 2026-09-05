#pragma once

class Image;

int runtime_overlay_images_init_or_reload(void);
void runtime_overlay_images_reset(void);
const Image *runtime_footprint_overlay_image();

