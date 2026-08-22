#pragma once

#include "assets/image_group_entry.h"
#include "assets/xml.h"

#include <string>
#include <unordered_map>
#include <vector>

struct ImageGroupFigureGraphicsContract {
    int present = 0;
    int runtime_selected_image = 0;
    int runtime_selected_empty_is_hidden = 0;
    std::string runtime_selected_source;
    std::string default_image_pattern;
    int default_frame_count = 0;
    std::string corpse_image_pattern;
    int corpse_frame_count = 0;
    std::string action_image_pattern;
    std::string action_state;
    int action_frame_count = 0;
    int action_min_wait_ticks = 0;
};

class ImageGroupPayload {
public:
    ImageGroupPayload(std::string key, std::string xml_path, xml_asset_source source);

    const std::string &key() const;
    const std::string &xml_path() const;
    xml_asset_source source() const;

    void add_entry(std::string internal_key, std::string image_id, ImageGroupEntry entry);
    void set_default_entry(const std::string &internal_key);
    const ImageGroupEntry *entry_for(const char *image_id) const;
    const ImageGroupEntry *entry_at_index(int index) const;
    const ImageGroupEntry *default_entry() const;
    const char *default_image_id() const;
    int entry_count() const;
    void set_figure_graphics_contract(ImageGroupFigureGraphicsContract contract);
    const ImageGroupFigureGraphicsContract *figure_graphics_contract() const;

private:
    std::string key_;
    std::string xml_path_;
    xml_asset_source source_ = XML_ASSET_SOURCE_AUTO;
    std::unordered_map<std::string, ImageGroupEntry> entries_;
    std::unordered_map<std::string, std::string> named_entry_keys_;
    std::vector<std::string> ordered_entry_keys_;
    std::string default_image_id_;
    std::string default_entry_key_;
    ImageGroupFigureGraphicsContract figure_graphics_contract_;
};

const ImageGroupPayload *image_group_payload_get(const char *path_key);
int image_group_payload_load(const char *path_key);


void image_group_payload_clear_all(void);

