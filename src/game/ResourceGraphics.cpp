#include "game/ResourceGraphics.h"

#include "core/crash_context.h"

#include <array>
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
