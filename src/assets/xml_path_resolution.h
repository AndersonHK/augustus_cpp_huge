#pragma once

#include "assets/xml.h"

#include <cstddef>
#include <string>
#include <vector>

struct GraphicsLayerSource {
    std::size_t layer_index = 0;
    std::string mod_name;
    std::string graphics_root;

    bool operator==(const GraphicsLayerSource &other) const
    {
        return layer_index == other.layer_index && graphics_root == other.graphics_root;
    }
};

std::vector<GraphicsLayerSource> xml_configured_graphics_sources();
std::vector<GraphicsLayerSource> xml_collect_assetlist_sources(
    const std::vector<GraphicsLayerSource> &layers,
    const char *assetlist_key);

int xml_resolve_assetlist_path(char *full_path, const char *assetlist_key, const GraphicsLayerSource &source);
int xml_resolve_image_path(
    const std::vector<GraphicsLayerSource> &layers,
    char *full_path,
    const char *assetlist_key,
    const char *image_file_name,
    const GraphicsLayerSource &source);
int xml_resolve_image_path(
    char *full_path,
    const char *assetlist_key,
    const char *image_file_name,
    const GraphicsLayerSource &source);

int xml_resolve_graphics_path(
    char *full_path,
    const char *relative_path,
    xml_asset_source source,
    xml_asset_source *resolved_source);
