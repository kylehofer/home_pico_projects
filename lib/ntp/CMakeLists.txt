# Finding all of our source
file(GLOB_RECURSE SOURCES ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
file(GLOB_RECURSE HEADERS ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR} "./*.h")

add_library(pico_ntp_client STATIC ${SOURCES} ${HEADERS})
target_include_directories(pico_ntp_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/")
target_include_directories(pico_ntp_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../lwip")

# pico_add_extra_outputs(pico_ntp_client)
target_link_libraries(pico_ntp_client
    pico_stdlib
    pico_cyw43_arch_lwip_poll

    # pico_hardware_sync
)