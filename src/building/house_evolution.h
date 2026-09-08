#pragma once

#include "building/building_fwd.h"
class Building;

enum HouseEvolutionWarning {
    HOUSE_EVOLUTION_FOURTH_RELIGION_DEVOLVE = 69,
    HOUSE_EVOLUTION_FIFTH_RELIGION_DEVOLVE = 70,
    HOUSE_EVOLUTION_FOURTH_RELIGION_EVOLVE = 71,
    HOUSE_EVOLUTION_FIFTH_RELIGION_EVOLVE = 72
};
const char *building_house_extended_evolution_translation(int warning);


class Building;

/**
 * Evolves/devolves houses if appropriate, and consumes pottery/furniture/oil/wine
 */
void building_house_process_evolve_and_consume_goods(void);

// Spread percentage savings over months without rounding them to whole periods.
bool building_house_consumes_goods_this_month(int month, int reduction_percent);

/**
 * Determine the text to show for evolution of a house, stored in house->evolve_text_id
 * @param house House to determine text for
 * @param worst_desirability_building The ID of the building with worst contribution to desirability
 */
void building_house_determine_evolve_text(Building house, int worst_desirability_building);

/**
 * Determine building with worst contribution to desirability
 * @param house House to determine worst building for
 * @return Worst desirability building ID
 */
building_type building_house_determine_worst_desirability_building_type(Building house);

