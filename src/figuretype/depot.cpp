#include "depot.h"

extern "C" {
#include "figure/action.h"
#include "figure/figure.h"
}

void figure_depot_recall(figure *f)
{
    if (f) {
        f->action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
    }
}
