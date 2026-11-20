@page guide_getting_started Getting started

@tableofcontents

This page builds the smallest useful program: open a file, find the video stream, decode it, and
print one line per frame. It is the base every other guide builds on.

Prerequisites: FFmpeg 4.0 or newer and a C++17 compiler. If the library is not installed yet, see
@ref guide_building.

## The program

```cpp
#include <iostream>

#include "avcpp/av.h"
#include "avcpp/codeccontext.h"
#include "avcpp/formatcontext.h"
#include "avcpp/packet.h"
#include "avcpp/stream.h"

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input file>\n";
        return 1;
    }

    av::init();

    std::error_code ec;
    av::FormatContext ictx;
    ictx.openInput(argv[1], ec);
    if (ec) {
        std::cerr << "Can't open input: " << ec.message() << '\n';
        return 1;
    }

    ictx.findStreamInfo(ec);
    if (ec) {
        std::cerr << "Can't find streams: " << ec.message() << '\n';
        return 1;
    }

    av::Stream vst;
    for (auto &&st : ictx.streams()) {
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
            vst = st;
            break;
        }
    }

    if (!vst.isValid()) {
        std::cerr << "Video stream not found\n";
        return 1;
    }

    av::VideoDecoderContext dec{vst};
    dec.open(ec);
    if (ec) {
        std::cerr << "Can't open codec: " << ec.message() << '\n';
        return 1;
    }

    for (;;) {
        av::Packet pkt = ictx.readPacket(ec);
        if (ec) {
            std::cerr << "Read error: " << ec.message() << '\n';
            return 1;
        }
        if (!pkt)
            break; // end of file

        if (pkt.streamIndex() != vst.index())
            continue;

        if (!dec.send(pkt, ec)) {
            std::cerr << "Can't send a packet: " << ec.message() << '\n';
            return 1;
        }

        for (;;) {
            av::VideoFrame frame = dec.receive(ec);
            if (ec) {
                std::cerr << "Can't receive a frame: " << ec.message() << '\n';
                return 1;
            }
            if (!frame)
                break;

            std::cout << frame.pts().seconds() << " s, "
                      << frame.width() << 'x' << frame.height()
                      << ", format " << frame.pixelFormat().name() << '\n';
        }
    }
}
```

## What each step does

**`av::init()`** registers the FFmpeg codecs and formats. Call it once, before any other avcpp or
FFmpeg call. Everything else is unsafe before it.

**`av::FormatContext`** owns the input. `openInput()` probes the container, `findStreamInfo()`
reads the stream parameters. Do not skip the second call: stream codec parameters are frequently
unknown until the file has been read.

**`av::Stream`** is a handle to one stream inside the context. It is copyable and cheap; destroying
it does not destroy the stream. `mediaType()`, `index()` and `timeBase()` are the accessors you will
use most.

**`av::VideoDecoderContext`** is constructed from the stream, so it picks up the codec from the
stream parameters. `open()` with no arguments is the common case; the extra arguments exist for
explicit codec selection and options.

**`send()` / `receive()`** are the modern FFmpeg codec API and the only one that is correct for
asynchronous decoders. A packet usually produces zero, one, or several frames; a frame can arrive
many packets later, and a rejected `send()` means "drain the decoder and retry". The @ref guide_decode
page explains the contract in detail.

## Errors

Every call that can fail takes an `OptionalErrorCode`. The default is `throws()`, which throws
`av::Exception` (a `std::system_error`). Passing a `std::error_code&` switches that call to the
non-throwing form and stores the error instead. This is why the program above declares one
`std::error_code ec` and reuses it.

The two styles do not mix inside a single call: either you get the code back, or you get an
exception. @ref guide_errors covers the categories and the complete list of codes.

## Link it

@code{.cmake}
add_executable(probe main.cpp)
target_link_libraries(probe PRIVATE avcpp::avcpp)
@endcode

## Next

- @ref guide_decode — frame timing, flushing, and why `receive()` returns empty frames.
- @ref guide_encode — writing frames back into a container.
- @ref guide_errors — the error model in full.
- @ref guide_hw_codecs — the same flow on a GPU.
