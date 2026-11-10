# Finding all of our source
file(GLOB_RECURSE SOURCES RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "./*.cpp")
add_library(dependency_executor STATIC ${SOURCES})

# pull in common dependencies
target_link_libraries(dependency_executor
    pico_stdlib
)

target_include_directories(dependency_executor
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/"
)