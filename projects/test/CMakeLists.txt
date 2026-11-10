# Adding the pico client
file(GLOB_RECURSE CPP_SOURCES ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")
file(GLOB_RECURSE C_SOURCES ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")

add_executable(pico_test ${CPP_SOURCES} ${C_SOURCES})

target_include_directories(pico_test PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# IF(PICO_POLL)
target_link_libraries(pico_test
    pico_cyw43_arch_lwip_poll
    pico_stdlib
    pico_sparkplug_client
    pico_ntp_client
    pico_multicore
    hardware_gpio
    hardware_pwm
    hardware_dma
)

target_compile_definitions(pico_test PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
    BROKER_ADDRESS=\"${BROKER_ADDRESS}\"
    NTP_ADDRESS=\"${NTP_ADDRESS}\"
)

pico_add_extra_outputs(pico_test)
pico_enable_stdio_usb(pico_test 1)
pico_enable_stdio_uart(pico_test 0)

# Links the OTA helper and builds picowota_pico_test.uf2 for the first flash
pico_ota_enable(pico_test)
