#pragma once

#include "sdkconfig.h"
#include <Preferences.h>
#include "../PreferencesKeys.h"
#include "../Logger.h"
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif

#define LOW_MEMORY_MAX_KEYPAD 20
#define LOW_MEMORY_MAX_TIMECONTROL 10
#define LOW_MEMORY_MAX_AUTH 10
#define LOW_MEMORY_UPDATE_CHECK_MIN_BLOCK 65536
#define LOW_MEMORY_HTTPS_MAX_SOCKETS 2

inline bool lowMemoryMode()
{
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    return esp_psram_get_size() <= 0;
#else
    return true;
#endif
}

inline void applyLowMemoryLimits(Preferences* preferences)
{
    if(!lowMemoryMode())
    {
        return;
    }

    Log->println("Low memory mode: no PSRAM available, limiting entry counts and disabling per-entry topics");

    if(preferences->getInt(preference_keypad_max_entries, LOW_MEMORY_MAX_KEYPAD) > LOW_MEMORY_MAX_KEYPAD)
    {
        preferences->putInt(preference_keypad_max_entries, LOW_MEMORY_MAX_KEYPAD);
    }
    if(preferences->getInt(preference_timecontrol_max_entries, LOW_MEMORY_MAX_TIMECONTROL) > LOW_MEMORY_MAX_TIMECONTROL)
    {
        preferences->putInt(preference_timecontrol_max_entries, LOW_MEMORY_MAX_TIMECONTROL);
    }
    if(preferences->getInt(preference_auth_max_entries, LOW_MEMORY_MAX_AUTH) > LOW_MEMORY_MAX_AUTH)
    {
        preferences->putInt(preference_auth_max_entries, LOW_MEMORY_MAX_AUTH);
    }
    if(preferences->getBool(preference_keypad_topic_per_entry, false))
    {
        preferences->putBool(preference_keypad_topic_per_entry, false);
    }
    if(preferences->getBool(preference_timecontrol_topic_per_entry, false))
    {
        preferences->putBool(preference_timecontrol_topic_per_entry, false);
    }
    if(preferences->getBool(preference_auth_topic_per_entry, false))
    {
        preferences->putBool(preference_auth_topic_per_entry, false);
    }
}
