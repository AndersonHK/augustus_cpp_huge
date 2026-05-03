#pragma once

#include <stdint.h>

void window_text_input_show(const uint8_t *title, const uint8_t *placeholder, const uint8_t *text, int max_length,
    void (*callback)(const uint8_t *));

void window_text_input_expanded_show(const uint8_t *title, const uint8_t *placeholder, const uint8_t *text,
     int max_length, void(*callback)(const uint8_t *), const char *allowed_chars);
