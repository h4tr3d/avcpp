AvCpp
=====

Wrapper for the FFMPEG that simplify usage it from C++ projects.

Currently covered next functionality:
  - Core helper & utility classes (AVFrame -> av::AudioSample & av::VideoFrame, AVRational -> av::Rational and so on)
  - Container formats & contexts muxing and demuxing
  - Codecs & codecs contexts: encoding and decoding
  - Streams (AVStream -> av::Stream)
  - Filters (audio & video): parsing from string, manual adding filters to the graph & other
  - SW Video & Audio resamplers


Requirements
------------

  - FFMPEG >= 1.0
    - libavformat >= 54.x.x
    - libavcodec >= 54.x.x
    - libavfilter >= 3.x.x
    - libavutil >= 51.x.x
    - libswscale >= 2.x.x
    - libswresample >= 0.x.x
    - libpostproc >= 52.x.x
  - GCC >= 4.8 (C++11)
  
At the current moment it is possible to compile code with compilator without C++11 support but with Boost.Thread and Boost.SmartPtr. Simple code modifications is needed.


### Debian

You should install FFmpeg packages from the deb-multimedia.org site:
```
sudo apt-get install libavformat-dev libavcodec-dev libavutil-dev libavfilter-dev libswscale-dev libswresample-dev libpostproc-dev
```

Note 1: I did not test building on Debian.

Note 2: Debian Wheezy repo contains only FFmpeg 1.0.8. I tested building only with 2.x. So it is strongly recoment use Wheezy back-ports repo.


### Ubuntu and Linux Mint

You should add samrog131 PPA:
```
sudo add-apt-repository ppa:samrog131/ppa
sudo apt-get update
sudo apt-get install libavformat-ffmpeg-dev libavcodec-ffmpeg-dev libavutil-ffmpeg-dev libavfilter-ffmpeg-dev libswscale-ffmpeg-dev libswresample-ffmpeg-dev libpostproc-ffmpeg-dev
```

To remove PPA:
```
sudo apt-add-repository --remove ppa:samrog131/ppa
sudo apt-get update
```



Near future plans
-----------------

  - ~~Building for Android (ready to commint but small code clean up is required)~~


Future plans (long and not)
---------------------------

  - Code cleanup
  - Remove all deprecates in Filters module (FFMPEG 2.x)
  - API redisign, make it more intuitively
  - Filters module complete rework
  - Add good samples
  - Make hight-level entities like av::Encoder, av::Decoder, av::Muxer, av::Demuxer and declare some API to combine them.
  - More advanced Android building
