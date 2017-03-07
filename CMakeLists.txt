cmake_minimum_required (VERSION 2.8)
project(AvCpp)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(FFMPEG_PKG_CONFIG_SUFFIX "" CACHE STRING "This suffix uses for FFmpeg component names searches by pkg-config")

include_directories(.)

add_definitions(-D__STDC_CONSTANT_MACROS)

# Compiler-specific C++11 activation.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (NOT (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7))
        message(FATAL_ERROR "${PROJECT_NAME} requires g++ 4.7 or greater.")
    endif ()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -Wextra")
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
    message(WARNING "I can't detect your C++11 compiler.")
endif ()

find_package(Threads)
find_package(FFmpeg
    COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER SWSCALE SWRESAMPLE REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIRS})

add_subdirectory(src)
#add_subdirectory(example/test-basic)
#add_subdirectory(example/sample-transcode)
add_subdirectory(example/api2-samples)
#add_subdirectory(example/sample-view)

