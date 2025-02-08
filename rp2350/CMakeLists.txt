cmake_minimum_required(VERSION 3.13...3.27)

# set PICO_SDK_PATH to local path
set(PICO_SDK_PATH "../pico-sdk")
set(PICO_BOARD "pico2")

include(pico_sdk_import.cmake)

project(hello_usb)

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_executable(hello_usb
        main.c
        verb.c
)

# pull in common dependencies
target_link_libraries(hello_usb pico_stdlib)

set_property(TARGET ${PROJECT_NAME} APPEND_STRING PROPERTY LINK_FLAGS "-Wl,--print-memory-usage")

# # enable usb output, disable uart output
# pico_enable_stdio_usb(hello_usb 0)
# pico_enable_stdio_uart(hello_usb 0)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(hello_usb)