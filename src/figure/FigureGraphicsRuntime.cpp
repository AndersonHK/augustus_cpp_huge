#include "figure/FigureGraphics.h"

#include "core/image.h"
#include "figure/action.h"
#include "figure/figure.h"
#include "figure/image.h"

namespace figure_type_registry_impl {

void FigureGraphics::apply_legacy_prefect_service_image_state(Figure &figure, int direction) const
{
    figure.is_enemy_image = 0;
    if (state_layer_for_action(figure.action_state)) {
        return;
    }
    direction = figure_image_normalize_direction(direction);
    switch (figure.action_state) {
        case FIGURE_ACTION_150_ATTACK:
        {
            const int frame_offset = figure.attack_image_offset >= 12 ?
                (figure.attack_image_offset - 12) / 2 :
                0;
            figure.image_id = legacy_attack_directional_frame_image_id(direction, frame_offset);
            break;
        }
        default: break;
    }
}

void FigureGraphics::apply_legacy_entertainment_image_state(Figure &figure, int direction) const
{
    figure.is_enemy_image = 0;
    direction = figure_image_normalize_direction(direction);
    if (figure.type == FIGURE_CHARIOTEER) {
        if (figure.action_state == FIGURE_ACTION_150_ATTACK ||
            figure.action_state == FIGURE_ACTION_149_CORPSE) {
            figure.image_id = legacy_directional_image_id(direction);
        }
        return;
    }

    const bool use_lion_tamer_whip = figure.type == FIGURE_LION_TAMER &&
        figure.wait_ticks_missile >= 96 &&
        figure.action_state != FIGURE_ACTION_149_CORPSE;

    if (figure.action_state == FIGURE_ACTION_150_ATTACK) {
        if (figure.type == FIGURE_GLADIATOR) {
            figure.image_id = legacy_attack_directional_frame_image_id(direction, figure.image_offset / 2);
            figure.adjust_legacy_gladiator_attack_image_row();
        } else if (use_lion_tamer_whip) {
            figure.image_id = legacy_action_directional_frame_image_id(direction, 0);
        } else {
            figure.image_id = legacy_directional_image_id(direction);
        }
    } else if (use_lion_tamer_whip) {
        figure.image_id = legacy_action_directional_frame_image_id(direction, figure.image_offset);
    }
}

} // namespace figure_type_registry_impl
