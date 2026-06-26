#include "building/religion.h"

#include "building/god_id_bridge.h"
#include "city/god.h"

#include <utility>

namespace building_type_registry_impl {

void ReligionPresentation::set_sound(std::string value)
{
    sound_ = std::move(value);
    fields_ |= FieldSound;
}

void ReligionPresentation::set_name_key(translation_key value)
{
    name_key_ = std::move(value);
    fields_ |= FieldName;
}

void ReligionPresentation::set_bonus_key(translation_key value)
{
    bonus_key_ = std::move(value);
    fields_ |= FieldBonus;
}

void ReligionPresentation::set_quote_key(translation_key value)
{
    quote_key_ = std::move(value);
    fields_ |= FieldQuote;
}

void ReligionPresentation::set_banner_group(std::string value)
{
    banner_group_ = std::move(value);
    fields_ |= FieldBannerGroup;
}

void ReligionPresentation::set_banner_image(std::string value)
{
    banner_image_ = std::move(value);
    fields_ |= FieldBannerImage;
}

void ReligionPresentation::set_content_y_offset(int value)
{
    content_y_offset_ = value;
    fields_ |= FieldContentYOffset;
}

void ReligionPresentation::set_height_blocks(int value)
{
    height_blocks_ = value;
    fields_ |= FieldHeightBlocks;
}

void ReligionPresentation::set_module_index(int value)
{
    module_index_ = value;
    fields_ |= FieldModuleIndex;
}

const char *ReligionPresentation::sound() const
{
    return sound_.c_str();
}

translation_key ReligionPresentation::name_key() const
{
    return name_key_;
}

translation_key ReligionPresentation::bonus_key() const
{
    return bonus_key_;
}

translation_key ReligionPresentation::quote_key() const
{
    return quote_key_;
}

const char *ReligionPresentation::banner_group() const
{
    return banner_group_.c_str();
}

const char *ReligionPresentation::banner_image() const
{
    return banner_image_.c_str();
}

int ReligionPresentation::content_y_offset() const
{
    return content_y_offset_;
}

int ReligionPresentation::height_blocks() const
{
    return height_blocks_;
}

int ReligionPresentation::module_index() const
{
    return module_index_;
}

int ReligionPresentation::has_any() const
{
    return fields_ != 0;
}

int ReligionPresentation::is_complete() const
{
    return (fields_ & FieldComplete) == FieldComplete &&
        !sound_.empty() && name_key_ && bonus_key_ && quote_key_ &&
        !banner_group_.empty() && !banner_image_.empty() &&
        height_blocks_ > 0 && module_index_ >= 0;
}

Religion::Religion(std::string path)
    : path_(std::move(path))
{
}

const char *Religion::path() const
{
    return path_.c_str();
}

void Religion::add_god(const ::God *god)
{
    if (god) {
        gods_.push_back(god);
    }
}

void Religion::set_all_gods()
{
    all_gods_ = 1;
}

int Religion::has_all_gods() const
{
    return all_gods_;
}

const std::vector<const ::God *> &Religion::gods() const
{
    return gods_;
}

int Religion::has_god(god_type god) const
{
    return has_god_runtime_id(god_id_bridge_runtime_from_legacy(god));
}

int Religion::has_god_runtime_id(int runtime_id) const
{
    if (all_gods_) {
        return 1;
    }
    if (runtime_id < 0) {
        return 0;
    }
    for (const ::God *current : gods_) {
        if (current && current->runtime_id() == runtime_id) {
            return 1;
        }
    }
    return 0;
}

void Religion::set_tier(ReligionTier tier)
{
    tier_ = tier;
}

ReligionTier Religion::tier() const
{
    return tier_;
}

int Religion::is_tier(ReligionTier tier) const
{
    return tier_ == tier;
}

void Religion::set_capacity(int capacity)
{
    capacity_ = capacity;
}

int Religion::capacity() const
{
    return capacity_;
}

ReligionPresentation &Religion::presentation()
{
    return presentation_;
}

const ReligionPresentation &Religion::presentation() const
{
    return presentation_;
}

} // namespace building_type_registry_impl
