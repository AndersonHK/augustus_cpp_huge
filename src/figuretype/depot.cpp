#include "depot.h"

void figure_depot_recall(Figure *f)
{
    if (f) {
        f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    }
}
