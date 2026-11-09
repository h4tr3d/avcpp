# Optional binary fixtures. The repository is private, so fixture tests are
# opt-in; point FETCHCONTENT_SOURCE_DIR_AVCPP_TEST_DATA at a local checkout to
# work without network access.
option(AVCPP_TEST_DATA "Fetch the avcpp-test-data fixtures and enable fixture tests" Off)

if (AVCPP_TEST_DATA)
    include(FetchContent)
    FetchContent_Declare(avcpp_test_data
        GIT_REPOSITORY git@github.com:h4tr3d/avcpp-test-data.git
        GIT_TAG v1
    )
    FetchContent_MakeAvailable(avcpp_test_data)
endif()

add_executable(test_executor
    Frame.cpp
    AvDeleter.cpp
    Packet.cpp
    Format.cpp
    Rational.cpp
    Timestamp.cpp
    Codec.cpp
    PixelSampleFormat.cpp
    Common.cpp
    Buffer.cpp
    FormatCustomIO_test.cpp
    CodecContext.cpp
    CodecContextSendReceive.cpp
    Fixtures.cpp
    Patterns.cpp)
target_link_libraries(test_executor PRIVATE Catch2::Catch2WithMain avcpp::avcpp)

if (TARGET avcpp::test_data)
    target_link_libraries(test_executor PRIVATE avcpp::test_data)
endif()

include(CTest)
include(Catch)
catch_discover_tests(test_executor)
