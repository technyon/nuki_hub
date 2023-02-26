cmake_minimum_required(VERSION 3.0.0)

set(ARDUINO_BOARD "ESP32 Dev Module [esp32.esp32]")

project(nuki_hub CXX)

# ARDUHAL_LOG_LEVEL_NONE, define ARDUHAL_LOG_LEVEL_ERROR, define ARDUHAL_LOG_LEVEL_WARN, define ARDUHAL_LOG_LEVEL_INFO,
# define ARDUHAL_LOG_LEVEL_DEBUG, define ARDUHAL_LOG_LEVEL_VERBOSE

set(LOG_LEVEL ARDUHAL_LOG_LEVEL_NONE)

#add_compile_definitions(DEBUG_SENSE_NUKI)
#add_compile_definitions(DEBUG_NUKI_COMMAND)
#add_compile_definitions(DEBUG_NUKI_CONNECT)
#add_compile_definitions(DEBUG_NUKI_COMMUNICATION)
#add_compile_definitions(DEBUG_NUKI_HEX_DATA)
#add_compile_definitions(DEBUG_NUKI_READABLE_DATA)

# Length of char arrays to store certificates for MQTTS
add_compile_definitions(TLS_CA_MAX_SIZE=2200)
add_compile_definitions(TLS_CERT_MAX_SIZE=1500)
add_compile_definitions(TLS_KEY_MAX_SIZE=1800)
add_compile_definitions(TX_PAYLOAD_BUFFER_SIZE=6144)
add_compile_definitions(ESP_PLATFORM)
add_compile_definitions(ESP32)
add_compile_definitions(ARDUINO_ARCH_ESP32)

include_directories(${PROJECT_NAME}
        PRIVATE
        lib/Crc16
        lib/NimBLE-Arduino/src
        lib/BleScanner/src
        lib/nuki_ble/src
        lib/WiFiManager
        lib/WebServer/src
        lib/Ethernet/src
        lib/MqttLogger/src
        lib/espMqttClient/src
        lib/AsyncTCP/src
        lib/ArduinoJson/src
)

file(GLOB SRCFILES
        Pins.h
        Config.h
        Network.cpp
        MqttReceiver.h
        NetworkLock.cpp
        NetworkOpener.cpp
        networkDevices/NetworkDevice.h
        networkDevices/WifiDevice.cpp
        networkDevices/W5500Device.cpp
        networkDevices/EthLan8720Device.cpp
        networkDevices/ClientSyncW5500.cpp
        networkDevices/espMqttClientW5500.cpp
        networkDevices/espMqttClientWifi.cpp
        QueryCommand.h
        NukiWrapper.cpp
        NukiOpenerWrapper.cpp
        MqttTopics.h
        Ota.cpp
        WebCfgServerConstants.h
        WebCfgServer.cpp
        PresenceDetection.cpp
        PreferencesKeys.h
        Gpio.cpp
        Logger.cpp
        RestartReason.h
#        include/RTOS.h
        lib/WiFiManager/WiFiManager.cpp
        lib/WiFiManager/wm_consts_en.h
        lib/WiFiManager/wm_strings_en.h
        lib/Crc16/Crc16.h
        lib/nuki_ble/src/NukiBle.cpp
        lib/nuki_ble/src/NukiBle.hpp
        lib/nuki_ble/src/NukiLock.cpp
        lib/nuki_ble/src/NukiOpener.cpp
        lib/nuki_ble/src/NukiConstants.h
        lib/nuki_ble/src/NukiOpenerConstants.h
        lib/nuki_ble/src/NukiLockConstants.h
        lib/nuki_ble/src/NukiDataTypes.h
        lib/nuki_ble/src/NukiUtils.cpp
        lib/nuki_ble/src/NukiLockUtils.cpp
        lib/nuki_ble/src/NukiOpenerUtils.cpp
        lib/BleScanner/src/BleInterfaces.h
        lib/BleScanner/src/BleScanner.cpp
        lib/MqttLogger/src/MqttLogger.cpp
        lib/AsyncTCP/src/AsyncTCP.cpp
)

file(GLOB_RECURSE SRCFILESREC
        lib/NimBLE-Arduino/src/*.c
        lib/NimBLE-Arduino/src/*.cpp
        lib/NimBLE-Arduino/src/*.h
        lib/WebServer/src/*.cpp
        lib/WebServer/src/*.h
        lib/Ethernet/src/*.cpp
        lib/Ethernet/src/*.h
        lib/espMqttClient/src/*.cpp
        lib/espMqttClient/src/*.h
        lib/espMqttClient/src/Packets/*.cpp
        lib/espMqttClient/src/Packets/*.h
        lib/espMqttClient/src/Transport/*.cpp
        lib/espMqttClient/src/Transport/*.h
        lib/ArduinoJson/src/*.h
        lib/ArduinoJson/src/*.hpp
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
        BLE
        WiFi
        WiFiClientSecure
        Update
        DNSServer
        Preferences
        Ethernet
        SPI
)

target_link_arduino_libraries(DNSServer PUBLIC WiFi)

# This is needed for the generation of HEX binary and uploading it
target_enable_arduino_upload(${PROJECT_NAME})

