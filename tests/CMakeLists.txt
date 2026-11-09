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
    CodecContext.cpp)
target_link_libraries(test_executor PRIVATE Catch2::Catch2WithMain avcpp::avcpp)

include(CTest)
include(Catch)
catch_discover_tests(test_executor)
