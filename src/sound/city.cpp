#include "city.h"

#include "building/building_type_registry_internal.h"
#include "graphics/window.h"

#include "core/file.h"
#include "game/settings.h"
#include "building/properties.h"
#include "city/figures.h"
#include "city/population.h"
#include "core/random.h"
#include "core/time.h"
#include "sound/device.h"

#include <array>
#include <initializer_list>
#include <vector>

#define SOUND_VIEWS_THRESHOLD 200
#define SOUND_DELAY_MILLIS 30000
#define SOUND_PLAY_INTERVAL_MILLIS 2000
#define AMBIENT_PLAY_INTERVAL_MILLIS 5000

typedef enum {
    SOUND_AMBIENT_NONE = 0,
    SOUND_AMBIENT_FIRST = 1,
    SOUND_AMBIENT_EMPTY_LAND1 = 1,
    SOUND_AMBIENT_EMPTY_LAND2,
    SOUND_AMBIENT_EMPTY_LAND3, // SOUND_AMBIENT_RIVER,
    SOUND_AMBIENT_EMPTY_TERRAIN01,
    SOUND_AMBIENT_EMPTY_TERRAIN02,
    SOUND_AMBIENT_EMPTY_LAND,
    SOUND_AMBIENT_WHEAT,
    SOUND_AMBIENT_MAX
} sound_ambient_type;

typedef const char *sound_filenames;

struct background_sound {
    int available = 0;
    int total_views = 0;
    int direction_views[5] = {};
    time_millis last_played_time = 0;
    struct {
        unsigned int total = 0;
        unsigned int current = 0;
        std::vector<sound_filenames> list;
    } filenames;
};

struct sound_city_data {
    std::array<background_sound, SOUND_CITY_MAX> city_sounds;
    std::array<background_sound, SOUND_AMBIENT_MAX> ambient_sounds;
    time_millis last_update_time = 0;
    time_millis ambient_last_played_time = 0;
};

static sound_city_data data;
static int sound_filenames_loaded = 0;

static void register_city_sound(sound_city_type sound, std::initializer_list<sound_filenames> filenames)
{
    background_sound &current_sound = data.city_sounds[sound];
    current_sound.filenames.list.assign(filenames.begin(), filenames.end());
    current_sound.filenames.total = static_cast<unsigned int>(current_sound.filenames.list.size());
}

static void register_ambient_sound(sound_ambient_type sound, std::initializer_list<sound_filenames> filenames)
{
    background_sound &current_sound = data.ambient_sounds[sound];
    current_sound.filenames.list.assign(filenames.begin(), filenames.end());
    current_sound.filenames.total = static_cast<unsigned int>(current_sound.filenames.list.size());
}

static void ensure_sound_filenames_loaded(void)
{
    if (sound_filenames_loaded) {
        return;
    }

    register_city_sound(SOUND_CITY_HOUSE_SLUM, {"wavs/house_slum1.wav", "wavs/house_slum2.wav", "wavs/house_slum3.wav", "wavs/house_slum4.wav", "wavs/house_poor1.wav", "wavs/house_poor2.wav", "wavs/house_poor3.wav", "wavs/house_poor4.wav"});
    register_city_sound(SOUND_CITY_HOUSE_POOR, {"wavs/house_poor1.wav", "wavs/house_poor2.wav", "wavs/house_poor3.wav", "wavs/house_poor4.wav", "wavs/house_mid1.wav", "wavs/house_mid2.wav", "wavs/house_mid3.wav", "wavs/house_mid4.wav"});
    register_city_sound(SOUND_CITY_HOUSE_MEDIUM, {"wavs/house_mid1.wav", "wavs/house_mid2.wav", "wavs/house_mid3.wav", "wavs/house_mid4.wav", "wavs/house_good2.wav", "wavs/house_good4.wav", "wavs/house_posh1.wav", "wavs/house_posh2.wav"});
    register_city_sound(SOUND_CITY_HOUSE_GOOD, {"wavs/house_good1.wav", "wavs/house_good2.wav", "wavs/house_good3.wav", "wavs/house_good4.wav", "wavs/house_posh1.wav", "wavs/house_posh2.wav", "wavs/house_posh3.wav", "wavs/house_posh4.wav"});
    register_city_sound(SOUND_CITY_HOUSE_POSH, {"wavs/house_posh1.wav", "wavs/house_posh2.wav", "wavs/house_posh3.wav", "wavs/house_posh4.wav", "wavs/house_good1.wav", "wavs/house_good2.wav", "wavs/house_good3.wav", "wavs/house_good4.wav"});
    register_city_sound(SOUND_CITY_AMPHITHEATER, {"wavs/ampitheatre.wav"});
    register_city_sound(SOUND_CITY_THEATER, {"wavs/theatre.wav"});
    register_city_sound(SOUND_CITY_HIPPODROME, {"wavs/hippodrome.wav"});
    register_city_sound(SOUND_CITY_COLOSSEUM, {"wavs/colloseum.wav"});
    register_city_sound(SOUND_CITY_GLADIATOR_SCHOOL, {"wavs/glad_pit.wav"});
    register_city_sound(SOUND_CITY_LION_PIT, {"wavs/lion_pit.wav"});
    register_city_sound(SOUND_CITY_ACTOR_COLONY, {"wavs/art_pit.wav"});
    register_city_sound(SOUND_CITY_CHARIOT_MAKER, {"wavs/char_pit.wav"});
    register_city_sound(SOUND_CITY_GARDEN, {"wavs/gardens1.wav", "wavs/gardens2.wav", "wavs/gardens3.wav", "wavs/gardens4.wav"});
    register_city_sound(SOUND_CITY_CLINIC, {"wavs/clinic.wav"});
    register_city_sound(SOUND_CITY_HOSPITAL, {"wavs/hospital.wav"});
    register_city_sound(SOUND_CITY_BATHHOUSE, {"wavs/baths.wav"});
    register_city_sound(SOUND_CITY_BARBER, {"wavs/barber.wav"});
    register_city_sound(SOUND_CITY_SCHOOL, {"wavs/school.wav"});
    register_city_sound(SOUND_CITY_ACADEMY, {"wavs/academy.wav"});
    register_city_sound(SOUND_CITY_LIBRARY, {"wavs/library.wav"});
    register_city_sound(SOUND_CITY_PREFECTURE, {ASSETS_DIRECTORY "/Sounds/Prefect.ogg", "wavs/prefecture.wav"});
    register_city_sound(SOUND_CITY_FORT, {"wavs/fort1.wav", "wavs/fort2.wav", "wavs/fort3.wav", "wavs/fort4.wav"});
    register_city_sound(SOUND_CITY_TOWER, {"wavs/tower1.wav"});
    register_city_sound(SOUND_CITY_WATCHTOWER, {"wavs/tower2.wav"});
    register_city_sound(SOUND_CITY_ARMOURY, {"wavs/tower3.wav"});
    register_city_sound(SOUND_CITY_WORKCAMP, {"wavs/tower4.wav"});
    register_city_sound(SOUND_CITY_TEMPLE_CERES, {"wavs/temp_farm.wav"});
    register_city_sound(SOUND_CITY_TEMPLE_NEPTUNE, {"wavs/temp_ship.wav"});
    register_city_sound(SOUND_CITY_TEMPLE_MERCURY, {"wavs/temp_comm.wav"});
    register_city_sound(SOUND_CITY_TEMPLE_MARS, {"wavs/temp_war.wav"});
    register_city_sound(SOUND_CITY_TEMPLE_VENUS, {"wavs/temp_love.wav"});
    register_city_sound(SOUND_CITY_MARKET, {"wavs/market1.wav", "wavs/market4.wav"});
    register_city_sound(SOUND_CITY_CARAVANSERAI, {"wavs/market2.wav"});
    register_city_sound(SOUND_CITY_TAVERN, {"wavs/market3.wav"});
    register_city_sound(SOUND_CITY_GRANARY, {"wavs/granary1.wav"});
    register_city_sound(SOUND_CITY_WAREHOUSE, {"wavs/warehouse1.wav"});
    register_city_sound(SOUND_CITY_MESS_HALL, {"wavs/warehouse2.wav"});
    register_city_sound(SOUND_CITY_SHIPYARD, {"wavs/shipyard1.wav", "wavs/shipyard2.wav"});
    register_city_sound(SOUND_CITY_DOCK, {"wavs/dock1.wav", "wavs/dock2.wav"});
    register_city_sound(SOUND_CITY_WHARF, {"wavs/wharf1.wav", "wavs/wharf2.wav"});
    register_city_sound(SOUND_CITY_PALACE, {"wavs/palace.wav"});
    register_city_sound(SOUND_CITY_ENGINEERS_POST, {ASSETS_DIRECTORY "/Sounds/Engineer.ogg", "wavs/eng_post.wav"});
    register_city_sound(SOUND_CITY_SENATE, {"wavs/senate.wav"});
    register_city_sound(SOUND_CITY_FORUM, {"wavs/forum.wav"});
    register_city_sound(SOUND_CITY_RESERVOIR, {"wavs/resevoir.wav"});
    register_city_sound(SOUND_CITY_FOUNTAIN, {"wavs/fountain.wav"});
    register_city_sound(SOUND_CITY_WELL, {"wavs/well.wav"});
    register_city_sound(SOUND_CITY_MILITARY_ACADEMY, {"wavs/mil_acad.wav"});
    register_city_sound(SOUND_CITY_BARRACKS, {"wavs/barracks.wav", "wavs/marching.wav"});
    register_city_sound(SOUND_CITY_ORACLE, {"wavs/oracle.wav"});
    register_city_sound(SOUND_CITY_BURNING_RUIN, {"wavs/burning_ruin.wav"});
    register_city_sound(SOUND_CITY_WHEAT_FARM, {"wavs/wheat_farm.wav"});
    register_city_sound(SOUND_CITY_VEGETABLE_FARM, {"wavs/veg_farm.wav"});
    register_city_sound(SOUND_CITY_FRUIT_FARM, {"wavs/figs_farm.wav"});
    register_city_sound(SOUND_CITY_OLIVE_FARM, {"wavs/olives_farm.wav"});
    register_city_sound(SOUND_CITY_VINE_FARM, {"wavs/vines_farm.wav"});
    register_city_sound(SOUND_CITY_PIG_FARM, {"wavs/meat_farm.wav"});
    register_city_sound(SOUND_CITY_QUARRY, {"wavs/quarry.wav"});
    register_city_sound(SOUND_CITY_IRON_MINE, {"wavs/mine.wav"});
    register_city_sound(SOUND_CITY_TIMBER_YARD, {"wavs/lumber_mill.wav"});
    register_city_sound(SOUND_CITY_CLAY_PIT, {"wavs/clay_pit.wav"});
    register_city_sound(SOUND_CITY_WINE_WORKSHOP, {"wavs/wine_workshop.wav"});
    register_city_sound(SOUND_CITY_OIL_WORKSHOP, {"wavs/oil_workshop.wav"});
    register_city_sound(SOUND_CITY_WEAPONS_WORKSHOP, {"wavs/weap_workshop.wav"});
    register_city_sound(SOUND_CITY_FURNITURE_WORKSHOP, {"wavs/furn_workshop.wav"});
    register_city_sound(SOUND_CITY_POTTERY_WORKSHOP, {"wavs/pott_workshop.wav"});
    register_city_sound(SOUND_CITY_CITY_MINT, {"wavs/coin.wav"});
    register_city_sound(SOUND_CITY_MISSION_POST, {ASSETS_DIRECTORY "/Sounds/MissionPost.ogg"});
    register_city_sound(SOUND_CITY_BRICKWORKS, {ASSETS_DIRECTORY "/Sounds/Brickworks.ogg"});
    register_city_sound(SOUND_CITY_LIGHTHOUSE, {ASSETS_DIRECTORY "/Sounds/Lighthouse.ogg"});
    register_city_sound(SOUND_CITY_DEPOT, {ASSETS_DIRECTORY "/Sounds/Ox.ogg"});
    register_city_sound(SOUND_CITY_CONCRETE_MAKER, {ASSETS_DIRECTORY "/Sounds/ConcreteMaker.ogg"});
    register_city_sound(SOUND_CITY_CONSTRUCTION_SITE, {ASSETS_DIRECTORY "/Sounds/Engineer.ogg"});
    register_city_sound(SOUND_CITY_NATIVE_HUT, {ASSETS_DIRECTORY "/Sounds/NativeHut.ogg"});
    register_city_sound(SOUND_CITY_AQUEDUCT, {"wavs/aquaduct.wav"});
    register_city_sound(SOUND_CITY_ARENA, {"wavs/colloseum.wav"});
    register_city_sound(SOUND_CITY_NATIVE_DECORATION, {"wavs/park.wav"});

    register_ambient_sound(SOUND_AMBIENT_EMPTY_LAND1, {"wavs/empty_land1.wav"});
    register_ambient_sound(SOUND_AMBIENT_EMPTY_LAND2, {"wavs/empty_land2.wav"});
    register_ambient_sound(SOUND_AMBIENT_EMPTY_LAND3, {"wavs/empty_land3.wav"});
    register_ambient_sound(SOUND_AMBIENT_EMPTY_TERRAIN01, {ASSETS_DIRECTORY "/Sounds/Terrain01.ogg"});
    register_ambient_sound(SOUND_AMBIENT_EMPTY_TERRAIN02, {ASSETS_DIRECTORY "/Sounds/Terrain02.ogg"});
    register_ambient_sound(SOUND_AMBIENT_EMPTY_LAND, {"wavs/empty_land.wav"});
    register_ambient_sound(SOUND_AMBIENT_WHEAT, {"wavs/wheat.wav"});

    sound_filenames_loaded = 1;
}

static void clear_direction_views(background_sound &sound)
{
    for (int &direction_view : sound.direction_views) {
        direction_view = 0;
    }
}

void sound_city_init(void)
{
    ensure_sound_filenames_loaded();

    data.last_update_time = time_get_millis();
    for (int sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        background_sound *current_sound = &data.city_sounds[sound];
        current_sound->last_played_time = data.last_update_time;
        current_sound->available = 0;
        current_sound->total_views = 0;
        current_sound->filenames.current = 0;
        clear_direction_views(*current_sound);
    }
    for (int sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        background_sound *current_sound = &data.ambient_sounds[sound];
        current_sound->last_played_time = data.last_update_time;
        current_sound->available = 0;
        current_sound->total_views = 0;
        current_sound->filenames.current = 0;
        clear_direction_views(*current_sound);
    }
}

void sound_city_set_volume(int percentage)
{
    sound_device_set_volume_for_type(SOUND_TYPE_CITY, percentage);
}

void sound_city_mark_building_view(building_type type, int num_workers, int direction, int has_water_access)
{
    const building_type_registry_impl::BuildingType *definition =
        building_type_registry_impl::definition_for_type(type);
    sound_city_type sound = definition && definition->has_sound() && definition->sound().has_city_sound() ?
        static_cast<sound_city_type>(definition->sound().city_sound()) :
        SOUND_CITY_NONE;
    if (sound == SOUND_CITY_NONE) {
        sound = static_cast<sound_city_type>(building_properties_for_type(type)->sound_id);
    }

    if (sound == SOUND_CITY_NONE) {
        return;
    }
    const model_building *model = model_get_building(type);
    int enemies_present = city_figures_enemies() > 0 || city_figures_imperial_soldiers() > 0;

    int mute_on_enemies = definition && definition->has_sound() ? definition->sound().mute_on_enemies() : 0;
    int always_play = definition && definition->has_sound() ? definition->sound().always_play() : 0;
    int requires_water_access = definition && definition->has_sound() ?
        definition->water_access().has_requirements() :
        0;

    // Shut off when:
    if ((requires_water_access && !has_water_access) ||
        (!always_play && (
            (model->laborers > 0 && num_workers <= 0) ||
            city_population() <= 0 ||
            (enemies_present && mute_on_enemies)))) {
        return;
    }

    data.city_sounds[sound].available = 1;
    ++data.city_sounds[sound].total_views;
    ++data.city_sounds[sound].direction_views[direction];
}

void sound_city_mark_construction_site_view(int direction)
{
    data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].available = 1;
    ++data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].total_views;
    ++data.city_sounds[SOUND_CITY_CONSTRUCTION_SITE].direction_views[direction];
}

void sound_city_decay_views(void)
{
    for (int sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        for (int d = 0; d < 5; d++) {
            data.city_sounds[sound].direction_views[d] = 0;
        }
        data.city_sounds[sound].total_views /= 2;
    }

    for (int sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        for (int d = 0; d < 5; d++) {
            data.ambient_sounds[sound].direction_views[d] = 0;
        }
        data.ambient_sounds[sound].total_views /= 2;
    }
}

void sound_city_progress_ambient(void)
{
    for (int sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        data.ambient_sounds[sound].available = 1;
        ++data.ambient_sounds[sound].total_views;
        ++data.ambient_sounds[sound].direction_views[SOUND_DIRECTION_CENTER];
    }
}

static const char *get_filename_for_sound(background_sound *sound)
{
    const char *filename = sound->filenames.list[sound->filenames.current];
    sound->filenames.current = (sound->filenames.current + 1) % sound->filenames.total;
    return filename;
}

static void play_sound(background_sound *sound, int direction)
{
    if (!setting_sound(SOUND_TYPE_CITY)->enabled || !sound->filenames.total) {
        return;
    }
    const char *filename = get_filename_for_sound(sound);
    if (sound_device_is_file_playing_on_channel(filename, SOUND_TYPE_CITY)) {
        return;
    }
    int left_pan;
    int right_pan;
    switch (direction) {
        case SOUND_DIRECTION_CENTER:
            left_pan = right_pan = 100;
            break;
        case SOUND_DIRECTION_LEFT:
            left_pan = 100;
            right_pan = 0;
            break;
        case SOUND_DIRECTION_RIGHT:
            left_pan = 0;
            right_pan = 100;
            break;
        default:
            left_pan = right_pan = 0;
            break;
    }
    sound_device_play_file_on_channel_panned(filename, SOUND_TYPE_CITY,
        setting_sound(SOUND_TYPE_CITY)->volume, left_pan, right_pan, 0);
}

static void sound_city_play_city(void)
{
    time_millis now = time_get_millis();

    if (now - data.last_update_time < SOUND_PLAY_INTERVAL_MILLIS) {
        // Only play 1 sound every 2 seconds
        return;
    }

    time_millis max_delay = 0;
    background_sound *sound_to_play = 0;
    for (int sound = SOUND_CITY_FIRST; sound < SOUND_CITY_MAX; sound++) {
        background_sound *current_sound = &data.city_sounds[sound];
        if (!current_sound->available) {
            continue;
        }
        current_sound->available = 0;
        if (current_sound->total_views >= SOUND_VIEWS_THRESHOLD &&
            now - current_sound->last_played_time >= SOUND_DELAY_MILLIS) {
            if (now - current_sound->last_played_time > max_delay) {
                max_delay = now - current_sound->last_played_time;
                sound_to_play = current_sound;
            }
        }
    }

    if (!sound_to_play) {
        return;
    }
    // always only one channel available... use it
    int direction;
    if (sound_to_play->direction_views[SOUND_DIRECTION_CENTER] > 10) {
        direction = SOUND_DIRECTION_CENTER;
    } else if (sound_to_play->direction_views[SOUND_DIRECTION_LEFT] > 10) {
        direction = SOUND_DIRECTION_LEFT;
    } else if (sound_to_play->direction_views[SOUND_DIRECTION_RIGHT] > 10) {
        direction = SOUND_DIRECTION_RIGHT;
    } else {
        direction = SOUND_DIRECTION_CENTER;
    }

    play_sound(sound_to_play, direction);
    sound_to_play->last_played_time = now;
    sound_to_play->total_views = 0;
    for (int d = 0; d < 5; d++) {
        sound_to_play->direction_views[d] = 0;
    }
    data.last_update_time = now;
}

static void sound_city_play_ambient(void)
{
    if (!window_is(WINDOW_CITY)) {
        return;
    }

    time_millis now = time_get_millis();

    // Skip if ambient interval not reached or too soon after a city sound
    if (now - data.ambient_last_played_time < AMBIENT_PLAY_INTERVAL_MILLIS * 3) {
        return;
    }
    if (now - data.last_update_time < SOUND_PLAY_INTERVAL_MILLIS) {
        return;
    }

    sound_city_progress_ambient();
    time_millis max_delay = 0;
    background_sound *sound_to_play = 0;
    for (int sound = SOUND_AMBIENT_FIRST; sound < SOUND_AMBIENT_MAX; sound++) {
        background_sound *current_sound = &data.ambient_sounds[sound];
        if (!current_sound->available) {
            continue;
        }

        if (now - current_sound->last_played_time >= AMBIENT_PLAY_INTERVAL_MILLIS) {
            if (now - current_sound->last_played_time > max_delay) {
                max_delay = now - current_sound->last_played_time;
                sound_to_play = current_sound;
            }
        }
    }

    if (!sound_to_play) {
        return;
    }

    play_sound(sound_to_play, SOUND_DIRECTION_CENTER);
    sound_to_play->last_played_time = now;
    sound_to_play->total_views = 0;
    for (int d = 0; d < 5; d++) {
        sound_to_play->direction_views[d] = 0;
    }
    data.ambient_last_played_time = now;
}

void sound_city_play(void)
{
    sound_city_play_city();
    sound_city_play_ambient();
}
