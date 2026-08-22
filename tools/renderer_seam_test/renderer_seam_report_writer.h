#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>

struct RendererSeamGeometryFixtureRunResult;

struct RendererSeamMatrixCase {
    std::string id;
    std::string mod;
    std::string climate;
    std::string backend;
    std::string scale_filter;
    int city_scale_percent = 0;
    bool grid = false;
    std::string orientation;
    std::string scene;
};

struct RendererSeamMatrixCaseResult {
    std::string result = "expected_skip";
    std::string skip_reason;
    std::string artifact_status = "not_created_expected_skip";
    std::string capture_source = "not_captured";
    std::filesystem::path screenshot_path;
    std::filesystem::path seam_mask_path;
    std::filesystem::path failure_overlay_path;
    int coverage_no_background = 0;
    int no_black_gap = 0;
    int same_surface_delta = 0;
    int grid_overlay_only = 0;
    int no_grid_side_effect_gap = 0;
    int backend_parity = 0;
    int sampled_pixels = 0;
    uint64_t seam_signature = 0;
    uint64_t interior_signature = 0;
    std::string failure_detail;

    static RendererSeamMatrixCaseResult expected_skip(
        const std::filesystem::path &artifacts_root,
        const RendererSeamMatrixCase &test_case);
    static RendererSeamMatrixCaseResult from_geometry_fixture(
        const RendererSeamGeometryFixtureRunResult &fixture_result);
};

struct RendererSeamReportContext {
    std::string matrix;
    std::filesystem::path game_root;
    std::filesystem::path artifacts_root;
};

struct RendererSeamReportSummary {
    std::size_t total_cases = 0;
    int passed = 0;
    int failed = 0;
    int expected_skipped = 0;

    std::string status() const;
};

class RendererSeamReportWriter {
public:
    explicit RendererSeamReportWriter(RendererSeamReportContext context);

    RendererSeamReportSummary summarize(
        const std::vector<RendererSeamMatrixCase> &cases,
        const std::vector<RendererSeamMatrixCaseResult> &results) const;
    std::filesystem::path prepare_artifacts() const;
    void write(
        const std::filesystem::path &result_path,
        const std::vector<RendererSeamMatrixCase> &cases,
        const std::vector<RendererSeamMatrixCaseResult> &results) const;

private:
    RendererSeamReportContext context_;

    std::string json_escape(const std::string &text) const;
    std::string json_string(const std::string &text) const;
    std::string json_path(const std::filesystem::path &path) const;

    void write_header(std::ostream &out, const RendererSeamReportSummary &summary) const;
    void write_summary(std::ostream &out, const RendererSeamReportSummary &summary) const;
    void write_assertions(std::ostream &out) const;
    void write_cases(
        std::ostream &out,
        const std::vector<RendererSeamMatrixCase> &cases,
        const std::vector<RendererSeamMatrixCaseResult> &results) const;
    void write_case(
        std::ostream &out,
        const RendererSeamMatrixCase &test_case,
        const RendererSeamMatrixCaseResult &result,
        bool is_last) const;
    void write_case_inputs(std::ostream &out, const RendererSeamMatrixCase &test_case) const;
    void write_pixel_assertions(std::ostream &out, const RendererSeamMatrixCaseResult &result) const;
    void write_artifacts(std::ostream &out, const RendererSeamMatrixCaseResult &result) const;
};
