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
  - GCC >= 4.8 (C++11)
  
At the current moment its possible to compile code with compilator without C++11 support but with Boost.Thread and Boost.SmartPtr. Simple code modifications is needed.


Near future plans
-----------------

  - Building for Android (ready to commint but small code clean up is required)


Future plans (long and not)
---------------------------

  - Code cleanup
  - Remove all deprecates in Filters module (FFMPEG 2.x)
  - API redisign, make it more intuitively
  - Filters module complete rework
  - Add good samples
  - Make hight-level entities like av::Encoder, av::Decoder, av::Muxer, av::Demuxer and declare some API to combine them.
