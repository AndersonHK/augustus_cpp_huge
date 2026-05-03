#pragma once

#include <stdint.h>

void encoding_simp_chinese_init(void);

void encoding_simp_chinese_to_utf8(const uint8_t *input, char *output, int output_length);

void encoding_simp_chinese_from_utf8(const char *input, uint8_t *output, int output_length);
