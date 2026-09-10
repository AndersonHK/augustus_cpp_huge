#pragma once

#include "assets/image_group_payload_internal.h"
#include "figure/figure_type_registry_internal.h"
#include "startup/startup_parser_graphics_test.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <ostream>

inline bool validate_extracted_animation_contract(std::ostream &errors)
{
    using namespace image_group_payload_internal;
    using namespace figure_type_registry_impl;
    const auto *gladiator = definition_for(FIGURE_GLADIATOR);
    if (!gladiator) return false;
    const auto &graphics = gladiator->graphics();
    for (int wait : { -128, -1, 0, 5 }) for (int direction = 0; direction < 8; ++direction) for (int frame = 1; frame <= 16; ++frame) {
        const auto *expected = graphics.cached_target_binding(GraphicsTargetRole::Action, direction, frame);
        if (!expected || graphics.cached_target_binding_for_state(FIGURE_ACTION_150_ATTACK, wait, frame - 1, 0, direction, 0) != expected) {
            errors << "An ungated action lost its animation because of an unrelated wait counter.\n";
            return false;
        }
    }
    auto gated = graphics;
    gated.action_min_wait_ticks = 5;
    gated.action_min_missile_wait_ticks = 3;
    if (gated.action_graphics_matches(FIGURE_ACTION_150_ATTACK, -1, 3) ||
        gated.action_graphics_matches(FIGURE_ACTION_150_ATTACK, 5, 2) ||
        !gated.action_graphics_matches(FIGURE_ACTION_150_ATTACK, 5, 3)) {
        errors << "An explicitly gated action ignored its wait requirements.\n";
        return false;
    }
    if (!image_group_payload_load("Walkers\\Group_115")) return false;
    struct FixtureDirectory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("vespasian_animation_contract_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ~FixtureDirectory() { std::error_code error; std::filesystem::remove_all(path, error); }
    } fixture;
    std::filesystem::create_directories(fixture.path / "AnimationContract");
    const GraphicsLayerSource source { 999, "AnimationContract", fixture.path.string() };
    const auto write = [&](const char *name, const std::string &body) {
        std::ofstream file(fixture.path / "AnimationContract" / (std::string(name) + ".xml"));
        file << "<assetlist name=\"AnimationContract\\" << name << "\">" << body << "</assetlist>";
        return file.good();
    };
    const std::string base = "<image id=\"unrelated\" group=\"Walkers\\Group_115\" image=\"corpse\"/>";
    const std::string selected = "<image id=\"selected\" group=\"Walkers\\Group_115\" image=\"gesture\" frame=\"16\"/>";
    if (!write("Before", base + selected) || !write("After", selected + base)) return false;
    const ResolvedImageEntry *before = materialize_source_entry("AnimationContract\\Before", source, "selected");
    const ResolvedImageEntry *after = materialize_source_entry("AnimationContract\\After", source, "selected");
    const auto *criminal = image_group_payload_get("Walkers\\Group_115");
    const auto *gesture = criminal ? criminal->entry_for("gesture") : nullptr;
    if (!before || !after || !gesture || before->has_animation || after->has_animation ||
        startup_parser::image_resource_fingerprint(before->footprint.slice.handle) !=
            startup_parser::image_resource_fingerprint(after->footprint.slice.handle) ||
        startup_parser::image_resource_fingerprint(before->footprint.slice.handle) !=
            startup_parser::image_resource_fingerprint(gesture->animation().frame_slice_at_offset(16, 0).handle)) {
        errors << "Named animation frame reference changed pixels or depended on document order.\n";
        return false;
    }
    const char *invalid[] = {
        "<image id=\"bad\" src=\"unused\"><animation frames=\"2\"/></image>",
        "<image id=\"bad\" src=\"unused\"><animation frames=\"2\"><frame src=\"unused\"/></animation></image>",
        "<image id=\"bad\" group=\"Walkers\\Group_115\" image=\"gesture\" frame=\"0\"/>",
        "<image id=\"bad\" group=\"Walkers\\Group_115\" image=\"gesture\" frame=\"1oops\"/>",
        "<image id=\"bad\" src=\"unused\" frame=\"1\"/>"
    };
    for (int i = 0; i < static_cast<int>(sizeof(invalid) / sizeof(invalid[0])); ++i) {
        const std::string name = "Invalid" + std::to_string(i);
        if (!write(name.c_str(), invalid[i]) || load_group_doc("AnimationContract\\" + name, source)) {
            errors << "Incomplete or malformed animation data was accepted: " << name << ".\n";
            return false;
        }
    }
    if (!write("Bounds", "<image id=\"bad\" group=\"Walkers\\Group_115\" image=\"gesture\" frame=\"17\"/>") ||
        materialize_source_entry("AnimationContract\\Bounds", source, "bad")) {
        errors << "Out-of-range named animation frame was accepted.\n";
        return false;
    }
    if (!write("Missing", "<image id=\"bad\" group=\"Walkers\\Group_115\" image=\"absent\" frame=\"1\"/>") ||
        materialize_source_entry("AnimationContract\\Missing", source, "bad")) {
        errors << "Missing named animation was accepted.\n";
        return false;
    }
    return true;
}
