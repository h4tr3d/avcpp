cmake_minimum_required (VERSION 3.11)

if(NOT DEFINED PROJECT_NAME)
    set(AVCPP_NOT_SUBPROJECT ON)
endif()

project(AvCpp LANGUAGES CXX VERSION 2.0.99)

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(FFMPEG_PKG_CONFIG_SUFFIX "" CACHE STRING "This suffix uses for FFmpeg component names searches by pkg-config")
set(AV_ENABLE_STATIC On CACHE BOOL "Enable static library build (On)")
set(AV_ENABLE_SHARED On CACHE BOOL "Enable shared library build (On)")
set(AV_BUILD_EXAMPLES On CACHE BOOL "Build example applications (On)")

# Compiler-specific C++11 activation.
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED yes)

# Warnings
set (AVCPP_WARNING_OPTIONS
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
       -Wall -Wextra>
     $<$<CXX_COMPILER_ID:MSVC>:
       /W4>)

# -pthread sets also some useful compile-time flags
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

find_package(FFmpeg
    COMPONENTS AVCODEC AVFORMAT AVUTIL AVDEVICE AVFILTER SWSCALE SWRESAMPLE REQUIRED)

add_subdirectory(src)

if (${AV_BUILD_EXAMPLES})
	add_subdirectory(example/api2-samples)
endif()

set(CATCH_DIR ${CMAKE_CURRENT_LIST_DIR}/catch2)
if (EXISTS ${CATCH_DIR}/CMakeLists.txt)
    enable_testing()
    set(CATCH_BUILD_TESTING Off CACHE STRING "")
    add_subdirectory(${CATCH_DIR})
    add_subdirectory(tests)
else()
    message(STATUS "Catch2 not found, testing skipped")
endif()

