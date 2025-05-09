cmake_minimum_required(VERSION 3.19)

project(avcpp_tests LANGUAGES CXX VERSION 0.0.1)

enable_testing()

#(Catch2 REQUIRED)

add_library(test_main STATIC test-main.cpp)
target_link_libraries(test_main PUBLIC Catch2::Catch2)

add_executable(test_executor
    Frame.cpp
    AvDeleter.cpp
    Packet.cpp
    Format.cpp
    Rational.cpp
    Timestamp.cpp
    Codec.cpp
    PixelSampleFormat.cpp
    Common.cpp)
target_link_libraries(test_executor PUBLIC Catch2::Catch2 test_main avcpp::avcpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../catch2/contrib")
message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}/../catch2/contrib")

include(CTest)
include(Catch)
catch_discover_tests(test_executor)
