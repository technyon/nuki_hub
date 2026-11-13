#pragma once

#include <Arduino.h>
#include <Preferences.h>

enum class OtaImage
{
    Unknown,
    Esp32,
    Esp32NoPsram,
    Esp32GlS10,
    Esp32Solo1,
    Esp32C3,
    Esp32S3,
    Esp32S3Oct,
    Esp32S3NoPsram,
    Esp32C5,
    Esp32C6,
    Esp32C61,
    Esp32H2,
    Esp32P4,
    Esp32P4V3,
    Esp32P4C5,
    Esp32P4C5V3
};

namespace OtaImages
{
    OtaImage current();
    const char* name(OtaImage image);
    const char* label(OtaImage image);
    OtaImage recommended(bool glS10EthernetInUse, String& reason);
    bool keepingCurrent(Preferences* preferences, OtaImage recommendedImage);
    OtaImage updateTarget(Preferences* preferences, bool glS10EthernetInUse);
    String url(const char* channel, OtaImage image, bool updater);
}
