#include "game/resource_graphics.h"

extern "C" {
#include "core/crash_context.h"
}

#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

namespace {

constexpr int RESOURCE_GRAPHICS_COUNT = RESOURCE_SLOT_COUNT;
constexpr int RESOURCE_CART_MARKER_BASE = 0x3f000000;

std::array<ResourceGraphics, RESOURCE_GRAPHICS_COUNT> g_resource_graphics;

const char *CART_DIRECTION_SUFFIXES[8] = {
    "_NE",
    "_E",
    "_SE",
    "_S",
    "_SW",
    "_W",
    "_NW",
    "_N"
};

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

int normalize_cart_direction(int direction)
{
    direction %= 8;
    if (direction < 0) {
        direction += 8;
    }
    return direction;
}

int string_ends_with(const std::string &value, const char *suffix)
{
    const size_t suffix_length = std::strlen(suffix);
    return value.size() >= suffix_length &&
        value.compare(value.size() - suffix_length, suffix_length, suffix) == 0;
}

int replace_direction_suffix(std::string &value, int direction)
{
    for (const char *suffix : CART_DIRECTION_SUFFIXES) {
        if (!string_ends_with(value, suffix)) {
            continue;
        }
        value.replace(value.size() - std::strlen(suffix), std::string::npos, CART_DIRECTION_SUFFIXES[direction]);
        return 1;
    }
    return 0;
}

int numeric_image_entry_with_offset(const std::string &entry_id, int direction, std::string &out_entry_id)
{
    constexpr const char *prefix = "Image_";
    constexpr size_t prefix_length = 6;
    if (entry_id.size() <= prefix_length || entry_id.compare(0, prefix_length, prefix) != 0) {
        return 0;
    }

    int image_number = 0;
    for (size_t i = prefix_length; i < entry_id.size(); ++i) {
        if (!std::isdigit(static_cast<unsigned char>(entry_id[i]))) {
            return 0;
        }
        image_number = image_number * 10 + entry_id[i] - '0';
    }

    char buffer[32];
    snprintf(buffer, sizeof(buffer), "Image_%0*d",
        static_cast<int>(entry_id.size() - prefix_length),
        image_number + direction);
    out_entry_id = buffer;
    return 1;
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

ImageGroupEntryRef ResourceGraphics::cart_image_for_direction(
    int carried_loads,
    int use_food_eight_load_variant,
    int direction) const
{
    direction = normalize_cart_direction(direction);
    const ImageGroupEntryRef &base = cart_image(carried_loads, use_food_eight_load_variant);
    if (!base.is_bound()) {
        return base;
    }

    std::string group_path = base.group_path();
    std::string entry_id = base.entry_id();
    if (replace_direction_suffix(group_path, direction)) {
        replace_direction_suffix(entry_id, direction);
        return ImageGroupEntryRef::from_group(std::move(group_path), std::move(entry_id));
    }

    std::string directional_entry_id;
    if (numeric_image_entry_with_offset(entry_id, direction, directional_entry_id)) {
        return ImageGroupEntryRef::from_group(std::move(group_path), std::move(directional_entry_id));
    }

    return base;
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

int resource_graphics_cart_marker_for_direction(int direction)
{
    return RESOURCE_CART_MARKER_BASE + normalize_cart_direction(direction);
}

int resource_graphics_cart_marker_is(unsigned int image_id)
{
    return image_id >= RESOURCE_CART_MARKER_BASE && image_id < RESOURCE_CART_MARKER_BASE + 8;
}

int resource_graphics_cart_marker_direction(unsigned int image_id)
{
    return resource_graphics_cart_marker_is(image_id) ? static_cast<int>(image_id) - RESOURCE_CART_MARKER_BASE : 0;
}
