# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

# Hack to remove dir path from the log source file path
string(LENGTH "${CMAKE_SOURCE_DIR}/" SOURCE_PATH_SIZE)
add_definitions("-DSOURCE_PATH_SIZE=${SOURCE_PATH_SIZE}")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)
set(PICO_SDK_PATH "${CMAKE_CURRENT_SOURCE_DIR}/pico-sdk")
set(FREERTOS_KERNEL_PATH, "${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)
include(FreeRTOS_Kernel_import.cmake)
include(config.cmake)

project(firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

add_executable(firmware 
        main.cc 
        configs/${BOARD_CONFIG}/layout.cc 
        keyscan.cc 
        usb.cc 
        joystick.cc 
        runner.cc 
        base.cc 
        utils.cc 
        rotary_encoder.cc 
        ssd1306.cc 
        config_modifier.cc 
        builtin_keycode.cc
        configuration.cc
        storage.cc
        sync.cc
        cJSON/cJSON.c)

pico_set_program_name(firmware "PicoMK")
pico_set_program_version(firmware "0.1")

pico_enable_stdio_uart(firmware 0)
pico_enable_stdio_usb(firmware 0)

# Add the standard library to the build
target_link_libraries(firmware 
        pico_stdlib 
        tinyusb_device 
        FreeRTOS-Kernel 
        FreeRTOS-Kernel-Heap4)

add_compile_definitions(
        PICO_HEAP_SIZE=65536)

add_subdirectory(pico-ssd1306)
add_subdirectory(littlefs)

# Add any user requested libraries
target_link_libraries(firmware
        hardware_adc
        hardware_i2c
        hardware_clocks
        hardware_regs
        pico_ssd1306
        littlefs
        )

target_include_directories(firmware PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
        ${CMAKE_CURRENT_LIST_DIR}/configs/${BOARD_CONFIG}
        )

pico_add_extra_outputs(firmware)

