#include "platform/render_2d_pipeline.h"

#include "core/config.h"

#include <math.h>

int Render2DPipeline::is_pixel_domain(render_domain domain) const
{
    switch (domain) {
        case RENDER_DOMAIN_PIXEL:
        case RENDER_DOMAIN_TOOLTIP_PIXEL:
        case RENDER_DOMAIN_SNAPSHOT_PIXEL:
            return 1;
        default:
            return 0;
    }
}

float Render2DPipeline::scale_for_domain(render_domain domain, int ui_scale_percentage) const
{
    switch (domain) {
        case RENDER_DOMAIN_UI:
        case RENDER_DOMAIN_TOOLTIP_UI:
        case RENDER_DOMAIN_SNAPSHOT_UI:
            return ui_scale_percentage > 0 ? ui_scale_percentage / 100.0f : 1.0f;
        case RENDER_DOMAIN_PIXEL:
        case RENDER_DOMAIN_TOOLTIP_PIXEL:
        case RENDER_DOMAIN_SNAPSHOT_PIXEL:
        default:
            return 1.0f;
    }
}

int Render2DPipeline::scale_logical_size(render_domain domain, int logical_size, int ui_scale_percentage) const
{
    float scale = scale_for_domain(domain, ui_scale_percentage);
    return (int) roundf(logical_size * scale);
}

static float logical_size_or_fallback(render_logical_unit fixed_size, float logical_size, int source_size)
{
    if (fixed_size > 0) {
        return static_cast<float>(fixed_size) / static_cast<float>(RENDER_LOGICAL_UNITS_PER_PIXEL);
    }
    return logical_size > 0.0f ? logical_size : (float) source_size;
}

static int configured_filter_override(image_filter *filter)
{
    switch (config_get(CONFIG_SCALE_FILTER)) {
        case CONFIG_SCALE_FILTER_NEAREST:
            *filter = IMAGE_FILTER_NEAREST;
            return 1;
        case CONFIG_SCALE_FILTER_LINEAR:
            *filter = IMAGE_FILTER_LINEAR;
            return 1;
        case CONFIG_SCALE_FILTER_BEST:
            *filter = IMAGE_FILTER_BEST;
            return 1;
        case CONFIG_SCALE_FILTER_AUTO:
        default:
            return 0;
    }
}

float Render2DPipeline::logical_width(const render_2d_request &request, const image &img) const
{
    return logical_size_or_fallback(request.fixed_logical_size.width, request.logical_width, img.width);
}

float Render2DPipeline::logical_height(const render_2d_request &request, const image &img) const
{
    return logical_size_or_fallback(request.fixed_logical_size.height, request.logical_height, img.height);
}

float Render2DPipeline::source_scale_x(const render_2d_request &request, const image &img) const
{
    float width = logical_width(request, img);
    return width > 0.0f ? img.width / width : 1.0f;
}

float Render2DPipeline::source_scale_y(const render_2d_request &request, const image &img) const
{
    float height = logical_height(request, img);
    return height > 0.0f ? img.height / height : 1.0f;
}

render_domain Render2DPipeline::tooltip_domain_for(render_domain domain) const
{
    return is_pixel_domain(domain) ? RENDER_DOMAIN_TOOLTIP_PIXEL : RENDER_DOMAIN_TOOLTIP_UI;
}

render_domain Render2DPipeline::snapshot_domain_for(render_domain domain) const
{
    return is_pixel_domain(domain) ? RENDER_DOMAIN_SNAPSHOT_PIXEL : RENDER_DOMAIN_SNAPSHOT_UI;
}

image_filter Render2DPipeline::configured_scale_filter(int platform_scale_percentage) const
{
    image_filter filter = IMAGE_FILTER_NEAREST;
    if (configured_filter_override(&filter)) {
        return filter;
    }
#ifndef __APPLE__
    return (platform_scale_percentage % 100) != 0 ? IMAGE_FILTER_LINEAR : IMAGE_FILTER_NEAREST;
#else
    return IMAGE_FILTER_LINEAR;
#endif
}

const char *Render2DPipeline::scale_quality_hint(image_filter filter) const
{
    switch (filter) {
        case IMAGE_FILTER_LINEAR:
            return "linear";
        case IMAGE_FILTER_BEST:
            return "best";
        case IMAGE_FILTER_NEAREST:
        default:
            return "nearest";
    }
}

image_filter Render2DPipeline::scale_filter(
    const render_2d_request &request,
    const image &img,
    float city_scale,
    int auto_force_nearest_filter) const
{
    image_filter filter = IMAGE_FILTER_NEAREST;
    if (configured_filter_override(&filter)) {
        return filter;
    }

    if (auto_force_nearest_filter) {
        return IMAGE_FILTER_NEAREST;
    }

    switch (request.scaling_policy) {
        case RENDER_SCALING_POLICY_PIXEL_ART:
            return IMAGE_FILTER_NEAREST;
        case RENDER_SCALING_POLICY_HIGH_QUALITY:
            return IMAGE_FILTER_BEST;
        case RENDER_SCALING_POLICY_AUTO:
        default:
            break;
    }

    float scale_x = source_scale_x(request, img);
    float scale_y = source_scale_y(request, img);
    float rounded_x = roundf(scale_x);
    float rounded_y = roundf(scale_y);

    if (scale_x > 1.0f || scale_y > 1.0f) {
        return IMAGE_FILTER_LINEAR;
    }

    if (fabsf(scale_x - rounded_x) < 0.001f && fabsf(scale_y - rounded_y) < 0.001f) {
        return IMAGE_FILTER_NEAREST;
    }

    if (fabsf(scale_x - city_scale) < 0.001f && fabsf(scale_y - city_scale) < 0.001f) {
        return IMAGE_FILTER_NEAREST;
    }

    return IMAGE_FILTER_LINEAR;
}
