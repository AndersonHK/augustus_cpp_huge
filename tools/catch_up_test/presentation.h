#pragma once
#include "graphics/graphics.h"
#include "graphics/image.h"
#include "city/warning.h"
#include "building/water_access_runtime.h"
#include "empire/city.h"
#include "empire/trade_route.h"
#include "core/image_packer.h"
#include "core/time.h"
#include "figure/route.h"

inline void validate_presentation_runtime()
{
    using namespace building_type_registry_impl;
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    require(empire_city_get_for_trade_route(1000000) == -1 && !empire_city_is_trade_route_sea(1000000), "Missing trade routes must not report sea access");
    image_packer packer{};
    require(image_packer_init(&packer, 17, 16384, 16384) == IMAGE_PACKER_OK, "Could not allocate atlas regression fixture");
    packer.options.reduce_image_size = 1;
    packer.options.fail_policy = IMAGE_PACKER_NEW_IMAGE;
    for (int i = 0; i < 17; ++i) packer.rects[i].input = {16384, 16384};
    const int packed = image_packer_pack(&packer);
    const int atlas_count = packer.result.images_needed;
    image_packer_free(&packer);
    require(packed == 17 && atlas_count == 17, "Atlas area accounting overflowed above four billion pixels");
    buffer saved_routes{};
    trade_routes_save_state(&saved_routes);
    int route_count = 1;
    for (int i = 0; i < empire_city_get_array_size(); ++i) {
        if (const auto *city = empire_city_get(i)) route_count = std::max(route_count, city->route_id + 2);
    }
    const int resource_count = resource_total_mapped();
    std::vector<uint8_t> limits(sizeof(int32_t) * (1 + route_count * resource_count));
    std::vector<uint8_t> trades(sizeof(int32_t) * route_count * resource_count);
    buffer limit_buffer, traded_buffer;
    buffer_init(&limit_buffer, limits.data(), limits.size()); buffer_write_i32(&limit_buffer, route_count);
    for (int i = 0; i < route_count * resource_count; ++i) buffer_write_i32(&limit_buffer, i + 1);
    buffer_reset(&limit_buffer); buffer_init(&traded_buffer, trades.data(), trades.size());
    trade_routes_migrate_to_buys_sells(&limit_buffer, &traded_buffer, 207);
    const bool rows_consumed = !limit_buffer.overflow && !traded_buffer.overflow && limit_buffer.index == limits.size() && traded_buffer.index == trades.size();
    buffer_reset(&saved_routes); trade_routes_load_state(&saved_routes); free(saved_routes.data);
    require(rows_consumed, "Missing empire cities shifted serialized trade route rows");
    city_warning_clear_all();
    const auto previous_time = time_get_millis();
    city_warning_show(WARNING_BUILDING_ROTATION, reinterpret_cast<const uint8_t *>("1/6 Pavilion"));
    time_set_millis(previous_time + 1);
    city_warning_show(WARNING_BUILDING_ROTATION, reinterpret_cast<const uint8_t *>("2/6 Pavilion"));
    time_set_millis(previous_time + 1000);
    city_warning_get(0); // Finish the repeat-warning flash before checking its new payload.
    require(city_warning_get(0) && std::strcmp(reinterpret_cast<const char *>(city_warning_get(0)), "2/6 Pavilion") == 0, "Variation warning retained its previous text");
    city_warning_clear_all();
    time_set_millis(previous_time);
    const auto *fountain = definition_for_type(type_from_attr("fountain"));
    int wet = 0, dry = 0;
    for (int y = 2; y < map_grid_height() - 2 && (!wet || !dry); ++y) for (int x = 2; x < map_grid_width() - 2; ++x) {
        if (terrain_map().contains(map_grid_offset(x, y), terrain_types().not_clear)) continue;
        const int access = water_access_runtime_building_type_has_required_access_at(fountain, x, y, 0);
        building ghost{}; ghost.type = fountain->type(); ghost.x = static_cast<unsigned char>(x); ghost.y = static_cast<unsigned char>(y);
        ghost.grid_offset = static_cast<short>(map_grid_offset(x, y)); ghost.num_workers = static_cast<short>(fountain->required_workers());
        ghost.has_water_access = static_cast<unsigned char>(access);
        BuildingGraphicsState state;
        Building preview(ghost, state);
        require(preview.has_water_access() == access && preview.is_working() == access, "Planned fountain water state disagrees with preview animation eligibility");
        if (access) ++wet; else ++dry;
    }
    require(wet && dry, "Presentation fixture requires both covered and uncovered fountain sites");
    graphics_reset_clip_rectangle();
    screen_set_pixel_render_scale();
    graphics_clear_screen();
    int column = 0;
    for (const char *group : {"Military\\Wall", "Military\\Wall_Northern", "Military\\Wall_Desert"}) {
        const auto graphic = ImageGroupEntryRef::from_group(group, "Image_0010");
        require(graphic.is_bound(), "Climate wall group was not extracted");
        graphic.draw(30 + column * 180, 100); graphic.draw_top(30 + column * 180, 100); ++column;
    }
    for (int orientation = 0; orientation < 2; ++orientation) {
        const auto graphic = ImageGroupEntryRef::from_group("Monuments\\Triumphal_Arch", orientation ? "Image_0002" : "Image_0000");
        require(graphic.is_bound(), "Triumphal arch group is missing");
        graphic.draw(80 + orientation * 240, 270); graphic.draw_top(80 + orientation * 240, 270);
    }
    const auto *tower = definition_for_type(type_from_attr("watchtower"));
    if (tower) {
        require(tower->graphics().default_target().option_count() == 4, "Watchtower requires all four authored visual choices");
        std::array<image_handle, 4> tower_handles{};
        for (int variant = 0; variant < 4; ++variant) {
            building record{}; record.id = 65003; record.type = tower->type(); record.state = BUILDING_STATE_IN_USE; record.x = record.y = 10;
            record.grid_offset = static_cast<short>(map_grid_offset(10, 10));
            BuildingGraphicsState state; state.set_variant(variant);
            building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{record.id, record.id, &record, tower, state}});
            auto *runtime = scope.runtime_for_record(&record);
            require(runtime && runtime->resolve_graphics_cache(), "Watchtower option failed to bind");
            require(runtime->cached_graphic_top() && runtime->cached_graphic_top()->is_valid(), "Watchtower top is missing");
            tower_handles[variant] = runtime->cached_graphic_top()->handle;
            if (const auto *slice = runtime->cached_graphic_footprint()) runtime_texture_draw(*slice, 50 + variant * 160, 450, COLOR_MASK_NONE, 1.0f);
            if (const auto *slice = runtime->cached_graphic_top()) runtime_texture_draw(*slice, 50 + variant * 160, 450, COLOR_MASK_NONE, 1.0f);
        }
        for (int rotation = 1; rotation <= 4; ++rotation) {
            city_view_rotate_right();
            for (int variant = 0; variant < 4; ++variant) {
                building record{}; record.id = 65003; record.type = tower->type(); record.state = BUILDING_STATE_IN_USE; record.x = record.y = 10;
                record.grid_offset = static_cast<short>(map_grid_offset(10, 10));
                BuildingGraphicsState state; state.set_variant(variant);
                building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{record.id, record.id, &record, tower, state}});
                auto *runtime = scope.runtime_for_record(&record);
                require(runtime && runtime->resolve_graphics_cache() && runtime->cached_graphic_top() && runtime->cached_graphic_top()->handle == tower_handles[variant ^ (rotation % 2)], "Camera rotation did not preserve the watchtower's selected style pair");
            }
        }
    }
    const int width = screen_pixel_width(), height = screen_pixel_height();
    std::vector<color_t> pixels(static_cast<size_t>(width) * height);
    require(graphics_renderer()->save_screen_buffer(pixels.data(), 0, 0, width, height, width), "Could not capture presentation fixture");
    std::filesystem::create_directories("out/catch-up-ui");
    SDL_Surface *surface = SDL_CreateRGBSurfaceFrom(pixels.data(), width, height, 32, width * 4, 0x00ff0000, 0x0000ff00, 0x000000ff, 0xff000000);
    require(surface != nullptr, "Could not create presentation screenshot");
    const int saved = SDL_SaveBMP(surface, "out/catch-up-ui/presentation.bmp"); SDL_FreeSurface(surface);
    require(saved == 0, "Could not save presentation screenshot");
    screen_set_ui_render_scale();
    std::fprintf(stdout, "Presentation contracts passed: climate walls, arch layers, watchtower choices, water preview eligibility and updated warning text.\n");
}
