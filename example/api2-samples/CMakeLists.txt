# transcoder sources

include_directories(../../src)

link_directories(${FFMPEG_LIBRARY_DIRS})

set(TARGETS
api2-decode
api2-decode-encode-video
)

add_executable(api2-decode av-decode.cpp)
add_executable(api2-decode-encode-video av-decode-encode-video.cpp)

foreach(target ${TARGETS})
   target_link_libraries(${target}
      ${CMAKE_THREAD_LIBS_INIT}
      ${FFMPEG_LIBRARIES}
      avcpp
  )
  if(WIN32)
  target_link_libraries(${AV_TARGET}
    ws2_32
  )
endif()

endforeach()
