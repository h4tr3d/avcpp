@page api_map Which type do I need

@tableofcontents

A one-page map from the job to the types that do it. Each row links to the generated reference for
the group and to the guide that shows the flow end to end.

| I want to… | Types | Reference | Guide |
|---|---|---|---|
| initialise the library | `av::init()` | @ref core | @ref guide_getting_started |
| open a file or a stream | `av::FormatContext`, `av::InputFormat` | @ref avformat | @ref guide_getting_started |
| enumerate streams | `av::Stream`, `av::FormatContext::streams()` | @ref avformat | @ref guide_getting_started |
| read compressed data | `av::Packet` | @ref avformat | @ref guide_decode |
| decode video | `av::VideoDecoderContext`, `av::VideoFrame` | @ref codec, @ref frame | @ref guide_decode |
| decode audio | `av::AudioDecoderContext`, `av::AudioSamples` | @ref codec, @ref frame | @ref guide_decode |
| decode subtitles | `av::SubtitleDecoderContext`, `av::SubtitleFrame` | @ref subtitle | @ref guide_subtitles |
| encode video | `av::VideoEncoderContext`, `av::findEncodingCodec()` | @ref codec | @ref guide_encode |
| encode audio | `av::AudioEncoderContext` | @ref codec | @ref guide_encode |
| write a container | `av::FormatContext`, `av::OutputFormat` | @ref avformat | @ref guide_encode |
| copy streams without decoding | `av::CodecParameters`, `av::Stream::setCodecParameters()` | @ref avformat | @ref guide_remux |
| filter frames | `av::FilterGraph`, `av::BufferSrcFilterContext`, `av::BufferSinkFilterContext` | @ref filter | @ref guide_filters |
| scale video | `av::VideoRescaler` | @ref frame | @ref guide_filters |
| resample audio | `av::AudioResampler` | @ref frame | @ref guide_filters |
| use a GPU | `av::hw::DeviceContext`, `av::hw::FramesContext` | @ref hw | @ref guide_hw_codecs |
| handle failures | `av::OptionalErrorCode`, `av::Errors`, `av::Exception` | @ref core | @ref guide_errors |
| convert time | `av::Timestamp`, `av::Rational` | @ref core | — |
| store options | `av::Dictionary`, `av::DictionaryArray` | @ref core | — |
| own raw data | `av::BufferRef`, `av::BufferRefView` | @ref core | — |

## The four objects behind everything

Almost every operation moves data between four types:

- **`av::Packet`** — compressed bytes, one stream, one timestamp.
- **`av::VideoFrame` / `av::AudioSamples`** — uncompressed data plus its format description.
- **`av::CodecContext2` and subclasses** — the state that turns one into the other.
- **`av::FormatContext`** — the container that holds streams, and therefore packets.

If you know which of these four a function takes and returns, you know where you are in the
pipeline. @ref guide_decode and @ref guide_encode cover the transitions; @ref guide_remux skips
them entirely.

## Naming conventions

- `setX()` / `x()` pairs are direct property access; they do not do I/O and do not fail.
- `create()` returns a value and can fail; constructors that can fail are absent on purpose.
- `*Range`, `*View`, `*List` types are non-owning views over memory owned by someone else.
- `raw()` returns the underlying FFmpeg pointer when you need to call FFmpeg directly.
