cmake_minimum_required(VERSION 3.11)

project(avcpp_tests LANGUAGES CXX VERSION 0.0.1)

enable_testing()

#(Catch2 REQUIRED)

add_library(test_main STATIC test-main.cpp)
target_link_libraries(test_main PUBLIC Catch2::Catch2)

add_executable(test_executor
    VideoFrame.cpp)
target_link_libraries(test_executor PUBLIC Catch2::Catch2 test_main PRIVATE AvCpp::AvCpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../catch2/contrib")
message(STATUS "${CMAKE_CURRENT_SOURCE_DIR}/../catch2/contrib")

include(CTest)
include(Catch)
catch_discover_tests(test_executor)
