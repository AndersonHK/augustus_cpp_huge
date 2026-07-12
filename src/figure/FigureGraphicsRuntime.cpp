#include "figure/FigureGraphics.h"

#include "assets/image_group_payload.h"
#include "core/image.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/image.h"

namespace figure_type_registry_impl {

int FigureGraphics::apply_legacy_image_state_for_direction(Figure &figure, int direction) const
{
    figure.is_enemy_image = 0;
    if (has_native_payload()) {
        const GraphicsTargetBinding *binding = cached_target_binding_for_figure_direction(figure, direction);
        return binding &&
            binding->is_resolved() &&
            binding->entry->footprint() &&
            binding->entry->footprint()->is_valid();
    }

    if (!has_legacy_default_source()) {
        return 0;
    }

    figure.image_id = legacy_image_id_for_figure_direction(figure, direction);
    return 1;
}

int FigureGraphics::apply_legacy_image_state(Figure &figure) const
{
    return apply_legacy_image_state_for_direction(figure, figure_image_direction(&figure));
}

void FigureGraphics::apply_legacy_prefect_service_image_state(Figure &figure, int direction) const
{
    direction = figure_image_normalize_direction(direction);
    switch (figure.action_state) {
        case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            figure.select_legacy_directional_frame_image(
                ::image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET),
                direction,
                figure.image_offset);
            break;
        case FIGURE_ACTION_75_PREFECT_AT_FIRE:
            figure.select_legacy_directional_frame_image(
                ::image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) + 96,
                direction,
                figure.image_offset / 2);
            break;
        case FIGURE_ACTION_150_ATTACK:
        {
            const int frame_offset = figure.attack_image_offset >= 12 ?
                (figure.attack_image_offset - 12) / 2 :
                0;
            figure.image_id = legacy_attack_directional_frame_image_id(direction, frame_offset);
            break;
        }
        case FIGURE_ACTION_149_CORPSE:
        default:
            apply_legacy_image_state_for_direction(figure, direction);
            break;
    }
}

void FigureGraphics::apply_legacy_entertainment_image_state(Figure &figure, int direction) const
{
    direction = figure_image_normalize_direction(direction);
    if (figure.type == FIGURE_CHARIOTEER) {
        figure.clear_legacy_cart_overlay_image();
        if (figure.action_state == FIGURE_ACTION_150_ATTACK ||
            figure.action_state == FIGURE_ACTION_149_CORPSE) {
            figure.image_id = legacy_directional_image_id(direction);
        } else {
            apply_legacy_image_state_for_direction(figure, direction);
        }
        return;
    }

    const bool use_lion_tamer_whip = figure.type == FIGURE_LION_TAMER &&
        figure.wait_ticks_missile >= 96 &&
        figure.action_state != FIGURE_ACTION_149_CORPSE;
    int cart_overlay_base_image_id = figure.cart_image_id;
    if (figure.type == FIGURE_LION_TAMER) {
        cart_overlay_base_image_id = ::image_group(GROUP_FIGURE_LION);
    }

    if (figure.action_state == FIGURE_ACTION_150_ATTACK) {
        if (figure.type == FIGURE_GLADIATOR) {
            figure.image_id = legacy_attack_directional_frame_image_id(direction, figure.image_offset / 2);
            figure.adjust_legacy_gladiator_attack_image_row();
        } else if (use_lion_tamer_whip) {
            figure.image_id = legacy_action_directional_frame_image_id(direction, 0);
        } else {
            figure.image_id = legacy_directional_image_id(direction);
        }
    } else if (figure.action_state == FIGURE_ACTION_149_CORPSE) {
        cart_overlay_base_image_id = 0;
        apply_legacy_image_state_for_direction(figure, direction);
    } else if (use_lion_tamer_whip) {
        figure.image_id = legacy_action_directional_frame_image_id(direction, figure.image_offset);
    } else {
        apply_legacy_image_state_for_direction(figure, direction);
    }
    figure.select_legacy_cart_overlay_image(cart_overlay_base_image_id, direction);
}

} // namespace figure_type_registry_impl
