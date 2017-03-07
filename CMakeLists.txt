cmake_minimum_required (VERSION 3.1)
project(AvCpp)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(FFMPEG_PKG_CONFIG_SUFFIX "" CACHE STRING "This suffix uses for FFmpeg component names searches by pkg-config")

include_directories(.)

add_definitions(-D__STDC_CONSTANT_MACROS)

# Compiler-specific C++11 activation.
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED yes)

# -pthread sets also some useful compile-time flags
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

find_package(FFmpeg
    COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER SWSCALE SWRESAMPLE REQUIRED)

include_directories(${FFMPEG_INCLUDE_DIRS})

add_subdirectory(src)
#add_subdirectory(example/test-basic)
#add_subdirectory(example/sample-transcode)
add_subdirectory(example/api2-samples)
#add_subdirectory(example/sample-view)

