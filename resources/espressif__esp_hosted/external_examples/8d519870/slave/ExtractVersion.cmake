### if code is from Component Registry, generate from coprocessor_fw_version.txt
### if code is from git, generate from top level idf_component.yml

if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/main/coprocessor_fw_version.txt")
	file(READ "${CMAKE_CURRENT_LIST_DIR}/main/coprocessor_fw_version.txt" VERSION_CONTENTS)
	set(VERSION_REGEX "^([0-9]+).([0-9]+).([0-9]+)")
	string(REGEX MATCH "${VERSION_REGEX}" VERSION_MATCH "${VERSION_CONTENTS}")
	if(VERSION_MATCH)
		set(VERSION_GENERATOR "coprocessor_fw_version.txt")
	else()
		message(FATAL_ERROR "version info not found in coprocessor_fw_version.txt")
	endif()
elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../idf_component.yml")
	file(READ "${CMAKE_CURRENT_LIST_DIR}/../idf_component.yml" VERSION_CONTENTS)
	set(VERSION_REGEX "^version: \"([0-9]+).([0-9]+).([0-9]+)\"")
	string(REGEX MATCH "${VERSION_REGEX}" VERSION_MATCH "${VERSION_CONTENTS}")
	if(VERSION_MATCH)
		set(VERSION_GENERATOR "idf_component.yml")
	else()
		message(FATAL_ERROR "version info not found in idf_component.yml")
	endif()
else()
	message(FATAL_ERROR "idf_component.yml not found")
endif()

# generate header file from the version info
file(WRITE "${CMAKE_CURRENT_LIST_DIR}/main/coprocessor_fw_version.h"
	"/* this is a generated file - do not modify\n"
	" * generated from ${VERSION_GENERATOR} */\n"
	"#ifndef __COPROCESSOR_FW_VERSION_H__\n"
	"#define __COPROCESSOR_FW_VERSION_H__\n"
	"#define PROJECT_VERSION_MAJOR_1 ${CMAKE_MATCH_1}\n"
	"#define PROJECT_VERSION_MINOR_1 ${CMAKE_MATCH_2}\n"
	"#define PROJECT_VERSION_PATCH_1 ${CMAKE_MATCH_3}\n"
	"#endif\n")

set(PROJECT_VERSION_MAJOR_1 "${CMAKE_MATCH_1}")
set(PROJECT_VERSION_MINOR_1 "${CMAKE_MATCH_2}")
set(PROJECT_VERSION_PATCH_1 "${CMAKE_MATCH_3}")

message(*************************************************************************************)
message("                    Building ESP-Hosted-MCU FW :: ${PROJECT_VERSION_MAJOR_1}.${PROJECT_VERSION_MINOR_1}.${PROJECT_VERSION_PATCH_1} ")
message(*************************************************************************************)
