# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources ${CMAKE_SOURCE_DIR}/src/*.*)
if(DEFINED NUKI_TARGET_H2)
  list(REMOVE_ITEM app_sources "${CMAKE_SOURCE_DIR}/src/networkDevices/WifiDevice.cpp")
  list(REMOVE_ITEM app_sources "${CMAKE_SOURCE_DIR}/src/networkDevices/WifiDevice.h")
endif()
idf_component_register(SRCS ${app_sources})
