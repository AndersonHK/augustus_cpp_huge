#pragma once

#include "assets/graphics_extraction_abi.h"

class GraphicsExtractionClient {
public:
    graphics_extraction_status_v1 runAugustus(
        const graphics_extraction_augustus_request_v1 &request,
        graphics_extraction_result_v1 &result) const;
    graphics_extraction_status_v1 bootstrapClimate(
        const graphics_extraction_climate_request_v1 &request,
        graphics_extraction_result_v1 &result) const;

#ifdef STARTUP_PARSER_TEST
    static bool moduleIsLoadedForTest();
#endif
};
