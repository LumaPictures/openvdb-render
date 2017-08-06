project(openvdb_visualizer)
cmake_minimum_required(VERSION 2.8)

option(BUILD_MAYA_PLUGIN "Build the maya plugin." ON)
option(BUILD_MTOA_EXTENSION "Build the MtoA extension." ON)
option(BUILD_ARNOLD_SHADER "Build the Arnold shader." ON)
option(BUILD_USD_TOOLS "Build the various USD tools." ON)
option(INCLUDE_HEADERS_IN_BUILD "Include the headers next to the source files for IDEs that need this." OFF)

list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

add_definitions(-std=c++11)
if (MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
else ()
    add_definitions(-Wall -Wextra -fPIC)
endif ()

if (NOT BUILD_USD_TOOLS)
    # Some depreceated headers are included with USD.
    if (MSVC)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /WX")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} /WX")
    else ()
        add_definitions(-Werror)
    endif ()
endif ()

if (BUILD_MAYA_PLUGIN)
    add_subdirectory(plugin)
endif ()
if (BUILD_MTOA_EXTENSION)
    add_subdirectory(extension)
endif ()
if (BUILD_ARNOLD_SHADER)
    add_subdirectory(shader)
endif ()
if (BUILD_USD_TOOLS)
    add_subdirectory(usd)
endif ()

install(FILES README.md
        DESTINATION docs/)

install(DIRECTORY katana
        DESTINATION .)
