@page guide_decode Decoding

@tableofcontents

Decoding turns compressed packets into frames. In avcpp the input side is @ref av::Packet, the
output side is @ref av::VideoFrame or @ref av::AudioSamples, and the state in between is a decoder
context.

## The shape of the loop

A decoder has an input queue and an output queue, and the two are not in lockstep: one packet can
produce several frames, and a frame can be produced long after its packet was accepted. The
contract is:

1. `send(packet)` returns `true` when the packet was accepted, `false` when the decoder wants its
   output drained first.
2. `receive()` returns a frame, or an empty value when there is nothing to read right now.
3. At end of stream, `flush()` switches the decoder to draining mode, and `receive()` keeps
   returning frames until it returns an empty value for good. `isEof()` then becomes `true`.

The reference implementation is `example/api2-samples/api2-send-receive.cpp`:

@snippet api2-send-receive.cpp send_receive_loop

Note what the loop does *not* assume: it never takes one packet to mean one frame, and it never
treats an empty `receive()` as an error. An empty result means "nothing available", not
"end of stream".

## Setting up

@snippet api2-send-receive.cpp send_receive_setup

`VideoDecoderContext` can be constructed from a stream, which takes the codec from the stream
parameters, or from a `Codec` when the codec has to be chosen explicitly (hardware decoding does
that, see @ref guide_hw_codecs).

Two settings are worth knowing before `open()`:

- `setRefCountedFrames(true)` makes the decoder hand out reference-counted frames. Copying such a
  frame is cheap, and it stays valid after the next `receive()` call. Without it, the frame buffer
  belongs to the decoder and is reused.
- `open()` accepts an options dictionary: `dec.open({{"threads", "4"}}, av::Codec(), ec)` sets the
  decoder thread count, as `example/api2-samples/api2-decode.cpp` does.

## Flushing

@snippet api2-send-receive.cpp send_receive_flush

`flush()` returns `false` when the output queue has to be drained before the switch to draining
mode can complete; retrying it after `drain()` is the correct response, and it is idempotent.
Skipping the flush silently loses the last frames of every file, which is easy to miss because the
count is usually small.

## Timestamps

A frame carries a presentation timestamp (`pts()`) and a time base (`timeBase()`). The timestamp is
in time base units, so seconds are `pts().seconds()`, and `pts()` is a `av::Timestamp` you can
compare, convert and print directly.

Do not mix timestamps from different contexts: the decoder, the encoder and each stream have their
own time base. When a frame is passed to another component, set the time base explicitly with
`frame.setTimeBase()`.

## The legacy path

Older code uses `decode(packet)`, which returns at most one frame per call and leaves the rest in
the decoder:

@snippet api2-decode.cpp legacy_decode_loop

This still works for software decoders, and it is what `example/api2-samples/api2-decode.cpp`
demonstrates. Do not use it with asynchronous hardware decoders, where a packet may produce its
frame much later: the frame would be dropped. Use `send()`/`receive()` everywhere.

## Audio

Audio decoding is the same flow with `av::AudioDecoderContext` and `av::AudioSamples`. Samples come
as planar or packed data depending on the sample format; `sampleFormat()`, `channelsCount()`,
`sampleRate()` and `isPlanar()` describe the layout, and `av::AudioResampler` converts between
layouts when a consumer needs something else.

## Next

- @ref guide_encode — the reverse direction.
- @ref guide_filters — what to do with the frames.
- @ref guide_hw_codecs — decoding on a GPU.
