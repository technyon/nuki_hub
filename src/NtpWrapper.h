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
    void initialize(String timeserver, NetworkDeviceType networkDeviceType);
    void enable();

private:
    static void cbSyncTime(struct timeval *tv);
};
