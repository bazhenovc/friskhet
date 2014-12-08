cmake_minimum_required(VERSION 3.0)

project(friskhet)

set(DK_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(DK_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${DK_BUILD_DIR})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${DK_BUILD_DIR})
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${DK_BUILD_DIR})

add_subdirectory(external/SDL2)

file(GLOB SRC code/*.c*)
file(GLOB HDR code/*.h*)

include_directories(external/SDL2/include)
include_directories(external/glm/)

add_executable(FGame ${SRC} ${HDR})
target_link_libraries(FGame SDL2 SDL2main)
