#!/usr/bin/env bash

set -ex

# CMake
(
    wget http://www.cmake.org/files/v3.10/cmake-3.10.1.tar.gz
    tar -xvzf cmake-3.4.1.tar.gz
    cd cmake-3.4.1/
    ./configure --prefix=/usr
    make
    sudo make install
)

# FFmpeg
# - https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-3
(
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
)

