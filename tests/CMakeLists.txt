cmake_minimum_required(VERSION 3.5)

project(avcpp_tests LANGUAGES CXX VERSION 0.0.1)

enable_testing()

#(Catch2 REQUIRED)

add_library(test_main OBJECT test-main.cpp)
target_link_libraries(test_main PUBLIC Catch2::Catch2)

add_executable(test_executor
    VideoFrame.cpp
    $<TARGET_OBJECTS:test_main>)
target_link_libraries(test_executor PUBLIC Catch2::Catch2 PRIVATE AvCpp::AvCpp)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/catch2/contrib")
message(STATUS "${CMAKE_SOURCE_DIR}/catch2/contrib")

include(CTest)
include(Catch)
catch_discover_tests(test_executor)
