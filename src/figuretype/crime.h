#pragma once

#include "figure/figure.h"


void figure_generate_criminals(void);

void figure_protestor_action(Figure *f);

void figure_protestor_update_graphics(Figure *f);

void figure_criminal_update_graphics(Figure *f);

void figure_rioter_action(Figure *f);

void figure_robber_action(Figure *f);

void figure_looter_action(Figure *f);

int figure_rioter_collapse_building(Figure *f);
