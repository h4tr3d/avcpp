include(GNUInstallDirs)

file(GLOB_RECURSE AV_SOURCES "*.cpp")
file(GLOB_RECURSE AV_HEADERS "*.h" "*.hpp")

set(AV_NAMESPACE avcpp)
set(AV_BASENAME avcpp)

# This is a little gross as it compiles everything twice, but it's good enough for my needs.
if (AV_ENABLE_SHARED)
    list (APPEND AV_TARGETS ${AV_BASENAME})
endif()

if (AV_ENABLE_STATIC)
    list (APPEND AV_TARGETS ${AV_BASENAME}-static)
endif()

foreach(TARGET ${AV_TARGETS})
    set(TYPE SHARED)
    if (${TARGET} MATCHES "-static")
        set(TYPE STATIC)
    endif()

    add_library(${TARGET} ${TYPE} ${AV_SOURCES} ${AV_HEADERS})

    target_compile_options(${TARGET} PRIVATE ${AV_WARNING_OPTIONS})
    target_link_libraries(${TARGET} PRIVATE Threads::Threads PUBLIC FFmpeg::FFmpeg)
    target_include_directories(${TARGET}
        PUBLIC
          $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>
          $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/avcpp>
        PRIVATE
          ${CMAKE_CURRENT_SOURCE_DIR})
    set_target_properties(${TARGET} PROPERTIES PUBLIC_HEADER "${AV_HEADERS}")

    if(WIN32)
      target_link_libraries(${TARGET} PRIVATE ws2_32)
    endif()

    add_library(${AV_NAMESPACE}::${TARGET} ALIAS ${TARGET})
endforeach()

if (TARGET ${AV_BASENAME}-static)
    set_target_properties(${AV_BASENAME}-static PROPERTIES OUTPUT_NAME ${AV_BASENAME})
endif()

########################### Installation ######################################

if (AVCPP_NOT_SUBPROJECT)
    # FFmpeg part, Ugly WA for imported libraries
    #export(TARGETS FFmpeg
    #    NAMESPACE FFmpeg::
    #    APPEND
    #    FILE ${CMAKE_CURRENT_BINARY_DIR}/avcpp-targets.cmake)

    install(TARGETS ${AV_TARGETS} FFmpeg
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
        DESTINATION lib/cmake/avcpp)

    include(CMakePackageConfigHelpers)
    configure_package_config_file(
      "Config.cmake.in"
      "avcpp-config.cmake"
      INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/avcpp
#      PATH_VARS
#        CMAKE_INSTALL_LIBDIR
      )
    write_basic_package_version_file("avcpp-config-version.cmake"
        VERSION 2.0.99
        COMPATIBILITY SameMajorVersion)
    install(
        FILES
          "${CMAKE_CURRENT_BINARY_DIR}/avcpp-config.cmake"
          "${CMAKE_CURRENT_BINARY_DIR}/avcpp-config-version.cmake"
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

        message(STATUS "LIBS: ${PLIBLIST} / ${PRIVATE_LIBS}")

        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/libavcpp.pc.in"
            "${CMAKE_CURRENT_BINARY_DIR}/libavcpp.pc"
            @ONLY
            )

        install(FILES   "${CMAKE_CURRENT_BINARY_DIR}/libavcpp.pc"
            DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/pkgconfig")

    endif()
endif()

