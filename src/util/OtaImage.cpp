#include "OtaImage.h"
#include "sdkconfig.h"
#include "../PreferencesKeys.h"
#include "esp_chip_info.h"
#include "esp_efuse.h"
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#endif
#if defined(CONFIG_IDF_TARGET_ESP32)
#include "bootloader_common.h"
#include "soc/efuse_reg.h"
#endif

OtaImage OtaImages::current()
{
#if defined(NUKI_TARGET_P4_C5)
#if defined(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
    return OtaImage::Esp32P4C5;
#else
    return OtaImage::Esp32P4C5V3;
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
#if defined(CONFIG_ESP32P4_SELECTS_REV_LESS_V3)
    return OtaImage::Esp32P4;
#else
    return OtaImage::Esp32P4V3;
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
    return OtaImage::Esp32C3;
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#if defined(CONFIG_SPIRAM_MODE_OCT)
    return OtaImage::Esp32S3Oct;
#elif defined(CONFIG_SPIRAM)
    return OtaImage::Esp32S3;
#else
    return OtaImage::Esp32S3NoPsram;
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
    return OtaImage::Esp32C5;
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
    return OtaImage::Esp32C6;
#elif defined(CONFIG_IDF_TARGET_ESP32C61)
    return OtaImage::Esp32C61;
#elif defined(CONFIG_IDF_TARGET_ESP32H2)
    return OtaImage::Esp32H2;
#elif defined(CONFIG_FREERTOS_UNICORE)
    return OtaImage::Esp32Solo1;
#elif defined(NUKI_TARGET_GL_S10)
    return OtaImage::Esp32GlS10;
#elif defined(CONFIG_SPIRAM)
    return OtaImage::Esp32;
#else
    return OtaImage::Esp32NoPsram;
#endif
}

const char* OtaImages::name(OtaImage image)
{
    switch(image)
    {
    case OtaImage::Esp32: return "esp32";
    case OtaImage::Esp32NoPsram: return "esp32nopsram";
    case OtaImage::Esp32GlS10: return "esp32gls10";
    case OtaImage::Esp32Solo1: return "esp32-solo1";
    case OtaImage::Esp32C3: return "esp32c3";
    case OtaImage::Esp32S3: return "esp32s3";
    case OtaImage::Esp32S3Oct: return "esp32s3oct";
    case OtaImage::Esp32S3NoPsram: return "esp32s3nopsram";
    case OtaImage::Esp32C5: return "esp32c5";
    case OtaImage::Esp32C6: return "esp32c6";
    case OtaImage::Esp32C61: return "esp32c61";
    case OtaImage::Esp32H2: return "esp32h2";
    case OtaImage::Esp32P4: return "esp32p4";
    case OtaImage::Esp32P4V3: return "esp32p4v3";
    case OtaImage::Esp32P4C5: return "esp32p4c5";
    case OtaImage::Esp32P4C5V3: return "esp32p4c5v3";
    default: return "";
    }
}

const char* OtaImages::label(OtaImage image)
{
    switch(image)
    {
    case OtaImage::Esp32: return "ESP32 with PSRAM";
    case OtaImage::Esp32NoPsram: return "ESP32 without PSRAM";
    case OtaImage::Esp32GlS10: return "GL-S10";
    case OtaImage::Esp32Solo1: return "ESP32-SOLO1";
    case OtaImage::Esp32C3: return "ESP32-C3";
    case OtaImage::Esp32S3: return "ESP32-S3 Quad PSRAM";
    case OtaImage::Esp32S3Oct: return "ESP32-S3 Octal PSRAM";
    case OtaImage::Esp32S3NoPsram: return "ESP32-S3 without PSRAM";
    case OtaImage::Esp32C5: return "ESP32-C5";
    case OtaImage::Esp32C6: return "ESP32-C6";
    case OtaImage::Esp32C61: return "ESP32-C61";
    case OtaImage::Esp32H2: return "ESP32-H2";
    case OtaImage::Esp32P4: return "ESP32-P4 (C6)";
    case OtaImage::Esp32P4V3: return "ESP32-P4 v3 (C6)";
    case OtaImage::Esp32P4C5: return "ESP32-P4 (C5)";
    case OtaImage::Esp32P4C5V3: return "ESP32-P4 v3 (C5)";
    default: return "Unknown";
    }
}

OtaImage OtaImages::recommended(bool glS10EthernetInUse, String& reason)
{
    OtaImage cur = current();
    esp_chip_info_t chip;
    esp_chip_info(&chip);
    reason = "";

#if defined(CONFIG_IDF_TARGET_ESP32S3)
#if defined(CONFIG_SPIRAM)
    if(esp_psram_is_initialized() && esp_psram_get_size() > 0)
    {
        reason = "the PSRAM of this chip is in use";
        return cur;
    }
#endif
    uint32_t word4 = esp_efuse_read_reg(EFUSE_BLK1, 4);
    uint32_t word5 = esp_efuse_read_reg(EFUSE_BLK1, 5);
    int psramCap = (((word5 >> 19) & 1) << 2) | ((word4 >> 3) & 3);
    switch(psramCap)
    {
    case 0:
        reason = "the chip's eFuse reports no embedded PSRAM";
        return OtaImage::Esp32S3NoPsram;
    case 2:
    case 4:
        reason = String("the chip's eFuse reports ") + (psramCap == 2 ? "2" : "4") + " MB quad PSRAM, which the current image cannot use";
        return OtaImage::Esp32S3;
    case 1:
    case 3:
        reason = String("the chip's eFuse reports ") + (psramCap == 1 ? "8" : "16") + " MB octal PSRAM, which the current image cannot use";
        return OtaImage::Esp32S3Oct;
    default:
        return OtaImage::Unknown;
    }
#elif defined(CONFIG_IDF_TARGET_ESP32)
    if(chip.cores == 1)
    {
        reason = "the chip has a single core";
        return OtaImage::Esp32Solo1;
    }
    if(glS10EthernetInUse)
    {
        reason = "the GL-S10 Ethernet interface is in use";
        return OtaImage::Esp32GlS10;
    }
    uint32_t package = bootloader_common_get_chip_ver_pkg();
    bool embeddedPsram = package == EFUSE_RD_CHIP_VER_PKG_ESP32PICOV302 || package == EFUSE_RD_CHIP_VER_PKG_ESP32D0WDR2V3;
#if defined(CONFIG_SPIRAM)
    if(esp_psram_is_initialized() && esp_psram_get_size() > 0)
    {
        reason = "PSRAM was found";
        return OtaImage::Esp32;
    }
    if(embeddedPsram)
    {
        reason = "the chip package has embedded PSRAM";
        return OtaImage::Esp32;
    }
    reason = "no PSRAM was found";
    return OtaImage::Esp32NoPsram;
#else
    if(embeddedPsram)
    {
        reason = "the chip package has embedded PSRAM, which the current image cannot use";
        return OtaImage::Esp32;
    }
    return OtaImage::Unknown;
#endif
#elif defined(CONFIG_IDF_TARGET_ESP32P4)
    bool v3 = chip.revision >= 300;
    reason = String("the chip is revision ") + (chip.revision / 100) + "." + (chip.revision % 100);
    if(cur == OtaImage::Esp32P4C5 || cur == OtaImage::Esp32P4C5V3)
    {
        return v3 ? OtaImage::Esp32P4C5V3 : OtaImage::Esp32P4C5;
    }
    return v3 ? OtaImage::Esp32P4V3 : OtaImage::Esp32P4;
#else
    return cur;
#endif
}

bool OtaImages::keepingCurrent(Preferences* preferences, OtaImage recommendedImage)
{
    return recommendedImage != OtaImage::Unknown && preferences->getString(preference_ota_image_keep, "") == name(recommendedImage);
}

OtaImage OtaImages::updateTarget(Preferences* preferences, bool glS10EthernetInUse)
{
    String reason;
    OtaImage cur = current();
    OtaImage rec = recommended(glS10EthernetInUse, reason);
    if(rec == OtaImage::Unknown || rec == cur || keepingCurrent(preferences, rec))
    {
        return cur;
    }
    return rec;
}

String OtaImages::url(const char* channel, OtaImage image, bool updater)
{
    String result = "https://raw.githubusercontent.com/technyon/nuki_hub/binary/ota/";
    if(strcmp(channel, "beta") == 0)
    {
        result += "beta/";
    }
    else if(strcmp(channel, "master") == 0)
    {
        result += "master/";
    }
    result += updater ? "nuki_hub_updater_" : "nuki_hub_";
    result += name(image);
    result += ".bin";
    return result;
}
