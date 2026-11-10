# Application side support for over the air updates. The bootloader itself is
# a separate executable, built from external/picowota.

# Finding all of our source
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(pico_ota STATIC ${SOURCES})

# pull in common dependencies
target_link_libraries(pico_ota
    pico_stdlib
    cpp_sparkplug
    picowota_reboot
)

target_include_directories(pico_ota
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/"
)

# pico_ota_enable(<target>)
#
# Links the OTA helper into <target>, and when the bootloader is being built,
# relinks <target> to run from the application slot (0x1005b000) and emits a
# combined bootloader + application image as picowota_<target>.uf2 for the
# initial flash over USB. The plain <target>.elf is what gets sent over the air.
#
# Call this from the directory that defines <target>, after the usual
# pico_add_extra_outputs(), so the combined image lands with the target's other
# outputs.
function(pico_ota_enable TARGET)
    target_link_libraries(${TARGET} pico_ota)

    if (TARGET picowota)
        picowota_build_combined(${TARGET})
    else()
        message(STATUS
            "picowota bootloader not built: ${TARGET} will be a normal standalone image"
        )
    endif()
endfunction()
