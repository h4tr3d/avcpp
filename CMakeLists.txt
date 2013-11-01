cmake_minimum_required (VERSION 2.8)
project(transcoder)

include_directories(.)

if(CMAKE_CROSSCOMPILING)
  set(QT_HEADERS_DIR "/usr/i486-mingw32/include")
  set(QT_LIBRARY_DIR "/usr/i486-mingw32/lib")

  set(CMAKE_EXE_LINKER_FLAGS 
    "${CMAKE_EXE_LINKER_FLAGS} -Wl,-subsystem,console -Wl,-enable-auto-import -Wl,-enable-runtime-pseudo-reloc")
endif()

set(BOOST_COMPONENTS
    system
    date_time
    filesystem
)
# Boost thread library is different on Win/Linux
if(WIN32)
  set(BOOST_COMPONENTS ${BOOST_COMPONENTS} thread_win32)
else()
  set(BOOST_COMPONENTS ${BOOST_COMPONENTS} thread)
endif()

#set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
# -Weffc++
add_definitions(-DBOOST_THREAD_USE_LIB=1 -D__STDC_CONSTANT_MACROS=1)

find_package(Threads)
find_package(Boost COMPONENTS ${BOOST_COMPONENTS} REQUIRED)
find_package(PkgConfig)

pkg_check_modules(FFMPEG REQUIRED libavformat libavcodec libswscale libswresample libavutil libavfilter)


add_subdirectory(src)
#add_subdirectory(example/test-basic)
add_subdirectory(example/sample-transcode)
#add_subdirectory(example/sample-view)
