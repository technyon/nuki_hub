#pragma once
#include "Arduino.h"

#include "enums/NetworkDeviceType.h"
#include "esp_netif_sntp.h"
#include <time.h>
#include <sys/time.h>

class String;

class NtpWrapper
{
public:
    NtpWrapper(String timeserver, NetworkDeviceType networkDeviceType);
    void initialize();

    bool isTimeSynced();

private:
    static void cbSyncTime(struct timeval *tv);
    static bool timeSynced;
    String timeserver;
    NetworkDeviceType networkDeviceType;
};
