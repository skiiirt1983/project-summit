# Project Summit PC simulator: the shell + framework + placeholder panels on LVGL,
# built for the host. Headless self-test always; SDL2 window if SDL2 is found.
#
#   cmake -S firmware/sim -B build/sim -G Ninja && cmake --build build/sim
#   build/sim/summit_sim --selftest --out sim-out
#   build/sim/summit_sim --sdl
cmake_minimum_required(VERSION 3.16)
project(summit_sim C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE RelWithDebInfo)
endif()

set(FW ${CMAKE_CURRENT_LIST_DIR}/..)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
endif()
option(SUMMIT_SIM_SDL "Build the SDL2 window mode" ${SDL2_FOUND})

# LVGL: same version as the device (firmware/shell/idf_component.yml).
include(FetchContent)
set(LV_BUILD_CONF_PATH ${CMAKE_CURRENT_LIST_DIR}/lv_conf.h CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v9.4.0
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(lvgl)
if(SUMMIT_SIM_SDL)
    target_compile_definitions(lvgl PUBLIC SUMMIT_SIM_SDL=1)
    target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)
endif()

find_package(ZLIB REQUIRED)

add_executable(summit_sim
    main.cpp demo_panels.cpp png.cpp
    ${FW}/framework/src/value.cpp ${FW}/framework/src/event_bus.cpp
    ${FW}/framework/src/state_store.cpp ${FW}/framework/src/settings.cpp
    ${FW}/framework/src/registry.cpp ${FW}/framework/src/system_settings.cpp
    ${FW}/shell/src/layout.cpp ${FW}/shell/src/glance.cpp ${FW}/shell/src/strip_state.cpp
    ${FW}/shell/src/rotation.cpp ${FW}/shell/src/pager.cpp ${FW}/shell/src/shell_ui.cpp)
target_include_directories(summit_sim PRIVATE ${FW}/framework/include ${FW}/shell/include)
target_link_libraries(summit_sim PRIVATE lvgl ZLIB::ZLIB)
target_compile_options(summit_sim PRIVATE -Wall -Wextra -Werror)
