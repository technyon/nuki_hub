list(APPEND app_sources ${CMAKE_SOURCE_DIR}/src/main.cpp)
list(APPEND app_sources ${CMAKE_SOURCE_DIR}/src/Config.h)
list(APPEND app_sources ../../src/Logger.h)
list(APPEND app_sources ../../src/NukiNetwork.h)
list(APPEND app_sources ../../src/PreferencesKeys.h)
list(APPEND app_sources ../../src/RestartReason.h)
list(APPEND app_sources ../../src/WebCfgServer.h)
list(APPEND app_sources ../../src/WebCfgServerConstants.h)

list(APPEND app_sources ../../src/Logger.cpp)
list(APPEND app_sources ../../src/NukiNetwork.cpp)
list(APPEND app_sources ../../src/WebCfgServer.cpp)

list(APPEND app_sources ../../src/enums/NetworkDeviceType.h)
list(APPEND app_sources ../../src/networkDevices/EthernetDevice.h)
list(APPEND app_sources ../../src/networkDevices/IPConfiguration.h)
list(APPEND app_sources ../../src/networkDevices/NetworkDevice.h)
list(APPEND app_sources ../../src/util/NetworkUtil.cpp)
list(APPEND app_sources ../../src/util/NetworkDeviceInstantiator.cpp)

if(NOT DEFINED NUKI_TARGET_H2)
  list(APPEND app_sources ../../src/networkDevices/WifiDevice.h)
endif()

list(APPEND app_sources ../../src/networkDevices/EthernetDevice.cpp)
list(APPEND app_sources ../../src/networkDevices/IPConfiguration.cpp)
list(APPEND app_sources ../../src/networkDevices/NetworkDevice.cpp)

if(NOT DEFINED NUKI_TARGET_H2)
  list(APPEND app_sources ../../src/networkDevices/WifiDevice.cpp)
endif()

idf_component_register(SRCS ${app_sources})
