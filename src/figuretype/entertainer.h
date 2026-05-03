#pragma once

#include "figure/figure.h"

#ifdef __cplusplus
extern "C" {
#endif

void figure_entertainer_action(figure *f);

void figure_tourist_action(figure *f);

void figure_spawn_tourist(void);

#ifdef __cplusplus
}
#endif
