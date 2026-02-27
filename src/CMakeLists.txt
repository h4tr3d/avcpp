include(GNUInstallDirs)

file(GLOB_RECURSE AV_SOURCES "avcpp/*.cpp")
file(GLOB_RECURSE AV_HEADERS "avcpp/*.h" "avcpp/*.hpp")

set(AV_NAMESPACE avcpp)
set(AV_BASENAME avcpp)

# setup versions for avconfig.h
macro (setup_package_version_avcpp _pkg)
    foreach(TYPE MAJOR MINOR PATCH TWEAK)
        if (DEFINED ${_pkg}_VERSION_${TYPE})
            set(AVCPP_${_pkg}_VERSION_${TYPE} ${${_pkg}_VERSION_${TYPE}})
        endif()
    endforeach()
endmacro()
foreach(PKG ${CORE_FFMPEG_COMPONENTS} ${OPTIONAL_FFMPEG_COMPONENTS})
    message(STATUS "Setup version defines for: ${PKG}")
    setup_package_version(${PKG})
    setup_package_version_avcpp(${PKG})
endforeach()

# This is a little gross as it compiles everything twice, but it's good enough for my needs.
list (APPEND AV_TARGETS ${AV_BASENAME})

if (AV_ENABLE_STATIC AND AV_ENABLE_SHARED)
    list (APPEND AV_TARGETS ${AV_BASENAME}-static)
endif()

foreach(TARGET ${AV_TARGETS})

    if (${TARGET} MATCHES "-static")
        set(TYPE STATIC)
    else()
        if (AV_ENABLE_SHARED)
            set(TYPE SHARED)
        else()
            set(TYPE STATIC)
        endif()
    endif()

    add_library(${TARGET} ${TYPE} ${AV_SOURCES} ${AV_HEADERS})

    # allow use static library to build shared one
    if (AV_ENABLE_STATIC_PIC AND ${TYPE} MATCHES "STATIC")
        message(STATUS "Enable POSITION_INDEPENDENT_CODE (-fPIC) build for static library")
        set_target_properties(${TARGET} PROPERTIES POSITION_INDEPENDENT_CODE ON)
    endif()

    target_compile_options(${TARGET} PRIVATE ${AVCPP_WARNING_OPTIONS})
    target_compile_definitions(${TARGET} PUBLIC __STDC_CONSTANT_MACROS)
    target_link_libraries(${TARGET} PRIVATE Threads::Threads PUBLIC FFmpeg::FFmpeg)
    target_include_directories(${TARGET}
        PUBLIC
          $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
          $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        #PRIVATE
        #  ${CMAKE_CURRENT_SOURCE_DIR}
      )
    set_target_properties(${TARGET} PROPERTIES PUBLIC_HEADER "${AV_HEADERS};${CMAKE_CURRENT_BINARY_DIR}/avcpp/avconfig.h")

    # If not CMAKE_CXX_STANARD provided globally (subproject), use latest supported one
    if (NOT CMAKE_CXX_STANDARD)
        if ("cxx_std_26" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
            target_compile_features(${TARGET} PUBLIC cxx_std_26)
            set(AVCPP_CXX_STANDARD 26)
        elseif ("cxx_std_23" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
            target_compile_features(${TARGET} PUBLIC cxx_std_23)
            set(AVCPP_CXX_STANDARD 23)
        elseif("cxx_std_20" IN_LIST CMAKE_CXX_COMPILE_FEATURES)
            target_compile_features(${TARGET} PUBLIC cxx_std_20)
            set(AVCPP_CXX_STANDARD 20)
        else()
            target_compile_features(${TARGET} PUBLIC cxx_std_17)
            set(AVCPP_CXX_STANDARD 17)
        endif()
    else()
        set(AVCPP_CXX_STANDARD ${CMAKE_CXX_STANDARD})
    endif()

    if(WIN32)
      target_link_libraries(${TARGET} PRIVATE ws2_32)
    endif()

    if(MSVC AND TYPE STREQUAL "SHARED")
        set_target_properties(${TARGET} PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS TRUE)
    endif()

    add_library(${AV_NAMESPACE}::${TARGET} ALIAS ${TARGET})
endforeach()

if (TARGET ${AV_BASENAME}-static)
    set_target_properties(${AV_BASENAME}-static PROPERTIES OUTPUT_NAME ${AV_BASENAME})
endif()

# prepare configuration file
configure_file(avcpp/avconfig.h.in avcpp/avconfig.h @ONLY)

########################### Installation ######################################

if (AVCPP_NOT_SUBPROJECT)
    # FFmpeg part, Ugly WA for imported libraries
    #export(TARGETS FFmpeg
    #    NAMESPACE FFmpeg::
    #    APPEND
    #    FILE ${CMAKE_CURRENT_BINARY_DIR}/avcpp-targets.cmake)

    # FFmpeg may not come from us
    if (TARGET FFmpeg)
        SET(TARGET_FFMPEG FFmpeg)
    endif ()
    install(TARGETS ${AV_TARGETS} ${TARGET_FFMPEG}
        EXPORT avcpp-targets
        LIBRARY DESTINATION        ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION        ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION        ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION       ${CMAKE_INSTALL_INCLUDEDIR}/avcpp
        PRIVATE_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/avcpp
        PUBLIC_HEADER DESTINATION  ${CMAKE_INSTALL_INCLUDEDIR}/avcpp)

    #cmake_policy(SET CMP0022 OLD)
    install(EXPORT avcpp-targets
        FILE avcpp-targets.cmake
        NAMESPACE avcpp::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avcpp)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
      "avcpp/Config.cmake.in"
      "avcpp/avcpp-config.cmake"
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avcpp
#      PATH_VARS
#        CMAKE_INSTALL_LIBDIR
      )
    write_basic_package_version_file("avcpp/avcpp-config-version.cmake"
        VERSION ${AVCPP_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(
        FILES
          "${CMAKE_CURRENT_BINARY_DIR}/avcpp/avcpp-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/avcpp/avcpp-config-version.cmake"
        DESTINATION
          "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}/cmake/avcpp"
      )

    if (PKG_CONFIG_FOUND)
        # convert lists of link libraries into -lstdc++ -lm and etc
        string(REPLACE " " ";" CXX_STD_LIBS "${CMAKE_CXX_STANDARD_LIBRARIES}")
        string(REPLACE " " ";" C_STD_LIBS "${CMAKE_C_STANDARD_LIBRARIES}")
        set(TOTAL_LIBS ${CMAKE_IMPLICIT_LINK_LIBRARIES} ${CXX_STD_LIBS} ${C_STD_LIBS} ${PLATFORM_LIBS})
        foreach (LIB ${TOTAL_LIBS})
            if (IS_ABSOLUTE ${LIB} AND EXISTS ${LIB})
                string(REPLACE
                    "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}"
                    "\${libdir}"
                    LIB_OUT
                    ${LIB})
                list(APPEND PLIBLIST "${LIB_OUT}")
            else()
                string(SUBSTRING ${LIB} 0 2 LIB_BEGIN)
                if (LIB_BEGIN STREQUAL "-l")
                    list(APPEND PLIBLIST "${LIB}")
                else()
                    list(APPEND PLIBLIST "-l${LIB}")
                endif()
            endif()
        endforeach()

        # blacklink of the libraries that should not be in Libs.private
        if (PLIBLIST)
            list(REMOVE_ITEM PLIBLIST "-lc" "-lpthread" "-pthread" "-l-pthread")
            list(REMOVE_DUPLICATES PLIBLIST)
            string(REPLACE ";" " " PRIVATE_LIBS "${PLIBLIST}")
        else()
            set(PRIVATE_LIBS "")
        endif()

        # compose pkg-config Requires line based on the selected FFmpeg components
        set(AVCPP_PC_REQUIRES )
        foreach (FFCOMP ${CORE_FFMPEG_COMPONENTS} ${OPTIONAL_FFMPEG_COMPONENTS})
            string(TOLOWER ${FFCOMP} _ffcomp)
            list(APPEND AVCPP_PC_REQUIRES "lib${_ffcomp} >= ${${FFCOMP}_VERSION}")
        endforeach()
        string(REPLACE ";" ", " AVCPP_PC_REQUIRES "${AVCPP_PC_REQUIRES}")

        message(STATUS "LIBS: ${PLIBLIST} / ${PRIVATE_LIBS}")
        message(STATUS "PC REQ: ${AVCPP_PC_REQUIRES}")

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/avcpp/libavcpp.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/avcpp/libavcpp.pc"
            @ONLY
            )

        install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/avcpp/libavcpp.pc"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

    endif()
endif()

