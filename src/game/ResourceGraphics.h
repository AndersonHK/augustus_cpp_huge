#pragma once

#include "game/resource.h"
#include "graphics/GraphicsDefinition.h"
#include "graphics/image.h"

class ResourceGraphics : public GraphicsDefinition {
public:
    ResourceGraphics()
        : GraphicsDefinition(GraphicsDefinitionKind::Resource)
    {
    }

    void set_panel_icon(ImageGroupEntryRef image);
    void set_empire_icon(ImageGroupEntryRef image);
    void set_editor_icon(ImageGroupEntryRef image);
    void set_editor_empire_icon(ImageGroupEntryRef image);

    const ImageGroupEntryRef &panel_icon() const;
    const ImageGroupEntryRef &empire_icon() const;
    const ImageGroupEntryRef &editor_icon() const;
    const ImageGroupEntryRef &editor_empire_icon() const;

private:
    ImageGroupEntryRef panel_icon_;
    ImageGroupEntryRef empire_icon_;
    ImageGroupEntryRef editor_icon_;
    ImageGroupEntryRef editor_empire_icon_;
};

const ResourceGraphics &resource_graphics(resource_type resource);
ResourceGraphics &mutable_resource_graphics(resource_type resource);
void resource_graphics_reset();
