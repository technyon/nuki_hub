#pragma once

#include <Arduino.h>
#include <string.h>
#include "../Logger.h"

// Copies a (preference-sourced) string into a fixed-size buffer, always
// null-terminated and truncated if needed. Configuration values are length
// limited only client-side in the web UI and not at all on config import, so an
// unbounded copy would corrupt memory on every boot.
inline void copyBounded(char* dest, const String& src, size_t destSize, const char* what)
{
    if (strlcpy(dest, src.c_str(), destSize) >= destSize)
    {
        Log->printf("Warning: %s too long (%u chars, max %u), truncated\n", what, (unsigned)src.length(), (unsigned)(destSize - 1));
    }
}
