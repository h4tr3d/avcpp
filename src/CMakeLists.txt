include_directories(.)

file(GLOB_RECURSE AV_SOURCES "*.cpp")
file(GLOB_RECURSE AV_HEADERS "*.h" "*.hpp")

set(AV_TARGET avcpp)

add_library(${AV_TARGET}        SHARED ${AV_SOURCES})
add_library(${AV_TARGET}_static STATIC ${AV_SOURCES})

set_target_properties(${AV_TARGET}_static PROPERTIES OUTPUT_NAME ${AV_TARGET})

target_link_libraries(${AV_TARGET}
    Threads::Threads
    FFmpeg::FFmpeg
)

target_link_libraries(${AV_TARGET}_static
    Threads::Threads
    FFmpeg::FFmpeg
)

set(PLATFORM_LIBS
    ${CMAKE_THREAD_LIBS_INIT}
    ${FFMPEG_LIBRARIES})

if(WIN32)
  target_link_libraries(${AV_TARGET} ws2_32)
  target_link_libraries(${AV_TARGET}_static ws2_32)
  list(APPEND PLATFORM_LIBS ws2_32)
endif()

########################### Installation ######################################

set(LIB_INSTALL_DIR lib     CACHE STRING "Install location of libraries")
set(BIN_INSTALL_DIR bin     CACHE STRING "Install location of executables")
set(INC_INSTALL_DIR include CACHE STRING "Intall location of headers")

install(TARGETS ${AV_TARGET} ${AV_TARGET}_static
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
        RUNTIME DESTINATION ${BIN_INSTALL_DIR})

#install(FILES ${AV_HEADERS} DESTINATION ${INC_INSTALL_DIR})
install(DIRECTORY ""
        DESTINATION ${INC_INSTALL_DIR}/avcpp
        FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.inl")

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
        list(REMOVE_ITEM PLIBLIST "-lc" "-lpthread")
        list(REMOVE_DUPLICATES PLIBLIST)
        string(REPLACE ";" " " PRIVATE_LIBS "${PLIBLIST}")
    else()
        set(PRIVATE_LIBS "")
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/libavcpp.pc.in"
        "${CMAKE_CURRENT_BINARY_DIR}/libavcpp.pc"
        @ONLY
    )

    install(FILES       "${CMAKE_CURRENT_BINARY_DIR}/libavcpp.pc"
            DESTINATION "${LIB_INSTALL_DIR}/pkgconfig")

endif()


