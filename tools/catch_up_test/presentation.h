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
#include "window/overlay_menu.h"
#include "game/state.h"
#include "input/scroll.h"
#include "core/encoding_simp_chinese.h"
#include "core/encoding_trad_chinese.h"
#include "platform/screen.h"
#include "figure/phrase.h"
#include "building/rotation.h"
#include "graphics/scrollbar.h"

inline void validate_scrollbar_pointer_capture()
{
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    scrollbar_type bar{};
    bar.x = 100; bar.y = 50; bar.height = 293; bar.dot_padding = 8; bar.elements_in_view = 10;
    scrollbar_init(&bar, 50, 110);
    mouse pointer{};
    pointer.x = 110; pointer.y = 50 + 26 + 8 + 100 + 2;
    pointer.left.is_down = pointer.left.went_down = 1;
    require(scrollbar_handle_mouse(&bar, &pointer, 0) && bar.scroll_position == 50, "Scrollbar jumped when grabbed off center");
    scrollbar_update_total_elements(&bar, 110); // Normal list redraw during the drag.
    pointer.left.went_down = 0;
    pointer.x = 500; pointer.y += 40;
    require(scrollbar_handle_mouse(&bar, &pointer, 0) && bar.scroll_position == 70, "Scrollbar lost pointer capture outside its width");
    pointer.y = -100;
    require(scrollbar_handle_mouse(&bar, &pointer, 0) && bar.scroll_position == 0, "Scrollbar drag failed to clamp to the start");
    pointer.y = 1000;
    require(scrollbar_handle_mouse(&bar, &pointer, 0) && bar.scroll_position == 100, "Scrollbar drag failed to clamp to the end");
    pointer.left.is_down = 0;
    scrollbar_handle_mouse(&bar, &pointer, 0);
    require(!bar.is_dragging_scrollbar_dot, "Scrollbar did not release pointer capture");
    scrollbar_init(&bar, 50, 5);
    require(!scrollbar_handle_mouse(&bar, &pointer, 0) && bar.scroll_position == 0, "Empty scrollbar retained a stale position");
}

inline void validate_overlay_menu_input()
{
    validate_scrollbar_pointer_capture();
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    int cursor_x = screen_pixel_width() / 3, cursor_y = screen_pixel_height() / 3;
    const int expected_x = cursor_x, expected_y = cursor_y;
    platform_screen_pixels_to_window(&cursor_x, &cursor_y);
    platform_screen_window_to_pixels(&cursor_x, &cursor_y);
    require(std::abs(cursor_x - expected_x) <= 2 && std::abs(cursor_y - expected_y) <= 2, "Cursor drawable/window coordinate conversion is inconsistent");
    const int original_overlay = game_state_overlay();
    const mouse original_mouse = *mouse_get_pixel();
    uint8_t orientation_bytes[4]{}, camera_bytes[8]{};
    buffer orientation, camera;
    buffer_init(&orientation, orientation_bytes, sizeof(orientation_bytes));
    buffer_init(&camera, camera_bytes, sizeof(camera_bytes));
    city_view_save_state(&orientation, &camera);
    window_city_show();
    window_overlay_menu_show();
    window_draw(1);
    require(window_is(WINDOW_OVERLAY_MENU), "Declarative overlay menu did not open");
    int vx, vy, vw, vh;
    city_view_get_viewport(&vx, &vy, &vw, &vh);
    const int right = screen_pixel_to_ui(vx + vw) - 10;
    auto hover = [](int x, int y) { mouse_set_logical_position(x, y); window_draw(1); window_draw(1); };
    auto click = [&](int x, int y) { hover(x, y); mouse_set_left_down(1); window_draw(1); mouse_set_left_down(0); window_draw(1); };
    hover(right - 40, 74 + 8 + 2 * 24 + 12); // Risks: opens on hover, without a click.
    click(right - 180 - 40, 74 + 8 + 2 * 24 + 8 + 12);
    require(window_is(WINDOW_CITY) && game_state_overlay() == OVERLAY_FIRE, "Hover submenu did not select its first leaf");
    window_overlay_menu_show();
    click(right - 40, 74 + 8 + 2 * 24 + 12); // Lock Risks, then cross another parent.
    hover(right - 40, 74 + 8 + 3 * 24 + 12);
    click(right - 180 - 40, 74 + 8 + 2 * 24 + 8 + 12);
    require(window_is(WINDOW_CITY) && game_state_overlay() == OVERLAY_FIRE, "Locked overlay parent changed on hover");
    window_overlay_menu_show();
    mouse_set_logical_position(0, 0); mouse_set_right_down(1); window_draw(1); mouse_set_right_down(0); window_draw(1);
    require(window_is(WINDOW_CITY), "Overlay menu did not dismiss on back input");
    game_state_set_overlay(original_overlay);
    window_overlay_menu_update();
    mouse_reset_button_state();
    mouse_set_position(original_mouse.x, original_mouse.y);
    mouse_set_inside_window(original_mouse.is_inside_window);
    mouse_set_window_focus(original_mouse.window_has_focus);
    buffer_reset(&orientation); buffer_reset(&camera);
    city_view_load_state(&orientation, &camera);
    scroll_stop();
}

inline void validate_presentation_runtime()
{
    using namespace building_type_registry_impl;
    const auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    uint8_t encoded[16]{}; char decoded[16]{};
    Figure speech_fixture(65000);
    speech_fixture.type = FIGURE_TRADE_SHIP;
    for (int phrase = 7; phrase <= 8; ++phrase) {
        speech_fixture.phrase_id = static_cast<signed char>(phrase);
        require(std::strcmp(figure_phrase_sound_file(speech_fixture), phrase == 7 ? "wavs/boats_exact1.wav" : "wavs/boats_exact2.wav") == 0, "Trade ship caption and sound disagree on trade direction");
    }
    speech_fixture.type = FIGURE_MARKET_SUPPLIER;
    speech_fixture.action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
    figure_phrase_determine(&speech_fixture);
    require(speech_fixture.phrase_id == 7 && std::strcmp(figure_phrase_sound_file(speech_fixture), "wavs/market_exact1.wav") == 0, "Market supplier departure caption and sound disagree");
    speech_fixture.action_state = 0;
    speech_fixture.type = FIGURE_DELIVERY_BOY;
    speech_fixture.phrase_id = 9;
    require(std::strcmp(figure_phrase_sound_file(speech_fixture), "wavs/granboy_exact3.wav") == 0, "Granary-boy voice selected the wrong sound group");
    speech_fixture.type = FIGURE_MISSIONARY;
    speech_fixture.phrase_id = 10;
    require(std::strcmp(figure_phrase_sound_file(speech_fixture), "wavs/mission_exact4.wav") == 0, "Missionary's exceptional fourth filename was lost");
    speech_fixture.phrase_id = 127;
    require(!figure_phrase_sound_file(speech_fixture), "Invalid saved phrase indexed outside the sound table");
    if (const auto dog = figure_type_from_xml_name("dog"); dog != FIGURE_NONE) {
        speech_fixture.type = static_cast<unsigned char>(dog);
        figure_phrase_determine(&speech_fixture);
        require(speech_fixture.phrase_id == -1 && figure_phrase_sound_id(speech_fixture) == -1 && std::strcmp(figure_phrase_sound_file(speech_fixture), "assets/Sounds/Dog_Bark.ogg") == 0, "Dog speech must use its authored bark without a human caption");
    }
    if (const auto citizen = figure_type_from_xml_name("wandering_citizen"); citizen != FIGURE_NONE) {
        speech_fixture.type = static_cast<unsigned char>(citizen);
        speech_fixture.phrase_id = 5;
        require(figure_phrase_sound_id(speech_fixture) == figure_phrase_voice_id("plebeian") && std::strcmp(figure_phrase_sound_file(speech_fixture), "wavs/pleb_great1.wav") == 0, "Ambient citizen inherited patrician speech instead of its authored voice");
    }
    encoding_simp_chinese_init();
    encoding_simp_chinese_from_utf8("\xe7\xb2\x92", encoded, sizeof(encoded));
    encoding_simp_chinese_to_utf8(encoded, decoded, sizeof(decoded));
    require(std::strcmp(decoded, "\xe7\xb2\x92") == 0, "Expanded Simplified Chinese font table did not round-trip its final glyph");
    encoding_trad_chinese_init();
    encoding_trad_chinese_from_utf8("\xe6\xb4\x81", encoded, sizeof(encoded));
    encoding_trad_chinese_to_utf8(encoded, decoded, sizeof(decoded));
    require(std::strcmp(decoded, "\xe6\xb4\x81") == 0, "Expanded Traditional Chinese font table did not round-trip its final glyph");
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
    if (const auto *shrine = definition_for_type(type_from_attr("shrine_neptune")); shrine && building_construction_type_can_cycle(shrine->type())) {
        const auto *previous_type = definition_for_type(building_construction_type());
        const int previous_rotation = building_rotation_get_rotation();
        struct RestoreSelection {
            const BuildingType *type; int rotation;
            ~RestoreSelection() { building_construction_set_type(type, rotation); city_warning_clear_all(); }
        } restore_selection{previous_type, previous_rotation};
        building_construction_set_type(shrine, 0);
        const int position = building_construction_type_cycle_position(shrine->type());
        require(building_rotation_get_rotation_with_limit(1000) == position, "Selecting a later cycle member must initialize its variation index");
        building_rotation_rotate_forward();
        building_construction_set_type(shrine, 0);
        building_rotation_rotate_forward();
        require(building_construction_type() == shrine->type() && building_rotation_get_rotation() == 1, "Changing tools must discard the old partial rotation cycle");
        building_rotation_rotate_forward();
        require(building_construction_type() != shrine->type() && building_rotation_get_rotation() == 0, "Advancing a two-rotation cycle must start the next definition at rotation zero");
        building_rotation_rotate_backward();
        require(building_construction_type() == shrine->type() && building_rotation_get_rotation() == 1, "Reverse cycling must restore the preceding definition's final rotation");
    }
    for (const char *name : {"wall", "tower", "watchtower", "gatehouse", "palisade", "palisade_gate"}) {
        const auto *def = definition_for_type(type_from_attr(name));
        require(def && def->presentation().show_durability, "Defensive durability was not declared in Augustus data");
    }
    require(definition_for_type(type_from_attr("roadblock"))->presentation().overlay_always_visible, "Roadblock overlay visibility was not declared");
    const auto *fountain = definition_for_type(type_from_attr("fountain"));
    int wet = 0, dry = 0, isolated_dry = 0;
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
        if (!access && !water_access_runtime_tile_has_inactive_access(ghost.grid_offset + 1, "fountain")) isolated_dry = ghost.grid_offset;
    }
    require(wet && dry, "Presentation fixture requires both covered and uncovered fountain sites");
    if (fountain->presentation().inactive_water_range) {
        require(isolated_dry != 0, "Inactive range fixture requires an isolated dry site");
        building record{}; record.id = 65004; record.type = fountain->type(); record.state = BUILDING_STATE_IN_USE;
        record.x = static_cast<unsigned char>(map_grid_offset_to_x(isolated_dry)); record.y = static_cast<unsigned char>(map_grid_offset_to_y(isolated_dry)); record.grid_offset = static_cast<short>(isolated_dry);
        building_runtime_impl::ScopedEphemeralBuildingRuntime scope({{record.id, record.id, &record, fountain, {}}});
        auto *runtime = scope.runtime_for_record(&record);
        require(runtime && runtime->building.Foundation, "Could not create water-range fixture");
        runtime->building.Foundation->state().begin_publication(record.x, record.y, 0);
        water_access_runtime_refresh_building(&runtime->building);
        require(water_access_runtime_tile_has_inactive_access(isolated_dry + 1, "fountain"), "Dry fountain did not expose its inactive range");
        water_access_runtime_remove_building(&runtime->building);
        require(!water_access_runtime_tile_has_inactive_access(isolated_dry + 1, "fountain"), "Deleted fountain left stale inactive coverage");
        runtime->building.Foundation->state().clear();
    }
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
    const auto *armoury = definition_for_type(type_from_attr("armoury"));
    require(armoury && armoury->has_labor() && armoury->preview_figure_type() == FIGURE_LABOR_SEEKER, "Armoury preview must follow its authored labor-seeker policy");
    if (tower) {
        require(tower->preview_figure_type() == FIGURE_WATCHMAN, "Watchtower preview must use its patrol figure");
        const auto *patrol = figure_type_registry_impl::default_profile_for(FIGURE_WATCHMAN);
        require(patrol && patrol->movement_profile().max_roam_length == 640, "Watchman preview requires the authored patrol range");
        const auto *willow = definition_for_type(type_from_attr("willow_tree"));
        require(willow && willow->presentation().panel == BuildingType::InformationPanel::Garden, "Willow must use the authored garden information panel");
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
    validate_overlay_menu_input();
    std::fprintf(stdout, "Presentation contracts passed: climate walls, arch layers, watchtower choices, water preview eligibility and updated warning text.\n");
}
