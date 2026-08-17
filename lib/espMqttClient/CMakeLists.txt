set(COMPONENT_SRCDIRS
    "src" "src/Packets" "src/Transport"
)

set(COMPONENT_ADD_INCLUDEDIRS
    "src" "src/Packets" "src/Transport" "src/MemoryPool/src"
)

set(COMPONENT_REQUIRES
    "arduino-esp32"
    "AsyncTCP"
    "esp_wifi"
    "network_provisioning"
)

register_component()

#target_compile_definitions(${COMPONENT_TARGET} PUBLIC -DESP32)
target_compile_options(${COMPONENT_TARGET} PRIVATE -fno-rtti)
