# transcoder sources

include_directories(../../src)

file(GLOB_RECURSE AV_TEST_VIEW_SOURCES "*.cpp")
file(GLOB_RECURSE AV_TEST_VIEW_HEADERS "*.h" "*.hpp")

list(APPEND AV_TEST_VIEW_SOURCES ${AV_SOURCES})
list(APPEND AV_TEST_VIEW_HEADERS ${AV_HEADERS})

set(AV_TEST_VIEW_TARGET av-view)

pkg_search_module(SDL REQUIRED sdl)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(${SDL_INCLUDE_DIRS})
link_directories(${FFMPEG_LIBRARY_DIRS})

add_executable(${AV_TEST_VIEW_TARGET} ${AV_TEST_VIEW_SOURCES})
target_link_libraries(${AV_TEST_VIEW_TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    ${SDL_LIBRARIES}
    avcpp
)

if(WIN32)
target_link_libraries(${AV_VIEW_TARGET}
    ws2_32
)
endif()
