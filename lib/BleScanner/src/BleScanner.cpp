/**
 * @file BleScanner.cpp
 *
 * Created: 2022
 * License: GNU GENERAL PUBLIC LICENSE (see LICENSE)
 *
 * This library provides a BLE scanner to be used by other libraries to
 * receive advertisements from BLE devices
 *
 */

#include "BleScanner.h"
#include <NimBLEUtils.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>

namespace BleScanner {

Scanner::Scanner(int reservedSubscribers) {
  subscribers.reserve(reservedSubscribers);
}

void Scanner::initialize(const std::string& deviceName, const bool wantDuplicates, const uint16_t interval, const uint16_t window) {
  if (!BLEDevice::isInitialized()) {
    if (wantDuplicates) {
      // reduce memory footprint, cache is not used anyway
      #ifdef CONFIG_BTDM_BLE_SCAN_DUPL
      NimBLEDevice::setScanDuplicateCacheSize(10);
      #endif
    }
    BLEDevice::init(deviceName);
  }

  bleScan = BLEDevice::getScan();

  #ifndef BLESCANNER_USE_LATEST_NIMBLE
  bleScan->setAdvertisedDeviceCallbacks(this, wantDuplicates);
  #else
  bleScan->setScanCallbacks(this, wantDuplicates);
  #endif
  bleScan->setInterval(interval);
  bleScan->setWindow(window);
  bleScan->setActiveScan(false);
}

void Scanner::update() {
  if (!scanningEnabled || bleScan->isScanning()) {
    return;
  }

  if (scanDuration == 0) {
    // Avoid unbridled growth of results vector
    bleScan->setMaxResults(0);
  } else {
    log_w("Ble scanner max results not 0. Be aware of memory issue due to unbridled growth of results vector");
  }

  #ifndef BLESCANNER_USE_LATEST_NIMBLE
  bool result = bleScan->start(scanDuration, nullptr, false);
  #else
  bool result = bleScan->start(scanDuration * 1000, false);
  #endif
  // if (!result) {
  //   scanErrors++;
  //   if (scanErrors % 100 == 0) {
  //     log_w("BLE Scan error (100x)");
  //   }
  // }
}

void Scanner::enableScanning(bool enable) {
  scanningEnabled = enable;
  if (!enable) {
    bleScan->stop();
  }
}

void Scanner::setScanDuration(const uint32_t value) {
  scanDuration = value;
}

void Scanner::subscribe(Subscriber* subscriber) {
  if (std::find(subscribers.begin(), subscribers.end(), subscriber) != subscribers.end()) {
    return;
  }
  subscribers.push_back(subscriber);
}

void Scanner::unsubscribe(Subscriber* subscriber) {
  auto it = std::find(subscribers.begin(), subscribers.end(), subscriber);
  if (it != subscribers.end()) {
    subscribers.erase(it);
  }
}

void Scanner::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
  for (const auto& subscriber : subscribers) {
    subscriber->onResult(advertisedDevice);
  }
}

void Scanner::whitelist(BLEAddress bleAddress) {
  BLEDevice::whiteListAdd(bleAddress);   
  bleScan->setFilterPolicy(BLE_HCI_SCAN_FILT_USE_WL);
}

} // namespace BleScanner