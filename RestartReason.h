#pragma once

enum class RestartReason
{
    RequestedViaMqtt,
    BLEBeaconWatchdog,
    RestartOnDisconnectWatchdog,
    RestartIntervalWatchdog,
    NetworkTimeoutWatchdog,
    WifiInitFailed,
    ReconfigureWifi,
    NetworkDeviceCriticalFailure,
    ConfigurationUpdated,
    RestartTimer,
    OTATimeout,
    DeviceUnpaired
};

#define RESTART_REASON_VALID_DETECT 0xa00ab00bc00bd00d;

extern int restartReason;
extern uint64_t restartReasonValid;

inline static void restartEsp(RestartReason reason)
{
    restartReason = (int)reason;
    restartReasonValid = RESTART_REASON_VALID_DETECT;
    ESP.restart();
}

inline static String getRestartReasion()
{
    uint64_t cmp = RESTART_REASON_VALID_DETECT;
    if(restartReasonValid != cmp)
    {
        return "UnknownNoRestartRegistered";
    }

    switch((RestartReason)restartReason)
    {
        case RestartReason::RequestedViaMqtt:
            return "RequestedViaMqtt";
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
        case RestartReason::NetworkDeviceCriticalFailure:
            return "NetworkDeviceCriticalFailure";
        case RestartReason::ConfigurationUpdated:
            return "ConfigurationUpdated";
        case RestartReason::RestartTimer:
            return "RestartTimer";
        case RestartReason::OTATimeout:
            return "OTATimeout";
        case RestartReason::DeviceUnpaired:
            return "DeviceUnpaired";
        default:
            return "Unknown: " + restartReason;
    }
}