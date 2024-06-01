#pragma once

/**
 * @file BleScanner.h
 *
 * Created: 2022
 * License: GNU GENERAL PUBLIC LICENSE (see LICENSE)
 *
 * This library provides a BLE scanner to be used by other libraries to
 * receive advertisements from BLE devices
 *
 */

#include "Arduino.h"
#include <string>
#include <NimBLEDevice.h>
#include "BleInterfaces.h"

// Access to a globally available instance of BleScanner, created when first used
// Note that BLESCANNER.initialize() has to be called somewhere
#define BLESCANNER BleScanner::Scanner::instance()

namespace BleScanner {

class Scanner : public Publisher, BLEAdvertisedDeviceCallbacks {
  public:
    Scanner(int reservedSubscribers = 10);
    ~Scanner() = default;

    static Scanner& instance() {
      static Scanner* scanner = new Scanner(); // only initialized once on first call
      return *scanner;
    }

    /**
     * @brief Initializes the BLE scanner
     *
     * @param deviceName
     * @param wantDuplicates true if you want to receive advertisements from devices for which an advertisement was allready received within the same scan (scanduration)
     * @param interval Time in ms from the start of a window until the start of the next window (so the interval time = window + time to wait until next window)
     * @param window time in ms to scan
     *
     * The default (same) interval and window parameters means continuesly scanning without delay between windows
     */
    void initialize(const std::string& deviceName = "blescanner", const bool wantDuplicates = true, const uint16_t interval = 23, const uint16_t window = 23);

    /**
     * @brief starts the scan if not allready running, this should be called in loop() or a task;
     *
     */
    void update();

    /**
     * @brief Set the Scan Duration
     *
     * @param value scan duration in seconds, 0 for indefinite scan
     */
    void setScanDuration(const uint32_t value);

    /**
     * @brief enable/disable scanning
     *
     * @param enable
     */
    void enableScanning(bool enable);

    /**
     * @brief Subscribe to the scanner and receive results
     *
     * @param subscriber
     */
    void subscribe(Subscriber* subscriber) override;

    /**
     * @brief Un-Subscribe the scanner
     *
     * @param subscriber
     */
    void unsubscribe(Subscriber* subscriber) override;

    /**
     * @brief Forwards the scan result to the subcriber which has the onResult implemented
     *
     * @param advertisedDevice
     */
    void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;

  private:
    uint32_t scanDuration = 0; //default indefinite scanning time
    BLEScan* bleScan = nullptr;
    std::vector<Subscriber*> subscribers;
    uint16_t scanErrors = 0;
    bool scanningEnabled = true;
};

} // namespace BleScanner

