#include "image.h"

extern "C" {
#include "assets/assets.h"
#include "core/crash_context.h"
#include "core/png_read.h"
#include "graphics/renderer.h"
#include "graphics/screen.h"
}

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

using ImageByKey = std::unordered_map<std::string, std::unique_ptr<Image>>;
using ImageByLegacy = std::unordered_map<const image *, std::unique_ptr<Image>>;

ImageByKey g_keyed_images;
ImageByLegacy g_legacy_images;

std::string make_key(std::string_view path_key)
{
    return std::string(path_key.data(), path_key.size());
}

Image *image_from_payload(const image &legacy_image)
{
    return static_cast<Image *>(legacy_image.resource_payload);
}

void report_invalid_image(const char *message, int id)
{
    char detail[64];
    snprintf(detail, sizeof(detail), "%d", id);
    error_context_report_error(message, detail);
}

void release_renderer_handle(image_handle handle)
{
    if (handle <= 0) {
        return;
    }
    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->release_image_resource) {
        return;
    }
    image temp = {};
    temp.resource_handle = handle;
    renderer->release_image_resource(&temp);
}

render_scaling_policy scaling_policy_for_domain(render_domain domain)
{
    return domain == RENDER_DOMAIN_PIXEL ||
            domain == RENDER_DOMAIN_TOOLTIP_PIXEL ||
            domain == RENDER_DOMAIN_SNAPSHOT_PIXEL ?
        RENDER_SCALING_POLICY_PIXEL_ART :
        RENDER_SCALING_POLICY_AUTO;
}

render_2d_request make_request(const image &img, int x, int y, color_t color, float scale, int silhouette)
{
    render_domain domain = graphics_renderer()->get_render_domain();
    render_2d_request request = {};
    request.img = &img;
    request.handle = img.resource_handle;
    request.x = scale ? x / scale : static_cast<float>(x);
    request.y = scale ? y / scale : static_cast<float>(y);
    request.logical_width = scale ? img.width / scale : static_cast<float>(img.width);
    request.logical_height = scale ? img.height / scale : static_cast<float>(img.height);
    request.color = color;
    request.domain = domain;
    request.scaling_policy = scaling_policy_for_domain(domain);
    request.use_silhouette = silhouette;
    return request;
}

color_t base_color_for_font(font_t font)
{
    switch (font) {
        case FONT_SMALL_PLAIN:
        case FONT_NORMAL_PLAIN:
        case FONT_LARGE_PLAIN:
            return COLOR_FONT_PLAIN;
        default:
            return COLOR_MASK_NONE;
    }
}

color_t color_with_alpha(color_t color, color_t alpha)
{
    return alpha >= 0xff ? color : color & ((alpha << COLOR_BITSHIFT_ALPHA) | COLOR_CHANNEL_RGB);
}

struct FullscreenImagePlacement {
    int x = 0;
    int y = 0;
    float scale = SCALE_NONE;
};

FullscreenImagePlacement fullscreen_placement_for(const Image &image, int x_offset = 0, int y_offset = 0)
{
    const int width = screen_width();
    const int height = screen_height();
    const float scale_w = image.width() / static_cast<float>(width);
    const float scale_h = image.height() / static_cast<float>(height);
    const float scale = scale_w < scale_h ? scale_w : scale_h;

    if (scale >= SCALE_NONE) {
        return { (width - image.width()) / 2 + x_offset, (height - image.height()) / 2 + y_offset, SCALE_NONE };
    }

    int x = x_offset;
    int y = y_offset;
    if (scale == scale_h) {
        x = static_cast<int>((x + width - image.width() / scale) / 2 * scale);
    }
    if (scale == scale_w) {
        y = static_cast<int>((y + height - image.height() / scale) / 2 * scale);
    }
    return { x, y, scale };
}

void draw_fullscreen_fit(const Image &image, int x_offset, int y_offset, color_t color)
{
    const FullscreenImagePlacement placement = fullscreen_placement_for(image, x_offset, y_offset);
    if (placement.scale == SCALE_NONE) {
        image.draw(placement.x, placement.y, color);
        return;
    }
    graphics_renderer()->draw_image(&image.legacy(), placement.x, placement.y, color, placement.scale);
}

} // namespace

ImageManager &image_manager()
{
    static ImageManager manager;
    return manager;
}

Image::Image(const image &legacy_image)
{
    sync_from_legacy(legacy_image);
}

Image &Image::from_id(int image_id)
{
    return image_manager().from_id(image_id);
}

Image &Image::from_legacy(image &legacy_image)
{
    return image_manager().from_legacy(legacy_image);
}

const Image &Image::from_legacy(const image &legacy_image)
{
    return image_manager().from_legacy(legacy_image);
}

Image &Image::letter(int letter_id)
{
    const image *img = image_letter(letter_id);
    if (!img) {
        report_invalid_image("Invalid font image", letter_id);
        return image_manager().from_id(0);
    }
    Image &letter_image = image_manager().from_legacy(*const_cast<image *>(img));
    letter_image.set_source_image_id(letter_id);
    return letter_image;
}

Image &Image::enemy(int image_id)
{
    const image *img = image_get_enemy(image_id);
    if (!img) {
        report_invalid_image("Invalid enemy image", image_id);
        return image_manager().from_id(0);
    }
    Image &enemy_image = image_manager().from_legacy(*const_cast<image *>(img));
    enemy_image.set_source_image_id(image_id);
    return enemy_image;
}

int Image::group(int group_id)
{
    return image_group(group_id);
}

int Image::load_climate(int climate_id, int is_editor, int force_reload, int keep_atlas_buffers, int extract_legacy_graphics)
{
    return image_load_climate(climate_id, is_editor, force_reload, keep_atlas_buffers, extract_legacy_graphics);
}

int Image::load_fonts(encoding_type encoding)
{
    return image_load_fonts(encoding);
}

int Image::load_enemy_graphics(int enemy_id)
{
    return image_load_enemy(enemy_id);
}

void Image::copy(const image_copy_info &copy_info)
{
    image_copy(&copy_info);
}

void Image::copy_isometric_footprint(const image_copy_info &copy_info)
{
    image_copy_isometric_footprint(&copy_info);
}

void Image::blend_footprint_color(int x, int y, color_t color, float scale)
{
    graphics_renderer()->draw_custom_image(color == COLOR_MASK_GREEN ?
        CUSTOM_IMAGE_GREEN_FOOTPRINT : CUSTOM_IMAGE_RED_FOOTPRINT, x, y, scale, 0);
}

const std::string &Image::key() const
{
    return key_;
}

const char *Image::key_c_str() const
{
    return key_.empty() ? nullptr : key_.c_str();
}

image_handle Image::render_handle() const
{
    return legacy_image_.resource_handle;
}

int Image::ref_count() const
{
    return ref_count_;
}

int Image::x_offset() const
{
    return legacy_image_.x_offset;
}

int Image::y_offset() const
{
    return legacy_image_.y_offset;
}

int Image::width() const
{
    return legacy_image_.width;
}

int Image::height() const
{
    return legacy_image_.height;
}

int Image::original_width() const
{
    return legacy_image_.original.width;
}

int Image::original_height() const
{
    return legacy_image_.original.height;
}

int Image::is_isometric() const
{
    return legacy_image_.is_isometric;
}

const image_animation *Image::animation() const
{
    return legacy_image_.animation;
}

image_animation *Image::animation()
{
    return legacy_image_.animation;
}

const Image *Image::top() const
{
    return legacy_image_.top ? &image_manager().from_legacy(*legacy_image_.top) : nullptr;
}

Image *Image::top()
{
    return legacy_image_.top ? &image_manager().from_legacy(*legacy_image_.top) : nullptr;
}

RuntimeDrawSlice Image::runtime_slice() const
{
    RuntimeDrawSlice slice;
    slice.handle = render_handle();
    slice.width = width();
    slice.height = height();
    slice.draw_offset_x = x_offset();
    slice.draw_offset_y = y_offset();
    slice.is_isometric = is_isometric();
    return slice;
}

const image &Image::legacy() const
{
    return legacy_image_;
}

image &Image::mutable_legacy_for_image_subsystem()
{
    return legacy_image_;
}

void Image::sync_from_legacy(const image &legacy_image)
{
    image *old_bound_legacy = bound_legacy_;
    int old_source_image_id = source_image_id_;
    legacy_image_ = legacy_image;
    bound_legacy_ = old_bound_legacy;
    source_image_id_ = old_source_image_id;
    if (!key_.empty()) {
        legacy_image_.resource_key = const_cast<char *>(key_.c_str());
        legacy_image_.resource_payload = this;
    }
}

void Image::set_render_handle(image_handle handle)
{
    legacy_image_.resource_handle = handle;
    if (bound_legacy_) {
        bound_legacy_->resource_handle = handle;
    }
}

void Image::retain()
{
    ++ref_count_;
}

int Image::release_ref()
{
    if (ref_count_ > 0) {
        --ref_count_;
    }
    return ref_count_;
}

void Image::release_renderer_resource()
{
    release_renderer_handle(render_handle());
    set_render_handle(0);
}

int Image::is_external() const
{
    return image_is_external(&legacy_image_);
}

void Image::load_external_data() const
{
    image_load_external_data(&legacy_image_);
}

int Image::get_external_dimensions(int &out_width, int &out_height) const
{
    return image_get_external_dimensions(&legacy_image_, &out_width, &out_height);
}

void Image::crop(const color_t *pixels)
{
    image_crop(&legacy_image_, pixels);
    if (bound_legacy_) {
        bound_legacy_->x_offset = legacy_image_.x_offset;
        bound_legacy_->y_offset = legacy_image_.y_offset;
        bound_legacy_->width = legacy_image_.width;
        bound_legacy_->height = legacy_image_.height;
    }
}

void Image::draw(int x, int y, color_t color, float scale) const
{
    ensure_ready_to_draw();
    render_2d_request request = make_request(legacy_image_, x, y, color, scale, 0);
    graphics_renderer()->draw_image_request(&request);
}

void Image::draw_silhouette(int x, int y, color_t color, float scale) const
{
    ensure_ready_to_draw();
    render_2d_request request = make_request(legacy_image_, x, y, color, scale, 1);
    graphics_renderer()->draw_image_request(&request);
}

void Image::draw_scaled_centered(int x, int y, color_t color, int draw_scale_percent) const
{
    float obj_draw_scale = 100.0f / draw_scale_percent;
    float scaled_x = (x + width() / 2.0f - (width() / obj_draw_scale) / 2.0f) * obj_draw_scale;
    float scaled_y = (y + height() / 2.0f - (height() / obj_draw_scale) / 2.0f) * obj_draw_scale;
    draw(static_cast<int>(scaled_x), static_cast<int>(scaled_y), color, obj_draw_scale);
}

void Image::draw_letter(font_t font, int x, int y, color_t color, float scale) const
{
    const font_definition *def = font_definition_for(font);
    int metric_scale = def->metric_scale_percentage > 0 ? def->metric_scale_percentage : 100;
    float base_scale = 100.0f / metric_scale;

    if (source_image_id_ >= IMAGE_FONT_MULTIBYTE_OFFSET) {
        switch (font) {
            case FONT_NORMAL_WHITE:
            {
                render_2d_request shadow = make_request(legacy_image_, x + 1, y + 1, 0xff311c10, base_scale * scale, 0);
                render_2d_request main = make_request(legacy_image_, x, y, COLOR_WHITE, base_scale * scale, 0);
                graphics_renderer()->draw_image_request(&shadow);
                graphics_renderer()->draw_image_request(&main);
                return;
            }
            case FONT_NORMAL_RED:
            {
                render_2d_request shadow = make_request(legacy_image_, x + 1, y + 1, 0xffe7cfad, base_scale * scale, 0);
                render_2d_request main = make_request(legacy_image_, x, y, 0xff731408, base_scale * scale, 0);
                graphics_renderer()->draw_image_request(&shadow);
                graphics_renderer()->draw_image_request(&main);
                return;
            }
            case FONT_NORMAL_GREEN:
            {
                render_2d_request shadow = make_request(legacy_image_, x + 1, y + 1, 0xffe7cfad, base_scale * scale, 0);
                render_2d_request main = make_request(legacy_image_, x, y, 0xff180800, base_scale * scale, 0);
                graphics_renderer()->draw_image_request(&shadow);
                graphics_renderer()->draw_image_request(&main);
                return;
            }
            case FONT_NORMAL_BLACK:
            case FONT_LARGE_BLACK:
            {
                render_2d_request shadow = make_request(legacy_image_, x + 1, y + 1, 0xffcead9c, base_scale * scale, 0);
                render_2d_request main = make_request(legacy_image_, x, y, COLOR_BLACK, base_scale * scale, 0);
                graphics_renderer()->draw_image_request(&shadow);
                graphics_renderer()->draw_image_request(&main);
                return;
            }
            case FONT_NORMAL_BROWN:
            case FONT_LARGE_BROWN:
            {
                render_2d_request request = make_request(legacy_image_, x, y, COLOR_FONT_PLAIN, base_scale * scale, 0);
                graphics_renderer()->draw_image_request(&request);
                return;
            }
            default:
                break;
        }
    }

    if (!color) {
        color = base_color_for_font(font);
    }
    render_2d_request request = make_request(legacy_image_, x, y, color, base_scale * scale, 0);
    graphics_renderer()->draw_image_request(&request);
}

void Image::draw_fullscreen_background() const
{
    graphics_renderer()->clear_screen();
    ensure_ready_to_draw();
    draw_fullscreen_fit(*this, 0, 0, COLOR_MASK_NONE);
}

void Image::draw_blurred_fullscreen(int intensity) const
{
    graphics_renderer()->clear_screen();

    color_t alpha = 0x80;
    color_t alpha_step = 0x60 / intensity;

    ensure_ready_to_draw();
    draw_fullscreen_fit(*this, 0, 0, color_with_alpha(COLOR_MASK_NONE, 0x80));
    for (int i = 1; i <= intensity; i++) {
        draw_fullscreen_fit(*this, 0, i, color_with_alpha(COLOR_MASK_NONE, alpha));
        draw_fullscreen_fit(*this, i, 0, color_with_alpha(COLOR_MASK_NONE, alpha));
        draw_fullscreen_fit(*this, 0, -i, color_with_alpha(COLOR_MASK_NONE, alpha));
        draw_fullscreen_fit(*this, -i, 0, color_with_alpha(COLOR_MASK_NONE, alpha));
        draw_fullscreen_fit(*this, -i, i, color_with_alpha(COLOR_MASK_NONE, alpha / 2));
        draw_fullscreen_fit(*this, i, -i, color_with_alpha(COLOR_MASK_NONE, alpha / 2));
        draw_fullscreen_fit(*this, -i, -i, color_with_alpha(COLOR_MASK_NONE, alpha / 2));
        draw_fullscreen_fit(*this, i, i, color_with_alpha(COLOR_MASK_NONE, alpha / 2));
        alpha -= alpha_step;
    }
}

void Image::draw_isometric_footprint(int x, int y, color_t color_mask, float scale) const
{
    ensure_ready_to_draw();
    int num_tiles = (width() + 2) / (FOOTPRINT_WIDTH + 2);
    x -= 30 * (num_tiles - 1);
    graphics_renderer()->draw_image(&legacy_image_, x, y, color_mask, scale);
}

void Image::draw_isometric_footprint_from_draw_tile(int x, int y, color_t color_mask, float scale) const
{
    ensure_ready_to_draw();
    int num_tiles = (width() + 2) / (FOOTPRINT_WIDTH + 2);
    y -= FOOTPRINT_HALF_HEIGHT * (num_tiles - 1);
    graphics_renderer()->draw_image(&legacy_image_, x, y, color_mask, scale);
}

void Image::draw_isometric_top(int x, int y, color_t color_mask, float scale) const
{
    const Image *top_image = top();
    if (!top_image) {
        return;
    }
    ensure_ready_to_draw();
    int num_tiles = (width() + 2) / (FOOTPRINT_WIDTH + 2);
    x -= 30 * (num_tiles - 1);
    y -= top_image->original_height() - FOOTPRINT_HALF_HEIGHT * num_tiles;
    graphics_renderer()->draw_image(&top_image->legacy(), x, y, color_mask, scale);
}

void Image::draw_isometric_top_from_draw_tile(int x, int y, color_t color_mask, float scale) const
{
    const Image *top_image = top();
    if (!top_image) {
        return;
    }
    ensure_ready_to_draw();
    y -= top_image->original_height() - FOOTPRINT_HALF_HEIGHT;
    graphics_renderer()->draw_image(&top_image->legacy(), x, y, color_mask, scale);
}

void Image::draw_set_isometric_top_from_draw_tile(int x, int y, color_t color_mask, float scale) const
{
    const Image *top_image = top();
    if (!top_image) {
        return;
    }
    ensure_ready_to_draw();
    y -= top_image->original_height() - FOOTPRINT_HALF_HEIGHT;
    graphics_renderer()->draw_silhouette(&top_image->legacy(), x, y, color_mask, scale);
}

void Image::bind_key(std::string_view path_key)
{
    key_ = make_key(path_key);
    legacy_image_.resource_key = key_.empty() ? nullptr : const_cast<char *>(key_.c_str());
    legacy_image_.resource_payload = key_.empty() ? nullptr : this;
}

void Image::bind_legacy(image &legacy_image)
{
    bound_legacy_ = &legacy_image;
    legacy_image.resource_payload = this;
    legacy_image.resource_handle = render_handle();
    legacy_image.resource_key = key_.empty() ? nullptr : const_cast<char *>(key_.c_str());
}

void Image::clear_bound_legacy()
{
    if (!bound_legacy_) {
        return;
    }
    bound_legacy_->resource_payload = nullptr;
    bound_legacy_->resource_key = nullptr;
    bound_legacy_->resource_handle = 0;
    bound_legacy_ = nullptr;
}

void Image::ensure_ready_to_draw() const
{
    if (is_external()) {
        load_external_data();
        return;
    }
    if ((legacy_image_.atlas.id >> IMAGE_ATLAS_BIT_OFFSET) == ATLAS_UNPACKED_EXTRA_ASSET && source_image_id_ >= 0) {
        assets_load_unpacked_asset(source_image_id_);
    }
}

void Image::set_source_image_id(int image_id)
{
    source_image_id_ = image_id;
}

Image &ImageManager::from_id(int image_id)
{
    const image *img = image_get(image_id);
    if (!img) {
        report_invalid_image("Invalid image id", image_id);
        return from_id(0);
    }
    Image &stored_image = from_legacy(*const_cast<image *>(img));
    stored_image.set_source_image_id(image_id);
    return stored_image;
}

Image &ImageManager::from_legacy(image &legacy_image)
{
    if (Image *existing = image_from_payload(legacy_image)) {
        existing->sync_from_legacy(legacy_image);
        existing->bind_legacy(legacy_image);
        return *existing;
    }
    if (legacy_image.resource_key && *legacy_image.resource_key) {
        if (Image *existing = find(legacy_image.resource_key)) {
            existing->sync_from_legacy(legacy_image);
            existing->bind_legacy(legacy_image);
            return *existing;
        }
    }
    return store_legacy_image(legacy_image);
}

const Image &ImageManager::from_legacy(const image &legacy_image)
{
    return from_legacy(*const_cast<image *>(&legacy_image));
}

Image *ImageManager::find(std::string_view path_key)
{
    if (path_key.empty()) {
        return nullptr;
    }
    auto it = g_keyed_images.find(make_key(path_key));
    return it == g_keyed_images.end() ? nullptr : it->second.get();
}

const Image *ImageManager::find(std::string_view path_key) const
{
    if (path_key.empty()) {
        return nullptr;
    }
    auto it = g_keyed_images.find(make_key(path_key));
    return it == g_keyed_images.end() ? nullptr : it->second.get();
}

Image &ImageManager::register_image(image &legacy_image, std::string_view path_key)
{
    Image &stored_image = store_keyed_image(path_key, legacy_image);
    stored_image.set_render_handle(legacy_image.resource_handle);
    stored_image.bind_legacy(legacy_image);
    stored_image.retain();
    return stored_image;
}

Image *ImageManager::acquire(image &legacy_image, std::string_view path_key)
{
    Image *stored_image = find(path_key);
    if (!stored_image) {
        return nullptr;
    }
    release(legacy_image);
    stored_image->retain();
    stored_image->bind_legacy(legacy_image);
    return stored_image;
}

Image *ImageManager::load_png(image &legacy_image, std::string_view path_key, const char *file_path)
{
    if (Image *existing = acquire(legacy_image, path_key)) {
        return existing;
    }
    if (!file_path || !*file_path || !png_load_from_file(file_path, 0)) {
        return nullptr;
    }

    int width = 0;
    int height = 0;
    if (!png_get_image_size(&width, &height) || width <= 0 || height <= 0) {
        png_unload();
        return nullptr;
    }

    color_t *pixels = static_cast<color_t *>(malloc(sizeof(color_t) * width * height));
    if (!pixels) {
        png_unload();
        return nullptr;
    }

    if (!png_read(pixels, 0, 0, width, height, 0, 0, width, 0)) {
        free(pixels);
        png_unload();
        return nullptr;
    }
    png_unload();

    legacy_image.x_offset = 0;
    legacy_image.y_offset = 0;
    legacy_image.width = width;
    legacy_image.height = height;
    legacy_image.original.width = width;
    legacy_image.original.height = height;

    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->upload_image_resource) {
        free(pixels);
        return nullptr;
    }

    renderer->upload_image_resource(&legacy_image, pixels, width, height);
    free(pixels);
    if (legacy_image.resource_handle <= 0) {
        return nullptr;
    }
    return &register_image(legacy_image, path_key);
}

Image *ImageManager::load_png(std::string_view path_key, const char *file_path)
{
    if (Image *existing = find(path_key)) {
        existing->retain();
        return existing;
    }
    image legacy_image = {};
    Image *loaded_image = load_png(legacy_image, path_key, file_path);
    if (loaded_image) {
        loaded_image->clear_bound_legacy();
    }
    return loaded_image;
}

Image *ImageManager::load_pixels(std::string_view path_key, const image &metadata, const color_t *pixels, int width, int height)
{
    if (path_key.empty() || !pixels || width <= 0 || height <= 0) {
        return nullptr;
    }
    if (Image *existing = find(path_key)) {
        existing->retain();
        return existing;
    }

    const graphics_renderer_interface *renderer = graphics_renderer();
    if (!renderer || !renderer->upload_image_resource) {
        return nullptr;
    }

    image uploaded_image = metadata;
    uploaded_image.resource_handle = 0;
    uploaded_image.resource_key = nullptr;
    uploaded_image.resource_payload = nullptr;
    uploaded_image.top = nullptr;
    uploaded_image.animation = nullptr;
    renderer->upload_image_resource(&uploaded_image, pixels, width, height);
    if (uploaded_image.resource_handle <= 0) {
        return nullptr;
    }

    Image &stored_image = store_keyed_image(path_key, uploaded_image);
    stored_image.set_render_handle(uploaded_image.resource_handle);
    stored_image.retain();
    return &stored_image;
}

void ImageManager::retain(std::string_view path_key)
{
    if (Image *stored_image = find(path_key)) {
        stored_image->retain();
    }
}

void ImageManager::release(std::string_view path_key)
{
    if (path_key.empty()) {
        return;
    }
    auto it = g_keyed_images.find(make_key(path_key));
    if (it == g_keyed_images.end()) {
        return;
    }
    if (it->second->release_ref() <= 0) {
        it->second->release_renderer_resource();
        it->second->clear_bound_legacy();
        g_keyed_images.erase(it);
    }
}

void ImageManager::release(image &legacy_image)
{
    Image *stored_image = image_from_payload(legacy_image);
    std::string key = legacy_image.resource_key ? legacy_image.resource_key : "";
    legacy_image.resource_payload = nullptr;
    legacy_image.resource_key = nullptr;
    legacy_image.resource_handle = 0;
    if (stored_image) {
        if (stored_image->release_ref() <= 0) {
            stored_image->release_renderer_resource();
            stored_image->clear_bound_legacy();
            if (!stored_image->key().empty()) {
                g_keyed_images.erase(stored_image->key());
            }
        }
        return;
    }
    if (!key.empty()) {
        release(key);
    }
}

void ImageManager::clear()
{
    for (auto &entry : g_keyed_images) {
        entry.second->clear_bound_legacy();
    }
    g_keyed_images.clear();
    g_legacy_images.clear();
}

Image &ImageManager::store_legacy_image(image &legacy_image)
{
    auto it = g_legacy_images.find(&legacy_image);
    if (it != g_legacy_images.end()) {
        it->second->sync_from_legacy(legacy_image);
        it->second->bind_legacy(legacy_image);
        return *it->second;
    }
    auto stored_image = std::make_unique<Image>(legacy_image);
    Image *stored_ptr = stored_image.get();
    stored_ptr->bind_legacy(legacy_image);
    g_legacy_images.emplace(&legacy_image, std::move(stored_image));
    return *stored_ptr;
}

Image &ImageManager::store_keyed_image(std::string_view path_key, const image &legacy_image)
{
    std::string key = make_key(path_key);
    auto it = g_keyed_images.find(key);
    if (it != g_keyed_images.end()) {
        it->second->sync_from_legacy(legacy_image);
        return *it->second;
    }
    auto stored_image = std::make_unique<Image>(legacy_image);
    stored_image->bind_key(key);
    Image *stored_ptr = stored_image.get();
    g_keyed_images.emplace(std::move(key), std::move(stored_image));
    return *stored_ptr;
}
