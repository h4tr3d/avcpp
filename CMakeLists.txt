cmake_minimum_required (VERSION 3.19)

# Declare the version version.txt
file(READ "VERSION.txt" AVCPP_VERSION)
string(STRIP "${AVCPP_VERSION}" AVCPP_VERSION)

if(NOT DEFINED PROJECT_NAME)
    set(AVCPP_NOT_SUBPROJECT ON)
endif()

project(AvCpp LANGUAGES CXX VERSION ${AVCPP_VERSION})

set(CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake ${CMAKE_MODULE_PATH})

set(FFMPEG_PKG_CONFIG_SUFFIX "" CACHE STRING "This suffix uses for FFmpeg component names searches by pkg-config")
set(AV_ENABLE_STATIC On CACHE BOOL "Enable static library build (On)")
set(AV_ENABLE_SHARED On CACHE BOOL "Enable shared library build (On)")
set(AV_ENABLE_STATIC_PIC On CACHE BOOL "Enable Position Independent Code build for the statis library")
set(AV_BUILD_EXAMPLES On CACHE BOOL "Build example applications (On)")
if (DEFINED AV_BUILD_TESTS)
    message(DEPRECATION "AV_BUILD_TESTS is deprecated, use AVCPP_ENABLE_TESTS")
    if (NOT DEFINED AVCPP_ENABLE_TESTS)
        set(AVCPP_ENABLE_TESTS ${AV_BUILD_TESTS})
    endif()
endif()
option(AVCPP_ENABLE_TESTS "Build tests" On)
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

if (AVCPP_ENABLE_TESTS)
    enable_testing()

    include(FetchContent)

    # The test framework is downloaded at configure time. Point
    # FETCHCONTENT_SOURCE_DIR_CATCH2 at a local checkout to work offline.
    set(AVCPP_CATCH2_DECLARE_ARGS
        URL "https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.1.tar.gz"
        URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
    )
    # DOWNLOAD_EXTRACT_TIMESTAMP is only understood by CMake 3.24 and newer.
    if (CMAKE_VERSION VERSION_GREATER_EQUAL 3.24)
        list(APPEND AVCPP_CATCH2_DECLARE_ARGS DOWNLOAD_EXTRACT_TIMESTAMP ON)
    endif()

    FetchContent_Declare(Catch2 ${AVCPP_CATCH2_DECLARE_ARGS})

    set(CATCH_BUILD_TESTING Off CACHE BOOL "" FORCE)
    set(CATCH_BUILD_EXAMPLES Off CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_DOCS Off CACHE BOOL "" FORCE)
    set(CATCH_INSTALL_EXTRAS On CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(Catch2)

    # Catch2 declares StringMaker<std::string_view> only when its own sources
    # are compiled as C++17 or newer. AppleClang defaults to an older standard,
    # which leaves that specialization undefined for tests built as C++17.
    if (TARGET Catch2)
        set_target_properties(Catch2 PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON)
    endif()
    if (TARGET Catch2WithMain)
        set_target_properties(Catch2WithMain PROPERTIES
            CXX_STANDARD 17
            CXX_STANDARD_REQUIRED ON)
    endif()

    list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
    add_subdirectory(tests)
endif()

