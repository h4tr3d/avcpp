include_directories(.)

file(GLOB_RECURSE AV_SOURCES "*.cpp")
file(GLOB_RECURSE AV_HEADERS "*.h" "*.hpp")

set(AV_TARGET avcpp)

include_directories(${Boost_INCLUDE_DIRS})

add_library(${AV_TARGET} STATIC ${AV_SOURCES})

target_link_libraries(${AV_TARGET}
    ${CMAKE_THREAD_LIBS_INIT}
    ${Boost_LIBRARIES}
    ${FFMPEG_LIBRARIES}
)

if(WIN32)
target_link_libraries(${AV_TARGET}
    ws2_32
)
endif()
