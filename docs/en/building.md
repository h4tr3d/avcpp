@page guide_building Building and installing

@tableofcontents

## Requirements

- FFmpeg >= 4.0:
  - libavformat >= 58.x.x,
  - libavcodec >= 58.x.x,
  - libavutil >= 56.x.x,
  - libswscale >= 5.x.x,
  - libswresample >= 3.x.x,
  - libavdevice >= 58.x.x (optional),
  - libavfilter >= 7.x.x (optional),
  - libpostproc >= 55.x.x (legacy and optional; removed in FFmpeg 8);
- GCC >= 9.0, Clang, or MSVC. C++17 is required;
- CMake >= 3.19 for the recommended build.

Older FFmpeg (down to 2.0) and older GCC (down to 6.0) may work, but are not
verified. The oldest Ubuntu release covered by CI is 22.04.

## Installing FFmpeg

On Debian, Ubuntu 19.10+, or Linux Mint 20.x and newer:

```sh
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev \
                     libavfilter-dev libswscale-dev libswresample-dev \
                     libavdevice-dev
```

`libpostproc` is legacy: the library does not use it, and FFmpeg 8 removed it
from the upstream tree, so newer distributions may not package it at all.

On Ubuntu 18.04 and Linux Mint 19.x add the FFmpeg 4 PPA first:

```sh
sudo add-apt-repository ppa:jonathonf/ffmpeg-4 -y
sudo apt update && sudo apt upgrade
```

Then install the same packages.

## Building with CMake

```sh
git clone https://github.com/h4tr3d/avcpp.git avcpp-git
cd avcpp-git
cmake -B build
cmake --build build -j
```

Tests are on by default; Catch2 is downloaded at configure time, so no
submodules are required. If FFmpeg lives in a non-standard place:

```sh
cmake -B build -DPC_FFMPEG_LIBRARY_DIRS=<path> -DPC_FFMPEG_INCLUDE_DIRS=<path>
```

To install into a prefix:

```sh
cmake -B build -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build -j
sudo cmake --install build
```

For packaging, use `DESTDIR`:

```sh
sudo cmake --install build --prefix /usr --strip
```

## Configuration options

| option | default | meaning |
| --- | --- | --- |
| `AV_ENABLE_STATIC` | `On` | build the static library |
| `AV_ENABLE_SHARED` | `On` | build the shared library |
| `AV_BUILD_EXAMPLES` | `On` | build `example/api2-samples` |
| `AVCPP_ENABLE_TESTS` | `On` | build the Catch2 test suite |
| `AVCPP_TEST_DATA` | `Off` | enable tests that need the binary fixtures from `avcpp-test-data` |
| `AV_DISABLE_AVFORMAT` | `Off` | do not use libavformat; also disables libavfilter and libavdevice |
| `AV_DISABLE_AVFILTER` | `Off` | do not use libavfilter; also disables libavdevice |
| `AV_DISABLE_AVDEVICE` | `Off` | do not use libavdevice |
| `AVCPP_ENABLE_DOCS` | `Off` | add the `docs`, `docs-check` and `docs-serve` targets |
| `CMAKE_CXX_STANDARD` | newest supported | override the C++ standard; C++17 is the documented minimum |

Deprecated: `AV_BUILD_TESTS` is an alias for `AVCPP_ENABLE_TESTS`.

FFmpeg discovery options:

- `FFMPEG_PKG_CONFIG_SUFFIX` — suffix appended to the pkg-config package names;
- `PC_<component>_LIBRARY_DIRS`, `PC_<component>_INCLUDE_DIRS` — hints for a single
  component;
- `PC_FFMPEG_LIBRARY_DIRS`, `PC_FFMPEG_INCLUDE_DIRS` — hints for all components.

`<component>` is one of `AVCODEC`, `AVDEVICE`, `AVFORMAT`, `AVFILTER`, `AVUTIL`,
`POSTPROC`, `SWSCALE`, `SWRESAMPLE`.

Set `FETCHCONTENT_SOURCE_DIR_CATCH2` to a local Catch2 checkout to configure
offline.

## Integrating as a git submodule

```sh
git submodule add https://github.com/h4tr3d/avcpp.git 3rdparty/avcpp
```

```cmake
add_subdirectory(3rdparty/avcpp)
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

avcpp detects that it is not the top-level project and skips installation.

## Integrating with FetchContent

```cmake
include(FetchContent)

FetchContent_Declare(
    avcpp
    GIT_REPOSITORY https://github.com/h4tr3d/avcpp.git
    GIT_TAG        v3.1.0
)
set(AV_ENABLE_STATIC On)
set(AV_ENABLE_SHARED Off)
FetchContent_MakeAvailable(avcpp)

target_link_libraries(prog PRIVATE avcpp::avcpp)
```

## Installing with vcpkg

```json
{
    "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
    "dependencies": [
        "avcpp"
    ],
    "name": "your-app-name",
    "version-string": "0.0.1"
}
```

```cmake
find_package(avcpp CONFIG REQUIRED)
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

## Building with Meson

@warning CMake is the supported build. The Meson build has no maintainer and is
not covered by CI.

```sh
meson setup build
meson compile -C build
meson test -C build
meson install -C build
```

`-Dbuild_tests=false` and `-Dbuild_samples=false` correspond to
`AVCPP_ENABLE_TESTS` and `AV_BUILD_EXAMPLES`; `-Dtest_data=true` corresponds to
`AVCPP_TEST_DATA`.

## Building the documentation

```sh
cmake -B build -DAVCPP_ENABLE_DOCS=ON
cmake --build build --target docs        # English + Russian, into build/docs/site
cmake --build build --target docs-serve  # same, then serve on localhost:8000
```

`docs-check` builds the reference and compares its warnings with
`docs/doxygen-baseline.txt`; it fails only when a new undocumented member
appears. Use `docs/build.sh --help` for the same commands without CMake.
