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
        lib/pubsubclient/src/
)

file(GLOB SRCFILES
#        "Lib/FreeRTOS/src/*.c"
#        "Lib/FreeRTOS/src/*.cpp"
        Network.cpp
        Nuki.cpp
        MqttTopics.h
        WebCfgServer.cpp
        lib/ESP32_BLE_Arduino-1.0.1/src/*.cpp
        lib/ESP32_BLE_Arduino-1.0.1/src/*.h
        lib/WiFiManager/WiFiManager.cpp
        lib/Crc16/Crc16.h
        lib/nuki_ble/src/NukiBle.cpp
        lib/nuki_ble/src/NukiConstants.h
        lib/nuki_ble/src/NukiUtills.h
        include_directories(Lib/PubSubClient)
        lib/pubsubclient/src/PubSubClient.cpp
)

file(GLOB_RECURSE SRCFILESREC
        lib/NimBLE-Arduino/src/*.c
        lib/NimBLE-Arduino/src/*.cpp
        lib/NimBLE-Arduino/src/*.h
)

add_executable(${PROJECT_NAME}
        main.cpp
        ${SRCFILES}
        ${SRCFILESREC}
)

target_link_esp32_sdk(${PROJECT_NAME})

# Arduino.h is included in hello_world.cpp, so link with Arduino core
target_link_arduino_libraries(${PROJECT_NAME}
    PRIVATE
        core
        WiFi
        Update
        WebServer
        DNSServer
        Preferences
#        esp32
#        Wire
#        SPIFFS
#        FS
)

target_link_arduino_libraries(DNSServer PUBLIC WiFi)

# This is needed for the generation of HEX binary and uploading it
target_enable_arduino_upload(${PROJECT_NAME})

