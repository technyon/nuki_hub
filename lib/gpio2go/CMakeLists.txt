cmake_minimum_required(VERSION 3.0.0)

if(NOT ARDUINO_BOARD)
    set(ARDUINO_BOARD "ESP32 Dev Module [esp32.esp32]")
endif()

project(gpio2go CXX)

# ARDUHAL_LOG_LEVEL_NONE, define ARDUHAL_LOG_LEVEL_ERROR, define ARDUHAL_LOG_LEVEL_WARN, define ARDUHAL_LOG_LEVEL_INFO,
# define ARDUHAL_LOG_LEVEL_DEBUG, define ARDUHAL_LOG_LEVEL_VERBOSE

set(LOG_LEVEL ARDUHAL_LOG_LEVEL_NONE)

#add_compile_definitions(DEBUG_SENSE_NUKI)
#add_compile_definitions(DEBUG_NUKI_COMMAND)
#add_compile_definitions(DEBUG_NUKI_CONNECT)
#add_compile_definitions(DEBUG_NUKI_COMMUNICATION)
#add_compile_definitions(DEBUG_NUKI_HEX_DATA)
#add_compile_definitions(DEBUG_NUKI_READABLE_DATA)

add_compile_definitions(ESP_PLATFORM)
add_compile_definitions(ESP32)
add_compile_definitions(ARDUINO_ARCH_ESP32)

include_directories(${PROJECT_NAME}
    PRIVATE
        src
)

set(SRCFILES
    src/PinMode.h
    src/Gpio2Go.cpp
    src/InterruptMode.h
)

file(GLOB_RECURSE SRCFILESREC

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
        )

target_link_arduino_libraries(${PROJECT_NAME}
    PRIVATE
        core
)

target_enable_arduino_upload(${PROJECT_NAME})

