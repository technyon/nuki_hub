#pragma once

#include "BleScanner.h"
#include "BleInterfaces.h"

struct PdDevice
{
    char address[18] = {0};
    char name[30] = {0};
    unsigned long timestamp;
};

class PresenceDetection : public BLEScannerSubscriber
{
public:
    PresenceDetection(BleScanner* bleScanner);
    virtual ~PresenceDetection();

    void initialize();
    void update();

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    BleScanner* _bleScanner;
    std::map<long long, PdDevice> _devices;
    uint _timeout = 20000;
};
