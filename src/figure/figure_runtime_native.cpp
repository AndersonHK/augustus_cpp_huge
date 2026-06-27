#include "figure/figure_runtime_native.h"

#include "building/distribution.h"
#include "building/list.h"
#include "building/local_workforce.h"
#include "building/maintenance.h"
#include "building/market.h"
#include "building/storage.h"
#include "city/festival.h"
#include "city/health.h"
#include "city/view.h"
#include "figure/action.h"
#include "figure/combat.h"
#include "figure/figure_runtime_api.h"
#include "figure/formation.h"
#include "figure/image.h"
#include "figure/movement.h"
#include "figure/PathingMode.h"
#include "figure/route.h"
#include "figuretype/fishing_boat.h"
#include "figuretype/supplier.h"
#include "graphics/image.h"
#include "map/building.h"
#include "map/road_access.h"

#include "building/building.h"
#include "building/building_type_registry_internal.h"
#include "core/crash_context.h"

#include "assets/assets.h"
#include "assets/image_group_payload.h"
#include "building/building_record.h"
#include "building/granary.h"
#include "building/monument.h"
#include "building/warehouse.h"
#include "city/figures.h"
#include "core/calc.h"
#include "core/config.h"
#include "core/image.h"
#include "figure/enemy_army.h"
#include "game/time.h"
#include "game/resource.h"
#include "map/data.h"
#include "map/grid.h"
#include "map/routing.h"
#include "map/terrain.h"
#include "scenario/gladiator_revolt.h"
#include "sound/effect.h"


#include <cstdio>
#include <memory>
#include <string>
#include <unordered_set>

namespace figure_runtime_native_impl {

constexpr int kInfiniteDistance = 10000;
constexpr int kRecalculateEnemyLocationTicks = 30;
constexpr int kMaxFoodStockedMarket = 800;

void reset_draw_request(FigureGraphicDrawRequest *request)
{
    if (request) {
        *request = {};
    }
}

int clamp_frame(int value, int min, int max)
{
    if (value < min) {
        return min;
    }
    if (value > max) {
        return max;
    }
    return value;
}

const char *warrior_direction_suffix(int direction)
{
    static constexpr const char *suffixes[] = { "ne", "e", "se", "s", "sw", "w", "nw", "n" };
    direction = clamp_frame(direction, 0, 7);
    return suffixes[direction];
}

int soldier_native_direction(const Figure *f)
{
    const formation *m = formation_get(f->formation_id);
    int direction = 0;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        direction = f->attack_direction;
    } else if (m && m->missile_fired) {
        direction = f->direction;
    } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD && m) {
        direction = m->direction;
    } else if (f->direction < 8) {
        direction = f->direction;
    } else {
        direction = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(direction);
}

int missile_native_direction(const Figure *f)
{
    int direction = f->direction;
    if (direction >= 8) {
        direction /= 2;
    }
    return figure_image_normalize_direction(direction);
}

int enemy_missile_native_direction(const Figure *f)
{
    const formation *m = formation_get(f->formation_id);
    int direction = 0;
    if (f->action_state == FIGURE_ACTION_150_ATTACK) {
        direction = f->attack_direction;
    } else if ((m && m->missile_fired) || f->direction < 8) {
        direction = f->direction;
    } else {
        direction = f->previous_tile_direction;
    }
    return figure_image_normalize_direction(direction);
}

const ImageGroupPayload *loaded_payload_for_path(const char *path)
{
    if (!path || !*path) {
        return nullptr;
    }
    const ImageGroupPayload *payload = image_group_payload_get(path);
    if (!payload) {
        if (!image_group_payload_load(path)) {
            return nullptr;
        }
        payload = image_group_payload_get(path);
    }
    return payload;
}

const ImageGroupEntry *native_entry(const char *group, const char *image)
{
    if (!image || !*image) {
        return nullptr;
    }
    const ImageGroupPayload *payload = loaded_payload_for_path(group);
    return payload ? payload->entry_for(image) : nullptr;
}

const ImageGroupEntry *native_warrior_entry(const Figure *f)
{
    if (!f) {
        return nullptr;
    }

    char name[64];
    switch (f->type) {
        case FIGURE_FORT_INFANTRY: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "auxinf_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(soldier_native_direction(f));
                if (f->action_state == FIGURE_ACTION_150_ATTACK) {
                    const int frame = f->attack_image_offset < 14 ?
                        1 :
                        clamp_frame(((f->attack_image_offset - 14) / 2) + 1, 1, 5);
                    std::snprintf(name, sizeof(name), "auxinf_f_%s_%02d", direction, frame);
                } else {
                    std::snprintf(name, sizeof(name), "auxinf_%s_%02d", direction,
                        clamp_frame(f->image_offset + 1, 1, 12));
                }
            }
            break;
        }
        case FIGURE_FORT_ARCHER: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "auxarch_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(soldier_native_direction(f));
                if (f->action_state == FIGURE_ACTION_150_ATTACK) {
                    const int frame = f->attack_image_offset < 14 ?
                        1 :
                        clamp_frame(((f->attack_image_offset - 14) / 2) + 1, 1, 5);
                    std::snprintf(name, sizeof(name), "auxarch_fm_%s_%02d", direction, frame);
                } else if (f->action_state == FIGURE_ACTION_84_SOLDIER_AT_STANDARD) {
                    const int frame = clamp_frame(figure_image_missile_launcher_offset(const_cast<Figure *>(f)), 0, 4) + 1;
                    std::snprintf(name, sizeof(name), "auxarch_fr_%s_%02d", direction, frame);
                } else {
                    std::snprintf(name, sizeof(name), "auxarch_%s_%02d", direction,
                        clamp_frame(f->image_offset + 1, 1, 12));
                }
            }
            break;
        }
        case FIGURE_ENEMY_CATAPULT: {
            if (f->action_state == FIGURE_ACTION_149_CORPSE) {
                std::snprintf(name, sizeof(name), "catapult_death_%02d",
                    clamp_frame(figure_image_corpse_offset(const_cast<Figure *>(f)) + 1, 1, 8));
            } else {
                const char *direction = warrior_direction_suffix(enemy_missile_native_direction(f));
                if (f->action_state == FIGURE_ACTION_151_ENEMY_INITIAL) {
                    const int frame = clamp_frame(figure_image_missile_launcher_offset(const_cast<Figure *>(f)) + 1, 1, 8);
                    if (frame == 1) {
                        std::snprintf(name, sizeof(name), "catapult_fe_%s_01", direction);
                    } else {
                        std::snprintf(name, sizeof(name), "catapult_fr_%s_%02d", direction, frame);
                    }
                } else {
                    std::snprintf(name, sizeof(name), "catapult_%s_01", direction);
                }
            }
            break;
        }
        case FIGURE_CATAPULT_MISSILE:
            std::snprintf(name, sizeof(name), "catapult_rock_%s_01",
                warrior_direction_suffix(missile_native_direction(f)));
            break;
        default:
            return nullptr;
    }

    char group[96];
    std::snprintf(group, sizeof(group), "Warriors\\%s", name);
    return native_entry(group, name);
}

const ImageGroupEntry *native_auxiliary_standard_flag(const Figure *f, const formation *m)
{
    if (!f || f->type != FIGURE_FORT_STANDARD || !m) {
        return nullptr;
    }
    const char *prefix = nullptr;
    if (m->figure_type == FIGURE_FORT_INFANTRY) {
        prefix = "auxinf";
    } else if (m->figure_type == FIGURE_FORT_ARCHER) {
        prefix = "auxarch";
    } else {
        return nullptr;
    }

    char name[32];
    if (m->is_halted) {
        std::snprintf(name, sizeof(name), "%s_banner_0", prefix);
    } else {
        std::snprintf(name, sizeof(name), "%s_banner_%02d", prefix, clamp_frame(f->image_offset / 2 + 1, 1, 8));
    }
    char group[64];
    std::snprintf(group, sizeof(group), "UI\\%s", name);
    return native_entry(group, name);
}

static bool add_draw_layer(FigureGraphicDrawRequest &request, const FigureGraphicDrawLayer &layer)
{
    if (!layer.slice.is_valid() || request.layer_count >= FigureGraphicDrawRequest::MAX_LAYERS) {
        return false;
    }
    request.layers[request.layer_count++] = layer;
    return true;
}

static bool add_draw_layer(
    FigureGraphicDrawRequest &request,
    const figure_type_registry_impl::FigureGraphicsLayer &layer)
{
    if (!layer.is_valid()) {
        return false;
    }
    FigureGraphicDrawLayer draw_layer;
    draw_layer.slice = layer.slice;
    draw_layer.x_offset = layer.offset.x;
    draw_layer.y_offset = layer.offset.y;
    draw_layer.draw_before_base = layer.draw_before_base;
    draw_layer.use_figure_color_mask = layer.use_figure_color_mask;
    return add_draw_layer(request, draw_layer);
}

void set_sprite_offset_from_image(FigureGraphicDrawRequest &request, const Image &image)
{
    if (const image_animation *animation = image.animation()) {
        request.sprite_offset_x = animation->sprite_offset_x;
        request.sprite_offset_y = animation->sprite_offset_y;
    }
}

const Image &prepare_legacy_draw_image(FigureGraphicDrawRequest &request, int image_id)
{
    const Image &image = figure_type_registry_impl::FigureGraphics::legacy_image(image_id);
    set_sprite_offset_from_image(request, image);
    return image;
}

int set_legacy_base_draw_request_image(FigureGraphicDrawRequest &request, int image_id)
{
    const Image &base_image = prepare_legacy_draw_image(request, image_id);
    request.base_slice = base_image.runtime_slice();
    return request.base_slice.is_valid();
}

static int set_base_draw_request_from_entry(FigureGraphicDrawRequest &request, const ImageGroupEntry &entry)
{
    const RuntimeDrawSlice *slice = entry.footprint();
    if (!slice || !slice->is_valid()) {
        return 0;
    }
    request.base_slice = *slice;
    if (entry.has_sprite_offset()) {
        request.sprite_offset_x = entry.sprite_offset_x();
        request.sprite_offset_y = entry.sprite_offset_y();
    }
    return 1;
}

static int set_base_draw_request_from_binding(
    FigureGraphicDrawRequest &request,
    const figure_type_registry_impl::GraphicsTargetBinding &binding)
{
    RuntimeDrawSlice slice = binding.resolved_slice();
    if (!slice.is_valid()) {
        return 0;
    }
    request.base_slice = slice;
    if (binding.entry && binding.entry->has_sprite_offset()) {
        request.sprite_offset_x = binding.entry->sprite_offset_x();
        request.sprite_offset_y = binding.entry->sprite_offset_y();
    }
    return 1;
}

static void apply_policy_draw_size(
    const figure_type_registry_impl::FigureGraphics &policy,
    FigureGraphicDrawRequest &request)
{
    if (!policy.has_fixed_logical_size()) {
        return;
    }
    request.fixed_logical_size = policy.fixed_logical_size;
    request.scaling_policy = policy.scaling_policy;
}

struct HippodromeHorseDrawOffset {
    int max_wait_ticks;
    int x;
    int y;
};

void hippodrome_horse_draw_offset(const Figure &f, int &x, int &y)
{
    static constexpr int MAX_WAIT_TICKS = 2147483647;
    static constexpr HippodromeHorseDrawOffset TOP[] = {
        {10, 10, -2}, {11, 10, -10}, {12, 10, -18}, {13, 10, -16},
        {20, 10, -14}, {21, 10, -10}, {MAX_WAIT_TICKS, 10, -2},
    };
    static constexpr HippodromeHorseDrawOffset RIGHT[] = {
        {9, -10, -12}, {10, -10, 4}, {11, -15, 2}, {13, -15, 0},
        {20, -10, -2}, {21, -10, -6}, {MAX_WAIT_TICKS, -10, -12},
    };
    static constexpr HippodromeHorseDrawOffset BOTTOM[] = {
        {9, 20, 4}, {10, 30, 4}, {11, 30, -4}, {13, 20, -6},
        {20, 20, -12}, {21, 20, -10}, {MAX_WAIT_TICKS, 20, -2},
    };
    static constexpr HippodromeHorseDrawOffset LEFT[] = {
        {9, -10, -12}, {10, -10, 4}, {11, -10, 2}, {13, -10, 0},
        {20, -10, -2}, {21, -10, -6}, {MAX_WAIT_TICKS, -10, -12},
    };

    const HippodromeHorseDrawOffset *offsets = TOP;
    switch (city_view_orientation()) {
        case DIR_2_RIGHT:
            offsets = RIGHT;
            break;
        case DIR_4_BOTTOM:
            offsets = BOTTOM;
            break;
        case DIR_6_LEFT:
            offsets = LEFT;
            break;
    }
    for (int i = 0; i < 7; i++) {
        if (f.wait_ticks_missile <= offsets[i].max_wait_ticks) {
            x += offsets[i].x;
            y += offsets[i].y;
            return;
        }
    }
}

building_type burning_ruin_type()
{
    static building_type type = BUILDING_NONE;
    if (type == BUILDING_NONE) {
        type = building_type_registry_impl::type_from_attr("burning_ruin");
    }
    return type;
}

bool is_burning_ruin(const building &b)
{
    return b.type == burning_ruin_type();
}

bool owner_state_matches(const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (!owner) {
        return false;
    }

    switch (owner_binding.required_owner_state) {
        case figure_type_registry_impl::OwnerStateRequirement::Any:
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUse:
            if (owner->state != BUILDING_STATE_IN_USE) {
                return false;
            }
            break;
        case figure_type_registry_impl::OwnerStateRequirement::InUseOrMothballed:
            if (owner->state != BUILDING_STATE_IN_USE && owner->state != BUILDING_STATE_MOTHBALLED) {
                return false;
            }
            break;
    }

    const building_type required_building_type = owner_binding.resolved_required_building_type();
    if (required_building_type != BUILDING_NONE && required_building_type != owner->type) {
        return false;
    }
    return true;
}

Figure *slot_figure(const building *owner, figure_type_registry_impl::FigureSlot slot)
{
    if (!owner) {
        return nullptr;
    }

    unsigned int figure_id = 0;
    switch (slot) {
        case figure_type_registry_impl::FigureSlot::Primary:
            figure_id = owner->figure_id;
            break;
        case figure_type_registry_impl::FigureSlot::Secondary:
            figure_id = owner->figure_id2;
            break;
        case figure_type_registry_impl::FigureSlot::Quaternary:
            figure_id = owner->figure_id4;
            break;
        case figure_type_registry_impl::FigureSlot::None:
        default:
            return nullptr;
    }
    return figure_id ? Figure::get(figure_id) : nullptr;
}

bool slot_matches(const Figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    if (owner_binding.slot == figure_type_registry_impl::FigureSlot::None) {
        return true;
    }

    Figure *tracked = slot_figure(owner, owner_binding.slot);
    return tracked && f && tracked->id() == f->id() && tracked->created_sequence == f->created_sequence;
}

bool owner_binding_matches(const Figure *f, const building *owner, const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    return owner_state_matches(owner, owner_binding) && slot_matches(f, owner, owner_binding);
}

bool owner_binding_requires_owner(const figure_type_registry_impl::OwnerBinding &owner_binding)
{
    // An "any/none/any" owner node means the profile only wants a source id
    // for save/debug context. Transient walkers can outlive that building.
    return owner_binding.slot != figure_type_registry_impl::FigureSlot::None ||
        owner_binding.resolved_required_building_type() != BUILDING_NONE ||
        !owner_binding.required_building_reference.empty() ||
        owner_binding.required_owner_state != figure_type_registry_impl::OwnerStateRequirement::Any;
}

bool owner_road_access(const building &owner, map_point &road)
{
    return map_closest_road_within_radius(owner.x, owner.y, owner.size, 2, &road.x, &road.y) != 0;
}

bool send_to_owner_road(Figure *f, const building &owner, int action_state)
{
    if (!f) {
        return false;
    }

    map_point road = { 0, 0 };
    if (!owner_road_access(owner, road)) {
        f->state = FIGURE_STATE_DEAD;
        return false;
    }

    f->action_state = action_state;
    f->destination_x = road.x;
    f->destination_y = road.y;
    Route::remove(f);
    return true;
}

bool exit_owner_cross_country(Figure *f, const building &owner, int action_state)
{
    if (!f) {
        return false;
    }

    map_point road = { 0, 0 };
    if (!owner_road_access(owner, road)) {
        f->state = FIGURE_STATE_DEAD;
        return false;
    }

    f->action_state = action_state;
    figure_movement_set_cross_country_destination(f, road.x, road.y);
    f->roam_length = 0;
    return true;
}

void retire_unsupported_native_state(Figure *f, const char *native_class)
{
    const ErrorContextScope scope("Native FigureType action", native_class);
    error_context_report_warning(
        "Native FigureType walker reached an unsupported action state.",
        "The legacy fallback for this walker has been retired; the invalid Figure will be removed.");
    if (f) {
        f->state = FIGURE_STATE_DEAD;
    }
}

road_service_effect primary_service_effect_for_profile(
    const figure_type_registry_impl::FigureTypeProfile *profile)
{
    if (!profile) {
        return ROAD_SERVICE_EFFECT_NONE;
    }
    // Service identity is profile-owned. Buildings choose a profile at spawn time,
    // and smart service pathing/road history only read this explicit effect.
    return profile->pathing_policy().effect;
}

bool is_road_history_tile(int grid_offset)
{
    return figure_type_registry_impl::PathingMode::citizenIsRoadLike(grid_offset);
}

class GenericFigureGraphics {
public:
    explicit GenericFigureGraphics(const figure_type_registry_impl::FigureTypeDefinition &definition)
        : policy_(definition.graphics())
    {
    }

    int draw_request_for(const Figure *f, FigureGraphicDrawRequest *request) const
    {
        reset_draw_request(request);
        if (!f || !request || !policy_.has_native_payload()) {
            return 0;
        }

        const figure_type_registry_impl::GraphicsTargetBinding *binding = target_for(f);
        if (!binding) {
            return 1;
        }
        if (!binding->is_complete()) {
            log_graphics_issue_once("Figure graphics target is incomplete.", f, binding->path, binding->image);
        }
        if (!binding->is_resolved() || !set_base_draw_request_from_binding(*request, *binding)) {
            log_graphics_issue_once(
                "Figure graphics image could not be resolved.",
                f,
                binding->path,
                binding->path + " image=" + binding->image);
            return 1;
        }
        apply_policy_draw_size(policy_, *request);

        if (!binding->uses_animation() && !binding->entry->has_sprite_offset()) {
            if (policy_.has_sprite_offset) {
                request->sprite_offset_x = policy_.sprite_offset_x;
                request->sprite_offset_y = policy_.sprite_offset_y;
            } else {
                log_graphics_issue_once(
                    "Figure graphics sprite offset is missing.",
                    f,
                    binding->path,
                    binding->path + " image=" + binding->image);
            }
            return 1;
        }

        return 1;
    }

private:
    const figure_type_registry_impl::GraphicsTargetBinding *target_for(const Figure *f) const
    {
        if (f->action_state == FIGURE_ACTION_149_CORPSE && !policy_.has_corpse_native_payload()) {
            log_graphics_issue_once("Figure graphics corpse target is missing.", f, "", "corpse_path_pattern");
            return nullptr;
        }
        const figure_type_registry_impl::GraphicsTargetBinding *binding =
            policy_.cached_target_binding_for_figure(*f);
        if (!binding) {
            log_graphics_issue_once("Figure graphics cached binding is missing.", f, "", "native_binding_cache");
        }
        return binding;
    }

    static void log_graphics_issue_once(
        const char *message,
        const Figure *f,
        const std::string &path,
        const std::string &detail)
    {
        static std::unordered_set<std::string> logged;
        char key[512];
        snprintf(
            key,
            sizeof(key),
            "%s|figure=%d|path=%s|detail=%s",
            message ? message : "",
            f ? f->id() : 0,
            path.c_str(),
            detail.c_str());
        if (!logged.insert(key).second) {
            return;
        }
        error_context_report_error(message, detail.c_str());
    }

    const figure_type_registry_impl::FigureGraphics &policy_;
};

static int warrior_graphic_draw_request_for_figure(
    const Figure *f,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !request) {
        return 0;
    }

    const ImageGroupEntry *entry = native_warrior_entry(f);
    if (!entry) {
        return 0;
    }

    set_base_draw_request_from_entry(*request, *entry);
    return 1;
}

static int fort_standard_graphic_draw_request_for_figure(
    const Figure *f,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !request || f->type != FIGURE_FORT_STANDARD || !f->cart_image_id) {
        return 0;
    }

    const Image &base_image = prepare_legacy_draw_image(*request, f->image_id);

    const formation *m = formation_get(f->formation_id);
    if (!m || m->in_distant_battle) {
        return 1;
    }

    add_draw_layer(*request, figure_type_registry_impl::FigureGraphics::image_layer(base_image, {}, 0));
    if (const ImageGroupEntry *native_flag = native_auxiliary_standard_flag(f, m)) {
        const figure_type_registry_impl::FigureGraphicsLayer flag_layer =
            figure_type_registry_impl::FigureGraphics::native_entry_layer_above(*native_flag, 0);
        if (add_draw_layer(*request, flag_layer)) {
            add_draw_layer(
                *request,
                figure_type_registry_impl::FigureGraphics::legacy_image_layer_above(
                    m->legion_flag_id,
                    flag_layer.height));
        }
        return 1;
    }

    const figure_type_registry_impl::FigureGraphicsLayer flag_layer =
        figure_type_registry_impl::FigureGraphics::legacy_image_layer_above(f->cart_image_id, 0);
    add_draw_layer(*request, flag_layer);
    add_draw_layer(
        *request,
        figure_type_registry_impl::FigureGraphics::legacy_image_layer_above(
            m->legion_flag_id,
            flag_layer.height));
    return 1;
}

static int map_flag_graphic_draw_request_for_figure(
    const Figure *f,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !request || f->type != FIGURE_MAP_FLAG || !f->cart_image_id) {
        return 0;
    }

    const Image &base_image = prepare_legacy_draw_image(*request, f->image_id);
    add_draw_layer(*request, figure_type_registry_impl::FigureGraphics::image_layer(base_image, {}, 0));

    add_draw_layer(*request, figure_type_registry_impl::FigureGraphics::legacy_image_layer_above(f->cart_image_id, 0));
    return 1;
}

static int enemy_graphic_draw_request_for_figure(
    const Figure *f,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !request || !f->is_enemy_image || f->cart_image_id) {
        return 0;
    }

    const Image &enemy_image = Image::enemy(f->image_id);
    set_sprite_offset_from_image(*request, enemy_image);
    add_draw_layer(*request, figure_type_registry_impl::FigureGraphics::image_layer(enemy_image, {}, 0));
    return request->layer_count > 0;
}

static int figure_graphics_draw_request_for_figure(
    const Figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!definition || !request) {
        return 0;
    }
    return GenericFigureGraphics(*definition).draw_request_for(f, request);
}

static bool update_legacy_figure_graphics_image_state_for_direction(
    Figure &figure,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    int direction)
{
    if (!definition) {
        return false;
    }

    Figure *f = &figure;
    const figure_type_registry_impl::FigureGraphics &graphics = definition->graphics();
    f->is_enemy_image = 0;
    if (graphics.has_native_payload()) {
        const figure_type_registry_impl::GraphicsTargetBinding *binding =
            graphics.cached_target_binding_for_figure_direction(*f, direction);
        return binding &&
            binding->is_resolved() &&
            binding->entry->footprint() &&
            binding->entry->footprint()->is_valid();
    }

    if (!graphics.has_legacy_default_source()) {
        return 0;
    }

    f->image_id = graphics.legacy_image_id_for_figure_direction(*f, direction);
    return true;
}

bool update_legacy_figure_graphics_image_state(
    Figure &figure,
    const figure_type_registry_impl::FigureTypeDefinition *definition)
{
    return update_legacy_figure_graphics_image_state_for_direction(
        figure,
        definition,
        figure_image_direction(&figure));
}

class RoamingServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner || !owner->id) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        if (f->type == FIGURE_PRIEST && f->destination_building.id()) {
            return 0;
        }

        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        const figure_type_registry_impl::FigureGraphics &graphics = definition()->graphics();

        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, graphics.max_image_offset);

        if (profile()->pathing_policy().mode == &figure_type_registry_impl::NearestUnemployed &&
            building_local_workforce::labor_seeker_is_workforce(f)) {
            f->is_ghost = 0;
            f->roam_length++;
            if (f->roam_length >= movement.max_roam_length) {
                building_local_workforce::labor_seeker_failed(f);
            } else {
                if (!building_local_workforce::prepare_labor_seeker_target(f)) {
                    building_local_workforce::labor_seeker_failed(f);
                } else {
                    figure_movement_move_ticks(f, movement.roam_ticks);
                    if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                        building_local_workforce::labor_seeker_arrived(f);
                    } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                        building_local_workforce::labor_seeker_failed(f);
                    }
                }
            }
            update_legacy_figure_graphics_image_state(*f, definition());
            return 1;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_125_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    if (movement.return_mode != figure_type_registry_impl::ReturnMode::ReturnToOwnerRoad) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        if (send_to_owner_road(f, *owner, FIGURE_ACTION_126_ROAMER_RETURNING)) {
                            f->roam_length = 0;
                        }
                    }
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_126_ROAMER_RETURNING:
                if (movement.return_mode != figure_type_registry_impl::ReturnMode::ReturnToOwnerRoad) {
                    return 0;
                }
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE ||
                    f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        update_legacy_figure_graphics_image_state(*f, definition());
        return 1;
    }
};

// Ownerless ambient walkers have two generic lifetime policies: stand still on
// their spawn tile, or roam briefly and expire instead of returning home.
class TransientWandererFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition() || !profile()) {
            return 0;
        }

        const figure_type_registry_impl::OwnerBinding &owner_binding = profile()->owner_binding();
        if (owner_binding_requires_owner(owner_binding)) {
            building *owner = building_get(f->building.id());
            if (!owner || !owner->id || !owner_binding_matches(f, owner, owner_binding)) {
                f->state = FIGURE_STATE_DEAD;
                update_image(f);
                return 1;
            }
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        const figure_type_registry_impl::FigureGraphics &graphics = definition()->graphics();

        f->clear_legacy_cart_overlay_image();
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, graphics.max_image_offset);

        if (profile()->pathing_policy().mode == &figure_type_registry_impl::StandStill) {
            execute_stand_still(f, movement);
            update_image(f);
            return 1;
        }
        if (profile()->pathing_policy().mode != &figure_type_registry_impl::TransientWander) {
            retire_unsupported_native_state(f, "transient_wanderer");
            update_image(f);
            return 1;
        }

        if (f->action_state == 0) {
            f->action_state = FIGURE_ACTION_125_ROAMING;
            figure_movement_init_roaming(f);
            f->roam_length = 0;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_125_ROAMING:
                f->is_ghost = 0;
                f->wait_ticks++;
                f->roam_length++;
                if (f->wait_ticks > movement.max_roam_length) {
                    f->state = FIGURE_STATE_DEAD;
                    f->image_offset = 0;
                } else {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            default:
                retire_unsupported_native_state(f, "transient_wanderer");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static void clear_stand_still_movement(Figure *f)
    {
        // Buggy transient saves may already contain a roaming route. Preserve
        // the current tile, but discard destination state so the policy is idle.
        Route::remove(f);
        f->routing_path_current_tile = 0;
        f->routing_path_length = 0;
        f->destination_x = f->x;
        f->destination_y = f->y;
        f->destination_grid_offset = f->grid_offset;
        f->previous_tile_x = f->x;
        f->previous_tile_y = f->y;
        f->previous_tile_direction = f->direction;
        f->progress_on_tile = FIGURE_TILE_PROGRESS_MAX;
        f->roam_length = 0;
        f->roam_choose_destination = 0;
        f->roam_random_counter = 0;
        f->roam_turn_direction = 0;
        f->roam_ticks_until_next_turn = 0;
    }

    static void tick_stand_still_lifetime(
        Figure *f,
        const figure_type_registry_impl::MovementProfile &movement)
    {
        f->is_ghost = 0;
        f->wait_ticks++;
        if (f->wait_ticks > movement.max_roam_length) {
            f->state = FIGURE_STATE_DEAD;
            f->image_offset = 0;
        }
    }

    void execute_stand_still(
        Figure *f,
        const figure_type_registry_impl::MovementProfile &movement) const
    {
        switch (f->action_state) {
            case 0:
                tick_stand_still_lifetime(f, movement);
                break;
            case FIGURE_ACTION_125_ROAMING:
                clear_stand_still_movement(f);
                f->action_state = 0;
                tick_stand_still_lifetime(f, movement);
                break;
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            default:
                retire_unsupported_native_state(f, "transient_wanderer");
                break;
        }
    }

    void update_image(Figure *f) const
    {
        update_legacy_figure_graphics_image_state(*f, definition());
    }
};

class DepotCartGraphics {
public:
    explicit DepotCartGraphics(const figure_type_registry_impl::FigureTypeDefinition &definition)
        : definition_(definition)
    {
    }

    int draw_request_for(const Figure &f, FigureGraphicDrawRequest &request) const
    {
        int dir = figure_image_normalize_direction(
            f.direction < 8 ? f.direction : f.previous_tile_direction);
        const figure_type_registry_impl::FigureGraphics &graphics = definition_.graphics();

        if (f.action_state == FIGURE_ACTION_149_CORPSE) {
            return set_legacy_base_draw_request_image(
                request,
                graphics.legacy_corpse_image_id(figure_image_corpse_offset(const_cast<Figure *>(&f))));
        }

        const int has_base = set_legacy_base_draw_request_image(
            request,
            graphics.legacy_direction_major_frame_image_id(
                figure_image_normalize_direction(dir),
                f.image_offset));

        if (!has_cart_layer(f)) {
            return has_base;
        }

        add_draw_layer(
            request,
            graphics.legacy_resource_cart_layer(
                static_cast<resource_type>(f.resource_id),
                f.loads_sold_or_carrying,
                (dir + 4) % 8));
        return has_base;
    }

private:
    static int has_cart_layer(const Figure &f)
    {
        switch (f.action_state) {
            case FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL:
            case FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE:
            case FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE:
            case FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION:
            case FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION:
            case FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING:
            case FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER:
            case FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE:
                return 1;
            default:
                return 0;
        }
    }

    const figure_type_registry_impl::FigureTypeDefinition &definition_;
};

static int depot_cart_graphic_draw_request_for_figure(
    const Figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !definition || !request || f->type != FIGURE_DEPOT_CART_PUSHER) {
        return 0;
    }
    const figure_type_registry_impl::FigureGraphics &graphics = definition->graphics();
    if (!graphics.has_legacy_resource_cart_graphics()) {
        return 0;
    }
    return DepotCartGraphics(*definition).draw_request_for(*f, *request);
}

static int hippodrome_horse_graphic_draw_request_for_figure(
    const Figure *f,
    FigureGraphicDrawRequest *request)
{
    reset_draw_request(request);
    if (!f || !request || f->type != FIGURE_HIPPODROME_HORSES || !f->cart_image_id) {
        return 0;
    }

    const Image &base_image = figure_type_registry_impl::FigureGraphics::legacy_image(f->image_id);
    set_sprite_offset_from_image(*request, base_image);

    int x_offset = 0;
    int y_offset = 0;
    hippodrome_horse_draw_offset(*f, x_offset, y_offset);

    add_draw_layer(
        *request,
        figure_type_registry_impl::FigureGraphics::image_layer(base_image, { x_offset, y_offset }));
    add_draw_layer(
        *request,
        figure_type_registry_impl::FigureGraphics::legacy_cart_overlay_layer(
            *f,
            { x_offset + f->x_offset_cart, y_offset + f->y_offset_cart },
            f->y_offset_cart < 0));
    return request->layer_count > 0;
}

class DepotStorageEndpoint {
public:
    explicit DepotStorageEndpoint(building *storage)
        : building_(storage)
    {
    }

    building *raw() const
    {
        return building_;
    }

    bool active() const
    {
        return building_is_active(building_);
    }

    bool accepts(resource_type resource) const
    {
        Building storage(building_);
        return resource != RESOURCE_NONE &&
            building_ &&
            building_->state == BUILDING_STATE_IN_USE &&
            building_storage_get_state(storage, resource, 0) != BUILDING_STORAGE_STATE_NOT_ACCEPTING;
    }

    int amount(resource_type resource) const
    {
        if (!building_ || resource == RESOURCE_NONE) {
            return 0;
        }
        Building storage(building_);
        if (storage.type && storage.type->is_granary()) {
            return building_granary_get_amount(storage, resource);
        }
        if (storage.type && storage.type->is_warehouse()) {
            return building_warehouse_get_amount(storage, resource);
        }
        return 0;
    }

    int remove(resource_type resource, int amount) const
    {
        if (!building_ || resource == RESOURCE_NONE) {
            return 0;
        }
        Building storage(building_);
        if (storage.type && storage.type->is_granary()) {
            return building_granary_try_remove_resource(storage, resource, amount);
        }
        if (storage.type && storage.type->is_warehouse()) {
            return building_warehouse_try_remove_resource(storage, resource, amount);
        }
        return 0;
    }

    int add(resource_type resource, int amount) const
    {
        if (!building_ || resource == RESOURCE_NONE) {
            return amount;
        }

        while (amount > 0) {
            Building storage(building_);
            const int unload_amount = amount > 2 ? 2 : 1;
            const unsigned char added_amount = building_storage_try_add_resource(storage, resource, unload_amount, 0);
            if (added_amount <= 0) {
                return amount;
            }
            amount -= added_amount;
        }
        return amount;
    }

    bool road_access(map_point &point) const
    {
        if (!building_) {
            return false;
        }
        Building storage(building_);
        if (storage.type && storage.type->is_granary()) {
            map_point_store_result(building_->x + 1, building_->y + 1, &point);
            return true;
        }
        if (storage.type && storage.type->is_warehouse()) {
            if (building_->has_road_access == 1) {
                map_point_store_result(building_->x, building_->y, &point);
                return true;
            }
            return map_has_road_access_warehouse(building_->x, building_->y, &point) != 0;
        }
        point.x = building_->road_access_x;
        point.y = building_->road_access_y;
        return building_->has_road_access != 0;
    }

private:
    building *building_ = nullptr;
};

class DepotOrderView {
public:
    explicit DepotOrderView(const building &depot)
        : order_(depot.data.depot.current_order)
    {
    }

    resource_type resource() const
    {
        return static_cast<resource_type>(order_.resource_type);
    }

    DepotStorageEndpoint source() const
    {
        return DepotStorageEndpoint(order_.src_storage_id ? building_get(order_.src_storage_id) : nullptr);
    }

    DepotStorageEndpoint destination() const
    {
        return DepotStorageEndpoint(order_.dst_storage_id ? building_get(order_.dst_storage_id) : nullptr);
    }

    bool has_route() const
    {
        return order_.src_storage_id != 0 &&
            order_.dst_storage_id != 0 &&
            order_.src_storage_id != order_.dst_storage_id &&
            resource() != RESOURCE_NONE;
    }

    bool condition_satisfied() const
    {
        if (!has_route() || order_.condition.condition_type == ORDER_CONDITION_NEVER) {
            return false;
        }

        const DepotStorageEndpoint src = source();
        const DepotStorageEndpoint dst = destination();
        if (!src.active() || !dst.active()) {
            return false;
        }

        const int source_amount = src.amount(resource());
        if (source_amount == 0) {
            return false;
        }

        switch (order_.condition.condition_type) {
            case ORDER_CONDITION_SOURCE_HAS_MORE_THAN:
                return source_amount >= order_.condition.threshold;
            case ORDER_CONDITION_DESTINATION_HAS_LESS_THAN:
                return dst.amount(resource()) < order_.condition.threshold;
            default:
                return true;
        }
    }

    bool source_should_wait_for_more(int source_amount) const
    {
        return (order_.condition.condition_type == ORDER_CONDITION_SOURCE_HAS_MORE_THAN &&
                source_amount < order_.condition.threshold) ||
            (order_.condition.condition_type == ORDER_CONDITION_DESTINATION_HAS_LESS_THAN &&
                source_amount < 4);
    }

private:
    const order &order_;
};

class DepotCartPusherFigure : public NativeFigure {
public:
    DepotCartPusherFigure(
        Figure *f,
        const figure_type_registry_impl::FigureTypeDefinition *definition,
        const figure_type_registry_impl::FigureTypeProfile *profile)
        : NativeFigure(f, definition, profile)
    {
    }

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        figure_image_increase_offset(f, definition()->graphics().max_image_offset);

        f->terrain_usage = config_get(CONFIG_GP_CARAVANS_MOVE_OFF_ROAD) ?
            TERRAIN_USAGE_ANY :
            TERRAIN_USAGE_PREFER_ROADS_HIGHWAY;

        building *depot = building_get(f->building.id());
        if (!owner_binding_matches(f, depot, profile()->owner_binding()) || !is_linked_to_depot(f, depot)) {
            f->state = FIGURE_STATE_DEAD;
            return 1;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL:
                start_order(*f, *depot);
                break;
            case FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE:
            case FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION:
            case FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE:
                advance_between_storages(*f, *depot);
                break;
            case FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE:
                wait_at_source(*f, *depot);
                break;
            case FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION:
                wait_at_destination(*f, *depot);
                break;
            case FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING:
                return_home(*f);
                break;
            case FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER:
                cancel_order(*f, *depot);
                break;
            default:
                retire_unsupported_native_state(f, "depot_cart_pusher");
                break;
        }

        return 1;
    }

private:
    static constexpr int kSpeed = 1;
    static constexpr int kFoodCapacity = 16;
    static constexpr int kOtherCapacity = 4;
    static constexpr int kRerouteDelay = 10;
    static constexpr int kLoadOffloadDelay = 10;

    static bool is_linked_to_depot(const Figure *f, const building *depot)
    {
        if (!f || !depot) {
            return false;
        }
        for (int i = 0; i < 3; i++) {
            if (depot->data.distribution.cartpusher_ids[i] == f->id()) {
                return true;
            }
        }
        return false;
    }

    bool set_destination(Figure &f, const DepotStorageEndpoint &destination, int action_state)
    {
        map_point road_access;
        if (!destination.road_access(road_access)) {
            return false;
        }
        f.destination_building = Building(destination.raw());
        f.destination_x = road_access.x;
        f.destination_y = road_access.y;
        f.action_state = action_state;
        Route::remove(&f);
        return true;
    }

    bool send_cart_home(Figure &f)
    {
        return set_destination(
            f,
            DepotStorageEndpoint(building_get(f.building.id())),
            FIGURE_ACTION_243_DEPOT_CART_PUSHER_RETURNING);
    }

    bool has_load(const Figure &f) const
    {
        return f.loads_sold_or_carrying > 0 && f.resource_id != RESOURCE_NONE;
    }

    resource_type carried_resource(const Figure &f) const
    {
        return static_cast<resource_type>(f.resource_id);
    }

    void try_reroute_order_dst(Figure &f, const DepotOrderView &order)
    {
        if (f.action_state != FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION &&
            f.action_state != FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION) {
            return;
        }
        if (!has_load(f)) {
            return;
        }

        const DepotStorageEndpoint destination = order.destination();
        if (f.action_state == FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION &&
            (destination.raw() == building_get(f.destination_building.id()) ||
             !destination.raw())) {
            return;
        }

        if (destination.accepts(carried_resource(f))) {
            if (!set_destination(f, destination, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
                f.action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
            }
        } else {
            f.action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
        }
    }

    void try_reroute_order_src(Figure &f, const DepotOrderView &order)
    {
        if (f.action_state != FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE &&
            f.action_state != FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE &&
            f.action_state != FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE) {
            return;
        }

        const DepotStorageEndpoint source = order.source();
        if (building_get(f.destination_building.id()) != source.raw() &&
            source.raw() &&
            source.accepts(order.resource())) {
            if (set_destination(f, source, FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE)) {
                return;
            }
        }

        if (!source.accepts(order.resource())) {
            if (has_load(f)) {
                const DepotStorageEndpoint destination = order.destination();
                if (destination.accepts(carried_resource(f)) &&
                    set_destination(f, destination, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
                    return;
                }
            }
            if (!set_destination(
                    f,
                    DepotStorageEndpoint(building_get(f.building.id())),
                    FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER)) {
                send_cart_home(f);
            }
        }
    }

    void unload_or_return(Figure &f, const DepotOrderView &order)
    {
        if (!has_load(f)) {
            send_cart_home(f);
            return;
        }

        const DepotStorageEndpoint source = order.source();
        const DepotStorageEndpoint destination = order.destination();
        const DepotStorageEndpoint target =
            f.action_state == FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE ? source : destination;

        if (!target.accepts(carried_resource(f))) {
            if (f.action_state == FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE) {
                send_cart_home(f);
            } else if (!set_destination(f, source, FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER)) {
                send_cart_home(f);
            }
            return;
        }

        f.loads_sold_or_carrying = target.add(carried_resource(f), f.loads_sold_or_carrying);
        if (f.loads_sold_or_carrying == 0) {
            city_health_dispatch_sickness(&f);
            f.resource_id = RESOURCE_NONE;
            send_cart_home(f);
        }
    }

    void start_order(Figure &f, building &depot)
    {
        if (!figure_type_registry_impl::PathingMode::citizenIsPassable(f.grid_offset)) {
            f.state = FIGURE_STATE_DEAD;
        }

        const DepotOrderView order(depot);
        if (order.condition_satisfied()) {
            if (set_destination(f, order.source(), FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE)) {
                f.wait_ticks = game_time_scale_legacy_day_ticks(kRerouteDelay) + 1;
            } else {
                f.state = FIGURE_STATE_DEAD;
            }
        } else {
            f.state = FIGURE_STATE_DEAD;
        }

        f.image_offset = 0;
    }

    void advance_between_storages(Figure &f, building &depot)
    {
        const DepotOrderView order(depot);
        if (f.wait_ticks > game_time_scale_legacy_day_ticks(kRerouteDelay)) {
            figure_movement_move_ticks_with_percentage(&f, kSpeed, 0);
            try_reroute_order_src(f, order);
            if (f.direction == DIR_FIGURE_AT_DESTINATION) {
                if (f.action_state == FIGURE_ACTION_239_DEPOT_CART_PUSHER_HEADING_TO_SOURCE ||
                    f.action_state == FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE) {
                    f.action_state = FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE;
                } else {
                    f.action_state = FIGURE_ACTION_242_DEPOT_CART_PUSHER_AT_DESTINATION;
                }
                f.wait_ticks = 0;
            } else if (f.direction == DIR_FIGURE_LOST) {
                f.action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                f.wait_ticks = 0;
            } else if (f.direction == DIR_FIGURE_REROUTE) {
                Route::remove(&f);
                f.wait_ticks = 0;
            }
        } else {
            f.wait_ticks++;
        }
        if (f.action_state == FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION) {
            try_reroute_order_dst(f, order);
        }
    }

    void wait_at_source(Figure &f, building &depot)
    {
        f.wait_ticks++;

        const DepotOrderView order(depot);
        try_reroute_order_src(f, order);
        if (f.action_state != FIGURE_ACTION_240_DEPOT_CART_PUSHER_AT_SOURCE) {
            return;
        }
        if (f.wait_ticks > game_time_scale_legacy_day_ticks(kLoadOffloadDelay)) {
            if (has_load(f)) {
                unload_or_return(f, order);
                f.wait_ticks = 0;
                return;
            }

            const DepotStorageEndpoint source = order.source();
            const int source_amount = source.amount(order.resource());
            if (order.source_should_wait_for_more(source_amount)) {
                return;
            }

            const int capacity = resource_is_food(order.resource()) ? kFoodCapacity : kOtherCapacity;
            const int amount_loaded = source.remove(order.resource(), capacity);
            if (amount_loaded > 0) {
                city_health_dispatch_sickness(&f);
                f.resource_id = static_cast<unsigned char>(order.resource());
                f.loads_sold_or_carrying = amount_loaded;

                if (set_destination(f, order.destination(), FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION)) {
                    f.wait_ticks = game_time_scale_legacy_day_ticks(kRerouteDelay) + 1;
                } else {
                    f.action_state = FIGURE_ACTION_244_DEPOT_CART_PUSHER_CANCEL_ORDER;
                }
            }
            f.wait_ticks = 0;
        }
        f.image_offset = 0;
    }

    void wait_at_destination(Figure &f, building &depot)
    {
        f.wait_ticks++;

        const DepotOrderView order(depot);
        if (f.wait_ticks > game_time_scale_legacy_day_ticks(kLoadOffloadDelay)) {
            unload_or_return(f, order);
            f.wait_ticks = 0;
        }
        try_reroute_order_dst(f, order);
    }

    void return_home(Figure &f)
    {
        figure_movement_move_ticks_with_percentage(&f, kSpeed, 0);
        if (f.direction == DIR_FIGURE_AT_DESTINATION) {
            f.action_state = FIGURE_ACTION_238_DEPOT_CART_PUSHER_INITIAL;
            f.state = FIGURE_STATE_DEAD;
        } else if (f.direction == DIR_FIGURE_REROUTE) {
            Route::remove(&f);
            f.wait_ticks = 0;
        }
    }

    void cancel_order(Figure &f, building &depot)
    {
        if (has_load(f)) {
            const DepotOrderView order(depot);
            const DepotStorageEndpoint source = order.source();
            const DepotStorageEndpoint destination = order.destination();
            bool rerouted = false;
            if (source.accepts(carried_resource(f))) {
                rerouted = set_destination(f, source, FIGURE_ACTION_250_DEPOT_CART_PUSHER_RETURN_TO_SOURCE);
            }
            if (!rerouted && destination.accepts(carried_resource(f))) {
                rerouted = set_destination(f, destination, FIGURE_ACTION_241_DEPOT_CART_PUSHER_HEADING_TO_DESTINATION);
            }
            if (!rerouted) {
                send_cart_home(f);
            }
        } else {
            send_cart_home(f);
        }
    }
};

class MarketSupplierFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE:
                move_to_storage(f, movement.roam_ticks);
                break;
            case FIGURE_ACTION_146_SUPPLIER_RETURNING:
                return_to_market(f, movement.roam_ticks);
                break;
            default:
                retire_unsupported_native_state(f, "market_supplier");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static int take_food_from_storage(Figure *f, building *market, building *storage)
    {
        const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
        if (!resource_is_food(resource)) {
            return 0;
        }

        if (!storage || !market) {
            return 0;
        }
        Building storage_obj(storage);

        const int market_units = market->resources[resource];
        const int max_units = kMaxFoodStockedMarket - market_units;
        const int max_loads = max_units / resource_units_per_load();
        if (max_loads <= 0) {
            return 0;
        }

        int amount_taken = 0;
        if (storage_obj.type && storage_obj.type->is_warehouse()) {
            const int warehouse_loads_stored = building_warehouse_get_available_amount(storage_obj, resource);
            const int warehouse_loads_take = warehouse_loads_stored > max_loads ? max_loads : warehouse_loads_stored;
            amount_taken = building_warehouse_try_remove_resource(storage_obj, resource, warehouse_loads_take);
        } else if (storage_obj.type && storage_obj.type->is_granary()) {
            const int granary_loads_stored = building_granary_count_available_resource(storage_obj, resource, 1);
            const int granary_loads_take = granary_loads_stored > max_loads ? max_loads : granary_loads_stored;
            amount_taken = building_granary_try_remove_resource(storage_obj, resource, granary_loads_take);
        } else {
            return 0;
        }
        if (!amount_taken) {
            return 0;
        }

        int previous_boy = f->id();
        for (int i = 0; i < amount_taken; i++) {
            previous_boy = figure_supplier_create_delivery_boy(previous_boy, f->id(), FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int take_resource_from_generic_building(Figure *f, building *source)
    {
        if (!source) {
            return 0;
        }

        const int num_loads = source->resources[resource_wine()] < 2 ? source->resources[resource_wine()] : 2;
        if (num_loads <= 0) {
            return 0;
        }

        source->resources[resource_wine()] -= num_loads;
        int boy = figure_supplier_create_delivery_boy(f->id(), f->id(), FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy, f->id(), FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int take_resource_from_warehouse(Figure *f, building *warehouse)
    {
        if (!warehouse) {
            return 0;
        }
        Building warehouse_obj(warehouse);
        if (!warehouse_obj.type || !warehouse_obj.type->is_warehouse()) {
            return take_resource_from_generic_building(f, warehouse);
        }

        const resource_type resource = static_cast<resource_type>(f->collecting_item_id);
        const int stored = building_warehouse_get_available_amount(warehouse_obj, resource);
        const int num_loads = stored < 2 ? stored : 2;
        if (num_loads <= 0) {
            return 0;
        }

        building_warehouse_try_remove_resource(warehouse_obj, resource, num_loads);
        int boy = figure_supplier_create_delivery_boy(f->id(), f->id(), FIGURE_DELIVERY_BOY);
        if (num_loads > 1) {
            figure_supplier_create_delivery_boy(boy, f->id(), FIGURE_DELIVERY_BOY);
        }
        return 1;
    }

    static int change_destination(Figure *f, building *destination)
    {
        Route::remove(f);
        f->destination_building = Building(destination);
        if (!destination) {
            return 0;
        }

        map_point road = { 0, 0 };
        int has_road_access = 0;
        Building destination_obj(destination);
        if (destination_obj.type && destination_obj.type->is_warehouse()) {
            has_road_access = map_has_road_access_warehouse(destination->x, destination->y, &road);
        } else if (destination_obj.type && destination_obj.type->is_granary()) {
            has_road_access = map_has_road_access_granary(destination->x, destination->y, &road);
        }
        if (!has_road_access) {
            return 0;
        }

        f->action_state = FIGURE_ACTION_145_SUPPLIER_GOING_TO_STORAGE;
        f->destination_x = road.x;
        f->destination_y = road.y;
        return 1;
    }

    static bool is_better_destination(Figure *f, resource_type resource, resource_storage_info *info)
    {
        building *old_destination = building_get(f->destination_building.id());
        if (!building_is_active(old_destination)) {
            return true;
        }
        Building old_destination_obj(old_destination);
        if (old_destination_obj.type && old_destination_obj.type->is_granary() && old_destination->resources[resource] <= 0) {
            return true;
        }
        if (old_destination_obj.type && old_destination_obj.type->is_warehouse() &&
            building_warehouse_get_amount(old_destination_obj, resource) <= 0) {
            return true;
        }

        const int old_distance = building_dist(f->x, f->y, 1, 1, old_destination);
        return info->min_distance <= old_distance / 2;
    }

    static int recalculate_destination(Figure *f)
    {
        const resource_type item = static_cast<resource_type>(f->collecting_item_id);
        building *market = building_get(f->building.id());
        if (!market) {
            return 0;
        }

        resource_storage_info info[RESOURCE_SLOT_COUNT] = { 0 };
        Market market_object(market);
        if (!market_object.needed_inventory(info) ||
            !market_object.resource_storages_for_supplier(info, f)) {
            return 0;
        }

        building *current_item_storage = info[item].building_id ? building_get(info[item].building_id) : nullptr;
        if (market == current_item_storage ||
            building_get(f->destination_building.id()) == current_item_storage) {
            return 1;
        }

        if (current_item_storage) {
            return is_better_destination(f, item, &info[item]) ?
                change_destination(f, current_item_storage) : 1;
        }

        const resource_type fetch_inventory = market_object.fetch_inventory(info);
        if (fetch_inventory == RESOURCE_NONE) {
            return 0;
        }

        market_object.set_fetch_inventory_id(fetch_inventory);
        f->collecting_item_id = fetch_inventory;
        building *fetch_storage = info[fetch_inventory].building_id ? building_get(info[fetch_inventory].building_id) : nullptr;
        return change_destination(f, fetch_storage);
    }

    static void move_to_storage(Figure *f, int roam_ticks)
    {
        figure_movement_move_ticks(f, roam_ticks);
        if (f->direction == DIR_FIGURE_AT_DESTINATION) {
            f->wait_ticks = 0;
            f->previous_tile_x = f->x;
            f->previous_tile_y = f->y;
            const int id = f->id();
            if (!resource_is_food(static_cast<resource_type>(f->collecting_item_id))) {
                if (!take_resource_from_warehouse(
                        f,
                        building_get(f->destination_building.id()))) {
                    f->state = FIGURE_STATE_DEAD;
                }
            } else if (!take_food_from_storage(
                    f,
                building_get(f->building.id()),
                building_get(f->destination_building.id()))) {
                f->state = FIGURE_STATE_DEAD;
            }

            f = Figure::get(id);
            f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
        } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
            f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
            f->destination_x = f->source_x;
            f->destination_y = f->source_y;
            Route::remove(f);
        } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS) {
            f->wait_ticks = 0;
            if (!recalculate_destination(f)) {
                f->action_state = FIGURE_ACTION_146_SUPPLIER_RETURNING;
                f->collecting_item_id = RESOURCE_NONE;
                f->destination_x = f->source_x;
                f->destination_y = f->source_y;
                Route::remove(f);
            }
        }
    }

    static void return_to_market(Figure *f, int roam_ticks)
    {
        figure_movement_move_ticks(f, roam_ticks);
        if (f->direction == DIR_FIGURE_AT_DESTINATION || f->direction == DIR_FIGURE_LOST) {
            f->state = FIGURE_STATE_DEAD;
        } else if (f->direction == DIR_FIGURE_REROUTE) {
            Route::remove(f);
        }
    }

    void update_image(Figure *f) const
    {
        update_legacy_figure_graphics_image_state(*f, definition());
    }
};

class DeliveryFollowerFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->is_ghost = 0;
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);
        f->clear_legacy_cart_overlay_image();

        Figure *leader = Figure::get(f->leading_figure_id);
        if (f->leading_figure_id <= 0 || leader->action_state == FIGURE_ACTION_149_CORPSE) {
            f->state = FIGURE_STATE_DEAD;
        } else if (leader->state == FIGURE_STATE_ALIVE) {
            if (can_follow(leader)) {
                figure_movement_follow_ticks(f, movement.roam_ticks);
            } else {
                f->state = FIGURE_STATE_DEAD;
            }
        } else {
            building *owner = building_get(f->building.id());
            if (owner) {
                owner->resources[f->collecting_item_id] += resource_units_per_load();
            }
            f->state = FIGURE_STATE_DEAD;
        }

        if (leader->is_ghost && !leader->height_adjusted_ticks) {
            f->is_ghost = 1;
        }
        update_image(f);
        return 1;
    }

private:
    static bool can_follow(const Figure *leader)
    {
        switch (leader->type) {
            case FIGURE_MARKET_SUPPLIER:
            case FIGURE_DELIVERY_BOY:
            case FIGURE_MESS_HALL_SUPPLIER:
            case FIGURE_MESS_HALL_COLLECTOR:
            case FIGURE_PRIEST_SUPPLIER:
            case FIGURE_PRIEST:
            case FIGURE_BARKEEP_SUPPLIER:
            case FIGURE_CARAVANSERAI_SUPPLIER:
            case FIGURE_CARAVANSERAI_COLLECTOR:
                return true;
            default:
                return false;
        }
    }

    void update_image(Figure *f) const
    {
        const int direction = figure_image_normalize_direction(
            f->direction < 8 ? f->direction : f->previous_tile_direction);
        update_legacy_figure_graphics_image_state_for_direction(*f, definition(), direction);
    }
};

class FishingBoatFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        return f ? FishingBoat::from(*f).advance(definition()) : 0;
    }
};

class EngineerServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_60_ENGINEER_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    exit_owner_cross_country(f, *owner, FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING);
                }
                break;
            case FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == owner->id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_62_ENGINEER_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_62_ENGINEER_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    send_to_owner_road(f, *owner, FIGURE_ACTION_63_ENGINEER_RETURNING);
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_63_ENGINEER_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_61_ENGINEER_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "engineer_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    void update_image(Figure *f) const
    {
        update_legacy_figure_graphics_image_state(*f, definition());
    }
};

class PrefectServiceFigure : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !definition()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);

        if (!fight_enemy(f)) {
            fight_fire(f, false);
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_70_PREFECT_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    exit_owner_cross_country(f, *owner, FIGURE_ACTION_71_PREFECT_ENTERING_EXITING);
                }
                break;
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (map_building_at(f->grid_offset) == owner->id) {
                        f->state = FIGURE_STATE_DEAD;
                    } else {
                        f->action_state = FIGURE_ACTION_72_PREFECT_ROAMING;
                        figure_movement_init_roaming(f);
                        f->roam_length = 0;
                    }
                }
                break;
            case FIGURE_ACTION_72_PREFECT_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    send_to_owner_road(f, *owner, FIGURE_ACTION_73_PREFECT_RETURNING);
                }
                if (f->state == FIGURE_STATE_ALIVE) {
                    figure_movement_roam_ticks(f, movement.roam_ticks);
                }
                break;
            case FIGURE_ACTION_73_PREFECT_RETURNING:
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_71_PREFECT_ENTERING_EXITING;
                    figure_movement_set_cross_country_destination(f, owner->x, owner->y);
                    f->roam_length = 0;
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                figure_movement_move_ticks(f, movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    f->action_state = FIGURE_ACTION_75_PREFECT_AT_FIRE;
                    Route::remove(f);
                    f->roam_length = 0;
                    f->wait_ticks = game_time_scale_legacy_day_ticks(50);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                } else if (f->wait_ticks++ > FIGURE_REROUTE_DESTINATION_TICKS && !fight_fire(f, true) && !in_combat(f)) {
                    if (send_to_owner_road(f, *owner, FIGURE_ACTION_73_PREFECT_RETURNING)) {
                        f->wait_ticks = 0;
                    }
                }
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                extinguish_fire(f, *owner);
                break;
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                if (!f->target_is_alive()) {
                    if (send_to_owner_road(f, *owner, FIGURE_ACTION_73_PREFECT_RETURNING)) {
                        f->roam_length = 0;
                    }
                    break;
                }
                figure_movement_move_ticks_with_percentage(f, movement.roam_ticks, 20);
                if (f->direction == DIR_FIGURE_AT_DESTINATION || f->wait_ticks++ > kRecalculateEnemyLocationTicks) {
                    Figure *target = &f->target_figure.get();
                    f->destination_x = target->x;
                    f->destination_y = target->y;
                    f->wait_ticks = 0;
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                retire_unsupported_native_state(f, "prefect_service");
                break;
        }

        update_image(f);
        return 1;
    }

private:
    static int get_enemy_distance(Figure *f, int x, int y)
    {
        if (f->type == FIGURE_RIOTER || f->type == FIGURE_ENEMY54_GLADIATOR) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_CRIMINAL_LOOTER || f->type == FIGURE_CRIMINAL_ROBBER) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
            return calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->is_enemy()) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        } else if (f->type == FIGURE_WOLF) {
            return 4 * calc_maximum_distance(x, y, f->x, f->y);
        }
        return kInfiniteDistance;
    }

    static int get_nearest_enemy(int x, int y, int *distance)
    {
        int min_enemy_id = 0;
        int min_dist = kInfiniteDistance;
        for (unsigned int i = 1; i < Figure::count(); i++) {
            Figure *f = Figure::get(i);
            if (f->is_dead()) {
                continue;
            }
            int dist = get_enemy_distance(f, x, y);
            if (dist != kInfiniteDistance && f->targeted_by_figure.save_id()) {
                Figure *pursuiter = &f->targeted_by_figure.get();
                if (get_enemy_distance(f, pursuiter->x, pursuiter->y) < dist * 2) {
                    continue;
                }
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_enemy_id = i;
            }
        }
        *distance = min_dist;
        return min_enemy_id;
    }

    static bool fight_enemy(Figure *f)
    {
        if (!city_figures_has_security_breach() && enemy_army_total_enemy_formations() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
        }
        f->wait_ticks_next_target++;
        if (f->wait_ticks_next_target < 10) {
            return false;
        }
        int distance = 0;
        int enemy_id = get_nearest_enemy(f->x, f->y, &distance);
        if (enemy_id > 0 && distance <= 30) {
            Figure *enemy = Figure::get(enemy_id);
            if (enemy->targeted_by_figure.save_id()) {
                enemy->targeted_by_figure.get().target_figure.clear();
            }
            f->wait_ticks = 0;
            f->action_state = FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY;
            f->destination_x = enemy->x;
            f->destination_y = enemy->y;
            f->target_figure.retarget(*enemy);
            enemy->targeted_by_figure.retarget(*f);
            f->target_figure_created_sequence = enemy->created_sequence;
            Route::remove(f);
            return true;
        }
        f->wait_ticks_next_target = 0;
        return false;
    }

    static bool fight_fire(Figure *f, bool force)
    {
        if (building_list_burning_size() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
            case FIGURE_ACTION_70_PREFECT_CREATED:
            case FIGURE_ACTION_71_PREFECT_ENTERING_EXITING:
            case FIGURE_ACTION_76_PREFECT_GOING_TO_ENEMY:
            case FIGURE_ACTION_77_PREFECT_AT_ENEMY:
                return false;
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
            case FIGURE_ACTION_75_PREFECT_AT_FIRE: {
                if (!force) {
                    return false;
                }
                building *burn = building_get(f->destination_building.id());
                if (burn &&
                    (burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED) &&
                    is_burning_ruin(*burn)) {
                    return true;
                }
                break;
            }
        }
        f->wait_ticks_missile++;
        if (f->wait_ticks_missile < 20 && !force) {
            return false;
        }
        int distance = 0;
        int ruin_id = building_maintenance_get_closest_burning_ruin(f->x, f->y, &distance);
        if (ruin_id > 0 && distance <= 25) {
            building *ruin = building_get(ruin_id);
            if (!ruin) {
                return false;
            }
            f->wait_ticks_missile = 0;
            f->action_state = FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE;
            f->wait_ticks = 0;
            f->destination_x = ruin->road_access_x;
            f->destination_y = ruin->road_access_y;
            f->destination_building = Building(ruin);
            Route::remove(f);
            ruin->figure_id4 = f->id();
            return true;
        }
        return false;
    }

    static void extinguish_fire(Figure *f, building &owner)
    {
        building *burn = building_get(f->destination_building.id());
        if (!burn) {
            f->wait_ticks = 1;
            return;
        }
        int distance = calc_maximum_distance(f->x, f->y, burn->x, burn->y);
        if ((burn->state == BUILDING_STATE_IN_USE || burn->state == BUILDING_STATE_MOTHBALLED)
            && is_burning_ruin(*burn) && distance < 2) {
            burn->fire_duration = 32;
            sound_effect_play(SOUND_EFFECT_FIRE_SPLASH);
        } else {
            f->wait_ticks = 1;
        }
        f->attack_direction = calc_general_direction(f->x, f->y, burn->x, burn->y);
        if (f->attack_direction >= 8) {
            f->attack_direction = 0;
        }
        f->wait_ticks--;
        if (f->wait_ticks <= 0) {
            if (!fight_fire(f, true)) {
                int x_road = 0;
                int y_road = 0;
                if (map_closest_road_within_radius(owner.x, owner.y, owner.size, 2, &x_road, &y_road)) {
                    f->action_state = FIGURE_ACTION_73_PREFECT_RETURNING;
                    f->destination_x = x_road;
                    f->destination_y = y_road;
                    Route::remove(f);
                } else {
                    f->state = FIGURE_STATE_DEAD;
                }
            }
        }
    }

    static bool in_combat(Figure *f)
    {
        return f->action_state == FIGURE_ACTION_150_ATTACK || f->action_state == FIGURE_ACTION_149_CORPSE;
    }

    void update_image(Figure *f) const
    {
        int dir = 0;
        if (f->action_state == FIGURE_ACTION_75_PREFECT_AT_FIRE ||
            f->action_state == FIGURE_ACTION_150_ATTACK) {
            dir = f->attack_direction;
        } else if (f->direction < 8) {
            dir = f->direction;
        } else {
            dir = f->previous_tile_direction;
        }
        dir = figure_image_normalize_direction(dir);

        switch (f->action_state) {
            case FIGURE_ACTION_74_PREFECT_GOING_TO_FIRE:
                f->select_legacy_directional_frame_image(
                    image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET),
                    dir,
                    f->image_offset);
                break;
            case FIGURE_ACTION_75_PREFECT_AT_FIRE:
                f->select_legacy_directional_frame_image(
                    image_group(GROUP_FIGURE_PREFECT_WITH_BUCKET) + 96,
                    dir,
                    f->image_offset / 2);
                break;
            case FIGURE_ACTION_150_ATTACK:
            {
                const int frame_offset = f->attack_image_offset >= 12 ?
                    (f->attack_image_offset - 12) / 2 :
                    0;
                f->image_id = definition()->graphics().legacy_attack_directional_frame_image_id(
                    figure_image_normalize_direction(dir),
                    frame_offset);
                break;
            }
            case FIGURE_ACTION_149_CORPSE:
            default:
                update_legacy_figure_graphics_image_state_for_direction(*f, definition(), dir);
                break;
        }
    }
};

class EntertainmentFigureBase : public NativeFigure {
public:
    using NativeFigure::NativeFigure;

protected:
    static bool is_finished_venue(const building *venue)
    {
        const building_type_registry_impl::BuildingType *definition =
            venue ? building_type_registry_impl::definition_for_type(venue->type) : nullptr;
        const bool is_hippodrome = definition && definition->is_hippodrome();
        const bool is_colosseum = definition && definition->attr_is("colosseum");
        return venue &&
            (!is_hippodrome || !venue->prev_part_building_id) &&
            ((!is_colosseum && !is_hippodrome) ||
                venue->monument.phase == MONUMENT_FINISHED);
    }

    static int show_days(const building *venue, figure_type_registry_impl::EntertainmentShowSlot slot)
    {
        if (!venue) {
            return 0;
        }
        switch (slot) {
            case figure_type_registry_impl::EntertainmentShowSlot::Days1:
                return venue->data.entertainment.days1;
            case figure_type_registry_impl::EntertainmentShowSlot::Days2:
                return venue->data.entertainment.days2;
            case figure_type_registry_impl::EntertainmentShowSlot::None:
            default:
                return 0;
        }
    }

    static void set_show_days(building *venue, figure_type_registry_impl::EntertainmentShowSlot slot, int duration)
    {
        if (!venue) {
            return;
        }
        switch (slot) {
            case figure_type_registry_impl::EntertainmentShowSlot::Days1:
                venue->data.entertainment.days1 = static_cast<unsigned char>(duration);
                break;
            case figure_type_registry_impl::EntertainmentShowSlot::Days2:
                venue->data.entertainment.days2 = static_cast<unsigned char>(duration);
                break;
            case figure_type_registry_impl::EntertainmentShowSlot::None:
            default:
                break;
        }
    }

    void update_show_for_arrival(Figure *f) const
    {
        Building main_venue = f->destination_building.main();
        building *venue = building_get(main_venue.id());
        if (!is_finished_venue(venue)) {
            return;
        }

        for (const figure_type_registry_impl::EntertainmentVenueTarget &target : profile()->venue_targets()) {
            if (target.resolved_building_type() != venue->type) {
                continue;
            }
            if (f->type == FIGURE_ACTOR) {
                venue->data.entertainment.play++;
                if (venue->data.entertainment.play >= 5) {
                    venue->data.entertainment.play = 0;
                }
            }
            set_show_days(venue, target.show_slot, profile()->show_duration());
            return;
        }
    }

    void update_image(Figure *f) const
    {
        int dir = figure_image_normalize_direction(f->direction < 8 ? f->direction : f->previous_tile_direction);

        if (f->type == FIGURE_CHARIOTEER) {
            f->clear_legacy_cart_overlay_image();
            if (f->action_state == FIGURE_ACTION_150_ATTACK ||
                f->action_state == FIGURE_ACTION_149_CORPSE) {
                f->image_id = definition()->graphics().legacy_directional_image_id(
                    figure_image_normalize_direction(dir));
            } else {
                update_legacy_figure_graphics_image_state_for_direction(*f, definition(), dir);
            }
            return;
        }

        const figure_type_registry_impl::FigureGraphics &graphics = definition()->graphics();
        const bool use_lion_tamer_whip = f->type == FIGURE_LION_TAMER &&
            f->wait_ticks_missile >= 96 &&
            f->action_state != FIGURE_ACTION_149_CORPSE;
        int cart_overlay_base_image_id = f->cart_image_id;
        if (f->type == FIGURE_LION_TAMER) {
            cart_overlay_base_image_id = image_group(GROUP_FIGURE_LION);
        }

        if (f->action_state == FIGURE_ACTION_150_ATTACK) {
            if (f->type == FIGURE_GLADIATOR) {
                f->image_id = graphics.legacy_attack_directional_frame_image_id(
                    figure_image_normalize_direction(dir),
                    f->image_offset / 2);
                f->adjust_legacy_gladiator_attack_image_row();
            } else if (use_lion_tamer_whip) {
                f->image_id = graphics.legacy_action_directional_frame_image_id(
                    figure_image_normalize_direction(dir),
                    0);
            } else {
                f->image_id = graphics.legacy_directional_image_id(
                    figure_image_normalize_direction(dir));
            }
        } else if (f->action_state == FIGURE_ACTION_149_CORPSE) {
            cart_overlay_base_image_id = 0;
            update_legacy_figure_graphics_image_state_for_direction(*f, definition(), dir);
        } else if (use_lion_tamer_whip) {
            f->image_id = graphics.legacy_action_directional_frame_image_id(
                figure_image_normalize_direction(dir),
                f->image_offset);
        } else {
            update_legacy_figure_graphics_image_state_for_direction(*f, definition(), dir);
        }
        f->select_legacy_cart_overlay_image(cart_overlay_base_image_id, dir);
    }

    static int get_enemy_distance(Figure *f, int x, int y)
    {
        if (f->type == FIGURE_RIOTER || f->type == FIGURE_ENEMY54_GLADIATOR) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_CRIMINAL_LOOTER || f->type == FIGURE_CRIMINAL_ROBBER) {
            return 3 * calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_INDIGENOUS_NATIVE && f->action_state == FIGURE_ACTION_159_NATIVE_ATTACKING) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->is_enemy()) {
            return calc_maximum_distance(x, y, f->x, f->y);
        }
        if (f->type == FIGURE_WOLF) {
            return 2 * calc_maximum_distance(x, y, f->x, f->y);
        }
        return kInfiniteDistance;
    }

    static bool fight_enemy(Figure *f)
    {
        if (!city_figures_has_security_breach() && enemy_army_total_enemy_formations() <= 0) {
            return false;
        }
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
            case FIGURE_ACTION_149_CORPSE:
                return false;
        }

        int min_enemy_id = 0;
        int min_dist = kInfiniteDistance;
        for (unsigned int i = 1; i < Figure::count(); i++) {
            Figure *enemy = Figure::get(i);
            if (enemy->is_dead()) {
                continue;
            }
            const int dist = get_enemy_distance(enemy, f->x, f->y);
            if (dist != kInfiniteDistance && enemy->targeted_by_figure.save_id()) {
                Figure *pursuiter = &enemy->targeted_by_figure.get();
                if (get_enemy_distance(enemy, pursuiter->x, pursuiter->y) < dist) {
                    continue;
                }
            }
            if (dist < min_dist) {
                min_dist = dist;
                min_enemy_id = static_cast<int>(i);
            }
        }

        if (min_enemy_id > 0 && min_dist <= 50) {
            Figure *enemy = Figure::get(min_enemy_id);
            if (enemy->targeted_by_figure.save_id()) {
                enemy->targeted_by_figure.get().target_figure.clear();
            }
            f->destination_x = enemy->x;
            f->destination_y = enemy->y;
            f->target_figure.retarget(*enemy);
            enemy->targeted_by_figure.retarget(*f);
            f->target_figure_created_sequence = enemy->created_sequence;
            Route::remove(f);
            return true;
        }
        f->wait_ticks_next_target = 0;
        return false;
    }
};

class EntertainmentServiceFigure : public EntertainmentFigureBase {
public:
    using EntertainmentFigureBase::EntertainmentFigureBase;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !profile()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_CARTPUSHER_CART));
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);
        f->wait_ticks_missile++;
        if (f->wait_ticks_missile >= 120) {
            f->wait_ticks_missile = 0;
        }

        if (scenario_gladiator_revolt_is_in_progress() && f->type == FIGURE_GLADIATOR &&
            (f->action_state == FIGURE_ACTION_94_ENTERTAINER_ROAMING ||
                f->action_state == FIGURE_ACTION_95_ENTERTAINER_RETURNING)) {
            f->type = FIGURE_ENEMY54_GLADIATOR;
            Route::remove(f);
            f->roam_length = 0;
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            return 1;
        }

        const int speed_factor = f->type == FIGURE_CHARIOTEER ? 2 : movement.roam_ticks;
        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                figure_image_increase_offset(f, 32);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_94_ENTERTAINER_ROAMING:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    send_to_owner_road(f, *owner, FIGURE_ACTION_95_ENTERTAINER_RETURNING);
                }
                figure_movement_roam_ticks(f, speed_factor);
                break;
            case FIGURE_ACTION_95_ENTERTAINER_RETURNING:
                figure_movement_move_ticks(f, speed_factor);
                if (f->direction == DIR_FIGURE_AT_DESTINATION ||
                    f->direction == DIR_FIGURE_REROUTE ||
                    f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            case FIGURE_ACTION_230_LION_TAMERS_HUNTING_ENEMIES:
                f->terrain_usage = TERRAIN_USAGE_ANY;
                if (!f->target_is_alive() && !fight_enemy(f)) {
                    f->state = FIGURE_STATE_DEAD;
                }
                figure_movement_move_ticks_with_percentage(f, 1, 50);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    Figure *target = &f->target_figure.get();
                    f->destination_x = target->x;
                    f->destination_y = target->y;
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_REROUTE || f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        update_image(f);
        return 1;
    }
};

class EntertainmentVenueSeekerFigure : public EntertainmentFigureBase {
public:
    using EntertainmentFigureBase::EntertainmentFigureBase;

    int execute() override
    {
        Figure *f = data_figure();
        if (!f || !profile()) {
            return 0;
        }

        building *owner = building_get(f->building.id());
        if (!owner_binding_matches(f, owner, profile()->owner_binding())) {
            f->state = FIGURE_STATE_DEAD;
            update_image(f);
            return 1;
        }

        const figure_type_registry_impl::MovementProfile &movement = profile()->movement_profile();
        f->select_legacy_cart_overlay_base_image(image_group(GROUP_FIGURE_CARTPUSHER_CART));
        f->terrain_usage = static_cast<unsigned char>(profile()->pathing_policy().terrain.legacy_usage);
        f->use_cross_country = 0;
        f->max_roam_length = static_cast<short>(movement.max_roam_length);
        figure_image_increase_offset(f, definition()->graphics().max_image_offset);

        if (scenario_gladiator_revolt_is_in_progress() && f->type == FIGURE_GLADIATOR &&
            (f->action_state == FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE)) {
            f->type = FIGURE_ENEMY54_GLADIATOR;
            Route::remove(f);
            f->roam_length = 0;
            f->action_state = FIGURE_ACTION_158_NATIVE_CREATED;
            return 1;
        }

        switch (f->action_state) {
            case FIGURE_ACTION_150_ATTACK:
                figure_combat_handle_attack(f);
                figure_image_increase_offset(f, 32);
                break;
            case FIGURE_ACTION_149_CORPSE:
                figure_combat_handle_corpse(f);
                break;
            case FIGURE_ACTION_90_ENTERTAINER_AT_SCHOOL_CREATED:
                f->is_ghost = 1;
                f->image_offset = 0;
                f->wait_ticks_missile = 0;
                f->wait_ticks--;
                if (f->wait_ticks <= 0) {
                    exit_owner_cross_country(f, *owner, FIGURE_ACTION_91_ENTERTAINER_EXITING_SCHOOL);
                }
                break;
            case FIGURE_ACTION_91_ENTERTAINER_EXITING_SCHOOL:
                f->use_cross_country = 1;
                f->is_ghost = 1;
                if (figure_movement_move_ticks_cross_country(f, movement.roam_ticks) == 1) {
                    if (!choose_destination(f)) {
                        f->state = FIGURE_STATE_DEAD;
                    }
                }
                break;
            case FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE:
                f->is_ghost = 0;
                f->roam_length++;
                if (f->roam_length >= movement.max_roam_length) {
                    f->state = FIGURE_STATE_DEAD;
                    break;
                }
                figure_movement_move_ticks(f, f->type == FIGURE_CHARIOTEER ? 2 : movement.roam_ticks);
                if (f->direction == DIR_FIGURE_AT_DESTINATION) {
                    update_show_for_arrival(f);
                    f->state = FIGURE_STATE_DEAD;
                } else if (f->direction == DIR_FIGURE_REROUTE) {
                    Route::remove(f);
                } else if (f->direction == DIR_FIGURE_LOST) {
                    f->state = FIGURE_STATE_DEAD;
                }
                break;
            default:
                return 0;
        }

        update_image(f);
        return 1;
    }

private:
    struct Candidate {
        building *venue = nullptr;
        map_point road = { 0, 0 };
        int route_distance = 0;
        int show_days = 0;
        int score = 0;
    };

    bool candidate_is_better(const Candidate &candidate, const Candidate &best) const
    {
        if (!best.venue) {
            return true;
        }
        if (candidate.score != best.score) {
            return candidate.score < best.score;
        }
        return candidate.route_distance < best.route_distance;
    }

    bool choose_destination(Figure *f) const
    {
        const Route::DistanceQuery route_query =
            Route::DistanceQuery::fromFigure(*f, PERFORMANCE_TRACKER_ROUTE_PURPOSE_VENUE);
        if (!route_query) {
            return false;
        }

        Candidate best;
        // Training walkers inspect the venue set declared by their profile. XML
        // decides whether this actor/gladiator/lion/charioteer targets one or more
        // venue types; the runtime only applies reachability and ranking.
        for (const figure_type_registry_impl::EntertainmentVenueTarget &target : profile()->venue_targets()) {
            const building_type target_building = target.resolved_building_type();
            if (target_building == BUILDING_NONE) {
                continue;
            }
            for (building *venue = building_first_of_type(target_building); venue; venue = venue->next_of_type) {
                if (venue->state != BUILDING_STATE_IN_USE ||
                    !is_finished_venue(venue)) {
                    continue;
                }
                if (city_festival_games_active() && venue->type != city_festival_games_active_venue_type()) {
                    continue;
                }

                const Route::RoadResult route =
                    route_query.findAccessRoad(
                        *venue,
                        2,
                        profile()->movement_profile().max_roam_length,
                        true);
                if (!route) {
                    continue;
                }

                Candidate candidate;
                candidate.venue = venue;
                candidate.road = route.road;
                candidate.route_distance = route.distance;
                candidate.show_days = show_days(venue, target.show_slot);
                // Match the legacy weighting, but use route distance so the score
                // reflects the path walkers actually take through the road network.
                candidate.score = 2 * candidate.show_days + candidate.route_distance;
                if (candidate_is_better(candidate, best)) {
                    best = candidate;
                }
            }
        }

        if (!best.venue) {
            return false;
        }

        f->destination_building = Building(best.venue);
        f->action_state = FIGURE_ACTION_92_ENTERTAINER_GOING_TO_VENUE;
        f->destination_x = best.road.x;
        f->destination_y = best.road.y;
        f->roam_length = 0;
        Route::remove(f);
        return true;
    }
};

std::unique_ptr<NativeFigure> make_controller(
    Figure *f,
    const figure_type_registry_impl::FigureTypeDefinition *definition,
    const figure_type_registry_impl::FigureTypeProfile *profile)
{
    if (!f || !definition || !profile) {
        return nullptr;
    }

    switch (profile->native_class()) {
        case figure_type_registry_impl::NativeClassId::LegacyAction:
            return nullptr;
        case figure_type_registry_impl::NativeClassId::RoamingService:
            return std::make_unique<RoamingServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EngineerService:
            return std::make_unique<EngineerServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::PrefectService:
            return std::make_unique<PrefectServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EntertainmentService:
            return std::make_unique<EntertainmentServiceFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::EntertainmentVenueSeeker:
            return std::make_unique<EntertainmentVenueSeekerFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::MarketSupplier:
            return std::make_unique<MarketSupplierFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::DeliveryFollower:
            return std::make_unique<DeliveryFollowerFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::TransientWanderer:
            return std::make_unique<TransientWandererFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::DepotCartPusher:
            return std::make_unique<DepotCartPusherFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::FishingBoat:
            return std::make_unique<FishingBoatFigure>(f, definition, profile);
        case figure_type_registry_impl::NativeClassId::None:
        default:
            return nullptr;
    }
}

} // namespace figure_runtime_native_impl

namespace {

struct FigureGraphicsDebugCounters {
    unsigned long long facade_draw_requests = 0;
    unsigned long long legacy_fallback_draws = 0;
    unsigned long long image_from_id_fallback_draws = 0;
};

FigureGraphicsDebugCounters g_figure_graphics_debug_counters;
std::unordered_set<int> g_logged_image_from_id_fallback_types;

void log_image_from_id_fallback_once(const Figure &figure)
{
    if (!g_logged_image_from_id_fallback_types.insert(figure.type).second) {
        return;
    }

    char detail[192];
    snprintf(
        detail,
        sizeof(detail),
        "figure_type=%d image_id=%d cart_image_id=%d",
        figure.type,
        figure.image_id,
        figure.cart_image_id);
    error_context_report_info("Figure graphics legacy Image::from_id fallback.", detail);
}

void record_debug_draw_result(const Figure &figure, bool used_facade_draw_request)
{
    if (used_facade_draw_request) {
        g_figure_graphics_debug_counters.facade_draw_requests++;
        return;
    }

    g_figure_graphics_debug_counters.legacy_fallback_draws++;
    g_figure_graphics_debug_counters.image_from_id_fallback_draws++;
    log_image_from_id_fallback_once(figure);
}

bool resolve_figure_graphic_draw_request(const Figure &figure, FigureGraphicDrawRequest &request)
{
    const Figure *f = &figure;
    if (figure_runtime_native_impl::warrior_graphic_draw_request_for_figure(f, &request)) {
        return true;
    }
    if (figure_runtime_native_impl::fort_standard_graphic_draw_request_for_figure(f, &request)) {
        return true;
    }
    if (figure_runtime_native_impl::map_flag_graphic_draw_request_for_figure(f, &request)) {
        return true;
    }

    const figure_type_registry_impl::FigureTypeDefinition *definition =
        figure_type_registry_impl::definition_for(static_cast<figure_type>(f->type));
    if (figure_runtime_native_impl::depot_cart_graphic_draw_request_for_figure(f, definition, &request)) {
        return true;
    }
    if (figure_runtime_native_impl::figure_graphics_draw_request_for_figure(f, definition, &request)) {
        return true;
    }
    if (figure_runtime_native_impl::hippodrome_horse_graphic_draw_request_for_figure(f, &request)) {
        return true;
    }
    if (figure_runtime_native_impl::enemy_graphic_draw_request_for_figure(f, &request)) {
        return true;
    }
    figure_runtime_native_impl::reset_draw_request(&request);
    if (!f->cart_image_id ||
        !figure_type_registry_impl::FigureGraphics::uses_legacy_cart_overlay(static_cast<figure_type>(f->type))) {
        return false;
    }

    const int has_base =
        figure_runtime_native_impl::set_legacy_base_draw_request_image(request, f->image_id);
    figure_runtime_native_impl::add_draw_layer(
        request,
        figure_type_registry_impl::FigureGraphics::legacy_cart_overlay_layer(
            *f,
            { f->x_offset_cart, f->y_offset_cart }));
    return has_base != 0;
}

} // namespace

bool figure_graphics_resolve_draw_request(const Figure &figure, FigureGraphicDrawRequest &request)
{
    const bool used_facade_draw_request = resolve_figure_graphic_draw_request(figure, request);
    record_debug_draw_result(figure, used_facade_draw_request);
    return used_facade_draw_request;
}
