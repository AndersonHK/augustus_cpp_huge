#include "assets/graphics_extraction_abi.h"

#include "assets/augustus_asset_extractor.h"
#include "assets/graphics_extractor_shims.h"

#include <cstring>
#include <string>

namespace extraction = vespasian::graphics::extraction;

namespace {

constexpr uint32_t known_flags = GRAPHICS_EXTRACTION_FORCE | GRAPHICS_EXTRACTION_WRITE_STAMP;
constexpr size_t climate_request_v1_base_size = offsetof(graphics_extraction_climate_request_v1, mode);

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

bool has_climate_paths(const graphics_extraction_climate_request_v1 &request)
{
    return request.struct_size >= sizeof(request);
}

void configure_extractor_shims(const char *game_root, const char *augustus_graphics, const char *julius_graphics)
{
    augustus_graphics_extractor_shims_set_game_root(game_root);
    augustus_graphics_extractor_shims_set_augustus_graphics_path(augustus_graphics);
    augustus_graphics_extractor_shims_set_julius_graphics_path(julius_graphics);
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

    try {
        configure_extractor_shims(request->game_root, request->output_graphics, request->julius_graphics);
        const extraction::ExtractorPaths paths(
            request->game_root ? request->game_root : "",
            request->source_graphics ? request->source_graphics : "",
            request->output_graphics ? request->output_graphics : "",
            request->julius_graphics ? request->julius_graphics : "");
        extraction::AugustusExtractor extractor;
        const extraction::AugustusExtractionReport report = extractor.extract(paths, options_from_flags(request->flags));
        copy_augustus_report(*result, report);
        return report.succeeded() ? GRAPHICS_EXTRACTION_STATUS_SUCCEEDED : GRAPHICS_EXTRACTION_STATUS_FAILED;
    } catch (...) {
        return GRAPHICS_EXTRACTION_STATUS_FAILED;
    }
}

extern "C" graphics_extraction_status_v1 graphics_extraction_bootstrap_climate_v1(
    const graphics_extraction_climate_request_v1 *request,
    graphics_extraction_result_v1 *result)
{
    if (!initialize_result(result) || !request ||
        !valid_common_request(request->struct_size, climate_request_v1_base_size, request->abi_version, request->flags, request->reserved) ||
        !request->images || request->image_count <= 0 || !request->group_image_ids || request->group_count <= 0 ||
        !request->source_name || !*request->source_name || !request->atlas_data) {
        return GRAPHICS_EXTRACTION_STATUS_INVALID_ABI;
    }

    const bool has_paths = has_climate_paths(*request);
    if (request->struct_size > climate_request_v1_base_size && !has_paths) {
        return GRAPHICS_EXTRACTION_STATUS_INVALID_ABI;
    }
    const uint32_t mode = has_paths ? request->mode : GRAPHICS_EXTRACTION_CLIMATE_RUNTIME_BOOTSTRAP;
    if (mode > GRAPHICS_EXTRACTION_CLIMATE_JULIUS_ONLY || (has_paths && request->path_reserved != 0)) {
        return GRAPHICS_EXTRACTION_STATUS_INVALID_ABI;
    }

    try {
        if (has_paths) {
            configure_extractor_shims(request->game_root, request->augustus_graphics, request->julius_graphics);
        }
        const extraction::LegacyClimateAtlas climate(
            static_cast<const image *>(request->images),
            request->image_count,
            request->group_image_ids,
            request->group_count,
            request->source_name,
            static_cast<const image_atlas_data *>(request->atlas_data));
        if (mode == GRAPHICS_EXTRACTION_CLIMATE_JULIUS_ONLY) {
            extraction::JuliusExtractor extractor;
            const extraction::JuliusExtractionReport report = extractor.extract(climate);
            copy_julius_report(*result, report);
            return report.succeeded() ? GRAPHICS_EXTRACTION_STATUS_SUCCEEDED : GRAPHICS_EXTRACTION_STATUS_FAILED;
        }

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
    } catch (...) {
        return GRAPHICS_EXTRACTION_STATUS_FAILED;
    }
}
