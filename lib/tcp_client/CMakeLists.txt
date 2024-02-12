# Finding all of our source
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(pico_tcp_client STATIC ${SOURCES})

# pull in common dependencies
target_link_libraries(pico_tcp_client
    pico_stdlib
    pico_cyw43_arch_lwip_poll
)

target_include_directories(pico_tcp_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")