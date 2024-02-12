# cmake_policy(SET CMP0057 NEW)
cmake_minimum_required(VERSION 3.17)

add_definitions(-DPICO)
add_definitions(-DPICO_CYW43_ARCH_POLL)
SET(BUILD_TARGET "PICO")

# Possibly re-enable for releases
# SET(CMAKE_BUILD_TYPE "Release")
# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra")
include(pico_sdk_import.cmake)
include(FetchContent)

project(home_controllers LANGUAGES C CXX ASM)

SET(BUILD_TARGET PICO CACHE BOOL "")
SET(MQTT_ENABLE_TESTING FALSE CACHE BOOL "")
SET(MQTT_PICO_POLL TRUE CACHE BOOL "")

set(PICO_BOARD pico_w)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

set(WIFI_SSID "${WIFI_SSID}" CACHE INTERNAL "Wifi SSID")
set(WIFI_PASSWORD "${WIFI_PASSWORD}" CACHE INTERNAL "Wifi Password")
set(BROKER_ADDRESS "${BROKER_ADDRESS}" CACHE INTERNAL "Broker Address")
set(NTP_ADDRESS "${NTP_ADDRESS}" CACHE INTERNAL "NTP Address")

IF(FETCH_REMOTE)
    FetchContent_Declare(
        cpp_sparkplug
        GIT_REPOSITORY https://github.com/kylehofer/cpp_sparkplug.git
        GIT_TAG main
    )
    FetchContent_MakeAvailable(cpp_sparkplug)
ELSE()
    add_subdirectory(./external/cpp_sparkplug)
ENDIF()

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/lib/lwip")

add_subdirectory(lib)
add_subdirectory(projects)