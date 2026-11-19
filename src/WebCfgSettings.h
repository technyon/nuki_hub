#pragma once

enum class SettingType
{
    Bool,
    Int,
    String
};

enum class SettingEffect
{
    None,
    ConfigChanged,
    RestartReconnect,
    RestartNoReconnect,
    ClearSession
};

struct Setting
{
    const char* token;
    const char* label;
    const char* preference;
    SettingType type;
    int defaultValue;
    int minValue;
    int maxValue;
    int maxLength;
    const char* args;
    SettingEffect effect;
    bool needsPsram = false;
    int lowMemoryMax = 0;
    bool isGpio = false; // value is a GPIO number that must be available on this chip, -1 disables it
};
