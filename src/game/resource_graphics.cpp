#include "game/resource_graphics.h"

extern "C" {
#include "core/crash_context.h"
}

#include <cstdio>
#include <utility>

namespace {

constexpr int RESOURCE_GRAPHICS_COUNT = RESOURCE_SLOT_COUNT;

std::array<ResourceGraphics, RESOURCE_GRAPHICS_COUNT> g_resource_graphics;

const char *resource_name(resource_type resource)
{
    const resource_data *data = resource_get_data(resource);
    return data && data->xml_attr_name ? data->xml_attr_name : "unknown";
}

void report_invalid_resource_graphic(const char *message, resource_type resource, int value)
{
    char detail[128];
    snprintf(detail, sizeof(detail), "%s resource=%d value=%d", resource_name(resource), resource, value);
    error_context_report_error(message, detail);
}

} // namespace

void ResourceGraphics::set_storage_images(std::array<ImageGroupEntryRef, 4> images)
{
    storage_images_ = std::move(images);
}

void ResourceGraphics::set_cart_images(
    ImageGroupEntryRef single_load,
    ImageGroupEntryRef multiple_loads,
    ImageGroupEntryRef eight_loads)
{
    cart_single_load_ = std::move(single_load);
    cart_multiple_loads_ = std::move(multiple_loads);
    cart_eight_loads_ = std::move(eight_loads);
}

void ResourceGraphics::set_panel_icon(ImageGroupEntryRef image)
{
    panel_icon_ = std::move(image);
}

void ResourceGraphics::set_empire_icon(ImageGroupEntryRef image)
{
    empire_icon_ = std::move(image);
}

void ResourceGraphics::set_editor_icon(ImageGroupEntryRef image)
{
    editor_icon_ = std::move(image);
}

void ResourceGraphics::set_editor_empire_icon(ImageGroupEntryRef image)
{
    editor_empire_icon_ = std::move(image);
}

const ImageGroupEntryRef &ResourceGraphics::empty_cart_image()
{
    static const ImageGroupEntryRef image = ImageGroupEntryRef::from_group("Walkers\\Group_097", "Image_0000");
    return image;
}

const ImageGroupEntryRef &ResourceGraphics::empty_storage_image()
{
    static const ImageGroupEntryRef image = ImageGroupEntryRef::from_group(
        "Industry\\Warehouse_Storage_Empty",
        "Image_0000");
    return image;
}

const ImageGroupEntryRef &ResourceGraphics::storage_image(int loads) const
{
    if (loads <= 0) {
        return empty_storage_image();
    }
    if (loads > static_cast<int>(storage_images_.size())) {
        report_invalid_resource_graphic("Invalid resource storage graphic load", RESOURCE_NONE, loads);
        return empty_storage_image();
    }
    return storage_images_[static_cast<size_t>(loads - 1)];
}

const ImageGroupEntryRef &ResourceGraphics::cart_image(int carried_loads, int use_food_eight_load_variant) const
{
    if (carried_loads <= 0) {
        return empty_cart_image();
    }
    if (carried_loads == 1) {
        return cart_single_load_;
    }
    if (use_food_eight_load_variant && carried_loads >= 8) {
        return cart_eight_loads_;
    }
    return cart_multiple_loads_;
}

const ImageGroupEntryRef &ResourceGraphics::panel_icon() const
{
    return panel_icon_;
}

const ImageGroupEntryRef &ResourceGraphics::empire_icon() const
{
    return empire_icon_;
}

const ImageGroupEntryRef &ResourceGraphics::editor_icon() const
{
    return editor_icon_;
}

const ImageGroupEntryRef &ResourceGraphics::editor_empire_icon() const
{
    return editor_empire_icon_;
}

const ResourceGraphics &resource_graphics(resource_type resource)
{
    if (resource < RESOURCE_NONE || resource >= RESOURCE_GRAPHICS_COUNT) {
        report_invalid_resource_graphic("Invalid resource graphics lookup", resource, 0);
        return g_resource_graphics[RESOURCE_NONE];
    }
    return g_resource_graphics[resource];
}

ResourceGraphics &mutable_resource_graphics(resource_type resource)
{
    if (resource < RESOURCE_NONE || resource >= RESOURCE_GRAPHICS_COUNT) {
        report_invalid_resource_graphic("Invalid mutable resource graphics lookup", resource, 0);
        return g_resource_graphics[RESOURCE_NONE];
    }
    return g_resource_graphics[resource];
}

void resource_graphics_reset()
{
    g_resource_graphics = {};
}
