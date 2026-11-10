# Finding all of our source
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(pico_wifi STATIC ${SOURCES})

# pull in common dependencies
target_link_libraries(pico_wifi
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    pico_unique_id
    cpp_sparkplug
    pico_ntp_client
)

target_include_directories(pico_wifi
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/"
)

target_compile_definitions(pico_wifi PRIVATE
    WIFI_SSID=\"${WIFI_SSID}\"
    WIFI_PASSWORD=\"${WIFI_PASSWORD}\"
)