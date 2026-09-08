#pragma once

#include "figure/figure.h"
#include <cstring>


void figure_phrase_determine(Figure *f);

int figure_phrase_play(Figure *f);

inline int figure_phrase_voice_id(const char *name)
{
    // These indices also select the original language archive's phrase groups.
    static const char *names[] = {"prefect", "troop", "engineer", "tax_collector", "market_lady", "cartpusher", "donkey", "boats", "priest", "teacher", "pupils", "bathhouse", "doctor", "barber", "actor", "gladiator", "lion_tamer", "charioteer", "patrician", "plebeian", "rioter", "homeless", "unemployed", "emigrant", "immigrant", "enemy", nullptr, nullptr, nullptr, nullptr, "missionary", "granary_boy", "ox"};
    if (!name) return -1;
    for (int i = 0; i < static_cast<int>(sizeof(names) / sizeof(*names)); ++i) if (names[i] && std::strcmp(name, names[i]) == 0) return i;
    return -1;
}
int figure_phrase_sound_id(const Figure &figure);
const char *figure_phrase_sound_file(const Figure &figure);
