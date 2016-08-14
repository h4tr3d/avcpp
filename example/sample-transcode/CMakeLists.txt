# transcoder sources

include_directories(../../src)

file(GLOB_RECURSE AV_TEST_TRANSCODE_SOURCES "*.cpp")
file(GLOB_RECURSE AV_TEST_TRANSCODE_HEADERS "*.h" "*.hpp")

list(APPEND AV_TEST_TRANSCODE_SOURCES ${AV_SOURCES})
list(APPEND AV_TEST_TRANSCODE_HEADERS ${AV_HEADERS})

set(AV_TEST_TRANSCODE_TARGET av-test-transcode)

link_directories(${FFMPEG_LIBRARY_DIRS})

add_executable(${AV_TEST_TRANSCODE_TARGET} ${AV_TEST_TRANSCODE_SOURCES})
target_link_libraries(${AV_TEST_TRANSCODE_TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    ${FFMPEG_LIBRARIES}
    avcpp
)

if(WIN32)
  target_link_libraries(${AV_TEST_TRANSCODE_TARGET} ws2_32)
endif()
