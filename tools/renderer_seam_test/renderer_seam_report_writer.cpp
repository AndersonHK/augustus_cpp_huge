#include "renderer_seam_report_writer.h"

#include "renderer_seam_geometry_fixture.h"

#include <fstream>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <system_error>
#include <utility>

std::string RendererSeamReportSummary::status() const
{
    return failed ? "failed" : expected_skipped ? "partial_expected_skip" : "passed";
}

RendererSeamMatrixCaseResult RendererSeamMatrixCaseResult::expected_skip(
    const std::filesystem::path &artifacts_root,
    const RendererSeamMatrixCase &test_case)
{
    RendererSeamMatrixCaseResult result;
    result.skip_reason = "offscreen city renderer capture and screenshot comparison are not wired in this harness slice";
    result.screenshot_path = artifacts_root / (test_case.id + ".png");
    result.seam_mask_path = artifacts_root / (test_case.id + ".seam-mask.png");
    result.failure_overlay_path = artifacts_root / (test_case.id + ".failure-overlay.png");
    return result;
}

RendererSeamMatrixCaseResult RendererSeamMatrixCaseResult::from_geometry_fixture(
    const RendererSeamGeometryFixtureRunResult &fixture_result)
{
    RendererSeamMatrixCaseResult result;
    result.result = fixture_result.result;
    result.skip_reason = fixture_result.skip_reason;
    result.artifact_status = fixture_result.artifact_status;
    result.capture_source = fixture_result.capture_source;
    result.screenshot_path = fixture_result.artifacts.screenshot_path;
    result.seam_mask_path = fixture_result.artifacts.seam_mask_path;
    result.failure_overlay_path = fixture_result.artifacts.failure_overlay_path;
    result.coverage_no_background = fixture_result.coverage_no_background;
    result.no_black_gap = fixture_result.no_black_gap;
    result.same_surface_delta = fixture_result.same_surface_delta;
    result.sampled_pixels = fixture_result.sampled_pixels;
    result.failure_detail = fixture_result.failure_detail;
    return result;
}

RendererSeamReportWriter::RendererSeamReportWriter(RendererSeamReportContext context)
    : context_(std::move(context))
{
}

RendererSeamReportSummary RendererSeamReportWriter::summarize(
    const std::vector<RendererSeamMatrixCase> &cases,
    const std::vector<RendererSeamMatrixCaseResult> &results) const
{
    RendererSeamReportSummary summary;
    summary.total_cases = cases.size();
    for (const RendererSeamMatrixCaseResult &result : results) {
        if (result.result == "pass") {
            ++summary.passed;
        } else if (result.result == "fail") {
            ++summary.failed;
        } else {
            ++summary.expected_skipped;
        }
    }
    return summary;
}

std::filesystem::path RendererSeamReportWriter::prepare_artifacts() const
{
    std::error_code error;
    std::filesystem::create_directories(context_.artifacts_root, error);
    if (error) {
        throw std::runtime_error(
            "Unable to create artifact folder: " + context_.artifacts_root.string() + " (" + error.message() + ")");
    }
    return context_.artifacts_root / "renderer_seam_results.json";
}

void RendererSeamReportWriter::write(
    const std::filesystem::path &result_path,
    const std::vector<RendererSeamMatrixCase> &cases,
    const std::vector<RendererSeamMatrixCaseResult> &results) const
{
    std::ofstream out(result_path, std::ios::binary);
    if (!out) {
        throw std::runtime_error("Unable to write JSON result file: " + result_path.string());
    }

    write_header(out, summarize(cases, results));
    write_assertions(out);
    write_cases(out, cases, results);
    out << "}\n";
}

std::string RendererSeamReportWriter::json_escape(const std::string &text) const
{
    std::ostringstream out;
    for (char ch : text) {
        switch (ch) {
            case '\\':
                out << "\\\\";
                break;
            case '"':
                out << "\\\"";
                break;
            case '\b':
                out << "\\b";
                break;
            case '\f':
                out << "\\f";
                break;
            case '\n':
                out << "\\n";
                break;
            case '\r':
                out << "\\r";
                break;
            case '\t':
                out << "\\t";
                break;
            default:
                if (static_cast<unsigned char>(ch) < 0x20) {
                    out << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                        << static_cast<int>(static_cast<unsigned char>(ch));
                } else {
                    out << ch;
                }
                break;
        }
    }
    return out.str();
}

std::string RendererSeamReportWriter::json_string(const std::string &text) const
{
    return "\"" + json_escape(text) + "\"";
}

std::string RendererSeamReportWriter::json_path(const std::filesystem::path &path) const
{
    return json_string(path.string());
}

void RendererSeamReportWriter::write_header(std::ostream &out, const RendererSeamReportSummary &summary) const
{
    out << "{\n";
    out << "  \"tool\": \"RendererSeamTest\",\n";
    out << "  \"matrix\": " << json_string(context_.matrix) << ",\n";
    out << "  \"game_root\": " << json_path(context_.game_root) << ",\n";
    out << "  \"artifacts_root\": " << json_path(context_.artifacts_root) << ",\n";
    out << "  \"status\": " << json_string(summary.status()) << ",\n";
    write_summary(out, summary);
}

void RendererSeamReportWriter::write_summary(std::ostream &out, const RendererSeamReportSummary &summary) const
{
    out << "  \"summary\": {\n";
    out << "    \"total_cases\": " << summary.total_cases << ",\n";
    out << "    \"passed\": " << summary.passed << ",\n";
    out << "    \"failed\": " << summary.failed << ",\n";
    out << "    \"expected_skipped\": " << summary.expected_skipped << "\n";
    out << "  },\n";
}

void RendererSeamReportWriter::write_assertions(std::ostream &out) const
{
    out << "  \"assertions\": [\n";
    out << "    \"coverage_no_background\",\n";
    out << "    \"no_black_gap\",\n";
    out << "    \"same_surface_delta\",\n";
    out << "    \"grid_overlay_only\",\n";
    out << "    \"no_grid_side_effect_gap\",\n";
    out << "    \"backend_parity\"\n";
    out << "  ],\n";
}

void RendererSeamReportWriter::write_cases(
    std::ostream &out,
    const std::vector<RendererSeamMatrixCase> &cases,
    const std::vector<RendererSeamMatrixCaseResult> &results) const
{
    out << "  \"cases\": [\n";
    for (std::size_t index = 0; index < cases.size(); ++index) {
        write_case(out, cases[index], results[index], index + 1 == cases.size());
    }
    out << "  ]\n";
}

void RendererSeamReportWriter::write_case(
    std::ostream &out,
    const RendererSeamMatrixCase &test_case,
    const RendererSeamMatrixCaseResult &result,
    bool is_last) const
{
    out << "    {\n";
    out << "      \"id\": " << json_string(test_case.id) << ",\n";
    write_case_inputs(out, test_case);
    out << "      \"result\": " << json_string(result.result) << ",\n";
    if (result.result == "expected_skip") {
        out << "      \"skip_reason\": " << json_string(result.skip_reason) << ",\n";
    }
    out << "      \"artifact_status\": " << json_string(result.artifact_status) << ",\n";
    out << "      \"capture_source\": " << json_string(result.capture_source) << ",\n";
    write_pixel_assertions(out, result);
    if (!result.failure_detail.empty()) {
        out << "      \"failure_detail\": " << json_string(result.failure_detail) << ",\n";
    }
    write_artifacts(out, result);
    out << "    }" << (is_last ? "\n" : ",\n");
}

void RendererSeamReportWriter::write_case_inputs(
    std::ostream &out,
    const RendererSeamMatrixCase &test_case) const
{
    out << "      \"inputs\": {\n";
    out << "        \"mod\": " << json_string(test_case.mod) << ",\n";
    out << "        \"climate\": " << json_string(test_case.climate) << ",\n";
    out << "        \"backend\": " << json_string(test_case.backend) << ",\n";
    out << "        \"scale_filter\": " << json_string(test_case.scale_filter) << ",\n";
    out << "        \"city_scale_percent\": " << test_case.city_scale_percent << ",\n";
    out << "        \"grid\": " << (test_case.grid ? "true" : "false") << ",\n";
    out << "        \"orientation\": " << json_string(test_case.orientation) << ",\n";
    out << "        \"scene\": " << json_string(test_case.scene) << "\n";
    out << "      },\n";
}

void RendererSeamReportWriter::write_pixel_assertions(
    std::ostream &out,
    const RendererSeamMatrixCaseResult &result) const
{
    out << "      \"pixel_assertions\": {\n";
    out << "        \"coverage_no_background\": " << (result.coverage_no_background ? "true" : "false") << ",\n";
    out << "        \"no_black_gap\": " << (result.no_black_gap ? "true" : "false") << ",\n";
    out << "        \"same_surface_delta\": " << (result.same_surface_delta ? "true" : "false") << ",\n";
    out << "        \"sampled_pixels\": " << result.sampled_pixels << "\n";
    out << "      },\n";
}

void RendererSeamReportWriter::write_artifacts(
    std::ostream &out,
    const RendererSeamMatrixCaseResult &result) const
{
    out << "      \"artifacts\": {\n";
    out << "        \"screenshot\": " << json_path(result.screenshot_path) << ",\n";
    out << "        \"seam_mask\": " << json_path(result.seam_mask_path) << ",\n";
    out << "        \"failure_overlay\": " << json_path(result.failure_overlay_path) << "\n";
    out << "      }\n";
}
