#pragma once

#include "BleScanner.h"
#include "BleInterfaces.h"
#include "Network.h"

struct PdDevice
{
    char address[18] = {0};
    char name[37] = {0};
    unsigned long timestamp = 0;
    int rssi = 0;
    bool hasRssi = false;
};

class PresenceDetection : public BleScanner::Subscriber
{
public:
    PresenceDetection(Preferences* preferences, BleScanner::Scanner* bleScanner, Network* network, char* buffer, size_t bufferSize);
    virtual ~PresenceDetection();

    void initialize();
    void update();

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    void buildCsv(const PdDevice& device);

    Preferences* _preferences;
    BleScanner::Scanner* _bleScanner;
    Network* _network;
    char* _csv = {0};
    size_t _bufferSize = 0;
    std::map<long long, PdDevice> _devices;
    int _timeout = 20000;
    int _csvIndex = 0;
};
