# The following lines of boilerplate have to be in your project's
# CMakeLists in this exact order for cmake to work correctly
cmake_minimum_required(VERSION 3.5)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

if(DEFINED ENV{HTTP_PATH})
  set(HTTP_PATH $ENV{HTTP_PATH})
else()
  #these both work
  set(HTTP_PATH "../../")
  #set(HTTP_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../)
  
  #this does not work for me...
  #set(HTTP_PATH ${CMAKE_CURRENT_LIST_DIR}/../../../PsychicHttp)
endif(DEFINED ENV{HTTP_PATH})

set(EXTRA_COMPONENT_DIRS ${HTTP_PATH})

project(PsychicHttp_IDF)