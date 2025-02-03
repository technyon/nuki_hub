#pragma once

enum class NukiPinState
{
    NotSet = 0,
    Valid = 1,
    Invalid = 2,
    NotConfigured = 4 // default value used for preferences.getInt() when not configured by user
};