cmake_minimum_required(VERSION 3.8.0)

project(torrent_node VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)

if(NOT CMAKE_BUILD_TYPE)
    message(STATUS "No build type selected, default is Release")
    set(CMAKE_BUILD_TYPE "Release")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/libev-4.25/lib/include/)
link_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/libev-4.25/lib/lib/)

set(CMAKE_PREFIX_PATH ${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/fmt-5.3.0/libs/)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/fmt-5.3.0/libs/include)

set(libsniper "std" "log" "cache" "xxhash" "net" "strings" "pico" "event" "http" "mhd")
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty)

add_subdirectory(src)
