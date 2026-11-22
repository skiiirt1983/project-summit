cmake_minimum_required(VERSION 3.16)
set(PROJECT_VER "0.0.1")
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/framework
    ${CMAKE_CURRENT_LIST_DIR}/shell
    ${CMAKE_CURRENT_LIST_DIR}/bsp
    ${CMAKE_CURRENT_LIST_DIR}/boot
    ${CMAKE_CURRENT_LIST_DIR}/web
    ${CMAKE_CURRENT_LIST_DIR}/panels
    ${CMAKE_CURRENT_LIST_DIR}/services)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
# ESP32-P4 v3.x support needs ESP-IDF 5.5.3 or later (ARCHITECTURE.md, Chip revision).
if("${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}" VERSION_LESS "5.5.3")
    message(FATAL_ERROR "Project Summit needs ESP-IDF >= 5.5.3, found "
        "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")
endif()
project(summit)
