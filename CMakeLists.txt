cmake_minimum_required (VERSION 2.8)
project(AvCpp)

include_directories(.)

if(CMAKE_CROSSCOMPILING)
  set(QT_HEADERS_DIR "/usr/i486-mingw32/include")
  set(QT_LIBRARY_DIR "/usr/i486-mingw32/lib")

  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console -Wl,-enable-auto-import -Wl,-enable-runtime-pseudo-reloc")
endif()

# C++11
#list(APPEND CMAKE_CXX_FLAGS "-std=c++11")
#set( CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11" )
if(CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-std=c++11)
endif()

add_definitions(-D__STDC_CONSTANT_MACROS)

# Compiler-specific C++11 activation.
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "GNU")
    execute_process(
        COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE GCC_VERSION)
    if (NOT (GCC_VERSION VERSION_GREATER 4.7 OR GCC_VERSION VERSION_EQUAL 4.7))
        message(FATAL_ERROR "${PROJECT_NAME} requires g++ 4.7 or greater.")
    endif ()
elseif ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
else ()
    message(FATAL_ERROR "Your C++ compiler does not support C++11.")
endif ()

find_package(Threads)
find_package(PkgConfig)

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libswscale libswresample libavutil libavfilter)


add_subdirectory(src)
#add_subdirectory(example/test-basic)
add_subdirectory(example/sample-transcode)
#add_subdirectory(example/sample-view)
