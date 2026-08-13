#include "assets/graphics_extraction_abi.h"

#include "assets/augustus_asset_extractor.h"

#include <cstring>
#include <string>

namespace extraction = vespasian::graphics::extraction;

namespace {

constexpr uint32_t known_flags = GRAPHICS_EXTRACTION_FORCE | GRAPHICS_EXTRACTION_WRITE_STAMP;

bool initialize_result(graphics_extraction_result_v1 *result)
{
    if (!result || result->struct_size < sizeof(*result)) {
        return false;
    }
    std::memset(result, 0, sizeof(*result));
    result->struct_size = sizeof(*result);
    result->abi_version = GRAPHICS_EXTRACTION_ABI_VERSION;
    return true;
}

bool valid_common_request(uint32_t struct_size, size_t required_size, uint32_t abi_version, uint32_t flags, uint32_t reserved)
{
    return struct_size >= required_size && abi_version == GRAPHICS_EXTRACTION_ABI_VERSION &&
        (flags & ~known_flags) == 0 && reserved == 0;
}

extraction::ExtractorOptions options_from_flags(uint32_t flags)
{
    return extraction::ExtractorOptions(
        (flags & GRAPHICS_EXTRACTION_FORCE) != 0,
        (flags & GRAPHICS_EXTRACTION_WRITE_STAMP) != 0);
}

void copy_augustus_report(
    graphics_extraction_result_v1 &result,
    const extraction::AugustusExtractionReport &report)
{
    result.succeeded = report.succeeded();
    result.source_files = report.source_files();
    result.groups_exported = report.groups_exported();
    result.images_exported = report.images_exported();
    result.pngs_written = report.pngs_written();
}

void copy_julius_report(
    graphics_extraction_result_v1 &result,
    const extraction::JuliusExtractionReport &report)
{
    result.succeeded = report.succeeded();
    result.groups_exported = report.groups_exported();
    result.images_exported = report.images_exported();
    result.pngs_written = report.pngs_written();
}

} // namespace

extern "C" graphics_extraction_status_v1 graphics_extraction_run_augustus_v1(
    const graphics_extraction_augustus_request_v1 *request,
    graphics_extraction_result_v1 *result)
{
    if (!initialize_result(result) || !request ||
        !valid_common_request(request->struct_size, sizeof(*request), request->abi_version, request->flags, request->reserved)) {
        return GRAPHICS_EXTRACTION_STATUS_INVALID_ABI;
    }

#ifdef JULIUS_GRAPHICS_EXTRACTOR
    return GRAPHICS_EXTRACTION_STATUS_UNAVAILABLE;
#else
    const extraction::ExtractorPaths paths(
        request->game_root ? request->game_root : "",
        request->source_graphics ? request->source_graphics : "",
        request->output_graphics ? request->output_graphics : "",
        request->julius_graphics ? request->julius_graphics : "");
    extraction::AugustusExtractor extractor;
    const extraction::AugustusExtractionReport report = extractor.extract(paths, options_from_flags(request->flags));
    copy_augustus_report(*result, report);
    return report.succeeded() ? GRAPHICS_EXTRACTION_STATUS_SUCCEEDED : GRAPHICS_EXTRACTION_STATUS_FAILED;
#endif
}

extern "C" graphics_extraction_status_v1 graphics_extraction_bootstrap_climate_v1(
    const graphics_extraction_climate_request_v1 *request,
    graphics_extraction_result_v1 *result)
{
    if (!initialize_result(result) || !request ||
        !valid_common_request(request->struct_size, sizeof(*request), request->abi_version, request->flags, request->reserved) ||
        !request->images || request->image_count <= 0 || !request->group_image_ids || request->group_count <= 0 ||
        !request->source_name || !*request->source_name || !request->atlas_data) {
        return GRAPHICS_EXTRACTION_STATUS_INVALID_ABI;
    }

    const extraction::LegacyClimateAtlas climate(
        static_cast<const image *>(request->images),
        request->image_count,
        request->group_image_ids,
        request->group_count,
        request->source_name,
        static_cast<const image_atlas_data *>(request->atlas_data));
#ifdef JULIUS_GRAPHICS_EXTRACTOR
    extraction::JuliusExtractor extractor;
    const extraction::JuliusExtractionReport report = extractor.extract(climate);
    copy_julius_report(*result, report);
    return report.succeeded() ? GRAPHICS_EXTRACTION_STATUS_SUCCEEDED : GRAPHICS_EXTRACTION_STATUS_FAILED;
#else
    extraction::RuntimeGraphicsExtractionService service;
    const extraction::ExtractionReport report = service.bootstrapAfterClimateLoad(
        climate,
        options_from_flags(request->flags));
    copy_augustus_report(*result, report.augustus());
    result->groups_exported += report.julius().groups_exported();
    result->images_exported += report.julius().images_exported();
    result->pngs_written += report.julius().pngs_written();
    result->succeeded = report.succeeded();
    return report.succeeded() ? GRAPHICS_EXTRACTION_STATUS_SUCCEEDED : GRAPHICS_EXTRACTION_STATUS_FAILED;
#endif
}
