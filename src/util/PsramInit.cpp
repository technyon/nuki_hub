#include "PsramInit.h"
#include "sdkconfig.h"

#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
#include "esp_psram.h"
#include "esp_private/esp_psram_extram.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static PsramInitState state = PsramInitState::NotConfigured;
static esp_err_t lateInitError = ESP_OK;

void psramLateInit()
{
#if defined(CONFIG_SOC_SPIRAM_SUPPORTED) && defined(CONFIG_SPIRAM)
    if(esp_psram_is_initialized())
    {
        state = PsramInitState::InitialisedAtBoot;
        return;
    }

    vTaskDelay(pdMS_TO_TICKS(100));

    lateInitError = esp_psram_init();
    if(lateInitError != ESP_OK)
    {
        state = PsramInitState::Failed;
        return;
    }

    if(!esp_psram_extram_test())
    {
        lateInitError = ESP_ERR_INVALID_RESPONSE;
        state = PsramInitState::Failed;
        return;
    }

    lateInitError = esp_psram_extram_add_to_heap_allocator();
    state = lateInitError == ESP_OK ? PsramInitState::InitialisedLate : PsramInitState::Failed;
#endif
}

PsramInitState psramInitState()
{
    return state;
}

esp_err_t psramLateInitError()
{
    return lateInitError;
}
