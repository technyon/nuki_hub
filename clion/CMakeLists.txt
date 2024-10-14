cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(nukihub)

add_compile_definitions(CONFIG_IDF_TARGET_ESP32)
add_compile_definitions(NUKI_64BIT_TIME)

set(SRCFILES
        ../src/Config.h
        ../src/NukiDeviceId.cpp
        ../src/CharBuffer.cpp
        ../src/NukiNetwork.cpp
        ../src/MqttReceiver.h
        ../src/NukiNetworkLock.cpp
        ../src/NukiNetworkOpener.cpp
        ../src/networkDevices/NetworkDevice.h
        ../src/networkDevices/NetworkDevice.cpp
        ../src/networkDevices/LAN8720Definitions.h
        ../src/networkDevices/DM9051Definitions.h
        ../src/networkDevices/W5500Definitions.h
        ../src/networkDevices/WifiDevice.cpp
        ../src/networkDevices/EthernetDevice.cpp
        ../src/networkDevices/IPConfiguration.cpp
        ../src/LockActionResult.h
        ../src/QueryCommand.h
        ../src/NukiWrapper.cpp
        ../src/NukiOpenerWrapper.cpp
        ../src/MqttTopics.h
        ../src/WebCfgServerConstants.h
        ../src/WebCfgServer.cpp
        ../src/PreferencesKeys.h
        ../src/Gpio.cpp
        ../src/Logger.cpp
        ../src/RestartReason.h
        #        include/RTOS.h
        ../lib/WiFiManager/WiFiManager.cpp
        ../lib/WiFiManager/wm_consts_en.h
        ../lib/WiFiManager/wm_strings_en.h
        ../lib/nuki_ble/src/NukiBle.cpp
        ../lib/nuki_ble/src/NukiBle.hpp
        ../lib/nuki_ble/src/NukiLock.cpp
        ../lib/nuki_ble/src/NukiOpener.cpp
        ../lib/nuki_ble/src/NukiConstants.h
        ../lib/nuki_ble/src/NukiOpenerConstants.h
        ../lib/nuki_ble/src/NukiLockConstants.h
        ../lib/nuki_ble/src/NukiDataTypes.h
        ../lib/nuki_ble/src/NukiUtils.cpp
        ../lib/nuki_ble/src/NukiLockUtils.cpp
        ../lib/nuki_ble/src/NukiOpenerUtils.cpp
        ../lib/BleScanner/src/BleInterfaces.h
        ../lib/BleScanner/src/BleScanner.cpp
        ../lib/MqttLogger/src/MqttLogger.cpp
        ../src/util/NetworkUtil.cpp
        ../src/enums/NetworkDeviceType.h
        ../src/util/NetworkDeviceInstantiator.cpp
        ../src/NukiOfficial.cpp
        ../src/NukiPublisher.cpp
)

file(GLOB_RECURSE SRCFILESREC
        lib/NimBLE-Arduino/src/*.c
        lib/NimBLE-Arduino/src/*.cpp
        lib/NimBLE-Arduino/src/*.h
        lib/ArduinoJson/src/*.h
        lib/ArduinoJson/src/*.hpp
)

add_executable(dummy
        ../src/main.cpp
        ${SRCFILES}
        ${SRCFILESREC}
)