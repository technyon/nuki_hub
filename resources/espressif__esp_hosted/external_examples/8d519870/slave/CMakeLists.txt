# The following lines of boilerplate have to be in your project's CMakeLists
# in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
list(APPEND EXCLUDE_COMPONENTS lwip esp_netif)
#set(COMPONENTS main nvs_flash protocomm esp_rom)
set(COMPONENTS esp_timer esptool_py bootloader main nvs_flash esp_rom esp_wifi protocomm driver)
#set(COMPONENTS main nvs_flash protocomm esp_gdbstub) # incase gdbsub needed
project(network_adapter)
idf_build_set_property(COMPILE_OPTIONS "-fdiagnostics-color=always" APPEND)

### add custom dependency for generated version header file
### reference: https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-guides/build-system.html#source-code-generation
if(EXISTS "${PROJECT_DIR}/main/coprocessor_fw_version.txt")
	add_custom_command(OUTPUT "${PROJECT_DIR}/main/coprocessor_fw_version.h"
		COMMAND ${CMAKE_COMMAND} -P "${PROJECT_DIR}/ExtractVersion.cmake"
		DEPENDS "${PROJECT_DIR}/main/coprocessor_fw_version.txt"
		VERBATIM)
elseif(EXISTS "${PROJECT_DIR}/../idf_component.yml")
	add_custom_command(OUTPUT "${PROJECT_DIR}/main/coprocessor_fw_version.h"
		COMMAND ${CMAKE_COMMAND} -P "${PROJECT_DIR}/ExtractVersion.cmake"
		DEPENDS "${PROJECT_DIR}/../idf_component.yml"
		VERBATIM)
else()
	message(FATAL_ERROR "***** coprocessor_fw_version.txt and idf_component.yml NOT found.\nUnable to generate comprocessor fw version info to build")
endif()

add_custom_target(version_header DEPENDS "${PROJECT_DIR}/main/coprocessor_fw_version.h")
add_dependencies(${PROJECT_NAME}.elf version_header)

### clean up generated file when project is cleaned
set_property(DIRECTORY "${PROJECT_DIR}" APPEND PROPERTY
	ADDITIONAL_CLEAN_FILES "${PROJECT_DIR}/main/coprocessor_fw_version.h")
