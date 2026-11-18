@page guide_subtitles Subtitles

@tableofcontents

Subtitles are the one part of the library that does not use the send/receive codec API. FFmpeg
still exposes subtitle codecs through the legacy `avcodec_decode_subtitle2()` and
`avcodec_encode_subtitle()` entry points, and avcpp follows that: `SubtitleCodecContext` therefore
has no `send()`, `receive()` or `flush()`.

## Decoding

@snippet api2-subtitles.cpp subtitle_decode

Two details differ from video:

- `decode()` takes an offset and fills a `decodedBytes` out-parameter. A packet can be consumed
  without producing a complete subtitle, in which case the returned frame is empty. That is normal,
  not an error, and the loop simply continues.
- The frame carries `startDisplayTime()` and `endDisplayTime()` in milliseconds, relative to its
  `pts()`. Both are unsigned and can be zero, and `pts() + startDisplayTime()` is the absolute
  presentation time in the frame time base.

A @ref av::SubtitleFrame holds up to `numRects()` rectangles. Each rectangle is one of three kinds:

| Type | Contents | Accessors |
|---|---|---|
| `SUBTITLE_TEXT` | plain text | `text()` |
| `SUBTITLE_ASS` | ASS dialogue line | `ass()` |
| `SUBTITLE_BITMAP` | palette image | `width()`, `height()`, `x()`, `y()`, `nbColors()` |

`isForced()` marks rectangles that are meant to be shown even with subtitles off. Bitmap rectangles
also expose their palette; see the reference of @ref av::SubtitleRect.

## Encoding

`SubtitleEncoderContext` is stricter than the decoder:

1. Call `setSubtitleHeader()` (or `setDefaultSubtitleHeader()`) **before** `open()`.
2. Pass only `SUBTITLE_ASS` rectangles with `startDisplayTime() == 0`. Text and bitmap encoders in
   FFmpeg reject anything else, and a non-zero start time is not what the encoder expects: the
   frame timestamp carries the timing.
3. Reuse one frame for one subtitle event, and set `pts()` and the time base on it.

The ASS path is the practical one: an encoder such as `ass` or `subrip` takes a dialogue line
without styling tags that it does not support. If the input is SRT and the output must be ASS,
decode to `SUBTITLE_TEXT` rectangles and build the ASS line yourself; there is no automatic
conversion in the library.

## Next

- @ref guide_decode — the frame model subtitles reuse.
- @ref guide_encode — writing the encoded packets into a container.
