#.rst:
# FindFFmpeg
# ----------
#
# Try to find the required ffmpeg components (default: AVFORMAT, AVUTIL, AVCODEC)
#
# Next variables can be used to hint FFmpeg libs search:
#
# ::
#
#   PC_<component>_LIBRARY_DIRS
#   PC_FFMPEG_LIBRARY_DIRS
#   PC_<component>_INCLUDE_DIRS
#   PC_FFMPEG_INCLUDE_DIRS
#
# Once done this will define
#
# ::
#
#   FFMPEG_FOUND         - System has the all required components.
#   FFMPEG_INCLUDE_DIRS  - Include directory necessary for using the required components headers.
#   FFMPEG_LIBRARIES     - Link these to use the required ffmpeg components.
#   FFMPEG_LIBRARY_DIRS  - Link directories
#   FFMPEG_DEFINITIONS   - Compiler switches required for using the required ffmpeg components.
#
# For each of the components it will additionally set.
#
# ::
#
#   AVCODEC
#   AVDEVICE
#   AVFORMAT
#   AVFILTER
#   AVUTIL
#   POSTPROC
#   SWSCALE
#
# the following variables will be defined
#
# ::
#
#   <component>_FOUND        - System has <component>
#   <component>_INCLUDE_DIRS - Include directory necessary for using the <component> headers
#   <component>_LIBRARIES    - Link these to use <component>
#   <component>_LIBRARY_DIRS - Link directories
#   <component>_DEFINITIONS  - Compiler switches required for using <component>
#   <component>_VERSION      - The components version
#
# the following import targets is created
#
# ::
#
#   FFmpeg::FFmpeg - for all components
#   FFmpeg::<component> - where <component> in lower case (FFmpeg::avcodec) for each components
#
# Windows cross building
# ----------------------
#
# pkg-config is used on the Nix platforms. You should point proper pkg-config wrappers:
#
# ::
#
#   PKG_CONFIG_EXECUTABLE - simplest way is a ponting proper pkg-config wrapper, if provided:
#
#       /usr/bin/x86_64-w64-mingw32-pkg-config
#       /usr/bin/i686-w64-mingw32-pkg-config
#
#          Sample usage in the Toolchain file:
#             find_program(PKG_CONFIG_EXECUTABLE NAMES ${COMPILER_PREFIX}-pkg-config)
#
#          It also can be pointed via ENV: 
#             export PKG_CONFIG=...
#
#   Env variables of the pkg-config:
#       see `man pkg-config`
#
#
# Credits
# -------
#
# Copyright (c) 2006, Matthias Kretz, <kretz@kde.org>
# Copyright (c) 2008, Alexander Neundorf, <neundorf@kde.org>
# Copyright (c) 2011, Michael Jansen, <kde@michael-jansen.biz>
# Copyright (c) 2017, Alexander Drozdov, <adrozdoff@gmail.com>
#
# Redistribution and use is allowed according to the terms of the BSD license.
# For details see the accompanying COPYING-CMAKE-SCRIPTS file.

include(FindPackageHandleStandardArgs)

# The default components were taken from a survey over other FindFFMPEG.cmake files
if (NOT FFmpeg_FIND_COMPONENTS)
  set(FFmpeg_FIND_COMPONENTS AVCODEC AVFORMAT AVUTIL)
endif ()

#
### Macro: set_component_found
#
# Marks the given component as found if both *_LIBRARIES AND *_INCLUDE_DIRS is present.
#
macro(set_component_found _component )
  if (${_component}_LIBRARIES AND ${_component}_INCLUDE_DIRS)
    # message(STATUS "  - ${_component} found.")
    set(${_component}_FOUND TRUE)
  else ()
    # message(STATUS "  - ${_component} not found.")
  endif ()
endmacro()

#
### Macro: find_component
#
# Checks for the given component by invoking pkgconfig and then looking up the libraries and
# include directories.
#
macro(find_component _component _pkgconfig _library _header)

  # use pkg-config to get the directories and then use these values
  # in the FIND_PATH() and FIND_LIBRARY() calls
  find_package(PkgConfig)
  if (PKG_CONFIG_FOUND)
    pkg_check_modules(PC_${_component} ${_pkgconfig})
  endif ()

  find_path(${_component}_INCLUDE_DIRS ${_header}
    HINTS
      ${PC_${_component}_INCLUDEDIR}
      ${PC_${_component}_INCLUDE_DIRS}
      ${PC_FFMPEG_INCLUDE_DIRS}
    PATH_SUFFIXES
      ffmpeg
  )

  find_library(${_component}_LIBRARY NAMES ${PC_${_component}_LIBRARIES} ${_library}
      HINTS
      ${PC_${_component}_LIBDIR}
      ${PC_${_component}_LIBRARY_DIRS}
      ${PC_FFMPEG_LIBRARY_DIRS}
  )

  #message(STATUS "L0: ${${_component}_LIBRARIES}")
  #message(STATUS "L1: ${PC_${_component}_LIBRARIES}")
  #message(STATUS "L2: ${_library}")

  set(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER} CACHE STRING "The ${_component} CFLAGS.")
  set(${_component}_VERSION      ${PC_${_component}_VERSION}      CACHE STRING "The ${_component} version number.")
  set(${_component}_LIBRARY_DIRS ${PC_${_component}_LIBRARY_DIRS} CACHE STRING "The ${_component} library dirs.")
  set(${_component}_LIBRARIES    ${PC_${_component}_LIBRARIES}    CACHE STRING "The ${_component} libraries.")

  set_component_found(${_component})

  mark_as_advanced(
    ${_component}_LIBRARY
    ${_component}_INCLUDE_DIRS
    ${_component}_LIBRARY_DIRS
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()


# Check for cached results. If there are skip the costly part.
if (NOT FFMPEG_LIBRARIES)

  # Check for all possible component.
  find_component(AVCODEC    libavcodec    avcodec  libavcodec/avcodec.h)
  find_component(AVFORMAT   libavformat   avformat libavformat/avformat.h)
  find_component(AVDEVICE   libavdevice   avdevice libavdevice/avdevice.h)
  find_component(AVUTIL     libavutil     avutil   libavutil/avutil.h)
  find_component(AVFILTER   libavfilter   avfilter libavfilter/avfilter.h)
  find_component(SWSCALE    libswscale    swscale  libswscale/swscale.h)
  find_component(POSTPROC   libpostproc   postproc libpostproc/postprocess.h)
  find_component(SWRESAMPLE libswresample swresample libswresample/swresample.h)

  # Check if the required components were found and add their stuff to the FFMPEG_* vars.
  foreach (_component ${FFmpeg_FIND_COMPONENTS})
    if (${_component}_FOUND)
      message(STATUS "Libs: ${${_component}_LIBRARIES} | ${PC_${_component}_LIBRARIES}")

      # message(STATUS "Required component ${_component} present.")
      set(FFMPEG_LIBRARIES    ${FFMPEG_LIBRARIES}    ${${_component}_LIBRARY} ${${_component}_LIBRARIES})
      set(FFMPEG_DEFINITIONS  ${FFMPEG_DEFINITIONS}  ${${_component}_DEFINITIONS})

      list(APPEND FFMPEG_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})
      list(APPEND FFMPEG_LIBRARY_DIRS ${${_component}_LIBRARY_DIRS})

      string(TOLOWER ${_component} _lowerComponent)
      if (NOT TARGET FFmpeg::${_lowerComponent})
        add_library(FFmpeg::${_lowerComponent} INTERFACE IMPORTED)
        set_target_properties(FFmpeg::${_lowerComponent} PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${${_component}_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES ${${_component}_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES "${${_component}_LIBRARY} ${${_component}_LIBRARIES} ${PC_${_component}_LIBRARIES}"
            INTERFACE_LINK_DIRECTORIES "${PC_${_component}_LIBDIR} ${PC_${_component}_LIBRARY_DIRS} ${PC_FFMPEG_LIBRARY_DIRS}"
            IMPORTED_LINK_INTERFACE_MULTIPLICITY 3)
      endif()
    else()
      # message(STATUS "Required component ${_component} missing.")
    endif()
  endforeach ()

  # Build the include path with duplicates removed.
  if (FFMPEG_INCLUDE_DIRS)
    list(REMOVE_DUPLICATES FFMPEG_INCLUDE_DIRS)
  endif ()

  # cache the vars.
  set(FFMPEG_INCLUDE_DIRS ${FFMPEG_INCLUDE_DIRS} CACHE STRING "The FFmpeg include directories." FORCE)
  set(FFMPEG_LIBRARIES    ${FFMPEG_LIBRARIES}    CACHE STRING "The FFmpeg libraries." FORCE)
  set(FFMPEG_DEFINITIONS  ${FFMPEG_DEFINITIONS}  CACHE STRING "The FFmpeg cflags." FORCE)
  set(FFMPEG_LIBRARY_DIRS ${FFMPEG_LIBRARY_DIRS} CACHE STRING "The FFmpeg library dirs." FORCE)

  mark_as_advanced(FFMPEG_INCLUDE_DIRS
                   FFMPEG_LIBRARIES
                   FFMPEG_DEFINITIONS
                   FFMPEG_LIBRARY_DIRS)

endif ()

if (NOT TARGET FFmpeg::FFmpeg)
  add_library(FFmpeg INTERFACE)
  set_target_properties(FFmpeg PROPERTIES
      INTERFACE_COMPILE_OPTIONS "${FFMPEG_DEFINITIONS}"
      INTERFACE_INCLUDE_DIRECTORIES ${FFMPEG_INCLUDE_DIRS}
      INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}"
      INTERFACE_LINK_DIRECTORIES "${FFMPEG_LIBRARY_DIRS}")
  add_library(FFmpeg::FFmpeg ALIAS FFmpeg)
endif()

set(_FFmpeg_REQUIRED_VARS)
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  set_component_found(${_component})
  # Compile the list of required vars
  if (FFmpeg_FIND_REQUIRED_${_component})
    list(APPEND _FFmpeg_REQUIRED_VARS ${_component}_LIBRARIES ${_component}_INCLUDE_DIRS)
  endif()
endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg 
  REQUIRED_VARS FFMPEG_INCLUDE_DIRS FFMPEG_LIBRARIES ${_FFmpeg_REQUIRED_VARS})
