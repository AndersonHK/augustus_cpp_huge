#include "sound/device.h"

#include "core/calc.h"
#include "core/config.h"
#include "core/file.h"
#include "core/log.h"
#include "core/time.h"
#include "game/campaign.h"
#include "game/settings.h"
#include "platform/platform.h"
#include "platform/vita/vita.h"

#include "SDL.h"
#include "SDL_mixer.h"

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifdef __vita__
#include <psp2/io/fcntl.h>
#endif

constexpr int k_audio_rate = 22050;
constexpr SDL_AudioFormat k_audio_format = AUDIO_S16;
constexpr int k_audio_channels = 2;
constexpr int k_audio_buffers = 1024;
constexpr int k_no_channel = -1;

#if SDL_VERSION_ATLEAST(2, 0, 7)
#define USE_SDL_AUDIOSTREAM
#endif

struct MixChunkDeleter {
    void operator()(Mix_Chunk *chunk) const noexcept
    {
        if (chunk) {
            Mix_FreeChunk(chunk);
        }
    }
};

struct MixMusicDeleter {
    void operator()(Mix_Music *music) const noexcept
    {
        if (music) {
            Mix_FreeMusic(music);
        }
    }
};

struct MixStreamDeleter {
    void operator()(SDL_AudioStream *stream) const noexcept
    {
        if (stream) {
            SDL_FreeAudioStream(stream);
        }
    }
};

using MixChunkPtr = std::unique_ptr<Mix_Chunk, MixChunkDeleter>;
using MixMusicPtr = std::unique_ptr<Mix_Music, MixMusicDeleter>;
using MixStreamPtr = std::unique_ptr<SDL_AudioStream, MixStreamDeleter>;

struct sound_channel {
    std::string filename;
    MixChunkPtr chunk;
    time_millis last_played = 0;
};

struct SoundTypeChannel {
    int start = 0;
    int total = 0;
};

struct sound_device_data {
    bool initialized = false;
    MixMusicPtr music;
    std::vector<sound_channel> channels;
    unsigned int total_channels = 0;
    void (*sound_finished_callback)(sound_type) = nullptr;
};

struct CustomMusicData {
    SDL_AudioFormat dst_format = 0;
#ifdef USE_SDL_AUDIOSTREAM
    MixStreamPtr stream;
#endif
    SDL_AudioCVT cvt = {};
    std::vector<std::uint8_t> buffer;
    int buffer_size = 0;
    int cur_read = 0;
    int cur_write = 0;

    void reset()
    {
#ifdef USE_SDL_AUDIOSTREAM
        stream.reset();
#endif
        buffer.clear();
        buffer_size = 0;
        cur_read = 0;
        cur_write = 0;
        cvt = {};
    }

    bool create(SDL_AudioFormat src_format, std::uint8_t src_channels, int src_rate,
        SDL_AudioFormat dst_format_in, std::uint8_t dst_channels, int dst_rate)
    {
        reset();
        dst_format = dst_format_in;

#ifdef USE_SDL_AUDIOSTREAM
        stream.reset(SDL_NewAudioStream(
            src_format, src_channels, src_rate,
            dst_format, dst_channels, dst_rate
        ));
        if (stream) {
            return true;
        }
#endif

        if (SDL_BuildAudioCVT(
                &cvt, src_format, src_channels, src_rate,
                dst_format, dst_channels, dst_rate) < 0) {
            return false;
        }

        buffer_size = dst_rate * dst_channels * 2 * 2;
        buffer.resize(static_cast<size_t>(buffer_size));
        cur_read = 0;
        cur_write = 0;
        return !buffer.empty();
    }

    bool active() const
    {
#ifdef USE_SDL_AUDIOSTREAM
        return stream != nullptr || !buffer.empty();
#else
        return !buffer.empty();
#endif
    }

    bool put(const void *audio_data, int len)
    {
        if (!audio_data || len <= 0 || !active()) {
            return false;
        }

#ifdef USE_SDL_AUDIOSTREAM
        if (stream) {
            return SDL_AudioStreamPut(stream.get(), audio_data, len) == 0;
        }
#endif

        std::vector<Uint8> converted_audio(static_cast<size_t>(len) * cvt.len_mult);
        if (converted_audio.empty()) {
            return false;
        }

        std::memcpy(converted_audio.data(), audio_data, static_cast<size_t>(len));
        cvt.buf = converted_audio.data();
        cvt.len = len;
        SDL_ConvertAudio(&cvt);
        int converted_len = cvt.len_cvt;
        if (converted_len <= 0) {
            return false;
        }

        if (converted_len + cur_write <= buffer_size) {
            std::memcpy(&buffer[static_cast<size_t>(cur_write)], converted_audio.data(),
                static_cast<size_t>(converted_len));
        } else {
            int end_len = buffer_size - cur_write;
            std::memcpy(&buffer[static_cast<size_t>(cur_write)], converted_audio.data(),
                static_cast<size_t>(end_len));
            std::memcpy(buffer.data(), &converted_audio[static_cast<size_t>(end_len)],
                static_cast<size_t>(converted_len - end_len));
        }
        cur_write = (cur_write + converted_len) % buffer_size;
        return true;
    }

    int get(Uint8 *dst, int len)
    {
        if (!dst || len <= 0 || !active()) {
            return 0;
        }

#ifdef USE_SDL_AUDIOSTREAM
        if (stream) {
            return SDL_AudioStreamGet(stream.get(), dst, len);
        }
#endif

        if (cur_read < cur_write) {
            int bytes_available = cur_write - cur_read;
            int bytes_to_copy = bytes_available < len ? bytes_available : len;
            std::memcpy(dst, &buffer[static_cast<size_t>(cur_read)], static_cast<size_t>(bytes_to_copy));
            cur_read += bytes_to_copy;
            return bytes_to_copy;
        }

        int bytes_available = buffer_size - cur_read;
        int bytes_copied = bytes_available < len ? bytes_available : len;
        std::memcpy(dst, &buffer[static_cast<size_t>(cur_read)], static_cast<size_t>(bytes_copied));

        if (bytes_copied < len) {
            int second_part_len = len - bytes_copied;
            bytes_available = cur_write;
            int bytes_second = bytes_available < second_part_len ? bytes_available : second_part_len;
            std::memcpy(&dst[static_cast<size_t>(bytes_copied)], buffer.data(),
                static_cast<size_t>(bytes_second));
            bytes_copied += bytes_second;
        }

        cur_read = (cur_read + bytes_copied) % buffer_size;
        return bytes_copied;
    }
};

static sound_device_data data{};
static SoundTypeChannel sound_type_to_channels[SOUND_TYPE_MAX] = {
    {0, 1},
    {0, 10},
    {0, 5},
    {0, 0}
};
static CustomMusicData custom_music{};
static void (*track_finished_callback)(void) = nullptr;

#ifdef __vita__
struct VitaMusicData {
    std::string filename;
    std::vector<std::uint8_t> buffer;
};
static VitaMusicData vita_music_data{};
#endif

static int percentage_to_volume(int percentage)
{
    int master_percentage = config_get(CONFIG_GENERAL_MASTER_VOLUME);
    return calc_adjust_with_percentage(percentage, master_percentage) * SDL_MIX_MAXVOLUME / 100;
}

static MixChunkPtr load_chunk(const char *filename)
{
    if (!filename || !*filename) {
        return {};
    }

    std::size_t size = 0;
    auto *audio_data = game_campaign_load_file(filename, &size);
    if (audio_data) {
        SDL_RWops *sdl_memory = SDL_RWFromMem(audio_data, static_cast<int>(size));
        free(audio_data);
        if (!sdl_memory) {
            return {};
        }
        return MixChunkPtr(Mix_LoadWAV_RW(sdl_memory, SDL_TRUE));
    }

    filename = dir_get_file(filename, MAY_BE_LOCALIZED);
    if (!filename) {
        return {};
    }

#if defined(__vita__) || defined(__ANDROID__)
    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        return {};
    }
    SDL_RWops *sdl_fp = SDL_RWFromFP(fp, SDL_TRUE);
    return MixChunkPtr(Mix_LoadWAV_RW(sdl_fp, SDL_TRUE));
#else
    return MixChunkPtr(Mix_LoadWAV(filename));
#endif
}

#ifdef __vita__
static bool load_music_for_vita(const char *filename)
{
    if (!filename || !*filename) {
        return false;
    }
    if (!vita_music_data.filename.empty() && vita_music_data.filename == filename && !vita_music_data.buffer.empty()) {
        return true;
    }

    FILE *fp = file_open(filename, "rb");
    if (!fp) {
        return false;
    }

    vita_music_data.filename = filename;
    vita_music_data.buffer.clear();
    fseek(fp, 0, SEEK_END);
    auto size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (size <= 0) {
        file_close(fp);
        return false;
    }
    vita_music_data.buffer.resize(static_cast<size_t>(size));
    auto bytes_read = fread(vita_music_data.buffer.data(), 1, static_cast<size_t>(size), fp);
    file_close(fp);
    if (bytes_read != static_cast<size_t>(size)) {
        vita_music_data.buffer.clear();
        return false;
    }
    return true;
}
#endif

static MixMusicPtr load_music_for_filename(const char *filename, bool allow_platform_loaders)
{
    if (!filename || !*filename) {
        return {};
    }

    std::size_t size = 0;
    auto *audio_data = game_campaign_load_file(filename, &size);
    if (audio_data) {
        SDL_RWops *sdl_music = SDL_RWFromMem(audio_data, static_cast<int>(size));
        free(audio_data);
        if (!sdl_music) {
            return {};
        }
        return MixMusicPtr(Mix_LoadMUS_RW(sdl_music, SDL_TRUE));
    }

#ifdef __vita__
    if (allow_platform_loaders && load_music_for_vita(filename)) {
        if (SDL_RWops *sdl_music = SDL_RWFromMem(vita_music_data.buffer.data(),
            static_cast<int>(vita_music_data.buffer.size()))) {
            return MixMusicPtr(Mix_LoadMUS_RW(sdl_music, SDL_TRUE));
        }
        return {};
    }
#elif defined(__ANDROID__)
    if (allow_platform_loaders) {
        FILE *fp = file_open(filename, "rb");
        if (!fp) {
            return {};
        }
        SDL_RWops *sdl_fp = SDL_RWFromFP(fp, SDL_TRUE);
        return MixMusicPtr(Mix_LoadMUS_RW(sdl_fp, SDL_TRUE));
    }
#endif

    return MixMusicPtr(Mix_LoadMUS(filename));
}

static void init_channels(void)
{
    if (data.initialized) {
        return;
    }

    data.total_channels = 0;
    for (int type = SOUND_TYPE_MIN; type < SOUND_TYPE_MAX; ++type) {
        sound_type_to_channels[type].start = static_cast<int>(data.total_channels);
        data.total_channels += static_cast<unsigned int>(sound_type_to_channels[type].total);
    }

    data.channels.clear();
    data.channels.resize(static_cast<size_t>(data.total_channels));

    data.initialized = true;
}

void sound_device_open(void)
{
#ifdef __WINDOWS__
    SDL_AudioInit("directsound");
#endif

    if (0 == Mix_OpenAudio(k_audio_rate, k_audio_format, k_audio_channels, k_audio_buffers)) {
        SDL_Log("Using default audio driver: %s", SDL_GetCurrentAudioDriver());
        init_channels();
        return;
    }
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize using default driver: %s", Mix_GetError());

    for (int i = 0; i < SDL_GetNumAudioDrivers(); i++) {
        const char *driver_name = SDL_GetAudioDriver(i);
        if (SDL_strcmp(driver_name, "disk") == 0 || SDL_strcmp(driver_name, "dummy") == 0) {
            continue;
        }
        if (0 == SDL_AudioInit(driver_name) &&
            0 == Mix_OpenAudio(k_audio_rate, k_audio_format, k_audio_channels, k_audio_buffers)) {
            SDL_Log("Using audio driver: %s", driver_name);
            init_channels();
            return;
        }
        SDL_Log("Not using audio driver %s, reason: %s", driver_name, SDL_GetError());
    }

    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Sound failed to initialize: %s", Mix_GetError());
    int max = SDL_GetNumAudioDevices(0);
    SDL_Log("Number of audio devices: %d", max);
    for (int i = 0; i < max; i++) {
        SDL_Log("Audio device: %s", SDL_GetAudioDeviceName(i, 0));
    }
}

static void stop_channel(int channel)
{
    if (!data.initialized) {
        return;
    }
    if (data.channels[channel].chunk) {
        Mix_HaltChannel(channel);
    }
    data.channels[channel].filename.clear();
    data.channels[channel].chunk.reset();
    data.channels[channel].last_played = 0;
}

void sound_device_close(void)
{
    if (!data.initialized) {
        return;
    }
    for (unsigned int i = 0; i < data.total_channels; i++) {
        stop_channel(static_cast<int>(i));
    }
    Mix_ChannelFinished(nullptr);
    Mix_CloseAudio();
    data.channels.clear();
    data.total_channels = 0;
    data.initialized = false;
}

static void callback_for_audio_finished(int channel)
{
    if (!data.sound_finished_callback) {
        return;
    }
    for (int type = SOUND_TYPE_MIN; type < SOUND_TYPE_MAX; type++) {
        if (channel >= sound_type_to_channels[type].start &&
            channel - sound_type_to_channels[type].start < sound_type_to_channels[type].total) {
            data.sound_finished_callback((sound_type) type);
            return;
        }
    }
    data.sound_finished_callback((sound_type) k_no_channel);
}

void sound_device_init_channels(void)
{
    if (!data.initialized) {
        return;
    }
    Mix_AllocateChannels(data.total_channels);
    log_info("Loading audio files", 0, 0);
    Mix_ChannelFinished(callback_for_audio_finished);
}

static int get_channel_for_filename(const char *filename, sound_type type)
{
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (data.channels[channel].filename == filename) {
            return channel;
        }
    }
    return k_no_channel;
}

int sound_device_is_file_playing_on_channel(const char *filename, sound_type type)
{
    if (!data.initialized || !filename) {
        return 0;
    }
    int channel = get_channel_for_filename(filename, type);
    if (channel == k_no_channel) {
        return 0;
    }
    return Mix_Playing(channel);
}

void sound_device_set_music_volume(int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    Mix_VolumeMusic(percentage_to_volume(volume_pct));
}

void sound_device_set_volume_for_type(sound_type type, int volume_pct)
{
    if (!data.initialized) {
        return;
    }
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (data.channels[channel].chunk) {
            Mix_VolumeChunk(data.channels[channel].chunk.get(), percentage_to_volume(volume_pct));
        }
    }
}

int sound_device_play_music(const char *filename, int volume_pct, int loop)
{
    if (!data.initialized || !config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        return 0;
    }
    sound_device_stop_music();
    if (!filename) {
        return 0;
    }

    data.music = load_music_for_filename(filename, true);
    if (!data.music) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Error opening music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }

    if (Mix_PlayMusic(data.music.get(), loop ? -1 : 0) == -1) {
        data.music.reset();
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
            "Error playing music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }
    sound_device_set_music_volume(volume_pct);
    return 1;
}

static void internal_on_track_finished(void)
{
    if (track_finished_callback) {
        track_finished_callback();
    }
}

int sound_device_play_track(const char *filename, int volume_pct, void (*on_finish)(void))
{
    if (!data.initialized || !config_get(CONFIG_GENERAL_ENABLE_AUDIO)) {
        return 0;
    }

    sound_device_stop_music();

    if (!filename) {
        return 0;
    }

    data.music = load_music_for_filename(filename, false);
    if (!data.music) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Error opening music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }

    track_finished_callback = on_finish;
    Mix_HookMusicFinished(internal_on_track_finished);

    if (Mix_PlayMusic(data.music.get(), 0) == -1) {
        data.music.reset();
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "Error playing music file '%s'. Reason: %s", filename, Mix_GetError());
        return 0;
    }

    sound_device_set_music_volume(volume_pct);
    return 1;
}

static int get_available_channel(sound_type type)
{
    int oldest_channel = k_no_channel;
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        int channel = i + sound_type_to_channels[type].start;
        if (!Mix_Playing(channel)) {
            if (oldest_channel == k_no_channel || data.channels[channel].last_played < data.channels[oldest_channel].last_played) {
                oldest_channel = channel;
            }
        }
    }
    return oldest_channel;
}

int sound_device_play_file_on_channel_panned(const char *filename, sound_type type,
    int volume_pct, int left_pct, int right_pct, int loop)
{
    if (!data.initialized || !config_get(CONFIG_GENERAL_ENABLE_AUDIO) || !filename) {
        return 0;
    }
    if (!setting_sound_is_enabled(type)) {
        return 0;
    }

    int channel = get_channel_for_filename(filename, type);
    if (channel == k_no_channel) {
        channel = get_available_channel(type);
        if (channel == k_no_channel) {
            return 0;
        }
        stop_channel(channel);
        data.channels[channel].chunk = load_chunk(filename);
        if (!data.channels[channel].chunk) {
            return 0;
        }
        data.channels[channel].filename = filename;
    }
    Mix_SetPanning(channel, left_pct * 255 / 100, right_pct * 255 / 100);
    Mix_VolumeChunk(data.channels[channel].chunk.get(), percentage_to_volume(volume_pct));
    if (Mix_PlayChannel(channel, data.channels[channel].chunk.get(), loop ? -1 : 0) == -1) {
        return 0;
    }
    data.channels[channel].last_played = time_get_millis();
    return 1;
}

int sound_device_play_file_on_channel(const char *filename, sound_type type, int volume_pct)
{
    return sound_device_play_file_on_channel_panned(filename, type, volume_pct, 100, 100, 0);
}

void sound_device_on_audio_finished(void (*callback)(sound_type))
{
    if (!data.initialized) {
        return;
    }
    data.sound_finished_callback = callback;
}

void sound_device_fadeout_music(int milisseconds)
{
    if (!data.initialized) {
        return;
    }
    Mix_FadeOutMusic(milisseconds);
}

int sound_device_pause_music(void)
{
    if (data.initialized && Mix_PlayingMusic()) {
        Mix_PauseMusic();
        return 1;
    }
    return 0;
}

int sound_device_resume_music(void)
{
    if (data.initialized && Mix_PausedMusic()) {
        Mix_ResumeMusic();
        SDL_Log("Resuming paused music.");
        return 1;
    }
    return 0;
}

void sound_device_stop_music(void)
{
    if (!data.initialized) {
        return;
    }
    if (data.music) {
        Mix_HaltMusic();
        data.music.reset();
    }
}

void sound_device_stop_type(sound_type type)
{
    for (int i = 0; i < sound_type_to_channels[type].total; i++) {
        stop_channel(i + sound_type_to_channels[type].start);
    }
}

static void custom_music_callback(void *dummy, Uint8 *dst, int len)
{
    std::memset(dst, 0, len);

    int volume = config_get(CONFIG_GENERAL_ENABLE_AUDIO) && config_get(CONFIG_GENERAL_ENABLE_VIDEO_SOUND) ?
        percentage_to_volume(config_get(CONFIG_GENERAL_VIDEO_VOLUME)) : 0;
    if (len <= 0 || volume == 0 || !custom_music.active()) {
        return;
    }

    std::vector<Uint8> mix_buffer(len);
    int bytes_copied = custom_music.get(mix_buffer.data(), len);
    if (bytes_copied <= 0) {
        return;
    }

    SDL_MixAudioFormat(dst, mix_buffer.data(), custom_music.dst_format, bytes_copied, volume);
}

void sound_device_use_custom_music_player(int bitdepth, int num_channels, int rate, const void *audio_data, int len)
{
    if (!data.initialized) {
        return;
    }

    SDL_AudioFormat format;
    if (bitdepth == 8) {
        format = AUDIO_U8;
    } else if (bitdepth == 16) {
        format = AUDIO_S16SYS;
    } else if (bitdepth == 32) {
        format = AUDIO_F32;
    } else {
        log_error("Custom music bitdepth not supported:", 0, bitdepth);
        return;
    }

    int device_rate;
    Uint16 device_format;
    int device_channels;
    Mix_QuerySpec(&device_rate, &device_format, &device_channels);
    if (!custom_music.create(format, static_cast<std::uint8_t>(num_channels), rate,
            device_format, static_cast<std::uint8_t>(device_channels), device_rate)) {
        return;
    }

    sound_device_write_custom_music_data(audio_data, len);
    Mix_HookMusic(custom_music_callback, nullptr);
}

void sound_device_write_custom_music_data(const void *audio_data, int len)
{
    if (!data.initialized || !audio_data || len <= 0 || !custom_music.active()) {
        return;
    }
    custom_music.put(audio_data, len);
}

void sound_device_use_default_music_player(void)
{
    if (!data.initialized) {
        return;
    }
    Mix_HookMusic(nullptr, nullptr);
    custom_music.reset();
}
