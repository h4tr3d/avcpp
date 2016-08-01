# transcoder sources

include_directories(../../src)

file(GLOB_RECURSE AV_TEST_SOURCES "*.cpp")
file(GLOB_RECURSE AV_TEST_HEADERS "*.h" "*.hpp")

list(APPEND AV_TEST_SOURCES ${AV_SOURCES})
list(APPEND AV_TEST_HEADERS ${AV_HEADERS})

set(AV_TEST_TARGET av-test)

link_directories(${FFMPEG_LIBRARY_DIRS})

add_executable(${AV_TEST_TARGET} ${AV_TEST_SOURCES})
target_link_libraries(${AV_TEST_TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    avcpp
)

if(WIN32)
  target_link_libraries(${AV_TEST_TARGET} ws2_32)
endif()
