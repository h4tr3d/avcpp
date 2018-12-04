#!/usr/bin/env bash

set -e

JOBS=$(cat /proc/cpuinfo | grep '^processor' | wc -l)
export MAKEFLAGS="-j${JOBS}"

# CMake
echo "Prepare CMake"
build_cmake()
{
    local CMAKE_VERSION_BASE=3.11
    local CMAKE_VERSION_MINOR=0
    local CMAKE_VERSION_FULL=${CMAKE_VERSION_BASE}.${CMAKE_VERSION_MINOR}
    local CMAKE_ARCH=x86_64

    #wget -c http://www.cmake.org/files/v${CMAKE_VERSION_BASE}/cmake-${CMAKE_VERSION_FULL}.tar.gz
    #tar -xzf cmake-${CMAKE_VERSION_FULL}.tar.gz
    #cd cmake-${CMAKE_VERSION_FULL}/
    #./configure --prefix=/usr
    #make
    #sudo make install

    wget -c https://cmake.org/files/v${CMAKE_VERSION_BASE}/cmake-${CMAKE_VERSION_FULL}-Linux-${CMAKE_ARCH}.tar.gz
    tar -xzf cmake-${CMAKE_VERSION_FULL}-Linux-${CMAKE_ARCH}.tar.gz
    export PATH=$(pwd)/cmake-${CMAKE_VERSION_FULL}-Linux-${CMAKE_ARCH}/bin:$PATH
    cmake --version
}

build_cmake

# Newer GCC
#if [ -n "$MATRIX_EVAL" ]; then
#    echo "Prepare GCC"
#    sudo add-apt-repository ppa:ubuntu-toolchain-r/test -y
#    sudo apt-get -qq update
#    (
#        eval "${MATRIX_EVAL}"
#        case "${CXX}" in
#            g++-5)
#                pkg='g\+\+-5'
#            ;;
#            g++-6)
#                pkg='g\+\+-6'
#            ;;
#            g++-7)
#                pkg='g\+\+-7'
#            ;;
#            *)
#                echo "Unknown compiler: ${CXX}"
#                exit 1
#            ;;
#        esac
#        #sudo apt-get install -y 'g\+\+-5' 'g\+\+-6' 'g\+\+-7'
#        sudo apt-get install -y "${pkg}"
#    )
#fi

# FFmpeg
# - https://launchpad.net/~jonathonf/+archive/ubuntu/ffmpeg-3
echo "Prepare FFmpeg"
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

set +e
