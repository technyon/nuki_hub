cmake_minimum_required(VERSION 3.0.0)

set(ARDUINO_BOARD "ESP32 Dev Module [esp32.esp32]")

project(nuki_hub CXX)

# ARDUHAL_LOG_LEVEL_NONE, define ARDUHAL_LOG_LEVEL_ERROR, define ARDUHAL_LOG_LEVEL_WARN, define ARDUHAL_LOG_LEVEL_INFO,
# define ARDUHAL_LOG_LEVEL_DEBUG, define ARDUHAL_LOG_LEVEL_VERBOSE

set(LOG_LEVEL ARDUHAL_LOG_LEVEL_NONE)

include_directories(${PROJECT_NAME}
        PRIVATE
        lib/Crc16
        lib/NimBLE-Arduino/src
        lib/BleScanner/src
        lib/nuki_ble/src
        lib/ESP32_BLE_Arduino-1.0.1/src
        lib/WiFiManager
        lib/pubsubclient/src
        lib/WebServer/src
        include
)

file(GLOB SRCFILES
#        "Lib/FreeRTOS/src/*.c"
#        "Lib/FreeRTOS/src/*.cpp"
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
        include/RTOS.h
        lib/ESP32_BLE_Arduino-1.0.1/src/*.cpp
        lib/ESP32_BLE_Arduino-1.0.1/src/*.h
        lib/WiFiManager/WiFiManager.cpp
        lib/Crc16/Crc16.h
        lib/nuki_ble/src/NukiBle.cpp
        lib/nuki_ble/src/NukiConstants.h
        lib/nuki_ble/src/NukiDataTypes.h
        lib/nuki_ble/src/NukiUtils.cpp
        lib/BleScanner/src/BleInterfaces.h
        lib/BleScanner/src/BleScanner.cpp
        lib/pubsubclient/src/PubSubClient.cpp
)

file(GLOB_RECURSE SRCFILESREC
        lib/NimBLE-Arduino/src/*.c
        lib/NimBLE-Arduino/src/*.cpp
        lib/NimBLE-Arduino/src/*.h
        lib/WebServer/src/*.cpp
        lib/WebServer/src/*.h
)

add_executable(${PROJECT_NAME}
        main.cpp
        ${SRCFILES}
        ${SRCFILESREC}
)

target_compile_definitions(${PROJECT_NAME}
        PRIVATE
        ARDUHAL_LOG_LEVEL=${LOG_LEVEL}
        CORE_DEBUG_LEVEL=${LOG_LEVEL}
        CONFIG_NIMBLE_CPP_LOG_LEVEL=0
        )

target_link_arduino_libraries(${PROJECT_NAME}
    PRIVATE
        core
        WiFi
        Update
#        WebServer
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

