# Finding all of our source. Not recursive, so copies kept in subfolders
# (such as temp/) are not compiled in.
file(GLOB SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(pico_tcp_client STATIC ${SOURCES})

# pull in common dependencies. cpp_mqtt_client provides TransmitBuffer and
# TransmitRing; its target comes from external/cpp_sparkplug.
target_link_libraries(pico_tcp_client
    pico_stdlib
    pico_cyw43_arch_lwip_poll
    cpp_mqtt_client
)

target_include_directories(pico_tcp_client
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/"
)
