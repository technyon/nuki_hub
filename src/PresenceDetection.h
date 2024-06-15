#pragma once

#include <Preferences.h>
#include "BleScanner.h"
#include "BleInterfaces.h"
#include <memory>
#include <mutex>

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
    PresenceDetection(Preferences* preferences, BleScanner::Scanner* bleScanner, char* buffer, size_t bufferSize);
    virtual ~PresenceDetection();

    void initialize();
    char* generateCsv();
    bool enabled();

    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

private:
    void buildCsv(const std::shared_ptr<PdDevice>& device);

    std::mutex mtx;

    Preferences* _preferences;
    BleScanner::Scanner* _bleScanner;
    char* _csv = {0};
    size_t _bufferSize = 0;
    std::map<long long, std::shared_ptr<PdDevice>> _devices;
    int _timeout = 20000;
    int _csvIndex = 0;
};
