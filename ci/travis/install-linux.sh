#!/usr/bin/env bash

set -ex

JOBS=$(cat /proc/cpuinfo | grep '^processor' | wc -l)
export MAKEFLAGS="-j${JOBS}"

# CMake
(
    CMAKE_VERSION_BASE=3.10
    CMAKE_VERSION_MINOR=1
    CMAKE_VERSION_FULL=${CMAKE_VERSION_BASE}.${CMAKE_VERSION_MINOR}
    wget -c http://www.cmake.org/files/v${CMAKE_VERSION_BASE}/cmake-${CMAKE_VERSION_FULL}.tar.gz
    tar -xvzf cmake-${CMAKE_VERSION_FULL}.tar.gz
    cd cmake-${CMAKE_VERSION_FULL}/
    ./configure --prefix=/usr
    make
    sudo make install
)

# Newer GCC
(
    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
    sudo apt-get -qq update
    sudo apt-get install -y g++5 g++6 g++7
)

# FFmpeg
# - https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-3
(
    sudo add-apt-repository ppa:jonathonf/ffmpeg-3 -y
    sudo add-apt-repository ppa:jonathonf/tesseract -y
    sudo apt-get -qq update
    sudo apt-get install -y libavcodec-dev \
                            libavdevice-dev \
                            libavfilter-dev \
                            libavformat-dev \
                            libavresample-dev \
                            libavutil-dev \
                            libpostproc-dev \
                            libswresample-dev \
                            libswscale-dev
)
