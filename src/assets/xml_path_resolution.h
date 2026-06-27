#pragma once

#include "assets/xml.h"

int xml_resolve_graphics_path(
    char *full_path,
    const char *relative_path,
    xml_asset_source source,
    xml_asset_source *resolved_source);
