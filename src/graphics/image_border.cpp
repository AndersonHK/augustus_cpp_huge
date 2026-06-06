#include "graphics/image_border.h"

#include <utility>

namespace {

ImageGroupEntryRef ui_entry(const char *group, const char *entry)
{
    return ImageGroupEntryRef::from_group(group, entry);
}

ImageBorder ui_border(
    const char *group,
    const char *top,
    const char *left,
    const char *bottom,
    const char *right)
{
    return ImageBorder(
        ui_entry(group, top),
        ui_entry(group, left),
        ui_entry(group, bottom),
        ui_entry(group, right));
}

} // namespace

ImageBorder::ImageBorder(
    ImageGroupEntryRef top,
    ImageGroupEntryRef left,
    ImageGroupEntryRef bottom,
    ImageGroupEntryRef right)
    : top_(std::move(top))
    , left_(std::move(left))
    , bottom_(std::move(bottom))
    , right_(std::move(right))
{
}

ImageBorder ImageBorder::image_small()
{
    return ui_border(
        "UI\\Image_Border_Small",
        "Image Border Small",
        "Image_0000",
        "Image_0001",
        "Image_0002");
}

ImageBorder ImageBorder::image_medium()
{
    return ui_border(
        "UI\\Image_Border_Medium",
        "Image Border Medium",
        "Image_0003",
        "Image_0004",
        "Image_0005");
}

ImageBorder ImageBorder::image_large()
{
    return ui_border(
        "UI\\Image_Border_Large",
        "Image Border Large",
        "Image_0006",
        "Image_0007",
        "Image_0008");
}

ImageBorder ImageBorder::large_banner()
{
    return ui_border(
        "UI\\Large_Banner_Border",
        "Large_Banner_Border",
        "Image_0027",
        "Image_0028",
        "Image_0029");
}

ImageBorder ImageBorder::mission_selection()
{
    return ui_border(
        "UI\\Mission_Selection_Border",
        "Mission Selection Border",
        "Image_0012",
        "Image_0013",
        "Image_0014");
}

void ImageBorder::draw(int x, int y, color_t color) const
{
    const Image &top = top_.image();
    const Image &left = left_.image();
    const Image &right = right_.image();

    int content_y = y + top.height() + top.y_offset();
    top_.draw(x, y, color);
    left_.draw(x, content_y, color);
    bottom_.draw(x, content_y + left.height() + left.y_offset(), color);
    right_.draw(x + top.width() + top.x_offset() - right.width() - right.x_offset(), content_y, color);
}
