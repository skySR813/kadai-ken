# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.3.1)
set(toolchainVersion 15_2_Rel1)
set(picotoolVersion 2.3.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2 CACHE STRING "Board type")

# Pull in Raspberry Pi Pico SDK (must be before project)
include(pico_sdk_import.cmake)

project(kadai_test_main C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

if (NOT FREERTOS_KERNEL_PATH OR
    NOT EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake")
    foreach(FREERTOS_CANDIDATE
            "${PICO_SDK_PATH}/../FreeRTOS-Kernel"
            "${PICO_SDK_PATH}/../../FreeRTOS-Kernel")
        if (EXISTS "${FREERTOS_CANDIDATE}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake")
            set(FREERTOS_KERNEL_PATH "${FREERTOS_CANDIDATE}" CACHE PATH "Path to the FreeRTOS Kernel" FORCE)
            break()
        endif()
    endforeach()
endif()
if (NOT FREERTOS_KERNEL_PATH OR
    NOT EXISTS "${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake")
    message(FATAL_ERROR "FreeRTOS Kernel not found. Set FREERTOS_KERNEL_PATH to its source directory.")
endif()
set(FREERTOS_CONFIG_FILE_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
include("${FREERTOS_KERNEL_PATH}/portable/ThirdParty/GCC/RP2350_ARM_NTZ/FreeRTOS_Kernel_import.cmake")

# Add executable. Default name is the project name, version 0.1

add_executable(kadai_test_main
        kadai_test_main.c
        tm1637.c
        color_lcd.c
        LSM6DSV16X.c
)

pico_set_program_name(kadai_test_main "kadai_test_main")
pico_set_program_version(kadai_test_main "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(kadai_test_main 1)
pico_enable_stdio_usb(kadai_test_main 1)

# Add the standard library to the build
target_link_libraries(kadai_test_main
        pico_stdlib)

# Add the standard include files to the build
target_include_directories(kadai_test_main PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}
)

# Add any user requested libraries
target_link_libraries(kadai_test_main 
        hardware_spi
        hardware_i2c
        FreeRTOS-Kernel-Heap4
        )

target_compile_definitions(kadai_test_main PRIVATE
        configNUMBER_OF_CORES=1
)

target_link_options(kadai_test_main PRIVATE -Wl,--print-memory-usage)

pico_add_extra_outputs(kadai_test_main)

