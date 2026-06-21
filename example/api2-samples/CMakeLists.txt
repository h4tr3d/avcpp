cmake_minimum_required (VERSION 3.19)

# transcoder sources

if(NOT DEFINED PROJECT_NAME)
    set(AVCPP_SAMPLES_NOT_SUBPROJECT ON)
endif()

project(AvCppSamples LANGUAGES CXX VERSION 2.4.0)

if(AVCPP_SAMPLES_NOT_SUBPROJECT)
    find_package(avcpp REQUIRED)
endif()

set(TARGETS
  api2-decode
  api2-decode-encode-video
  api2-scale-video
  api2-decode-audio
  api2-decode-rasample-audio
  api2-decode-encode-audio
  api2-decode-filter-encode
  api2-dict-basic
  api2-timestamp
  api2-demux-seek
  api2-hw-encode
  api2-decode-raw-h264
)

if(NOT WIN32)
  list(APPEND TARGETS api2-remux)
endif()

if (AV_DISABLE_AVFORMAT)
  list(REMOVE_ITEM TARGETS
    api2-decode
    api2-decode-encode-video
    api2-decode-encode-audio
    api2-decode-filter-encode
    api2-hw-encode
    api2-decode-audio
    api2-remux
    api2-scale-video
    api2-decode-rasample-audio
    api2-demux-seek
  )
endif()

if (AV_DISABLE_AVFILTER)
  list(REMOVE_ITEM TARGETS
    api2-decode-filter-encode
  )
endif()

if (NOT "cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
  list(REMOVE_ITEM TARGETS api2-decode-raw-h264)
endif()

# helper for the sample target
function(api2_sample_target target)
  add_executable(${target} ${target}.cpp)

  target_link_libraries(${target}
      avcpp::avcpp
  )

  if (AVCPP_WARNING_OPTIONS)
      target_compile_options(${target} PRIVATE ${AVCPP_WARNING_OPTIONS})
  endif()

  if(WIN32)
      target_link_libraries(${target} ws2_32)
  endif()
endfunction()

# Common targets
foreach(target ${TARGETS})
  api2_sample_target(${target})
endforeach()

# Additional options
if ("api2-decode-raw-h264" IN_LIST TARRGETS)
  set_property(TARGET api2-decode-raw-h264 PROPERTY CXX_STANDARD 20)
endif()

