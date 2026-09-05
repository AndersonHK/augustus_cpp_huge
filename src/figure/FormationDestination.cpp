#include "figure/FormationDestination.h"

#include "core/log.h"
#include "figure/figure_runtime_native.h"
#include "figure/formation.h"
#include "figure/FigureGraphics.h"
#include "map/grid.h"

#include <algorithm>
#include <exception>

namespace {

FormationDestination *destinations_by_tile[GRID_SIZE * GRID_SIZE] = {};
FormationDestination *first_published_destination = nullptr;

const figure_type_registry_impl::FigureGraphics &destination_graphics()
{
    const figure_type_registry_impl::FigureGraphics *graphics =
        figure_type_registry_impl::FigureGraphics::for_type(FIGURE_FORT_STANDARD);
    if (!graphics || !graphics->standard().enabled) {
        log_error("Formation destination graphics are not defined", "fort_standard", 0);
        std::terminate();
    }
    return *graphics;
}

} // namespace

FigureGraphicDrawRequest FormationDestination::graphic_draw_request() const
{
    const formation &owner = *owner_;
    const figure_type_registry_impl::FigureGraphics &graphics = destination_graphics();
    const int pole_frame = std::clamp(20 - owner.morale / 5, 0, 20);
    const figure_type_registry_impl::FigureGraphicsLayerSet layers = graphics.standard_layers(
        owner.figure_type_id(), owner.is_halted, owner.legion_flag_id, pole_frame, animation_frame_);

    FigureGraphicDrawRequest request;
    request.sprite_offset_x = layers.sprite_offset.x;
    request.sprite_offset_y = layers.sprite_offset.y;
    // The pole defines the composite's coordinate scale. Treating it as an
    // overlay left the request using the unrelated default soldier scale.
    if (layers.count > 0) request.set_base_slice(layers.layers[0].slice);
    request.required_layer_count = 2;
    for (int index = 1; index < layers.count; index++) {
        if (!request.add_layer(layers.layers[index])) break;
    }
    if (!request.has_base_slice() || !request.is_semantically_complete()) {
        const char *definition = owner.formation_type_definition ? owner.formation_type_definition->key() : "missing_definition";
        log_error("Formation destination graphics request is incomplete", definition, static_cast<int>(owner.id));
        std::terminate();
    }
    return request;
}

FormationDestination &FormationDestination::operator=(const FormationDestination &)
{
    clear();
    animation_frame_ = 0;
    return *this;
}

void FormationDestination::place(int tile_x, int tile_y)
{
    if (!map_grid_is_inside(tile_x, tile_y, 1)) {
        const char *definition = owner_ && owner_->formation_type_definition ? owner_->formation_type_definition->key() : "unowned";
        log_error("Formation destination is outside the map", definition, map_grid_offset(tile_x, tile_y));
        std::terminate();
    }
    const int grid_offset = map_grid_offset(tile_x, tile_y);
    if (grid_offset_ == grid_offset) return;
    unpublish();
    publish(grid_offset);
}

void FormationDestination::clear()
{
    unpublish();
    animation_frame_ = 0;
}

void FormationDestination::publish(int grid_offset)
{
    grid_offset_ = grid_offset;
    next_on_tile_ = destinations_by_tile[grid_offset];
    if (next_on_tile_) next_on_tile_->previous_on_tile_ = this;
    destinations_by_tile[grid_offset] = this;

    next_published_ = first_published_destination;
    if (next_published_) next_published_->previous_published_ = this;
    first_published_destination = this;
}

void FormationDestination::unpublish()
{
    if (grid_offset_ < 0) return;
    if (previous_on_tile_) previous_on_tile_->next_on_tile_ = next_on_tile_;
    else destinations_by_tile[grid_offset_] = next_on_tile_;
    if (next_on_tile_) next_on_tile_->previous_on_tile_ = previous_on_tile_;
    if (previous_published_) previous_published_->next_published_ = next_published_;
    else first_published_destination = next_published_;
    if (next_published_) next_published_->previous_published_ = previous_published_;
    previous_on_tile_ = nullptr;
    next_on_tile_ = nullptr;
    previous_published_ = nullptr;
    next_published_ = nullptr;
    grid_offset_ = -1;
}

int FormationDestination::formation_at(int grid_offset)
{
    if (!map_grid_is_valid_offset(grid_offset)) return 0;
    for (FormationDestination *destination = destinations_by_tile[grid_offset]; destination; destination = destination->next_on_tile_) {
        if (destination->owner_ && destination->owner_->in_use && destination->owner_->is_legion &&
            !destination->owner_->in_distant_battle) {
            return static_cast<int>(destination->owner_->id);
        }
    }
    return 0;
}

void FormationDestination::draw_at(int grid_offset, int screen_x, int screen_y, float scale, int highlighted_formation)
{
    if (!map_grid_is_valid_offset(grid_offset)) return;
    for (FormationDestination *destination = destinations_by_tile[grid_offset]; destination; destination = destination->next_on_tile_) {
        if (!destination->owner_ || destination->owner_->in_distant_battle) continue;
        const color_t color_mask = destination->owner_->id == static_cast<unsigned int>(highlighted_formation) ?
            COLOR_MASK_LEGION_HIGHLIGHT : COLOR_MASK_NONE;
        destination->draw(screen_x, screen_y, scale, color_mask);
    }
}

void FormationDestination::draw(int screen_x, int screen_y, float scale, color_t color_mask) const
{
    const FigureGraphicDrawRequest request = graphic_draw_request();
    // The asset's sprite offset identifies the bottom of the pole. It is the
    // standard's baseline and belongs exactly at the center of the anchor tile.
    request.draw_with_baseline_at_tile_center(screen_x, screen_y, color_mask, scale);
}

void FormationDestination::advance_graphics()
{
    for (FormationDestination *destination = first_published_destination; destination; destination = destination->next_published_) {
        destination->advance_animation();
    }
}

void FormationDestination::advance_animation()
{
    const figure_type_registry_impl::FigureStandardGraphics &graphics = destination_graphics().standard();
    const int cycle = graphics.moving_frame_divisor * graphics.moving_frame_count;
    animation_frame_ = cycle > 0 ? (animation_frame_ + 1) % cycle : 0;
}
