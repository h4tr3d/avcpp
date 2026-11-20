@page guide_encode Encoding

@tableofcontents

Encoding takes frames and produces packets. The setup is longer than decoding, because the output
has to be described: which container, which codec, which resolution, which time base. After that,
the loop is short.

## Setup

@snippet api2-decode-encode-video.cpp encoder_setup

Three objects are involved:

- **`av::OutputFormat`** names the container. `setFormat(string(), out)` fills it from the output
  file name; constructing it with an explicit name forces a container that the extension does not
  imply.
- **`av::FormatContext`** owns the output. `setFormat()` must happen before `addStream()`.
- **`av::VideoEncoderContext`** is built from the codec returned by `findEncodingCodec()`. Some
  codecs need more than the parameters above — see @ref guide_hw_codecs for the hardware cases and
  the frame-rate requirement of QSV.

Order matters. `open()` the encoder before `addStream()`, and `openOutput()` after the streams
exist, because the muxer needs to see them. `writeHeader()` starts the container; `dump()` is a
diagnostic that prints the stream layout and is safe to remove.

## The loop

@snippet api2-decode-encode-video.cpp encode_loop

The parts that are easy to get wrong:

- **Time base.** The encoder was configured with a time base, and the frame must carry the same one
  before it is encoded: `frame.setTimeBase(encoder.timeBase())`. A mismatch produces packets with
  nonsensical timestamps, not an error.
- **Stream index.** `Packet::setStreamIndex()` selects the output stream. With one stream it is `0`.
- **Flushing.** `encoder.encode()` without a frame drains the encoder, and a `do`/`while` is needed
  because one drain call can return several packets. The example drains inside the "decoder is at
  EOF" branch, which is where the last frames appear.
- **Writing.** `octx.writePacket(pkt)` also accepts an error code. Writing is buffered; nothing is
  final until `writeTrailer()`.

## Finishing

`writeTrailer()` writes the container index and flushes the muxer. Without it, an MP4 file is
unplayable even though the size looks plausible. If the muxer buffers packets, `writePacket()` with
no arguments flushes them before the trailer.

## Audio

`av::AudioEncoderContext` works the same way; set `setSampleRate()`, `setChannelsCount()`,
`setSampleFormat()` and then `open()`. Convert the input samples to the encoder layout with
`av::AudioResampler` before encoding, unless they already match.

## Next

- @ref guide_remux — when re-encoding is not needed at all.
- @ref guide_filters — modifying frames before they reach the encoder.
- @ref guide_hw_codecs — hardware encoders and zero-copy pipelines.
