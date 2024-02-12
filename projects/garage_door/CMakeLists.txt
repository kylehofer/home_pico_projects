# Adding the pico client
file(GLOB_RECURSE CPP_SOURCES ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE C_SOURCES ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(pico_garage_door ${CPP_SOURCES} ${C_SOURCES})

target_include_directories(pico_garage_door PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# IF(PICO_POLL)
target_link_libraries(pico_garage_door
    pico_cyw43_arch_lwip_poll
    pico_stdlib
    hardware_adc
    pico_sparkplug_client
    pico_ntp_client
    pico_multicore
)

target_compile_definitions(pico_garage_door PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    BROKER_ADDRESS=\"${BROKER_ADDRESS}\"
    NTP_ADDRESS=\"${NTP_ADDRESS}\"
)

# ELSE()
# target_link_libraries(pico_garage_door
# pico_cyw43_arch_lwip_threadsafe_background
# pico_stdlib
# hardware_adc
# pico_sparkplug_client
# pico_multicore
# )
# ENDIF()
pico_add_extra_outputs(pico_garage_door)
pico_enable_stdio_usb(pico_garage_door 1)

pico_enable_stdio_uart(pico_garage_door 0)