# transcoder sources

include_directories(../../src)

link_directories(${FFMPEG_LIBRARY_DIRS})

set(TARGETS
api2-decode
api2-decode-encode-video
api2-scale-video
api2-decode-audio
api2-decode-rasample-audio
api2-decode-encode-audio
api2-dict-basic
)

foreach(target ${TARGETS})
   add_executable(${target} ${target}.cpp)

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
