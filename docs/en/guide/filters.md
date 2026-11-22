@page guide_filters Filters

@tableofcontents

A filter graph sits between a decoder and an encoder: frames enter through a source filter, pass
through any number of filters described by a string, and leave through a sink filter.

## Building the graph

@snippet api2-decode-filter-encode.cpp filter_graph

Four steps:

1. **Name the endpoints.** `Filter{"buffer"}` and `Filter{"buffersink"}` are the standard entry and
   exit filters of every graph. They are not part of the chain string.
2. **Describe the input.** The source arguments must match the frames that will be pushed: size,
   pixel format, time base and pixel aspect ratio. A mismatch fails at `config()` time or, worse,
   produces a silently wrong graph.
3. **Parse the chain.** `parse(chain, source, sink)` resolves the filter names and wires the graph.
   The chain syntax is FFmpeg's own: `scale=320x200:-1:flags=lanczos,split [s0][s1]; ...`.
4. **Configure.** `config()` negotiates formats and allocates the filters. Nothing can be pushed
   before it.

`BufferSrcFilterContext` and `BufferSinkFilterContext` are typed handles over the two endpoints.
Build them after `config()`, from the same contexts.

## Pushing and pulling

@snippet api2-decode-filter-encode.cpp filter_pump

One frame in does not mean one frame out: `split`, `fps` and timeline filters change the ratio, and
a sink can return several frames after one push. The inner loop keeps calling `getVideoFrame()`
until it returns `false`, and that `false` is the normal "no more output right now" case.

`AV_BUFFERSRC_FLAG_KEEP_REF` keeps the pushed frame valid after the call, which is what the example
needs because it reuses the decoder frame afterwards. Drop it when the frame is not used again.

An `EAGAIN` from the sink is not an error: the example checks `ec.value() != -EAGAIN` explicitly,
because the sink reports it as a value while the graph is starving for input.

## Timestamps

Filtered frames carry their own timestamps, derived from the input time base. When the encoder uses
a different time base, set it on each filtered frame before encoding — exactly as with plain
decoded frames.

## Text and audio

The same graph works for audio with `buffer`/`abuffersink`, `addAudioSamples()` and
`getAudioSamples()`. Text filters (for example `subtitles=`) are configured through the chain
string and do not need a frame loop.

## Next

- @ref guide_encode — where the filtered frames go.
- @ref guide_hw_codecs — filtering around a hardware pipeline.
