#pragma once

#include "core/image.h"
#include "core/legacy_image_extractor.h"
#include "graphics/renderer.h"

#include <string>
#include <utility>

namespace vespasian::graphics::extraction {

class ExtractorPaths {
public:
    ExtractorPaths() = default;
    ExtractorPaths(
        std::string game_root,
        std::string source_graphics,
        std::string output_graphics,
        std::string julius_graphics)
        : game_root_(std::move(game_root))
        , source_graphics_(std::move(source_graphics))
        , output_graphics_(std::move(output_graphics))
        , julius_graphics_(std::move(julius_graphics))
    {
    }

    bool has_overrides() const
    {
        return !game_root_.empty() || !source_graphics_.empty() || !output_graphics_.empty() || !julius_graphics_.empty();
    }
    const std::string &game_root() const { return game_root_; }
    const std::string &source_graphics() const { return source_graphics_; }
    const std::string &output_graphics() const { return output_graphics_; }
    const std::string &julius_graphics() const { return julius_graphics_; }

private:
    std::string game_root_;
    std::string source_graphics_;
    std::string output_graphics_;
    std::string julius_graphics_;
};

class ExtractorOptions {
public:
    ExtractorOptions() = default;
    ExtractorOptions(bool force, bool write_stamp)
        : force_(force)
        , write_stamp_(write_stamp)
    {
    }

    bool force() const { return force_; }
    bool write_stamp() const { return write_stamp_; }

private:
    bool force_ = false;
    bool write_stamp_ = true;
};

class AugustusExtractionReport {
public:
    AugustusExtractionReport() = default;
    AugustusExtractionReport(bool succeeded, int source_files, int groups_exported, int images_exported, int pngs_written)
        : succeeded_(succeeded)
        , source_files_(source_files)
        , groups_exported_(groups_exported)
        , images_exported_(images_exported)
        , pngs_written_(pngs_written)
    {
    }

    bool succeeded() const { return succeeded_; }
    int source_files() const { return source_files_; }
    int groups_exported() const { return groups_exported_; }
    int images_exported() const { return images_exported_; }
    int pngs_written() const { return pngs_written_; }

private:
    bool succeeded_ = false;
    int source_files_ = 0;
    int groups_exported_ = 0;
    int images_exported_ = 0;
    int pngs_written_ = 0;
};

class ExtractionReport {
public:
    void set_julius(JuliusExtractionReport report)
    {
        julius_ = report;
        has_julius_ = true;
    }
    void set_augustus(AugustusExtractionReport report)
    {
        augustus_ = report;
        has_augustus_ = true;
    }

    bool succeeded() const { return (!has_julius_ || julius_.succeeded()) && (!has_augustus_ || augustus_.succeeded()); }
    const JuliusExtractionReport &julius() const { return julius_; }
    const AugustusExtractionReport &augustus() const { return augustus_; }

private:
    JuliusExtractionReport julius_;
    AugustusExtractionReport augustus_;
    bool has_julius_ = false;
    bool has_augustus_ = false;
};

class AugustusExtractor {
public:
    AugustusExtractionReport extract(const ExtractorPaths &paths, const ExtractorOptions &options);
};

class RuntimeGraphicsExtractionService {
public:
    ExtractionReport bootstrapAfterClimateLoad(const LegacyClimateAtlas &climate, const ExtractorOptions &options);
};

} // namespace vespasian::graphics::extraction
