#pragma once
#include <NimBLEDevice.h>

class BLEScannerSubscriber {
  public:
    virtual void onResult(NimBLEAdvertisedDevice* advertisedDevice) = 0;
};

class BLEScannerPublisher {
  public:
    virtual void subscribe(BLEScannerSubscriber* subscriber) = 0;
    virtual void unsubscribe(BLEScannerSubscriber* subscriber) = 0;
};
