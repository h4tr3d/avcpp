@page guide_remux Remuxing

@tableofcontents

Remuxing moves compressed packets from one container to another without decoding them. It is the
cheapest way to change a container, drop a stream, or cut at keyframes, and it is the best first
test of a pipeline: if remuxing produces a playable file, the container side is correct.

## Copy the stream descriptions

@snippet api2-remux.cpp remux_streams

The important call is `stream.codecParameters()`: it returns the portable description of the codec
inside the stream. Copying it to the output stream tells the muxer what the packets will contain,
without opening any codec.

`codecParameters().decodingCodec()` looks up the codec by identifier so the program can check
whether the target container supports it. A muxer happily accepts a stream it cannot store, and the
failure then appears at write time instead of setup time.

`codecParameters().codecTag(0)` clears the codec tag. Tags are container-specific four-character
codes; leaving the input tag would make the output stream claim a tag that belongs to a different
container.

## Move the packets

@snippet api2-remux.cpp remux_loop

Packets are rewritten, not re-timed: the loop only maps the input stream index to the output one.
Timestamps stay in the input stream time base, which is why the example copies `setTimeBase()` from
the input stream during setup.

`writePacket()` without arguments flushes the muxer's internal queue before `writeTrailer()`. Some
muxers hold the last few packets until the trailer; without the flush they can be reordered or lost.

## What cannot be remuxed

- A codec the target container does not support. The example skips such streams, which is a
  legitimate policy: `Errors::FormatCodecUnsupported` is reported instead of aborting.
- A change of resolution or codec parameters in the middle of a stream. Packets after the change
  belong to a different configuration and need a new stream.
- Anything that requires touching the payload: filtering, scaling, re-timing.

For those, decode and encode instead: @ref guide_decode and @ref guide_encode.

## Next

- @ref guide_encode — when the codec has to change.
- @ref guide_filters — when the frames have to change.
