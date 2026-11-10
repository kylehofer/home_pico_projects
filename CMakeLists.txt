# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()

set(sdkVersion 2.1.1)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.1.1)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)

if(EXISTS ${picoVscode})
    include(${picoVscode})
endif()

# ====================================================================================
# cmake_policy(SET CMP0057 NEW)
cmake_minimum_required(VERSION 3.17)

add_definitions(-DPICO)
add_definitions(-DPICO_CYW43_ARCH_POLL)
SET(BUILD_TARGET "PICO")

add_compile_definitions(
    DEBUGGING=1
)

# Possibly re-enable for releases
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")

include(cmake/CPM.cmake)
include(pico_sdk_import.cmake)
include(FetchContent)

pico_sdk_init()

project(home_controllers LANGUAGES C CXX ASM)

set(PICO_BOARD pico_w)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# set(WIFI_PASSWORD "gnN&E*yx*59lo3ZAWRICpEMca$UFFkNxft28Mid6ehjoxMzjZaUC0gPL6YeVm9" CACHE INTERNAL "Wifi Password")
set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "Wifi SSID")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "Wifi Password")
set(BROKER_ADDRESS "${BROKER_ADDRESS}" CACHE INTERNAL "Broker Address")
set(NTP_ADDRESS "${NTP_ADDRESS}" CACHE INTERNAL "NTP Address")

# FORCE_BIG_ENDIAN
add_compile_definitions(FORCE_LITTLE_ENDIAN=1)

# set(LIBCXX_ENABLE_THREADS OFF)
set(CPP_SPARKPLUG_PAHO_SYNC OFF)
set(CPP_SPARKPLUG_PAHO_ASYNC OFF)
set(FETCH_REMOTE OFF)

# include_directories("/home/kyle/.pico-sdk/sdk/2.1.1/src/common/pico_base_headers/include")
# include_directories("/home/kyle/.pico-sdk/sdk/2.1.1/src/common/pico_stdlib_headers/include")
IF(FETCH_REMOTE)
    CPMAddPackage(
        cpp_sparkplug
        GIT_REPOSITORY https://github.com/kylehofer/cpp_sparkplug.git
        GIT_TAG main
        OPTIONS
        "BUILD_TARGET PICO"
        "CPP_SPARKPLUG_MQTT ON"
        "CPP_SPARKPLUG_TESTS OFF"
        "CPP_SPARKPLUG_PAHO_ASYNC OFF"
        "CPP_SPARKPLUG_PAHO_SYNC OFF"
        "CPP_SPARKPLUG_STATIC ON"
        "CPP_SPARKPLUG_SHARED OFF"
        "FETCH_REMOTE ON"
    )
ELSE()
    SET(CPP_SPARKPLUG_MQTT ON CACHE BOOL "")
    SET(CPP_SPARKPLUG_TESTS OFF CACHE BOOL "")
    SET(CPP_SPARKPLUG_PAHO_ASYNC OFF CACHE BOOL "")
    SET(CPP_SPARKPLUG_PAHO_SYNC OFF CACHE BOOL "")
    SET(CPP_SPARKPLUG_STATIC ON CACHE BOOL "")
    SET(CPP_SPARKPLUG_SHARED OFF CACHE BOOL "")
    add_subdirectory(./external/cpp_sparkplug)
ENDIF()

SET(EXECUTION_TIME_STATIC ON CACHE BOOL "")

# add_subdirectory(./external/elapsed_time)

# The picowota OTA bootloader. It carries its own copy of the WiFi credentials
# because it has to bring the network up before any of our code runs, so feed
# it the ones we already have. Note that these are passed to the compiler
# unquoted and stringified, so an SSID or password containing whitespace or a
# '#' will not survive; ours do not.
option(PICO_OTA "Build the picowota OTA bootloader and combined images" ON)

IF(PICO_OTA AND WIFI_SSID AND WIFI_PASSWORD)
    set(PICOWOTA_WIFI_SSID "${WIFI_SSID}")
    set(PICOWOTA_WIFI_PASS "${WIFI_PASSWORD}")
    add_subdirectory(./external/picowota)
ELSE()
    IF(PICO_OTA)
        message(WARNING
            "WIFI_SSID/WIFI_PASSWORD are not set, so the picowota bootloader cannot "
            "be built. Projects will be built as standalone images without OTA."
        )
    ENDIF()

    # The application side reboot helper stands alone, so pico_ota still builds
    # and the projects still compile without the bootloader.
    add_subdirectory(./external/picowota/picowota_reboot picowota_reboot)
ENDIF()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/lwip")

add_subdirectory(lib)
add_subdirectory(projects)