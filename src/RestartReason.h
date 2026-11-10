#pragma once

#include "RtcState.h"

enum class RestartReason
{
    RequestedViaMqtt,
    RequestedViaWebServer,
    RequestedViaSerial,
    BLEError,
    BLEBeaconWatchdog,
    RestartOnDisconnectWatchdog,
    RestartIntervalWatchdog,
    NetworkTimeoutWatchdog,
    WifiInitFailed,
    ReconfigureWifi,
    ReconfigureETH,
    NetworkDeviceCriticalFailure,
    NetworkDeviceCriticalFailureNoWifiFallback,
    ConfigurationUpdated,
    GpioConfigurationUpdated,
    RestartTimer,
    OTACompleted,
    OTATimeout,
    OTAAborted,
    OTAUnknownState,
    OTAReboot,
    ImportCompleted,
    DeviceUnpaired,
    NukiHubReset,
    ReconfigureWebServer,
    DisableNetworkIfNotConnected,
    RestartServices,
    NotApplicable,
};

#define RESTART_REASON_VALID_DETECT 0xa00ab00bc00bd00d;

// Aliases for fields of rtcState (RtcState.h).
extern int& restartReason;
extern uint64_t& restartReasonValidDetect;
extern bool& rebuildGpioRequested;
extern RestartReason currentRestartReason;
extern bool restartReason_isValid;

inline static void restartEsp(RestartReason reason)
{
    if(reason == RestartReason::GpioConfigurationUpdated)
    {
        rebuildGpioRequested = true;
    }
    restartReason = (int)reason;
    restartReasonValidDetect = RESTART_REASON_VALID_DETECT;
    ESP.restart();
}

inline static void initializeRestartReason()
{
    uint64_t cmp = RESTART_REASON_VALID_DETECT;
    restartReason_isValid = (restartReasonValidDetect == cmp);
    if(restartReason_isValid)
    {
        currentRestartReason = (RestartReason)restartReason;
        memset(&restartReasonValidDetect, 0, sizeof(restartReasonValidDetect));
    }
    else
    {
        rebuildGpioRequested = false;
    }
}

inline static String restartReasonToString(RestartReason reason)
{
    switch(reason)
    {
    case RestartReason::RequestedViaMqtt:
        return "RequestedViaMqtt";
    case RestartReason::RequestedViaWebServer:
        return "RequestedViaWebServer";
    case RestartReason::RequestedViaSerial:
        return "RequestedViaSerial";
    case RestartReason::ReconfigureWebServer:
        return "ReconfigureWebServer";
    case RestartReason::BLEError:
        return "BLEError";
    case RestartReason::BLEBeaconWatchdog:
        return "BLEBeaconWatchdog";
    case RestartReason::RestartOnDisconnectWatchdog:
        return "RestartOnDisconnectWatchdog";
    case RestartReason::RestartIntervalWatchdog:
        return "RestartIntervalWatchdog";
    case RestartReason::NetworkTimeoutWatchdog:
        return "NetworkTimeoutWatchdog";
    case RestartReason::WifiInitFailed:
        return "WifiInitFailed";
    case RestartReason::ReconfigureWifi:
        return "ReconfigureWifi";
    case RestartReason::ReconfigureETH:
        return "ReconfigureETH";
    case RestartReason::NetworkDeviceCriticalFailure:
        return "NetworkDeviceCriticalFailure";
    case RestartReason::NetworkDeviceCriticalFailureNoWifiFallback:
        return "NetworkDeviceCriticalFailureNoWifiFallback";
    case RestartReason::ConfigurationUpdated:
        return "ConfigurationUpdated";
    case RestartReason::GpioConfigurationUpdated:
        return "GpioConfigurationUpdated";
    case RestartReason::RestartTimer:
        return "RestartTimer";
    case RestartReason::OTACompleted:
        return "OTACompleted";
    case RestartReason::OTATimeout:
        return "OTATimeout";
    case RestartReason::OTAAborted:
        return "OTAAborted";
    case RestartReason::OTAUnknownState:
        return "OTAUnknownState";
    case RestartReason::OTAReboot:
        return "RebootToOTA";
    case RestartReason::ImportCompleted:
        return "ConfigImportCompleted";
    case RestartReason::DeviceUnpaired:
        return "DeviceUnpaired";
    case RestartReason::NukiHubReset:
        return "NukiHubFactoryReset";
    case RestartReason::DisableNetworkIfNotConnected:
        return "NetworkDisabledOnNotConnected";
    case RestartReason::NotApplicable:
        return "NotApplicable";
    case RestartReason::RestartServices:
        return "RestartServices";
    default:
        return "Unknown: " + String((int)reason);
    }
}

inline static String getRestartReason()
{
    return restartReasonToString(currentRestartReason);
}

inline static String espResetReasonToString(esp_reset_reason_t reason)
{
    switch(reason)
    {
    case esp_reset_reason_t::ESP_RST_UNKNOWN:
        return "ESP_RST_UNKNOWN: Reset reason can not be determined.";
    case esp_reset_reason_t::ESP_RST_POWERON:
        return "ESP_RST_POWERON: Reset due to power-on event.";
    case esp_reset_reason_t::ESP_RST_EXT:
        return "ESP_RST_EXT: Reset by external pin";
    case esp_reset_reason_t::ESP_RST_SW:
        return "ESP_RST_SW: Software reset via esp_restart.";
    case esp_reset_reason_t::ESP_RST_PANIC:
        return "ESP_RST_PANIC: Software reset due to exception/panic.";
    case esp_reset_reason_t::ESP_RST_INT_WDT:
        return "ESP_RST_INT_WDT: Reset (software or hardware) due to interrupt watchdog";
    case esp_reset_reason_t::ESP_RST_TASK_WDT:
        return "ESP_RST_TASK_WDT: Reset due to task watchdog.";
    case esp_reset_reason_t::ESP_RST_WDT:
        return "ESP_RST_WDT: Reset due to other watchdogs.";
    case esp_reset_reason_t::ESP_RST_DEEPSLEEP:
        return "ESP_RST_DEEPSLEEP: Reset after exiting deep sleep mode.";
    case esp_reset_reason_t::ESP_RST_BROWNOUT:
        return "ESP_RST_BROWNOUT: Brownout reset (software or hardware)";
    case esp_reset_reason_t::ESP_RST_SDIO:
        return "ESP_RST_SDIO: Reset over SDIO.";
    default:
        return "Unknown: " + String((int)reason);
    }
}

inline static String getEspRestartReason()
{
    return espResetReasonToString(esp_reset_reason());
}

inline static void recordRestartHistory()
{
    if(rtcState.restartHistoryValidDetect != RESTART_HISTORY_VALID_DETECT)
    {
        memset(rtcState.restartHistory, 0, sizeof(rtcState.restartHistory));
        rtcState.currentUptimeMinutes = 0;
        rtcState.restartHistoryValidDetect = RESTART_HISTORY_VALID_DETECT;
    }
    for(int i = RESTART_HISTORY_SIZE - 1; i > 0; i--)
    {
        rtcState.restartHistory[i] = rtcState.restartHistory[i - 1];
    }
    rtcState.restartHistory[0].fwReason = (uint8_t)(restartReason_isValid ? currentRestartReason : RestartReason::NotApplicable);
    rtcState.restartHistory[0].espReason = (uint8_t)esp_reset_reason();
    rtcState.restartHistory[0].uptimeMinutes = rtcState.currentUptimeMinutes;
    rtcState.currentUptimeMinutes = 0;
}

#ifndef NUKI_HUB_UPDATER
inline bool rebuildGpio()
{
    bool rebGpio = rebuildGpioRequested;
    rebuildGpioRequested = false;
    return restartReason_isValid && rebGpio;
}
#endif