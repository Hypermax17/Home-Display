cmake_minimum_required(VERSION 3.16)
project(home_display C ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HD_BUILD_SIM   "PC-Simulator (SDL2) bauen"        ON)
option(HD_BUILD_TESTS "Unit-Tests bauen"                 ON)
option(HD_BUILD_STM32 "Firmware fuer STM32F746G-DISCO bauen (braucht cmake/arm-none-eabi.cmake)" OFF)

# ---------------------------------------------------------------- LVGL
include(FetchContent)
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/config/lv_conf.h" CACHE STRING "" FORCE)
set(LV_CONF_INCLUDE_SIMPLE OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
FetchContent_Declare(lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG        v9.2.2
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(lvgl)

# ---------------------------------------------------------------- Kernschicht (portabel, kein HAL/LVGL)
add_library(hd_core STATIC
    core/src/app_bus.c
    core/src/data_service.c
    core/src/dpt.c
    core/src/knx_ip.c
    core/src/knx_mock.c)
target_include_directories(hd_core PUBLIC core/include)
target_compile_options(hd_core PRIVATE -Wall -Wextra)

# Haus-Konfiguration (spaeter generiert)
add_library(hd_config STATIC config/house_config.c)
target_link_libraries(hd_config PUBLIC hd_core)

# ---------------------------------------------------------------- UI (LVGL, kennt nur app_bus + model)
file(GLOB HD_UI_SRC CONFIGURE_DEPENDS ui/*.c ui/fonts/*.c)
add_library(hd_ui STATIC ${HD_UI_SRC})
target_include_directories(hd_ui PUBLIC ui)
target_link_libraries(hd_ui PUBLIC hd_core lvgl)
target_compile_options(hd_ui PRIVATE -Wall -Wextra)

# ---------------------------------------------------------------- Simulator
if(HD_BUILD_SIM AND EMSCRIPTEN)
    # Browser-Variante (WebAssembly): laeuft auf Windows/iPad/... im Browser, Mock-Backend
    target_compile_definitions(lvgl PUBLIC HD_SIM=1 HD_WEB=1)
    target_compile_definitions(hd_ui PUBLIC HD_SIM=1 HD_WEB=1)
    add_executable(hd_web platform/sim/main.c platform/sim/sim_headless.c)
    target_compile_options(hd_web PRIVATE -Wall -Wextra)
    target_link_libraries(hd_web PRIVATE hd_ui hd_core hd_config)
    target_link_options(hd_web PRIVATE -sALLOW_MEMORY_GROWTH=1 -sEXPORTED_RUNTIME_METHODS=ccall -sINITIAL_MEMORY=32MB
        -sSINGLE_FILE=1 --shell-file ${CMAKE_CURRENT_SOURCE_DIR}/platform/web/shell.html)
    set_target_properties(hd_web PROPERTIES OUTPUT_NAME index SUFFIX ".html")
elseif(HD_BUILD_SIM)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
    target_link_libraries(lvgl PUBLIC PkgConfig::SDL2)
    target_compile_definitions(lvgl PUBLIC HD_SIM=1)
    target_compile_definitions(hd_ui PUBLIC HD_SIM=1)
    find_package(Threads REQUIRED)
    add_executable(hd_sim
        platform/sim/main.c
        platform/sim/sim_udp.c
        platform/sim/sim_headless.c)
    target_link_libraries(hd_sim PRIVATE hd_ui hd_core hd_config Threads::Threads)
    target_compile_options(hd_sim PRIVATE -Wall -Wextra)
endif()

# ---------------------------------------------------------------- Tests
if(HD_BUILD_TESTS)
    enable_testing()
    add_executable(test_core tests/test_core.c)
    target_link_libraries(test_core PRIVATE hd_core)
    target_compile_options(test_core PRIVATE -Wall -Wextra)
    add_test(NAME core COMMAND test_core)
endif()

# ---------------------------------------------------------------- Firmware (STM32F746G-DISCO)
if(HD_BUILD_STM32)
    add_subdirectory(platform/stm32)
endif()
