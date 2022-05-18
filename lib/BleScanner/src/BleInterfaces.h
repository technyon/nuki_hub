#pragma once

/**
 * @file BleInterfaces.h
 *
 * Created: 2022
 * License: GNU GENERAL PUBLIC LICENSE (see LICENSE)
 *
 * This library provides a BLE scanner to be used by other libraries to
 * receive advertisements from BLE devices
 *
 */

#include <NimBLEDevice.h>

namespace BleScanner {


class Subscriber {
  public:
    virtual void onResult(NimBLEAdvertisedDevice* advertisedDevice) = 0;
};

class Publisher {
  public:
    virtual void subscribe(Subscriber* subscriber) = 0;
    virtual void unsubscribe(Subscriber* subscriber) = 0;
    virtual void enableScanning(bool enable) = 0;
};

} // namespace BleScanner
