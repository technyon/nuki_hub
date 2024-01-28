#pragma once

enum class AccessLevel
{
    Full = 0,
    LockOnly = 1,
    ReadOnly = 2,
    LockAndUnlock = 3
};