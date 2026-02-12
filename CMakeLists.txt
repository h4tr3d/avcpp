cmake_minimum_required (VERSION 3.19)

# Declare here
set (AVCPP_VERSION 2.90.1)

if(NOT DEFINED PROJECT_NAME)
    set(AVCPP_NOT_SUBPROJECT ON)
endif()

project(AvCpp LANGUAGES CXX VERSION ${AVCPP_VERSION})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(FFMPEG_PKG_CONFIG_SUFFIX "" CACHE STRING "This suffix uses for FFmpeg component names searches by pkg-config")
set(AV_ENABLE_STATIC On CACHE BOOL "Enable static library build (On)")
set(AV_ENABLE_SHARED On CACHE BOOL "Enable shared library build (On)")
set(AV_BUILD_EXAMPLES On CACHE BOOL "Build example applications (On)")
set(AV_BUILD_TESTS On CACHE BOOL "Build tests (On)")
option(AV_DISABLE_AVFORMAT "Disable libavformat usage. Also turns off: AVFILTER, AVDEVICE" Off)
option(AV_DISABLE_AVFILTER "Disable libavfilter usage. Also turns off: AVDEVICE" Off)
option(AV_DISABLE_AVDEVICE "Disable libavdevice usage." Off)

# Compiler-specific C++ standard activation
#set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED yes)

# Warnings
set (AVCPP_WARNING_OPTIONS
     $<$<OR:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>,$<CXX_COMPILER_ID:GNU>>:
       -Wall -Wextra -Werror=return-type -Wwrite-strings>
     $<$<CXX_COMPILER_ID:MSVC>:
       /W4>)

# -pthread sets also some useful compile-time flags
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads)

# this components required by the all others
set(CORE_FFMPEG_COMPONENTS AVCODEC AVUTIL SWSCALE SWRESAMPLE)
set(OPTIONAL_FFMPEG_COMPONENTS AVFORMAT AVFILTER AVDEVICE)
if (AV_DISABLE_AVFORMAT)
    list(REMOVE_ITEM OPTIONAL_FFMPEG_COMPONENTS AVFORMAT AVFILTER AVDEVICE)
endif()
if (AV_DISABLE_AVFILTER)
    list(REMOVE_ITEM OPTIONAL_FFMPEG_COMPONENTS AVFILTER AVDEVICE)
endif()
if (AV_DISABLE_AVDEVICE)
    list(REMOVE_ITEM OPTIONAL_FFMPEG_COMPONENTS AVDEVICE)
endif()
message(STATUS "CORE_FFMPEG_COMPONENTS: ${CORE_FFMPEG_COMPONENTS}")
message(STATUS "OPTI_FFMPEG_COMPONENTS: ${OPTIONAL_FFMPEG_COMPONENTS}")
find_package(FFmpeg
    COMPONENTS ${CORE_FFMPEG_COMPONENTS} ${OPTIONAL_FFMPEG_COMPONENTS} REQUIRED)

# define useful version macroses for propagate into avconfig.h
include(SetupPackageVersion)

add_subdirectory(src)

if (${AV_BUILD_EXAMPLES})
	add_subdirectory(example/api2-samples)
endif()

if (AV_BUILD_TESTS)
    set(CATCH_DIR ${CMAKE_CURRENT_LIST_DIR}/catch2)
    if (EXISTS ${CATCH_DIR}/CMakeLists.txt)
        enable_testing()
        set(CATCH_BUILD_TESTING Off CACHE STRING "")
        add_subdirectory(${CATCH_DIR})
        add_subdirectory(tests)
    else()
        message(STATUS "Catch2 not found, testing skipped")
    endif()
endif()

