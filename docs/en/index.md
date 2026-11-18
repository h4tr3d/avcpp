@mainpage avcpp

**A C++17 wrapper for FFmpeg that turns its C API into types with constructors,
destructors and error codes.**

FFmpeg is a set of C libraries with manual memory management, out-parameters and
integer error codes. avcpp keeps the same model — no hidden copies, no buffering
you did not ask for — but wraps it into RAII types in the `av` namespace:

| FFmpeg | avcpp |
| --- | --- |
| `AVFrame` | @ref av::VideoFrame, @ref av::AudioSamples, @ref av::SubtitleFrame |
| `AVPacket` | @ref av::Packet |
| `AVCodecContext` | @ref av::CodecContext2 and its encoder/decoder subclasses |
| `AVFormatContext` | @ref av::FormatContext |
| `AVStream` | @ref av::Stream |
| `AVFilterGraph` | @ref av::FilterGraph |
| `AVDictionary` | @ref av::Dictionary, @ref av::DictionaryArray |
| `AVRational` | @ref av::Rational |
| `AVBufferRef` | @ref av::BufferRef |

## What is covered

- container formats and contexts: demuxing, muxing, seeking;
- codecs and codec contexts: decoding and encoding, including the send/receive API;
- frames, audio samples and subtitles, including text and ASS payloads and bitmap rectangles;
- filter graphs: parsing from a string or building them by hand;
- software video scaling and audio resampling;
- hardware acceleration: devices, hardware frames, zero-copy decode/encode paths.

## Requirements

- FFmpeg >= 4.0 (libavformat, libavcodec, libavutil, libswscale, libswresample;
  libavfilter and libavdevice are optional at build time);
- a compiler with C++17 support (GCC >= 9, Clang, MSVC);
- CMake >= 3.19 for the recommended build.

## Getting started

```sh
git clone https://github.com/h4tr3d/avcpp.git
cd avcpp
cmake -B build
cmake --build build -j
```

Call `av::init()` once before using anything else, then link against
`avcpp::avcpp`. The rest of the setup — package managers, `FetchContent`,
`add_subdirectory`, configuration options — is on the @ref guide_building page.

## Where to go next

- @ref guide_building — installing, integrating and configuring the build.
- @ref guide_hw_codecs — hardware decoding, encoding and zero-copy pipelines.
- @ref core — errors, logging, time bases, buffers, dictionaries and helpers.
- @ref avformat — containers, streams and packets.
- @ref codec — codecs and codec contexts.
- @ref frame — frames, pixel and sample formats, rescalers.
- @ref filter — filter graphs.
- @ref hw — hardware devices and zero-copy pipelines.
- @ref subtitle — subtitle frames and their payloads.

Example programs live in `example/api2-samples/` in the repository; each one is a
self-contained `.cpp` file that can be built and run on its own.
