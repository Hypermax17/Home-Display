# Firmware fuer STM32F746G-DISCO (Mock-Backend). Aufruf siehe README.md im Wurzelverzeichnis:
#   cmake -S . -B build-stm32 -DCMAKE_TOOLCHAIN_FILE=cmake/arm-none-eabi.cmake -DHD_BUILD_SIM=OFF \
#         -DHD_BUILD_TESTS=OFF -DHD_BUILD_STM32=ON -DCMAKE_BUILD_TYPE=Release

# ---- ST-Treiber (HAL, CMSIS, BSP) aus dem offiziellen STM32CubeF7-Paket holen ---------------
set(CUBE_TAG v1.17.2)
set(CUBE_DIR "${CMAKE_BINARY_DIR}/STM32CubeF7" CACHE PATH "STM32CubeF7 (wird automatisch geklont)")
set(CUBE_MARKER "${CUBE_DIR}/Drivers/BSP/Components/ft5336/ft5336.c")
if(NOT EXISTS "${CUBE_MARKER}")
    find_package(Git REQUIRED)
    message(STATUS "Lade STM32CubeF7 ${CUBE_TAG} (HAL/CMSIS/BSP) ...")
    file(REMOVE_RECURSE "${CUBE_DIR}")
    execute_process(COMMAND ${GIT_EXECUTABLE} clone --depth 1 --branch ${CUBE_TAG} --filter=blob:none --sparse
                            https://github.com/STMicroelectronics/STM32CubeF7.git "${CUBE_DIR}"
                    RESULT_VARIABLE r1)
    execute_process(COMMAND ${GIT_EXECUTABLE} sparse-checkout set --skip-checks
                            Drivers/CMSIS/Include Drivers/CMSIS/Device Drivers/STM32F7xx_HAL_Driver
                            Drivers/BSP/STM32746G-Discovery Drivers/BSP/Components Utilities/Fonts
                    WORKING_DIRECTORY "${CUBE_DIR}" RESULT_VARIABLE r2)
    execute_process(COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1
                            Drivers/STM32F7xx_HAL_Driver Drivers/CMSIS/Device/ST/STM32F7xx
                            Drivers/BSP/STM32746G-Discovery Drivers/BSP/Components/Common
                            Drivers/BSP/Components/ft5336 Drivers/BSP/Components/rk043fn48h
                    WORKING_DIRECTORY "${CUBE_DIR}" RESULT_VARIABLE r3)
    if(NOT r1 EQUAL 0 OR NOT r2 EQUAL 0 OR NOT r3 EQUAL 0 OR NOT EXISTS "${CUBE_MARKER}")
        message(FATAL_ERROR "STM32CubeF7 konnte nicht geladen werden (Netzwerk/Git?). "
                            "Alternativ CUBE_DIR auf ein vorhandenes Cube-Paket setzen.")
    endif()
endif()

set(HAL_DIR  "${CUBE_DIR}/Drivers/STM32F7xx_HAL_Driver")
set(CMSIS_DEV "${CUBE_DIR}/Drivers/CMSIS/Device/ST/STM32F7xx")
set(BSP_DIR  "${CUBE_DIR}/Drivers/BSP/STM32746G-Discovery")
set(HAL_MODULES hal hal_cortex hal_rcc hal_rcc_ex hal_gpio hal_pwr hal_pwr_ex hal_dma hal_dma_ex
                hal_flash hal_flash_ex hal_ltdc hal_ltdc_ex hal_dma2d hal_sdram ll_fmc hal_i2c hal_i2c_ex)
set(HAL_SRC "")
foreach(m ${HAL_MODULES})
    list(APPEND HAL_SRC "${HAL_DIR}/Src/stm32f7xx_${m}.c")
endforeach()

add_library(stm32_drivers STATIC
    ${HAL_SRC}
    ${CMSIS_DEV}/Source/Templates/system_stm32f7xx.c
    ${CMSIS_DEV}/Source/Templates/gcc/startup_stm32f746xx.s
    ${BSP_DIR}/stm32746g_discovery.c
    ${BSP_DIR}/stm32746g_discovery_lcd.c
    ${BSP_DIR}/stm32746g_discovery_sdram.c
    ${BSP_DIR}/stm32746g_discovery_ts.c
    ${CUBE_DIR}/Drivers/BSP/Components/ft5336/ft5336.c
    ${CUBE_DIR}/Utilities/Fonts/font24.c)
target_include_directories(stm32_drivers PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}          # stm32f7xx_hal_conf.h
    ${HAL_DIR}/Inc
    ${HAL_DIR}/Inc/Legacy
    ${CMSIS_DEV}/Include
    ${CUBE_DIR}/Drivers/CMSIS/Include
    ${BSP_DIR})
target_compile_definitions(stm32_drivers PUBLIC STM32F746xx USE_HAL_DRIVER)
target_compile_options(stm32_drivers PRIVATE -w) # Fremdcode: Warnungen nicht anzeigen

# ---- Firmware ---------------------------------------------------------------------------
# Ausbaustufe 0..7 (siehe main.c); 7 = komplette Anwendung
set(HD_BOOT_LEVEL 7 CACHE STRING "Ausbaustufe der Firmware (0..7)")
add_executable(hd_firmware main.c stm32f7xx_it.c fault_screen.c)
target_compile_definitions(hd_firmware PRIVATE HD_BOOT_LEVEL=${HD_BOOT_LEVEL})
target_link_libraries(hd_firmware PRIVATE hd_ui hd_core hd_config stm32_drivers)
target_compile_options(hd_firmware PRIVATE -Wall -Wextra)
target_link_options(hd_firmware PRIVATE
    -T${CMAKE_CURRENT_SOURCE_DIR}/STM32F746NGHx_FLASH.ld
    --specs=nano.specs --specs=nosys.specs
    -Wl,-Map=${CMAKE_BINARY_DIR}/hd_firmware.map -Wl,--gc-sections -Wl,--print-memory-usage)
set_target_properties(hd_firmware PROPERTIES SUFFIX ".elf" LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/STM32F746NGHx_FLASH.ld")

add_custom_command(TARGET hd_firmware POST_BUILD
    COMMAND ${CMAKE_OBJCOPY} -O binary $<TARGET_FILE:hd_firmware> ${CMAKE_BINARY_DIR}/hd_firmware.bin
    COMMAND ${CMAKE_OBJCOPY} -O ihex   $<TARGET_FILE:hd_firmware> ${CMAKE_BINARY_DIR}/hd_firmware.hex
    COMMENT "hd_firmware.bin / .hex erzeugen")
