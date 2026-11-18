@page guide_hw_codecs Hardware decoding and encoding

@tableofcontents

## 1. Purpose

This page describes the `av::hw` API. It gives safe and convenient access to the FFmpeg
hardware codecs.

It answers the following questions:

- how to create a device and a frame pool;
- how to enable a hardware decoder;
- how to enable a hardware encoder;
- how to move a frame between system memory and the device;
- how to build a path without copying;
- how to check that a device is available without making tests fail;
- which limitations exist on each platform.

The audience is avcpp developers and authors of applications built on avcpp.

The generated reference for these types is the @ref hw group.

## 2. Terms

| Term | Meaning |
|---|---|
| Device | A hardware node: a GPU, a media block, a video card. In FFmpeg this is `AVHWDeviceContext`. |
| Frame pool | A set of surfaces of one size and format. In FFmpeg this is `AVHWFramesContext`. |
| Surface | A frame in device memory. The application does not read it directly. |
| System memory | Ordinary RAM. The frame is reachable through the `data[]` pointers. |
| Zero copy | A path on which the frame never leaves device memory. |
| Upload | Moving a frame from system memory into device memory. |
| Download | Moving a frame from device memory into system memory. |
| `DeviceType` | The device type: `vaapi`, `qsv`, `cuda`, `d3d11va` and others. |
| `EncodingMode` | The encoder mode: `Auto`, `Frames`, `Device`. |

Rule: one term means one concept. Do not use the word "buffer" for a device, a pool and a frame.

## 3. FFmpeg layers

FFmpeg splits the hardware work into three layers.

1. `AVHWDeviceContext` describes the device. It is created once.
2. `AVHWFramesContext` describes the pool of surfaces. It is tied to the device.
3. `AVCodecContext` uses the device or the pool. The `hw_device_ctx` field sets the device; the
   `hw_frames_ctx` field sets the pool.

The `AVFrame` layer holds the frame descriptor. When `frame->hw_frames_ctx` is not empty, the frame
lives in device memory.

The layer diagram, with the corresponding avcpp types:

@image html hw-architecture.svg "API architecture"

## 4. Object ownership

`av::hw::DeviceContext` and `av::hw::FramesContext` own the FFmpeg objects. A `BufferRef` lives
inside. Copying the object adds a reference; destroying it drops one.

`VideoFrame` does not own the device. The frame holds a reference to the pool through
`hw_frames_ctx`, and the pool holds a reference to the device through `device_ref`.

The ownership diagram:

@image html hw-ownership.svg "Object ownership"

Rules:

- Create the device before the pool. Destroy the pool before the device. The reference counter
  allows the other order, but the explicit order is easier to read.
- Never free an `AVBufferRef*` by hand. Pass it to `wrap()`, which adds a reference.
- Do not keep an `AVHWDeviceContext*` alive after its `DeviceContext` is destroyed.
- Every `*Range` type is non-owning. Do not keep a range longer than its owner.

## 5. API types

### 5.1. `DeviceType`

`DeviceType` is a thin wrapper over `AVHWDeviceType`.

```cpp
av::hw::DeviceType type = av::hw::DeviceType::fromName("vaapi");
if (type.isNone()) {
    // FFmpeg does not know this type, or the type is not compiled in
}
const char* name = type.name(); // "vaapi" or "none"
```

Lookup is by name. The code does not depend on the numeric values of the enumeration, which
matters for old FFmpeg versions.

### 5.2. `DeviceContext`

`DeviceContext` is created by a factory. There is no constructor, because creating a device can
fail.

```cpp
std::error_code ec;
av::hw::DeviceContext device =
    av::hw::DeviceContext::create(type, "/dev/dri/renderD128", {}, ec);
if (!device.isValid()) {
    // ec holds the reason
}
```

Overloads:

```cpp
static DeviceContext create(DeviceType type,
                            const char* device = nullptr,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());

static DeviceContext create(DeviceType type,
                            const std::string& device,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());

static DeviceContext derive(const DeviceContext& source,
                            DeviceType type,
                            const Dictionary& options = {},
                            OptionalErrorCode ec = throws());
```

`derive()` creates a derived device, for example an OpenCL device from a VAAPI device. Do not derive
a device for QSV: it can crash.

### 5.3. A device string, not a path

The `device` parameter is a `const char*` or a `std::string`. That is deliberate: a device is not
always a file.

The value depends on the backend:

- VAAPI and DRM: a file path, for example `/dev/dri/renderD128`;
- CUDA: a device index, for example `"1"`;
- D3D11VA: the adapter name;
- VideoToolbox: a name, or an empty string;
- default: `nullptr` or an empty string.

`std::filesystem::path` implies a file and cannot express an index or an adapter name, which is why
the API takes a string.

The `const char*` overload does not copy the string; the caller keeps it alive. The
`const std::string&` overload keeps a copy inside the range.

### 5.4. `FramesContext`

```cpp
av::hw::FramesContext frames =
    av::hw::FramesContext::create(device,
                                  av::hw::hwPixelFormat(type),
                                  AV_PIX_FMT_NV12,
                                  1920, 1080,
                                  0,
                                  ec);
```

`initialPoolSize = 0` means the default pool size: FFmpeg picks it.

Methods:

```cpp
bool isValid() const;
DeviceType device() const;     // a copy of the device owner
PixelFormat format() const;    // the surface format
PixelFormat swFormat() const;  // the format used for upload and download
int width() const;
int height() const;
PixelFormatList transferFormats(AVHWFrameTransferDirection direction,
                                OptionalErrorCode ec = throws()) const;
```

`swFormat` is the format of the frame in system memory. Not every device supports `NV12`; check
`FrameConstraints::swFormats()`.

### 5.5. Ranges without allocation

The API returns ranges. A range does not allocate memory and does not copy the array.

| Function | Type | Memory owner |
|---|---|---|
| `allDeviceTypes()` | `DeviceTypeRange` | FFmpeg |
| `probeDevices(device)` | `DeviceProbeRange` | FFmpeg, probed while iterating |
| `frameConstraints(device, ec)` | `FrameConstraints` | the owning object |
| `FrameConstraints::hwFormats()` | `PixelFormatRange` | `FrameConstraints` |
| `FrameConstraints::swFormats()` | `PixelFormatRange` | `FrameConstraints` |
| `codecConfigs(codec)` | `CodecConfigRange` | FFmpeg |
| `findDecoders(id, type)` | `CodecRange` | FFmpeg |
| `findEncoders(id, type)` | `CodecRange` | FFmpeg |
| `FramesContext::transferFormats()` | `PixelFormatList` | the owning object |

`PixelFormatList` owns an `AVPixelFormat*` array. `range()` returns a non-owning view; the list
itself can be iterated directly.

Example:

```cpp
for (av::hw::DeviceType type : av::hw::allDeviceTypes()) {
    std::cout << type.name() << '\n';
}

for (const av::hw::DeviceProbe& probe : av::hw::probeDevices()) {
    std::cout << probe.type.name() << ": "
              << (probe.available ? "available" : probe.error.message())
              << '\n';
}
```

`probeDevices()` creates a temporary device for every type. That is the only reliable check:
`av_hwdevice_iterate_types()` enumerates the compiled-in backends, not the hardware that is
present.

### 5.6. Codec configurations

`codecConfigs(codec)` returns the list of `AVCodecHWConfig`. Each entry says which device type and
which format the codec supports.

`findCodecConfig(codec, type, methodsMask)` returns the first matching entry.

`findDecoder(id, type)` and `findEncoder(id, type)` look a codec up by identifier and device type.
The function walks every codec with that identifier, which matters for QSV: the generic `h264`
codec does not support QSV, `h264_qsv` does.

`findDecoders()` and `findEncoders()` return a lazy range. The range does not allocate.

### 5.7. Frame constraints

```cpp
av::hw::FrameConstraints constraints =
    av::hw::FrameConstraints::query(device, ec);
if (constraints.isValid()) {
    for (av::PixelFormat format : constraints.swFormats())
        std::cout << format.name() << '\n';

    std::cout << constraints.maxWidth() << 'x' << constraints.maxHeight() << '\n';
}
```

`FrameConstraints` owns the FFmpeg structure. `hwFormats()` and `swFormats()` return non-owning
views.

Some backends report very wide bounds, from `0x0` to `INT_MAX`. Do not treat those numbers as the
exact limits of the hardware.

## 6. Decoding

### 6.1. Order of calls

1. Create a `DeviceContext`.
2. Create a `VideoDecoderContext` with the codec found by `findDecoder()`.
3. Call `setHwDeviceContext(device)` before `open()`.
4. Optionally call `setHwTransferToSystem(true)`.
5. Call `open()`.
6. Feed packets through `send()` and read frames through `receive()`.
7. At the end of the stream, call `flush()` and drain the remaining frames.

The sequence:

@image html hw-decode.svg "Decoding sequence"

Example:

```cpp
av::Codec codec = av::hw::findDecoder(AV_CODEC_ID_H264, type);
av::VideoDecoderContext decoder(vst, codec);
decoder.setHwDeviceContext(device, ec);
decoder.setHwTransferToSystem(transferToSystem);
decoder.open(av::Codec(), ec);

while ((packet = ictx.readPacket(ec))) {
    if (packet.streamIndex() != vst.index())
        continue;

    for (;;) {
        const bool accepted = decoder.send(packet, ec);
        for (;;) {
            av::VideoFrame frame = decoder.receive(ec);
            if (!frame.isValid())
                break;
            consume(frame);
        }
        if (accepted)
            break;
    }
}
```

Do not use the legacy `decode()` with a hardware decoder when you need every frame: asynchronous
backends such as QSV can return a frame later. The `send()`/`receive()` path drains them all. Do not
forget `flush()`.

### 6.2. Automatic transfer to system memory

By default a hardware decoder returns frames on the device, which is what zero copy needs.

When the application needs the `data[]` pointers, enable the automatic transfer:

```cpp
decoder.setHwTransferToSystem(true);
```

`receive()` then returns frames in system memory. A decoder cannot be switched between the two modes
in the middle of a stream: pick the mode before `open()`.

### 6.3. `get_format`

The decoder sets only `hw_device_ctx`. The static `get_format` callback reads that field and derives
the device type. `AVCodecContext::opaque` is not used, so the application is free to use it for its
own purposes.

Some decoders, for example QSV in FFmpeg 4.x, advertise only `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX`.
avcpp creates a frame pool for them before `open()`, taking the size from `coded_width` and
`coded_height` and the format from the device constraints. Without that pool, FFmpeg 4.x silently
falls back to system memory.

There is no silent fallback to the software decoder: when the hardware path is impossible, the API
returns an error, so the application learns about the problem immediately.

## 7. Encoding

### 7.1. Modes

`EncodingMode` selects how frames reach the encoder.

| Mode | Meaning |
|---|---|
| `Auto` | A frame pool first; if the pool cannot be created, the device. |
| `Frames` | A frame pool only. Requires `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX`. |
| `Device` | The device only. Requires `AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX`. |

`Auto` is the recommended mode for new code.

### 7.2. Order of calls

1. Create the encoder.
2. Set width, height, time base, frame rate and bit rate.
3. Call `configureHwEncoding()`.
4. Call `open()`.
5. Feed frames through `encode()`.
6. Call `encode()` without a frame to drain.

The sequence:

@image html hw-encode.svg "Encoding sequence"

Example:

```cpp
av::Codec codec = av::hw::findEncoder(AV_CODEC_ID_H264, type);
av::VideoEncoderContext encoder(codec);
encoder.setWidth(1920);
encoder.setHeight(1080);
encoder.setTimeBase(av::Rational{1, 25});
encoder.setFrameRate(av::Rational{25, 1});
encoder.setBitRate(4'000'000);

encoder.configureHwEncoding(device, av::hw::EncodingMode::Auto,
                            AV_PIX_FMT_NONE, ec);
encoder.open(av::Codec(), ec);
```

`swFormat = AV_PIX_FMT_NONE` enables format selection. The order is:

1. the requested format;
2. the current `pix_fmt` of the codec;
3. `NV12` from the device constraints;
4. `YUV420P` from the constraints;
5. the first format from the constraints.

### 7.3. Automatic upload

Automatic upload is on by default. When the input frame is in system memory, `encode()` uploads it
into the pool first and then hands it to the encoder.

To disable it:

```cpp
encoder.setHwAutoUpload(false);
```

Disable it only for an explicit zero-copy path. Otherwise the encoder receives a frame of the wrong
kind.

### 7.4. Frame rate

Some encoders need the frame rate before `open()`, QSV for example. Without it QSV fails with
"Current frame rate is unsupported".

```cpp
encoder.setFrameRate(av::Rational{25, 1});
```

## 8. Zero copy

### 8.1. The idea

The decoder creates a pool of surfaces. The encoder uses the same pool. The frame never leaves the
device.

FFmpeg's own `vaapi_transcode.c` example uses this approach, and it works for VAAPI, QSV and CUDA.

```cpp
av::VideoFrame first = receiveFirstHardwareFrame();
av::hw::FramesContext frames = first.hwFramesContext();

encoder.configureHwEncoding(frames, ec); // automatic upload is off
encoder.open(av::Codec(), ec);

// Every following frame goes to encode() without a transfer.
```

The diagram:

@image html hw-transcode-zero-copy.svg "Zero copy"

### 8.2. Why not `av_hwframe_map`

`av_hwframe_map()` is not a reliable path between different contexts: some backends return
`ENOSYS`, and a derived QSV pool can crash.

The correct path is to take the pool from the decoder's first frame and hand it to the encoder.

### 8.3. Fallback path

When the pool is not available, use the copying path:

1. `frame.toSystemMemory()`;
2. `encoder.configureHwEncoding(device, Auto)`;
3. `encoder.encode(softwareFrame)` with automatic upload enabled.

The `api2-hw-transcode` example makes this choice automatically.

## 9. Scaling

`VideoRescaler` works with system memory. A fully hardware pipeline that changes the size needs an
intermediate download.

The path:

1. hardware decoding;
2. `toSystemMemory()`;
3. `VideoRescaler::rescale()`;
4. `toHwFrame()` or automatic upload;
5. hardware encoding.

The diagram:

@image html hw-scale-pipeline.svg "Scaling pipeline"

Example: `api2-hw-scale`.

Note that a transfer between system memory and the device costs time. Measure both paths on the
target hardware; with small frames the software pipeline is sometimes faster.

## 10. Frame transfer

`VideoFrame` provides four operations.

| Method | Direction | Notes |
|---|---|---|
| `toSystemMemory(format, ec)` | device → system | `format = AV_PIX_FMT_NONE` picks the format automatically |
| `toHwFrame(frames, ec)` | system → device | Calls `av_hwframe_get_buffer()` first |
| `mapTo(frames, flags, ec)` | mapping | Backend dependent, can return `ENOSYS` |
| `hwFramesContext()` | query | Returns the frame's pool |
| `hwDeviceContext()` | query | Returns the pool's device |

`toHwFrame()` allocates a surface in the pool first and then calls `av_hwframe_transfer_data()`.
Calling `av_hwframe_transfer_data()` directly without a surface returns `EINVAL` on some backends.

`VideoFrame::clone()` does not clone a hardware frame: it returns `HardwareTransferUnsupported`.
Copying a surface is not cheap. When a second owner is needed, copy the `VideoFrame` object instead;
the copy adds a reference to the `AVFrame`.

After a transfer or a rescale, set the timestamp and the stream index explicitly. The new object does
not always inherit `m_timeBase` from the avcpp wrapper.

## 11. Choosing a device

### 11.1. Order of selection

1. Get the compiled-in types: `allDeviceTypes()`.
2. Check availability: `isDeviceAvailable(type)` or `probeDevices()`.
3. Find a decoder: `findDecoder(codecId, type)`.
4. Find an encoder: `findEncoder(codecId, type)`.
5. Check the configurations: `findCodecConfig()`.
6. Create the device: `DeviceContext::create()`.
7. Check the formats: `frameConstraints()`.

### 11.2. Platforms

| Platform | Main types | Probe only |
|---|---|---|
| Linux | `vaapi`, `qsv`, `cuda`, `vdpau` | `opencl`, `vulkan`, `drm` |
| Windows | `d3d11va`, `dxva2`, `qsv`, `cuda`, `amf` | `opencl`, `vulkan` |
| macOS | `videotoolbox` | — |
| Android | `mediacodec` | — |

"Probe only" means the type is useful for checking a device or a pool; there is no tested hardware
codec for it.

### 11.3. QSV

QSV needs its own codec; the generic `h264` does not work.

```cpp
av::Codec decoder = av::hw::findDecoder(AV_CODEC_ID_H264, qsvType);
```

The QSV encoder needs `setFrameRate()` before `open()`. The QSV decoder can return the first frames
late; use `send()` and `receive()`.

QSV with FFmpeg 4.x goes through libmfx, and that combination is incomplete in `ubuntu:22.04`:

- the decoder works, but frames stay in system memory; frames on the device come only from the
  legacy `-hwaccel qsv` path;
- `av_hwframe_transfer_data()` to a QSV pool never returns: the driver spawns threads endlessly;
- `MFXVideoENCODE_Init()` rejects every parameter and `open()` returns `AVERROR(ENOSYS)`.

The same commands in `ffmpeg(1)` behave the same way, so this is a limitation of the environment,
not of avcpp. The tests skip these steps and print a warning. Use VAAPI to exercise Intel hardware
on FFmpeg 4.x. Since FFmpeg 5.0, QSV goes through oneVPL and does not have these limitations.

## 12. Errors

| Code | Cause | Action |
|---|---|---|
| `HardwareDeviceUnavailable` | The device cannot be created | Check the driver and the device name |
| `HardwareDeviceInvalid` | An empty or invalid `DeviceContext` | Create the device again |
| `HardwareConfigNotFound` | The codec does not support the device | Pick another codec or mode |
| `HardwareFramesInvalid` | The pool was not created | Check the format, the size and the constraints |
| `HardwareTransferUnsupported` | The transfer is impossible | Use system memory |
| `HardwareCodecNotFound` | No hardware codec | Use a software codec |
| `CodecAlreadyOpened` | Configuration after `open()` | Configure the codec before `open()` |

Every function takes an `OptionalErrorCode`. By default an error throws; pass a `std::error_code`
when you want the code back.

## 13. FFmpeg and avcpp correspondence

| FFmpeg | avcpp |
|---|---|
| `AVHWDeviceType` | `av::hw::DeviceType` |
| `AVHWDeviceContext` | `av::hw::DeviceContext` |
| `AVHWFramesContext` | `av::hw::FramesContext` |
| `AVHWFramesConstraints` | `av::hw::FrameConstraints` |
| `AVCodecHWConfig` | `av::hw::codecConfigs()` |
| `av_hwdevice_iterate_types()` | `av::hw::allDeviceTypes()` |
| `av_hwdevice_ctx_create()` | `DeviceContext::create()` |
| `av_hwdevice_ctx_create_derived_opts()` | `DeviceContext::derive()` |
| `av_hwframe_ctx_alloc()` + `av_hwframe_ctx_init()` | `FramesContext::create()` |
| `av_hwframe_transfer_get_formats()` | `FramesContext::transferFormats()` |
| `av_hwframe_get_buffer()` + `av_hwframe_transfer_data()` | `VideoFrame::toHwFrame()` |
| `av_hwframe_transfer_data()` into system memory | `VideoFrame::toSystemMemory()` |
| `av_hwframe_map()` | `VideoFrame::mapTo()` |
| `avcodec_get_hw_config()` | `av::hw::codecConfigs()` |

## 14. FFmpeg compatibility

The minimum version is FFmpeg 4.0.

- `AVCPP_HAS_HWCONTEXT` tells whether the API is available.
- `av_hwdevice_ctx_create_derived_opts()` appeared in FFmpeg 4.3; older versions take a fallback.
- Decoders that advertise only `AV_CODEC_HW_CONFIG_METHOD_HW_FRAMES_CTX` get a frame pool from
  avcpp. This covers QSV in FFmpeg 4.x.
- Device types and formats are looked up by name; the code does not depend on new enumeration
  values.
- An `AV_DISABLE_AVFORMAT=ON` build contains no hardware tests and no examples with a demuxer. The
  library itself still builds.

## 15. Verification

The tests live in `tests/Hardware.cpp` and `tests/Fixtures.cpp`. They must not fail on a machine
without hardware.

The rule:

- no device or no codec → `SKIP`;
- the device exists but cannot be created → `FAIL`.

Environment variables:

```sh
AVCPP_HW_DEVICES=vaapi,qsv   # restrict the set, fixtures included
AVCPP_HW_SKIP=cuda           # exclude a backend
```

The variables also affect the fixture tests.

The default set:

- Linux: VAAPI, QSV, CUDA, VDPAU;
- Windows: D3D11VA, DXVA2, QSV, CUDA, AMF;
- macOS: VideoToolbox;
- Android: MediaCodec.

Commands:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure

./build/tests/test_executor '[hw]'
./build/tests/test_executor '*Hardware fixtures*'
```

Verified:

- FFmpeg 9.x on Linux: VAAPI, QSV, CUDA — every hardware test and fixture passes;
- FFmpeg 4.4 in `ubuntu:22.04` with Intel passthrough (VAAPI): all 91 `[hw]` assertions and both
  fixtures with frame transfer;
- FFmpeg 4.4 in `ubuntu:22.04` with Intel passthrough (QSV): 60 `[hw]` assertions; QSV skips
  transfer, the encoder and frames on the device;
- FFmpeg 4.4 in `ubuntu:22.04` with NVIDIA passthrough (CUDA): 96 `[hw]` assertions and both
  fixtures, zero copy included;
- a build without `libavformat`: 49 tests;
- the library builds as C++17;
- Meson: build and tests;
- Valgrind: no memory errors in avcpp code. The reports contain only caches of the NVIDIA, CUDA,
  OpenCL and DBus drivers.

Docker verification:

```sh
# Intel (VAAPI and QSV)
docker run --rm --memory=3g --device /dev/dri:/dev/dri \
    -e LIBVA_DRIVER_NAME=iHD -e AVCPP_HW_DEVICES=vaapi,qsv \
    -v "$PWD":/src:ro -w /src IMAGE bash -lc \
    'cmake -B /tmp/b -DAVCPP_TEST_DATA=ON ... && cmake --build /tmp/b -j && /tmp/b/tests/test_executor "[hw]"'

# NVIDIA (CUDA): pass the devices and the driver libraries through
docker run --rm --memory=4g \
    --device /dev/nvidia0 --device /dev/nvidiactl --device /dev/nvidia-uvm \
    --device /dev/nvidia-uvm-tools --device /dev/nvidia-modeset \
    -v /dev/nvidia-caps:/dev/nvidia-caps -v /path/to/nvidia-libs:/nvidia:ro \
    -e LD_LIBRARY_PATH=/nvidia -e AVCPP_HW_DEVICES=cuda \
    -v "$PWD":/src:ro -w /src IMAGE bash -lc \
    'cmake -B /tmp/b ... && /tmp/b/tests/test_executor "*Hardware fixtures*"'
```

Without `nvidia-container-toolkit` the `--gpus all` flag does not work; pass the devices and the
driver libraries through by hand.

Limit the container memory and set a `timeout`. The QSV test on FFmpeg 4.x exhausts memory without
those measures.

## 16. Examples

In `example/api2-samples`:

| Example | Purpose |
|---|---|
| `api2-hw-probe` | List devices, constraints and codec configurations |
| `api2-hw-decode` | Hardware decoding, with and without transfer |
| `api2-hw-encode` | Synthetic frames and a hardware encoder |
| `api2-hw-transcode` | Transcoding without copying, plus the fallback path |
| `api2-hw-scale` | Hardware decoding, scaling, encoding |

Running them:

```sh
./build/example/api2-samples/api2-hw-probe
./build/example/api2-samples/api2-hw-decode vaapi input.mp4 --to-system
./build/example/api2-samples/api2-hw-transcode vaapi input.mp4 output.mkv
./build/example/api2-samples/api2-hw-scale vaapi input.mp4 scaled.mkv
```

## 17. Limitations

- The API does not pick a device automatically; the application sets the type and the name.
- The API does not silently fall back to a software codec; that is the application's decision.
- `VideoRescaler` cannot work with surfaces directly.
- `VideoFrame::clone()` does not copy a hardware frame.
- `mapTo()` is backend dependent.
- A derived QSV pool is not used.
- VideoToolbox in FFmpeg 4.x creates neither a pool for a manual frames context nor constraints.
  Only the codec paths work: the decoder and the encoder create the pool themselves.
- QSV in FFmpeg 4.x: no frame transfer, no encoder, and the decoder returns frames in system
  memory. Details in section 11.3.
- A CUDA device can become unavailable after suspend or a driver change; probe it again.
- The set of formats and the constraints depend on the driver.
- The hardware path is not always faster; measure the result.

## 18. Checklist

- [ ] `av::init()` is called before anything else.
- [ ] The device type comes from `DeviceType::fromName()`.
- [ ] The device is created with `DeviceContext::create()`.
- [ ] Errors are checked through `std::error_code`.
- [ ] The decoder is configured before `open()`.
- [ ] For QSV a dedicated codec is found with `findDecoder()`.
- [ ] For QSV the frame rate is set before `open()`.
- [ ] The transfer mode is chosen before `open()`.
- [ ] Zero copy reuses the pool of the first frame.
- [ ] Automatic upload is disabled only for zero copy.
- [ ] The timestamp and the stream index are set after a transfer.
- [ ] The pool is destroyed before the device.
- [ ] Tests without hardware report `SKIP`, not `FAIL`.
