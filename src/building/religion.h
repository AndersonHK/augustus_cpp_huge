#pragma once

extern "C" {
#include "city/constants.h"
}

#include "translation/translation.h"

#include <string>
#include <vector>

class God;

namespace building_type_registry_impl {

enum class ReligionTier {
    None,
    Shrine,
    Small,
    Large,
    Grand,
    Oracle
};

class ReligionPresentation {
public:
    void set_sound(std::string value);
    void set_name_key(translation_key value);
    void set_bonus_key(translation_key value);
    void set_quote_key(translation_key value);
    void set_banner_group(std::string value);
    void set_banner_image(std::string value);
    void set_content_y_offset(int value);
    void set_height_blocks(int value);
    void set_module_index(int value);

    const char *sound() const;
    translation_key name_key() const;
    translation_key bonus_key() const;
    translation_key quote_key() const;
    const char *banner_group() const;
    const char *banner_image() const;
    int content_y_offset() const;
    int height_blocks() const;
    int module_index() const;
    int has_any() const;
    int is_complete() const;

private:
    enum Field {
        FieldSound = 1 << 0,
        FieldName = 1 << 1,
        FieldBonus = 1 << 2,
        FieldQuote = 1 << 3,
        FieldBannerGroup = 1 << 4,
        FieldBannerImage = 1 << 5,
        FieldContentYOffset = 1 << 6,
        FieldHeightBlocks = 1 << 7,
        FieldModuleIndex = 1 << 8,
        FieldComplete = FieldSound | FieldName | FieldBonus | FieldQuote | FieldBannerGroup |
            FieldBannerImage | FieldContentYOffset | FieldHeightBlocks | FieldModuleIndex
    };

    unsigned int fields_ = 0;
    std::string sound_;
    translation_key name_key_;
    translation_key bonus_key_;
    translation_key quote_key_;
    std::string banner_group_;
    std::string banner_image_;
    int content_y_offset_ = 0;
    int height_blocks_ = 0;
    int module_index_ = -1;
};

class Religion {
public:
    explicit Religion(std::string path);

    const char *path() const;
    void add_god(const ::God *god);
    void set_all_gods();
    int has_all_gods() const;
    const std::vector<const ::God *> &gods() const;
    int has_god(god_type god) const;
    int has_god_runtime_id(int runtime_id) const;
    void set_tier(ReligionTier tier);
    ReligionTier tier() const;
    int is_tier(ReligionTier tier) const;
    void set_capacity(int capacity);
    int capacity() const;
    ReligionPresentation &presentation();
    const ReligionPresentation &presentation() const;

private:
    std::string path_;
    std::vector<const ::God *> gods_;
    int all_gods_ = 0;
    ReligionTier tier_ = ReligionTier::None;
    int capacity_ = 0;
    ReligionPresentation presentation_;
};

} // namespace building_type_registry_impl
