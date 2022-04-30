#include "BleScanner.h"
#include <NimBLEUtils.h>
#include <NimBLEScan.h>
#include <NimBLEAdvertisedDevice.h>

BleScanner::BleScanner(int reservedSubscribers) {
  subscribers.reserve(reservedSubscribers);
}

void BleScanner::initialize(const std::string& deviceName, const bool wantDuplicates, const uint16_t interval, const uint16_t window) {
  if (!BLEDevice::getInitialized()) {
    BLEDevice::init(deviceName);
  }
  bleScan = BLEDevice::getScan();
  bleScan->setAdvertisedDeviceCallbacks(this, wantDuplicates);
  bleScan->setActiveScan(true);
  bleScan->setInterval(interval);
  bleScan->setWindow(window);
}

void BleScanner::update() {
  if (bleScan->isScanning()) {
    return;
  }
  bool result = bleScan->start(scanDuration, nullptr, false);
  if (!result) {
    log_w("BLE Scan error");
  }
}

void BleScanner::setScanDuration(const uint32_t value) {
  scanDuration = value;
}

void BleScanner::subscribe(BLEScannerSubscriber* subscriber) {
  if (std::find(subscribers.begin(), subscribers.end(), subscriber) != subscribers.end()) {
    return;
  }
  subscribers.push_back(subscriber);
}

void BleScanner::unsubscribe(BLEScannerSubscriber* subscriber) {
  auto it = std::find(subscribers.begin(), subscribers.end(), subscriber);
  if (it != subscribers.end()) {
    subscribers.erase(it);
  }
}

void BleScanner::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
  for (const auto& subscriber : subscribers) {
    subscriber->onResult(advertisedDevice);
  }
}

