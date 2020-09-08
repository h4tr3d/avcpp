#!/usr/bin/env sh

meson_build_type=release

if [ "$BUILD_TYPE" = "Release" ]; then
    meson_build_type=release
elif [ "$BUILD_TYPE" = "Debug" ]; then
    meson_build_type=debug
fi

meson_build() {
    mkdir meson-build || return 1
    cd meson-build || return 1
    meson setup .. || return 1
    meson configure --buildtype $meson_build_type || return 1
    # Old Ubuntu Meson does not recognize -C ., OSX require it
    if [ "$TRAVIS_OS_NAME" = "osx" ]; then
        meson compile -C . || return 1
    else
        meson compile || return 1
    fi
    meson test || return 1
    cd ..
}

main() {
    if [ x"$NO_MESON" = x"1" ]; then
        echo "Meson build skipped"
    else
        meson_build || return 1
    fi
    return 0
}

main
