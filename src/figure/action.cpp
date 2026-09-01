#include "city/entertainment.h"
#include "figuretype/animal.h"
#include "figuretype/cartpusher.h"
#include "figuretype/crime.h"
#include "figuretype/docker.h"
#include "figuretype/entertainer.h"
#include "figuretype/editor.h"
#include "figuretype/enemy.h"
#include "figuretype/migrant.h"
#include "figuretype/missile.h"
#include "figuretype/native.h"
#include "figuretype/service.h"
#include "figuretype/supplier.h"
#include "figuretype/trader.h"
#include "figuretype/wall.h"
#include "figuretype/water.h"
#include "figuretype/workcamp.h"

#include "figure/action.h"
#include "figure/figure.h"
#include "figure/figure_runtime_api.h"
#include "game/performance_tracker.h"
#include "figuretype/soldier.h"

#include "city/figures.h"


static void figure_nobody_action(Figure *f)
{
    (void) f;
}

static void figure_retired_native_action(Figure *f)
{
    // These enum slots are now owned by FigureType controllers. If runtime
    // binding fails, remove the figure instead of falling through to stale C.
    if (f) {
        f->state = FIGURE_STATE_DEAD;
    }
}

// Indices intentionally match figure_type enum values.
using FigureAction = void (*)(Figure *f);

static FigureAction figure_action_callbacks[] = {
    figure_nobody_action, //0
    figure_immigrant_action,
    figure_emigrant_action,
    figure_homeless_action,
    figure_cartpusher_action,
    figure_labor_seeker_action,
    figure_explosion_cloud_action,
    figure_tax_collector_action,
    figure_retired_native_action,
    figure_warehouseman_action,
    figure_retired_native_action, //10
    figure_soldier_action,
    figure_soldier_action,
    figure_soldier_action,
    figure_military_standard_action,
    figure_entertainer_action,
    figure_entertainer_action,
    figure_entertainer_action,
    figure_entertainer_action,
    figure_trade_caravan_action,
    figure_trade_ship_action, //20
    figure_trade_caravan_donkey_action,
    figure_protestor_action,
    figure_rioter_action,
    figure_rioter_action,
    figure_retired_native_action,
    figure_market_trader_action,
    figure_priest_action,
    figure_school_child_action,
    figure_teacher_action,
    figure_librarian_action, //30
    figure_barber_action,
    figure_bathhouse_worker_action,
    figure_doctor_action,
    figure_doctor_action,
    figure_retired_native_action,
    figure_editor_flag_action,
    figure_flotsam_action,
    figure_nobody_action,
    figure_supplier_action,
    figure_retired_native_action, //40
    figure_indigenous_native_action,
    figure_tower_sentry_action,
    figure_enemy43_spear_action,
    figure_enemy44_sword_action,
    figure_enemy45_sword_action,
    figure_enemy_camel_action,
    figure_enemy_elephant_action,
    figure_enemy_chariot_action,
    figure_enemy49_fast_sword_action,
    figure_enemy50_sword_action, //50
    figure_enemy51_spear_action,
    figure_enemy52_mounted_archer_action,
    figure_enemy53_axe_action,
    figure_enemy_gladiator_action,
    figure_nobody_action,
    figure_nobody_action,
    figure_enemy_caesar_legionary_action,
    figure_native_trader_action,
    figure_arrow_action,
    figure_javelin_action, //60
    figure_bolt_action,
    figure_ballista_action,
    figure_nobody_action,
    figure_missionary_action,
    figure_seagulls_action,
    figure_delivery_boy_action,
    figure_shipwreck_action,
    figure_sheep_action,
    figure_wolf_action,
    figure_zebra_action, //70
    figure_spear_action,
    figure_hippodrome_horse_action,
    figure_workcamp_worker_action,
    figure_workcamp_slave_action,
    figure_workcamp_architect_action,
    figure_supplier_action,
    figure_delivery_boy_action,
    figure_supplier_action,
    figure_tavern_action,
    figure_supplier_action, // 80
    figure_tourist_action,
    figure_watchman_action,
    figure_watchtower_archer_action,
    figure_friendly_arrow_action,
    figure_supplier_action,
    figure_robber_action,
    figure_looter_action,
    figure_delivery_boy_action,
    figure_supplier_action,
    figure_fort_supplier_action, // 90
    figure_retired_native_action,
    figure_soldier_action,
    figure_retired_native_action,
    figure_soldier_action,
    figure_enemy_catapult_action,
    figure_catapult_missile_action,
};

void figure_action_refresh_graphics(Figure *f)
{
    if (!f) return;
    switch (f->type) {
        case FIGURE_EXPLOSION: figure_explosion_cloud_update_graphics(f); break;
        case FIGURE_PROTESTER: figure_protestor_update_graphics(f); break;
        case FIGURE_RIOTER:
        case FIGURE_CRIMINAL:
        case FIGURE_CRIMINAL_ROBBER:
        case FIGURE_CRIMINAL_LOOTER: figure_criminal_update_graphics(f); break;
        case FIGURE_FLOTSAM: figure_flotsam_update_graphics(f); break;
        case FIGURE_FISH_GULLS: figure_seagulls_update_graphics(f); break;
        case FIGURE_SHEEP: figure_sheep_update_graphics(f); break;
        case FIGURE_WOLF: figure_wolf_update_graphics(f); break;
        case FIGURE_ZEBRA: figure_zebra_update_graphics(f); break;
        case FIGURE_HIPPODROME_HORSES: figure_hippodrome_horse_update_graphics(f); break;
    }
}

void figure_action_handle(void)
{
    PerformanceTrackerScope scope(PERFORMANCE_TRACKER_BUCKET_FIGURE);
    city_figures_reset();
    city_entertainment_set_hippodrome_has_race(0);
    for (unsigned int i = 1; i < Figure::count(); i++) {
        Figure *f = Figure::get(i);
        if (!f || !f->state) {
            continue;
        }
        if (f->targeted_by_figure.save_id()) {
            Figure &attacker = f->targeted_by_figure.get();
            if (attacker.state != FIGURE_STATE_ALIVE || attacker.target_figure.save_id() != i) {
                f->targeted_by_figure.clear();
            }
        }
        if (!figure_runtime_execute(f)) {
            if (f->type == FIGURE_DOCKER) {
                reinterpret_cast<figuretype::Docker *>(f)->docker_action();
            } else {
                figure_action_callbacks[f->type](f);
            }
        }
        if (f->state == FIGURE_STATE_DEAD) {
            f->remove();
        }
    }
}
