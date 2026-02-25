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
#   SWRESAMPLE
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
# Copyright (c) 2017-2026, Alexander Drozdov, <adrozdoff@gmail.com>
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
### Macro: parse_lib_version
#
# Reads the file '${_pkgconfig}/version.h' (and version_major.h) in the component's _INCLUDE_DIR,
# and parses #define statements for COMPONENT_VERSION_(MAJOR|MINOR|PATCH)
# into a dotted string ${_component}_VERSION.
#
# Needed if the version is not supplied via pkgconfig's PC_${_component}_VERSION
macro(parse_lib_version _component _libname )
  set(_version_h       "${${_component}_INCLUDE_DIRS}/${_libname}/version.h")
  set(_version_major_h "${${_component}_INCLUDE_DIRS}/${_libname}/version_major.h")

  string(TOUPPER "${_libname}" _prefix)

  if(EXISTS "${_version_major_h}")
    file(STRINGS "${_version_major_h}" _maj_version
        REGEX "^[ \t]*#define[ \t]+${_prefix}_VERSION_MAJOR[ \t]+[0-9]+[ \t]*$")
      string(REGEX REPLACE
        "^.*${_prefix}_VERSION_MAJOR[ \t]+([0-9]+)[ \t]*$"
        "\\1"
        _maj_match "${_maj_version}")
    set(_parts_major "${_maj_match}")
  endif()

  if(EXISTS "${_version_h}")
    #message(STATUS "Parsing ${_component} version from ${_version_h}")
    set(_parts)

    # prepend initially major part, if any
    if (_parts_major)
        list(APPEND _parts "${_parts_major}")
    endif()

    foreach(_lvl MAJOR MINOR MICRO)
      if (_parts_major AND "${_lvl}" STREQUAL "MAJOR")
        continue()
      endif()

      file(STRINGS "${_version_h}" _lvl_version
        REGEX "^[ \t]*#define[ \t]+${_prefix}_VERSION_${_lvl}[ \t]+[0-9]+[ \t]*$")
      string(REGEX REPLACE
        "^.*${_prefix}_VERSION_${_lvl}[ \t]+([0-9]+)[ \t]*$"
        "\\1"
        _lvl_match "${_lvl_version}")
      list(APPEND _parts "${_lvl_match}")
    endforeach()
    list(JOIN _parts "." ${_component}_VERSION)
    message(STATUS "Found ${_component} version: ${${_component}_VERSION}")
  endif()
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
  find_package(PkgConfig QUIET)
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

  find_library(${_component}_LIBRARIES NAMES ${PC_${_component}_LIBRARIES} ${_library}
      HINTS
      ${PC_${_component}_LIBDIR}
      ${PC_${_component}_LIBRARY_DIRS}
      ${PC_FFMPEG_LIBRARY_DIRS}
  )

  # Take version from PkgConfig, or parse from its version.h header
  if (PC_${_component}_VERSION)
    set(${_component}_VERSION ${PC_${_component}_VERSION})
  else()
    parse_lib_version(${_component} ${_pkgconfig})
  endif()

  set(${_component}_DEFINITIONS  ${PC_${_component}_CFLAGS_OTHER})

  set_component_found(${_component})

  mark_as_advanced(
    ${_component}_INCLUDE_DIRS
    ${_component}_LIBRARIES
    ${_component}_DEFINITIONS
    ${_component}_VERSION)

endmacro()

macro(setup_component _component _pkgconfig _library _header)
  set(_${_component}_PKGCONFIG_NAME ${_pkgconfig})
  set(_${_component}_LIBRARY_NAME   ${_library})
  set(_${_component}_HEADER_NAME    ${_header})
endmacro()

# Check for cached results. If there are skip the costly part.
if (TRUE)
  # setup internal vars for all possible components
  setup_component(AVCODEC    libavcodec    avcodec  libavcodec/avcodec.h)
  setup_component(AVFORMAT   libavformat   avformat libavformat/avformat.h)
  setup_component(AVDEVICE   libavdevice   avdevice libavdevice/avdevice.h)
  setup_component(AVUTIL     libavutil     avutil   libavutil/avutil.h)
  setup_component(AVFILTER   libavfilter   avfilter libavfilter/avfilter.h)
  setup_component(SWSCALE    libswscale    swscale  libswscale/swscale.h)
  setup_component(POSTPROC   libpostproc   postproc libpostproc/postprocess.h)
  setup_component(SWRESAMPLE libswresample swresample libswresample/swresample.h)

  # Check if the required components were found and add their stuff to the FFMPEG_* vars.
  foreach (_component ${FFmpeg_FIND_COMPONENTS})
    # find only requested componsnts
    find_component(${_component} ${_${_component}_PKGCONFIG_NAME} ${_${_component}_LIBRARY_NAME} ${_${_component}_HEADER_NAME})
    if (${_component}_FOUND)
      message(STATUS "Libs: ${${_component}_LIBRARIES} | ${PC_${_component}_LIBRARIES}")

      # message(STATUS "Required component ${_component} present.")
      set(FFMPEG_LIBRARIES    ${FFMPEG_LIBRARIES}    ${${_component}_LIBRARIES})
      set(FFMPEG_DEFINITIONS  ${FFMPEG_DEFINITIONS}  ${${_component}_DEFINITIONS})

      list(APPEND FFMPEG_INCLUDE_DIRS ${${_component}_INCLUDE_DIRS})

      string(TOLOWER ${_component} _lowerComponent)
      if (NOT TARGET FFmpeg::${_lowerComponent})
        add_library(FFmpeg::${_lowerComponent} INTERFACE IMPORTED)
        set_target_properties(FFmpeg::${_lowerComponent} PROPERTIES
            INTERFACE_COMPILE_OPTIONS "${${_component}_DEFINITIONS}"
            INTERFACE_INCLUDE_DIRECTORIES ${${_component}_INCLUDE_DIRS}
            INTERFACE_LINK_LIBRARIES "${${_component}_LIBRARY} ${${_component}_LIBRARIES} ${PC_${_component}_LIBRARIES}"
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

  mark_as_advanced(FFMPEG_INCLUDE_DIRS
                   FFMPEG_LIBRARIES
                   FFMPEG_DEFINITIONS
                   FFMPEG_LIBRARY_DIRS)

endif ()

if (NOT TARGET FFmpeg::FFmpeg)
  add_library(FFmpeg INTERFACE)
  set_target_properties(FFmpeg PROPERTIES
      INTERFACE_COMPILE_OPTIONS "${FFMPEG_DEFINITIONS}"
      INTERFACE_INCLUDE_DIRECTORIES "${FFMPEG_INCLUDE_DIRS}"
      INTERFACE_LINK_LIBRARIES "${FFMPEG_LIBRARIES}")
  add_library(FFmpeg::FFmpeg ALIAS FFmpeg)
endif()

# Now set the noncached _FOUND vars for the components.
foreach (_component AVCODEC AVDEVICE AVFORMAT AVUTIL POSTPROCESS SWSCALE)
  set_component_found(${_component})
endforeach ()

# Compile the list of required vars
set(_FFmpeg_REQUIRED_VARS FFMPEG_LIBRARIES FFMPEG_INCLUDE_DIRS)
foreach (_component ${FFmpeg_FIND_COMPONENTS})
  list(APPEND _FFmpeg_REQUIRED_VARS ${_component}_LIBRARIES ${_component}_INCLUDE_DIRS)
endforeach ()

# Give a nice error message if some of the required vars are missing.
find_package_handle_standard_args(FFmpeg DEFAULT_MSG ${_FFmpeg_REQUIRED_VARS})
