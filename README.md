# AvCpp [![Build Status](https://travis-ci.org/h4tr3d/avcpp.svg?branch=master)](https://travis-ci.org/h4tr3d/avcpp) [![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/h4tr3d/avcpp.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/h4tr3d/avcpp/context:cpp) [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com)

Wrapper for the FFmpeg that simplify usage it from C++ projects.

Currently covered next functionality:

- Core helper & utility classes (AVFrame -> av::AudioSample & av::VideoFrame, AVRational -> av::Rational and so on)
- Container formats & contexts muxing and demuxing
- Codecs & codecs contexts: encoding and decoding
- Streams (AVStream -> av::Stream)
- Filters (audio & video): parsing from string, manual adding filters to the graph & other
- SW Video & Audio resamplers

You can read the full documentation [here](https://h4tr3d.github.io/avcpp/).

## Requirements

- FFmpeg >= 2.0
  - libavformat >= 54.x.x
  - libavcodec >= 54.x.x
  - libavfilter >= 3.x.x
  - libavutil >= 51.x.x
  - libswscale >= 2.x.x
  - libswresample >= 0.x.x
  - libpostproc >= 52.x.x
- GCC >= 5.0 (C++11 is required)
- CMake (> 3.11) or Meson(> 50.0)

### Debian, Ubuntu 19.10 and Linux Mint 20.x or newer

You should install FFmpeg packages from the deb-multimedia.org site:

```bash
sudo apt-get install libavformat-dev \
                     libavcodec-dev \
                     libavutil-dev \
                     libavfilter-dev \
                     libswscale-dev \
                     libswresample-dev \
                     libpostproc-dev
```

Note 1: I did not test building on Debian.

Note 2: Debian Wheezy repo contains only FFmpeg 1.0.8. I tested building only with 2.x. So it is strongly recoment use Wheezy back-ports repo.

### Ubuntu 18.04 and Linux Mint 19.x

If you are on Ubuntu bionic or Linux Mint 19.x you should add [ffmpeg-4](https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-4) PPA:

```bash
sudo add-apt-repository ppa:jonathonf/ffmpeg-4 -y
sudo apt update && sudo apt upgrade
```

After that  just install the same packages as above.

------

## Build

There are two ways to compile eitehr with CMake or with meson.
By default meson is faster, but if your project uses CMake, those instructions might be better for integration.

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

### Building with meson

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
To see all of the available options just type `meson configure` and `meson configure --help` to get more insormation.

Installing the project:

Just type `meson install` and the project will be installed in the configured prefix (/usr/local by default).

Running the tests:

To run the test just use `meson test`.
If you disabled the test this will do nothing.
