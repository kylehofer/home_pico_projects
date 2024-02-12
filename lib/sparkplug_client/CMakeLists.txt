include(FetchContent)

# Finding all of our source
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(pico_sparkplug_client STATIC ${SOURCES})

# pull in common dependencies
target_link_libraries(pico_sparkplug_client
    pico_stdlib
    pico_tcp_client
    cpp_sparkplug
    pico_ntp_client
)

target_include_directories(pico_sparkplug_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")