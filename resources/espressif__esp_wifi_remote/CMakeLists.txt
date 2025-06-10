if(NOT CONFIG_ESP_WIFI_ENABLED AND NOT CONFIG_ESP_HOST_WIFI_ENABLED)
    set(src_wifi_is_remote esp_wifi_remote.c esp_wifi_remote_net.c)
endif()

if(CONFIG_ESP_WIFI_REMOTE_LIBRARY_EPPP)
    set(src_wifi_remote_eppp eppp/wifi_remote_rpc_client.cpp eppp/wifi_remote_rpc_server.cpp eppp/eppp_init.c)
endif()

idf_component_register(INCLUDE_DIRS include
                    SRCS ${src_wifi_is_remote}
                         ${src_wifi_remote_eppp}
                    PRIV_INCLUDE_DIRS eppp
                    REQUIRES esp_event esp_netif
                    PRIV_REQUIRES esp_wifi esp-tls vfs)


set(IDF_VER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idf_v${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}")

# Check if we're on tagged version of ESP-IDF and if we need to supply specific version
string(REGEX MATCH "^v[0-9]+\\.[0-9]+(\\.[0-9]+)?(-dirty)?$" IDF_VER_TAG "${IDF_VER}")
if (IDF_VER_TAG)
    string(REGEX REPLACE "-dirty$" "" IDF_VER_TAG "${IDF_VER_TAG}")
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/idf_tag_${IDF_VER_TAG}")
        set(IDF_VER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/idf_tag_${IDF_VER_TAG}")
    endif()
endif()

idf_component_get_property(wifi esp_wifi COMPONENT_LIB)

set(TARGET_INCLUDE_TYPE "INTERFACE")
set(TARGET_SOURCE_TYPE "INTERFACE")
if(NOT CONFIG_ESP_WIFI_ENABLED AND NOT CONFIG_ESP_HOST_WIFI_ENABLED)
    set(src_wifi_with_remote ${IDF_VER_DIR}/esp_wifi_with_remote.c)
    # We need to build wifi sources with wifi-remote properties, so the injected wifi headers are used
    get_target_property(wifi_sources ${wifi} SOURCES)
    set_target_properties(${wifi} PROPERTIES SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/dummy_src.c")
    set(TARGET_INCLUDE_TYPE "PUBLIC")
    set(TARGET_SOURCE_TYPE "PRIVATE")
endif()

if(CONFIG_ESP_WIFI_REMOTE_LIBRARY_EPPP)
    set(TARGET_INCLUDE_TYPE "PUBLIC")
    set(TARGET_SOURCE_TYPE "PRIVATE")
else()
    set(src_wifi_remote_weak ${IDF_VER_DIR}/esp_wifi_remote_weak.c)
endif()


target_include_directories(${COMPONENT_LIB} ${TARGET_INCLUDE_TYPE} ${IDF_VER_DIR}/include)
target_sources(${COMPONENT_LIB}             ${TARGET_SOURCE_TYPE}  ${src_wifi_remote_weak}
                                                                   ${src_wifi_with_remote}
                                                                   ${wifi_sources})

# Update wifi include directories to prepend the injected dir with modified headers supporting SLAVE capability
get_target_property(original_wifi_dirs ${wifi} INTERFACE_INCLUDE_DIRECTORIES)
set(updated_wifi_dirs "${IDF_VER_DIR}/include/injected" ${original_wifi_dirs})
set_target_properties(${wifi} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${updated_wifi_dirs}")

target_link_libraries(${wifi} PUBLIC ${COMPONENT_LIB})
