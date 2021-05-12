#!/usr/bin/env bash

#set -ex

# WA for strange OSX error
#shell_session_update() { :; }

#brew update
#brew install python
brew link --overwrite python

# GH workflow specific part
if [ -n "$GITHUB_WORKFLOW" ]; then
    if [ -z "$SKIP_MESON" -o "$SKIP_MESON" = "false" ]; then
        brew install meson
        brew install ffmpeg
    fi
fi

# WA for homebrew
true
