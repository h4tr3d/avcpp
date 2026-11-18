#pragma once

/**
 * Library initialization.
 *
 * @file
 * @ingroup core
 */

/**
 * @defgroup core Core utilities
 * @brief Errors, logging, time bases, buffers, dictionaries and other helpers.
 *
 * The small types every other part of the library is built on: error codes and
 * their categories, the logging bridge, rational numbers and time bases,
 * reference counted buffers, dictionaries, and the handful of generic helpers
 * used across the API.
 */

/**
 * @defgroup avformat Containers and I/O
 * @brief Format contexts, streams and packets: demuxing, muxing and seeking.
 *
 * Reading and writing container formats. The entry point is
 * av::FormatContext, which owns the streams; av::Stream exposes per-stream
 * parameters and time bases; av::Packet carries the compressed data between
 * the container and the codec.
 */

/**
 * @defgroup codec Codecs
 * @brief Codecs, codec contexts and codec parameters.
 *
 * Describing a codec, opening it, and moving frames through it in both
 * directions. av::Codec is a descriptor, av::CodecContext2 (and its encoder and
 * decoder subclasses) is the working state, av::CodecParameters is the
 * portable subset that travels with a stream.
 */

/**
 * @defgroup frame Frames and formats
 * @brief Video frames, audio samples, their pixel and sample formats, rescalers.
 *
 * Everything that holds uncompressed data. av::VideoFrame and av::AudioSamples
 * share the av::Frame interface; av::PixelFormat, av::SampleFormat and
 * av::ChannelLayout describe the data inside them; av::VideoRescaler and
 * av::AudioResampler convert between representations.
 */

/**
 * @defgroup filter Filtering
 * @brief Filter graphs, their contexts, pads and the buffer source and sink.
 *
 * A filter graph is built from av::FilterGraph, fed through av::BufferSrc and
 * drained through av::BufferSink. av::Filter, av::FilterContext and
 * av::FilterPad give access to the individual nodes when a graph has to be
 * assembled by hand instead of parsed from a string.
 */

/**
 * @defgroup hw Hardware acceleration
 * @brief Hardware devices, hardware frames and hardware codec configuration.
 *
 * Device enumeration and creation, hardware frame pools, and the transfer
 * between hardware and system memory that zero-copy pipelines are built on.
 * See av::hw::DeviceContext, av::hw::FramesContext and
 * av::hw::DeviceProbeRange.
 */

/**
 * @defgroup subtitle Subtitles
 * @brief Subtitle frames, their text and ASS payloads, and bitmap rectangles.
 *
 * av::SubtitleFrame carries one subtitle event: either a set of text/ASS
 * rectangles or bitmap rectangles with palette data. Subtitle codecs use the
 * legacy decode/encode entry points, not the send/receive API.
 */

namespace av {

/**
 * @namespace av
 * @brief Root namespace of the library.
 *
 * Every public type and function lives in `av`, usually as a thin RAII wrapper
 * over the corresponding FFmpeg type. Wrappers do not copy data unless the
 * documentation of the specific method says so; ownership and lifetime are
 * stated per class and per method.
 *
 * Call av::init() before anything else.
 */

/**
 * Initialize all subsystems the library needs.
 *
 * Must be called once, before any other avcpp functionality. FFmpeg registers
 * its codecs and formats during this call, so nothing else is safe to use
 * beforehand.
 */
void init();

} // ::av
