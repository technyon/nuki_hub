cmake_minimum_required(VERSION 3.0.0)

set(ARDUINO_BOARD "ESP32 Dev Module [esp32.esp32]")

project(nuki_hub CXX)

include_directories(${PROJECT_NAME}
        PRIVATE
        lib/Crc16
        lib/NimBLE-Arduino/src
        lib/nuki_ble/src
        lib/ESP32_BLE_Arduino-1.0.1/src
        lib/WiFiManager
        lib/pubsubclient/src
        lib/EthernetWebServer/src
)

file(GLOB SRCFILES
        Pins.h
        Network.cpp
        networkDevices/NetworkDevice.h
        networkDevices/WifiDevice.cpp
        networkDevices/W5500Device.cpp
        NukiWrapper.cpp
        MqttTopics.h
        WebCfgServer.cpp
        PresenceDetection.cpp
        PreferencesKeys.h
        SpiffsCookie.cpp
        Version.h
        lib/ESP32_BLE_Arduino-1.0.1/src/*.cpp
        lib/ESP32_BLE_Arduino-1.0.1/src/*.h
        lib/WiFiManager/WiFiManager.cpp
        lib/Crc16/Crc16.h
        lib/nuki_ble/src/NukiBle.cpp
        lib/nuki_ble/src/NukiConstants.h
        lib/nuki_ble/src/NukiDataTypes.h
        lib/nuki_ble/src/NukiUtils.cpp
        lib/nuki_ble/src/BleScanner.cpp
        lib/pubsubclient/src/PubSubClient.cpp
)

file(GLOB_RECURSE SRCFILESREC
        lib/NimBLE-Arduino/src/*.c
        lib/NimBLE-Arduino/src/*.cpp
        lib/NimBLE-Arduino/src/*.h
        lib/EthernetWebServer/src/*.cpp
        lib/EthernetWebServer/src/*.h
)

add_executable(${PROJECT_NAME}
        main.cpp
        ${SRCFILES}
        ${SRCFILESREC}
)

# Arduino.h is included in hello_world.cpp, so link with Arduino core
target_link_arduino_libraries(${PROJECT_NAME}
    PRIVATE
        core
        WiFi
        Update
        WebServer
        DNSServer
        Preferences
        SPIFFS
        Ethernet
#        esp32
#        Wire
#        FS
)

target_link_arduino_libraries(DNSServer PUBLIC WiFi)

# This is needed for the generation of HEX binary and uploading it
target_enable_arduino_upload(${PROJECT_NAME})

