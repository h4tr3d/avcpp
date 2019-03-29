# AvCpp [![Build Status](https://travis-ci.org/h4tr3d/avcpp.svg?branch=master)](https://travis-ci.org/h4tr3d/avcpp) [![Language grade: C/C++](https://img.shields.io/lgtm/grade/cpp/g/h4tr3d/avcpp.svg?logo=lgtm&logoWidth=18)](https://lgtm.com/projects/g/h4tr3d/avcpp/context:cpp) [![PRs Welcome](https://img.shields.io/badge/PRs-welcome-brightgreen.svg)](http://makeapullrequest.com) 

Wrapper for the FFmpeg that simplify usage it from C++ projects.

Currently covered next functionality:
  - Core helper & utility classes (AVFrame -> av::AudioSample & av::VideoFrame, AVRational -> av::Rational and so on)
  - Container formats & contexts muxing and demuxing
  - Codecs & codecs contexts: encoding and decoding
  - Streams (AVStream -> av::Stream)
  - Filters (audio & video): parsing from string, manual adding filters to the graph & other
  - SW Video & Audio resamplers


Requirements
------------

  - FFmpeg >= 2.0
    - libavformat >= 54.x.x
    - libavcodec >= 54.x.x
    - libavfilter >= 3.x.x
    - libavutil >= 51.x.x
    - libswscale >= 2.x.x
    - libswresample >= 0.x.x
    - libpostproc >= 52.x.x
  - GCC >= 5.0 (C++11 is required)

### Debian

You should install FFmpeg packages from the deb-multimedia.org site:
```
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


### Ubuntu and Linux Mint

You should add [ffmpeg-3](https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-3) PPA:
```
sudo add-apt-repository ppa:jonathonf/ffmpeg-3 -y
sudo add-apt-repository ppa:jonathonf/tesseract -y
sudo apt update && sudo apt upgrade
sudo apt install libavcodec-dev \
                 libavdevice-dev \
                 libavfilter-dev \
                 libavformat-dev \
                 libavresample-dev \
                 libavutil-dev \
                 libpostproc-dev \
                 libswresample-dev \
                 libswscale-dev
```

Build
-----

```
git clone https://github.com/h4tr3d/avcpp.git avcpp-git
cd avcpp-git
mkdir build
cd build
cmake ..
make -j8
```

If FFmpeg located in non-standard place:
```
cmake -DPC_FFMPEG_LIBRARY_DIRS=<some_path> -DPC_FFMPEG_INCLUDE_DIRS=<some_path> ..
```

To point install prefix:
```
cmake -DCMAKE_INSTALL_PREFIX=/usr ..
```

Install:
```
sudo make install
```
or (for packaging)
```
sudo make DESTDIR=<some_prefix> install
```

Refer to CMake documentation for more details that can cover some special cases.

Near future plans
-----------------

  - ~~Building for Android (ready to commint but small code clean up is required)~~
  - CI with complex build matrix:
    - FFmpeg 3.x and FFmpeg 2.8
    - GCC 5 and 7, MSVS 2015 and 2017, clang
    - Linux, OSX, Windows
    - Tests

Future plans (long and not)
---------------------------

  - Code cleanup
  - Remove all deprecates in Filters module (FFmpeg 2.x)
  - API redisign, make it more intuitively
  - Filters module complete rework
  - Add good samples
  - Make hight-level entities like av::Encoder, av::Decoder, av::Muxer, av::Demuxer and declare some API to combine them.
  - More advanced Android building
