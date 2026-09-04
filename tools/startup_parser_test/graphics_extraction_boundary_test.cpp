#include "graphics_extraction_boundary_test.h"

#include "assets/graphics_extraction_client.h"

#include <ostream>

bool validate_graphics_extraction_boundary(std::ostream &errors)
{
#ifdef _WIN32
    const bool initially_loaded = GraphicsExtractionClient::moduleIsLoadedForTest();
    for (int attempt = 0; attempt < 3; ++attempt) {
        graphics_extraction_augustus_request_v1 request = {};
        request.struct_size = sizeof(request);
        request.abi_version = GRAPHICS_EXTRACTION_ABI_VERSION + 1;
        graphics_extraction_result_v1 result = {};
        result.struct_size = sizeof(result);
        if (GraphicsExtractionClient().runAugustus(request, result) != GRAPHICS_EXTRACTION_STATUS_INVALID_ABI) {
            errors << "GraphicsExtractor did not reject an incompatible ABI through its dynamic client.\n";
            return false;
        }
        if (GraphicsExtractionClient::moduleIsLoadedForTest() != initially_loaded) {
            errors << "GraphicsExtractor remained loaded after a client operation.\n";
            return false;
        }

        graphics_extraction_climate_request_v1 climate_request = {};
        climate_request.struct_size = sizeof(climate_request);
        climate_request.abi_version = GRAPHICS_EXTRACTION_ABI_VERSION + 1;
        result = {};
        result.struct_size = sizeof(result);
        if (GraphicsExtractionClient().bootstrapClimate(climate_request, result) != GRAPHICS_EXTRACTION_STATUS_INVALID_ABI) {
            errors << "GraphicsExtractor climate entry point did not reject an incompatible ABI.\n";
            return false;
        }
        if (GraphicsExtractionClient::moduleIsLoadedForTest() != initially_loaded) {
            errors << "GraphicsExtractor remained loaded after a climate client operation.\n";
            return false;
        }
    }
#else
    (void) errors;
#endif
    return true;
}
