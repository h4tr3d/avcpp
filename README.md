# AvCpp [![Build Status](https://github.com/h4tr3d/avcpp/actions/workflows/cmake-ci.yml/badge.svg)](https://github.com/h4tr3d/avcpp/actions/workflows/cmake-ci.yml) [![CodeQL C++](https://github.com/h4tr3d/avcpp/actions/workflows/codeql.yml/badge.svg)](https://github.com/h4tr3d/avcpp/security/code-scanning) [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](https://makeapullrequest.com)

Wrapper for the FFmpeg that simplify usage it from C++ projects.

Currently covered next functionality:

- Core helper & utility classes (`AVFrame` → `av::AudioSample` & `av::VideoFrame`, `AVRational` → `av::Rational` and so on)
- Container formats & contexts muxing and demuxing
- Codecs & codecs contexts: encoding and decoding
- Streams (`AVStream` → `av::Stream`)
- Filters (audio & video): parsing from string, manual adding filters to the graph & other
- SW Video & Audio resamplers

You can read the full documentation [here](https://h4tr3d.github.io/avcpp/).

## Requirements

- FFmpeg >= 4.0
  - libavformat >= 58.x.x
  - libavcodec >= 58.x.x
  - libavdevice >= 58.x.x
  - libavfilter >= 7.x.x
  - libavutil >= 56.x.x
  - libswscale >= 5.x.x
  - libswresample >= 3.x.x
  - libpostproc >= 55.x.x
- GCC >= 9.0 (C++17 is required. [See](https://gcc.gnu.org/projects/cxx-status.html#cxx17), GCC from 6.0 may built code, but unchecked)
- CMake (> 3.19)

> [!NOTE]
> Oldest versions of the FFmpeg (at least >=2.0) may be successfully built and work but is it not checked for now. Same notes valid for the GCC version from 6.0.

### Debian, Ubuntu 19.10+ and Linux Mint 20.x or newer

> [!NOTE] 
>
> Build verification done only for oldest Ubuntu version provided by the GitHub Flow. You can look it [here](https://github.com/h4tr3d/avcpp/blob/master/.github/workflows/cmake-ci.yml#L25). For now, it is 22.04.

You should install FFmpeg packages from the deb-multimedia.org site:

```bash
sudo apt-get install libavformat-dev \
                     libavcodec-dev \
                     libavutil-dev \
                     libavfilter-dev \
                     libswscale-dev \
                     libswresample-dev \
                     libpostproc-dev \
                     libavdevice-dev
```

> [!NOTE]
>
> **Note 1**: I did not test building on Debian.

>  [!NOTE]
>
> **Note 2**: Debian Wheezy repo contains only FFmpeg 1.0.8. I tested building only with ~2.x~ 4.0. So it is strongly recommended use Wheezy back-ports repo.

### Ubuntu 18.04 and Linux Mint 19.x

If you are on Ubuntu bionic or Linux Mint 19.x you should add [ffmpeg-4](https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-4) PPA:

```bash
sudo add-apt-repository ppa:jonathonf/ffmpeg-4 -y
sudo apt update && sudo apt upgrade
```

After that  just install the same packages as above.

------

## Build & Usage

### Building with CMake

```bash
git clone --recurse-submodules https://github.com/h4tr3d/avcpp.git avcpp-git
cd avcpp-git
mkdir build
cd build
cmake ..
make -j8
```

If your Git version so old (refer to the [SO for clarification](https://stackoverflow.com/questions/3796927/how-to-git-clone-including-submodules)) you can just
replace `--recurse-submodules` with pair of `git submodule init && git submodule update`.

If FFmpeg located in non-standard place:

```bash
cmake -DPC_FFMPEG_LIBRARY_DIRS=<some_path> -DPC_FFMPEG_INCLUDE_DIRS=<some_path> ..
```

To point install prefix:

```bash
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
```

Install:

```bash
sudo make install
```

or (for packaging)

```bash
sudo make DESTDIR=<some_prefix> install
```

Refer to CMake documentation for more details that can cover some special cases.

#### Useful configure directives

- `AV_ENABLE_STATIC` - Bool, enable static library build, On by default.
- `AV_ENABLE_SHARED` - Bool, enable shared library build, On by default.
- `AV_BUILD_EXAMPLES` - Bool, enable examples build, On by default.
- `AV_BUILD_TESTS` - Bool, enable tests build, On by default.
- `AV_DISABLE_AVFORMAT` - Bool, disable livavformat usage. libavformat enabled by default. As dependency disables libavfilter and libavdevice.
- `AV_DISABLE_AVFILTER` - Bool, disable libavfilter usage. libavfilter enabled by default. As dependency disables libavdevice.
- `AV_DISABLE_AVDEVICE` - Bool, disable libavdevice usage. libavdevice enabled by default.
- C++ related
  - `CMAKE_CXX_STANDARD` - Can be defined globally to override default C++ version. C++17 required at least.
- FFmpeg related:
  - `FFMPEG_PKG_CONFIG_SUFFIX` - String, TBD
  - `PC_<component>_LIBRARY_DIRS` - Path, hint where FFmpeg component library looking
  - `PC_FFMPEG_LIBRARY_DIRS` - Path, hint where a whole FFmpeg libraries looking
  - `PC_<component>_INCLUDE_DIRS` - Path, hint where a FFmpeg component includes looking
  - `PC_FFMPEG_INCLUDE_DIRS` - Path, hint where a whole FFmpeg includes looking
  - `<component>` are:
    - `AVCODEC`
    - `AVDEVICE`
    - `AVFORMAT`
    - `AVFILTER`
    - `AVUTIL`
    - `POSTPROC`
    - `SWSCALE`
    - `SWRESAMPLE`

### Integrate to the CMake project as Git Submodule

AvCpp can be simple integrated into CMake-based project as Git submodule.

Just do something like:

```sh
git submodule add https://github.com/h4tr3d/avcpp.git 3rdparty/avcpp
```

And in the your CMakeLists.txt:

```cmake
add_subdirectory(3rdparty/avcpp)
```

AvCpp detects that it included as non-root projects and skips some steps, like installation.

CMake parameters pointed above may be sets before `add_subdirectory()`.

Than use via `target_link_libraries`:

```cmake
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

### Integrate to the CMake project via FetchContent

Just do in your CMakeLists.txt:

```cmake
include(FetchContent)
FetchContent_Declare(
    avcpp
    GIT_REPOSITORY https://github.com/h4tr3d/avcpp.git
    GIT_TAG        v2.90.2
)
FetchContent_MakeAvailable(avcpp)
```

Than use via `target_link_libraries`:

```cmake
target_link_libraries(prog PRIVATE avcpp::avcpp)
```

CMake arguments can be set before `FetchContent_MakeAvailable` call:

```cmake
FetchContent_Declare(
    avcpp
    ...
)
set(AV_ENABLE_STATIC On)
set(AV_ENABLE_SHARED Off)
FetchContent_MakeAvailable(avcpp)
```

### Use via Vcpkg

Just put into vcpkg.json:

```json
{
    "$schema": "https://raw.githubusercontent.com/microsoft/vcpkg-tool/main/docs/vcpkg.schema.json",
    "dependencies": [
        ...
        "avcpp",
        ...
    ],
    "name": "your-app-name",
    "version-string": "0.0.1"
}
```

Than in the your CMakeLists.txt:

```cmake
find_package(avcpp CONFIG REQUIRED)
target_link_libraries(prog PRIVATE avcpp::avcpp)
```



### Building with Meson

> [!CAUTION]
>
> CMake usage strongly recommended.
> Meson build has no maintainer and broken for the last code. It is excluded from the CI builds.

Before you can begin with the building you have to clone the repository like this:

```bash
git clone https://github.com/h4tr3d/avcpp.git avcpp-git
cd avcpp-git
```

IDE Integration:

There are extentions for various IDEs like VS Code/Codium, Eclipse, Xcode, etc.
Refer to the [docs](https://mesonbuild.com/IDE-integration.html) for more information.

Building the project:

If you don't have the dependencies installed, meson will download and compile them.
Because ffmpeg is so large (~2000 c files), you should consider using your package manager to install them.
You can then build the project with the following commands:

```bash
mkdir build
cd build
meson ..
meson compile
```

Configuring the project:

By default the sample projects and the test are compiled.
If you don't want this you can disable it with the following commands:

```bash
meson configure -Dbuild_tests=false
meson configure -Dbuild_samples=false
```

You can set the install prefix using `meson --prefix <your/own/prefix>`.
To see all of the available options just type `meson configure` and `meson configure --help` to get more information.

Installing the project:

Just type `meson install` and the project will be installed in the configured prefix (/usr/local by default).

Running the tests:

To run the test just use `meson test`.
If you disabled the test this will do nothing.
