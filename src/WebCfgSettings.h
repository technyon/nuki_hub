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
};
