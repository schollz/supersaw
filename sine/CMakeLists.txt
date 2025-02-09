cmake_minimum_required(VERSION 3.13...3.27)

# set PICO_SDK_PATH to local path
set(PICO_SDK_PATH "../pico-sdk")
set(PICO_BOARD "pico2")
set(PICO_EXTRAS_PATH "../pico-extras")
include(pico_sdk_import.cmake)

# We also need PICO EXTRAS
include(pico_extras_import.cmake)

project(hello_usb)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(hello_usb
        main.c
        verb.c
)

# pull in common dependencies
target_link_libraries(hello_usb 
        pico_stdlib
        pico_audio_i2s
        hardware_clocks
)

target_compile_definitions(hello_usb PRIVATE
        USE_AUDIO_I2S=1
        PICO_AUDIO_I2S_DATA_PIN=18
        PICO_AUDIO_I2S_CLOCK_PIN_BASE=16
)

set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

target_compile_options(${PROJECT_NAME} PRIVATE
    -O3
)

# # enable usb output, disable uart output
# pico_enable_stdio_usb(hello_usb 0)
# pico_enable_stdio_uart(hello_usb 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(hello_usb)