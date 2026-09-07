#pragma once
#include "figuretype/crime.h"
#include "figure/unit_type.h"
#include "core/config.h"
#include "figure/figure_type_registry_internal.h"
#include "game/defines.h"
#include "figure/FigureGraphics.h"
#include "graphics/image.h"

inline void validate_cart_criminal_graphics()
{
    using namespace figure_type_registry_impl;
    auto require = [](bool value, const char *message) { if (!value) throw std::runtime_error(message); };
    const int off_road = config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD);
    for (figure_type type : {FIGURE_TRADE_CARAVAN, FIGURE_TRADE_CARAVAN_DONKEY}) {
        const auto *profile = default_profile_for(type);
        const auto *unit = unit_type_registry_impl::find_unit_type(type);
        require(profile && unit && unit->combat_stats().health == 10, "Trade figure has no complete native profile/stats");
        require(profile->native_class() == (type == FIGURE_TRADE_CARAVAN ? NativeClassId::LandTrade : NativeClassId::TradeFollower), "Trade figure fell back to a legacy controller");
        if (type == FIGURE_TRADE_CARAVAN) require(profile->trade.initial_wait_ticks == (profile->pathing_policy().terrain_setting != CONFIG_MAX_ENTRIES ? 50 : 20), "Caravan arrival timing lost Julius/Augustus parity");
        auto cleanup = [](Figure *figure) { if (figure && figure->id()) figure->remove(); };
        std::unique_ptr<Figure, decltype(cleanup)> figure(Figure::create(type, 5, 5, DIR_0_TOP), cleanup);
        require(figure && figure->id(), "Could not create native caravan fixture");
        for (int setting : {0, 1}) {
            config_set(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD, setting);
            figure_runtime_apply_profile_movement(figure.get());
            const bool conditional = profile->pathing_policy().terrain_setting != CONFIG_MAX_ENTRIES;
            require(figure->terrain_usage == (conditional && setting ? TERRAIN_USAGE_ANY : profile->pathing_policy().terrain.legacy_usage), "Caravan terrain setting did not update movement");
            auto route = profile->pathing_policy().routePolicySelection(PERMISSION_NONE, RouteNeighborhood::FourWay);
            require(route.terrain.legacy_usage == figure->terrain_usage, "Caravan route disagrees with active movement terrain");
        }
        config_set(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD, off_road);
        require(figure_runtime_execute(figure.get()) == 1, "Caravan execution used the legacy action table");
        for (int action : {FIGURE_ACTION_100_TRADE_CARAVAN_CREATED, FIGURE_ACTION_149_CORPSE}) for (int direction = 0; direction < 8; ++direction) for (int frame = 0; frame < 12; ++frame) {
            figure->action_state = static_cast<unsigned char>(action);
            figure->direction = static_cast<signed char>(direction);
            figure->image_offset = static_cast<unsigned char>(frame);
            FigureGraphicDrawRequest request;
            require(figure_graphics_resolve_draw_request(*figure, request) && request.has_base_slice(), "Native caravan directional graphics failed");
        }
    }
    std::fprintf(stdout, "Native caravan contracts passed: explicit stats/controllers, live terrain and route policy, 384 alive/dead animation frames.\n");
    int carts = 0;
    for (int resource = RESOURCE_NONE + 1; resource < RESOURCE_SLOT_COUNT; ++resource) {
        if (FigureGraphics::resource_cart_image(static_cast<resource_type>(resource), 1).group_path().empty()) continue;
        for (int loads : {1, 4, 8}) for (int direction = 0; direction < 8; ++direction) {
            auto ref = FigureGraphics::resource_cart_image_for_direction(static_cast<resource_type>(resource), loads, 1, direction);
            require(ref.is_bound() && ref.runtime_slice().is_valid(), "Resource cart direction/load graphic failed to resolve");
            if (game_defines_legacy_figure_logical_units_per_source_pixel() == 96) {
                require(ref.fixed_logical_size().width == ref.source_pixel_width() * 96 && ref.fixed_logical_size().height == ref.source_pixel_height() * 96, "Loaded resource cart is missing Vespasian logical scaling");
            }
            ++carts;
        }
    }
    for (figure_type type : {FIGURE_CRIMINAL_ROBBER, FIGURE_CRIMINAL_LOOTER, FIGURE_RIOTER}) {
        auto cleanup = [](Figure *figure) { if (figure && figure->id()) figure->remove(); };
        std::unique_ptr<Figure, decltype(cleanup)> figure(Figure::create(type, 5, 5, DIR_0_TOP), cleanup);
        require(figure && figure->id(), "Could not create criminal graphics fixture");
        for (int action : {FIGURE_ACTION_120_RIOTER_CREATED, FIGURE_ACTION_121_RIOTER_MOVING, FIGURE_ACTION_149_CORPSE}) {
            figure->action_state = static_cast<unsigned char>(action);
            for (int direction = 0; direction < 9; ++direction) for (int frame = 0; frame < 160; ++frame) {
                figure->direction = static_cast<signed char>(direction == 8 ? DIR_FIGURE_ATTACK : direction);
                figure->wait_ticks = static_cast<short>(frame);
                figure->image_offset = static_cast<unsigned char>(frame % (action == FIGURE_ACTION_121_RIOTER_MOVING ? 12 : 32));
                figure_runtime_graphics_begin_update(figure.get());
                figure_criminal_update_graphics(figure.get());
                FigureGraphicDrawRequest request;
                require(figure_graphics_resolve_draw_request(*figure, request) && request.has_base_slice(), "Criminal idle, walking or death animation failed to resolve");
            }
        }
    }
    std::fprintf(stdout, "Cart/criminal graphics contracts passed: %d cart direction/load variants and 12,960 criminal frames.\n", carts);
}
