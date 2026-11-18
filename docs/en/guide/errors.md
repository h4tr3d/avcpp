@page guide_errors Error handling

@tableofcontents

Every operation that can fail follows one convention: it takes an `OptionalErrorCode` as its last
parameter. The convention removes the need for two overloads of every function, and it lets the
caller choose between exceptions and error codes per call site — not per library.

## Two styles

```cpp
// Throwing style: the default.
av::Packet pkt = ictx.readPacket();          // throws av::Exception on failure

// Error-code style: pass your own std::error_code.
std::error_code ec;
av::Packet pkt = ictx.readPacket(ec);
if (ec) {
    std::cerr << ec.category().name() << ": " << ec.message() << '\n';
}
```

`av::throws()` is the default argument. It is an `OptionalErrorCode` that is not a null pointer, so
the library knows the caller wants an exception. Passing a `std::error_code&` gives you a
non-owning view of it: the variable must outlive the call, which it does in every normal pattern.

The two styles are exclusive inside one call. Mixing them in one program is fine — pick the style
per boundary: exceptions inside an application, error codes at an API edge that has to stay
`noexcept`.

## What an empty code means

`if (ec)` is the test. An `std::error_code` that was never set compares equal to
`std::error_code{}` and its `value()` is `0`.

Most functions call `clear_if(ec)` on entry, so a code from a previous call does not leak into the
next result. This is why a loop can reuse one `std::error_code` for the whole pipeline, and why
`example/api2-samples/api2-send-receive.cpp` checks `ec` after each step without clearing it by
hand.

## Categories

Two categories exist:

| Category | Origin | Example codes |
|---|---|---|
| `av::avcpp_category()` | checks made by the library itself | `Errors::CodecAlreadyOpened`, `Errors::FormatCodecUnsupported` |
| `av::ffmpeg_category()` | return values of FFmpeg calls | `AVERROR(EAGAIN)`, `AVERROR(ENOSYS)` |

`av::Errors` is an enum with an `std::error_code` adapter, so comparison is direct:

```cpp
if (ec == av::Errors::FormatCodecUnsupported) {
    // the muxer cannot store this codec
}
```

Codes you will meet in practice:

| Code | Meaning | Typical reaction |
|---|---|---|
| `CodecAlreadyOpened` | a setting was changed after `open()` | move the setting before `open()` |
| `CodecIsNotOpened` | an operation needs an open codec | call `open()` first |
| `FormatCodecUnsupported` | the container cannot store the codec | skip the stream or re-encode |
| `FormatNoStreams` | the output context has no streams | add streams before `writeHeader()` |
| `FormatHeaderNotWriten` | packets were written before the header | call `writeHeader()` first |
| `FrameInvalid` | an operation on an empty frame | check `isValid()` before using it |
| `HardwareTransferUnsupported` | a transfer is impossible | see @ref guide_hw_codecs |
| `ResamplerInputChanges` | the input layout changed after initialisation | create a new resampler |

## Catching

```cpp
try {
    av::FormatContext ictx;
    ictx.openInput(path);
    ictx.findStreamInfo();
} catch (const av::Exception &e) {
    std::cerr << e.code().category().name() << ": " << e.what() << '\n';
}
```

`av::Exception` derives from `std::system_error`, so `code()` gives the same
`std::error_code` the non-throwing style would have produced. There is no separate exception type
per failure, and no string matching is ever needed.

## Writing your own wrappers

The convention is small enough to follow in application code:

```cpp
void check(av::OptionalErrorCode ec, int err)
{
    av::throws_if(ec, err, av::avcpp_category()); // or ffmpeg_category()
}
```

`throws_if()` throws or stores the code depending on what the caller passed, and `clear_if()` clears
it. Together they are the whole protocol, and using them keeps your code indistinguishable from the
library's own.

## Next

- @ref guide_getting_started — the convention in a complete program.
- @ref guide_hw_codecs — the hardware-specific codes.
