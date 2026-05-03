#pragma once

#include "core/buffer.h"

int empire_xml_parse_file(const char *filename, int info_only);

const char *empire_xml_read_info(void);
