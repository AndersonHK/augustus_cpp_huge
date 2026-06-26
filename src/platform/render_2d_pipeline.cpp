#include "platform/render_2d_pipeline.h"

#include "core/config.h"

#include <math.h>

static int is_pixel_domain(render_domain domain)
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

float Render2DPipeline::source_scale_x(const render_2d_request &request, const image &img) const
{
    float logical_width = request.logical_width > 0.0f ? request.logical_width : (float) img.width;
    return logical_width > 0.0f ? img.width / logical_width : 1.0f;
}

float Render2DPipeline::source_scale_y(const render_2d_request &request, const image &img) const
{
    float logical_height = request.logical_height > 0.0f ? request.logical_height : (float) img.height;
    return logical_height > 0.0f ? img.height / logical_height : 1.0f;
}

render_domain Render2DPipeline::tooltip_domain_for(render_domain domain) const
{
    return is_pixel_domain(domain) ? RENDER_DOMAIN_TOOLTIP_PIXEL : RENDER_DOMAIN_TOOLTIP_UI;
}

render_domain Render2DPipeline::snapshot_domain_for(render_domain domain) const
{
    return is_pixel_domain(domain) ? RENDER_DOMAIN_SNAPSHOT_PIXEL : RENDER_DOMAIN_SNAPSHOT_UI;
}

image_filter Render2DPipeline::scale_filter(
    const render_2d_request &request,
    const image &img,
    float city_scale,
    int auto_force_nearest_filter) const
{
    switch (config_get(CONFIG_SCALE_FILTER)) {
        case CONFIG_SCALE_FILTER_NEAREST:
            return IMAGE_FILTER_NEAREST;
        case CONFIG_SCALE_FILTER_LINEAR:
            return IMAGE_FILTER_LINEAR;
        case CONFIG_SCALE_FILTER_BEST:
            return IMAGE_FILTER_BEST;
        case CONFIG_SCALE_FILTER_AUTO:
        default:
            break;
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
