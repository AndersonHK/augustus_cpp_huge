#include "system.h"

#include "game/settings.h"
#include "sound/city.h"
#include "sound/device.h"
#include "sound/effect.h"
#include "sound/music.h"
#include "sound/speech.h"

#include <stdio.h>

static int disabled;

void sound_system_disable(void)
{
    disabled = 1;
}

int sound_system_is_disabled(void)
{
    return disabled;
}

void sound_system_init(void)
{
    if (disabled) {
        return;
    }
    sound_device_open();
    sound_device_init_channels();

    sound_city_set_volume(setting_sound(SOUND_TYPE_CITY)->volume);
    sound_effect_set_volume(setting_sound(SOUND_TYPE_EFFECTS)->volume);
    sound_music_set_volume(setting_sound(SOUND_TYPE_MUSIC)->volume);
    sound_speech_set_volume(setting_sound(SOUND_TYPE_SPEECH)->volume);
}

void sound_system_shutdown(void)
{
    if (disabled) {
        return;
    }
    sound_device_close();
}
