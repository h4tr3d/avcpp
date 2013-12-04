
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE    := avcpp-android

GLOBAL_C_INCLUDES := \
  $(LOCAL_PATH)/../../src

LOCAL_SRC_FILES := \
  $(LOCAL_PATH)/../../src/videoframe.cpp \
  $(LOCAL_PATH)/../../src/filterbufferref.cpp \
  $(LOCAL_PATH)/../../src/filteropaque.cpp    \
  $(LOCAL_PATH)/../../src/streamcoder.cpp     \
  $(LOCAL_PATH)/../../src/filter.cpp          \
  $(LOCAL_PATH)/../../src/filtergraph.cpp     \
  $(LOCAL_PATH)/../../src/avtime.cpp          \
  $(LOCAL_PATH)/../../src/codec.cpp           \
  $(LOCAL_PATH)/../../src/filterinout.cpp     \
  $(LOCAL_PATH)/../../src/rational.cpp        \
  $(LOCAL_PATH)/../../src/stream.cpp          \
  $(LOCAL_PATH)/../../src/avutils.cpp         \
  $(LOCAL_PATH)/../../src/filters/buffersrc.cpp \
  $(LOCAL_PATH)/../../src/filters/buffersink.cpp \
  $(LOCAL_PATH)/../../src/containerformat.cpp \
  $(LOCAL_PATH)/../../src/filterpad.cpp       \
  $(LOCAL_PATH)/../../src/frame.cpp           \
  $(LOCAL_PATH)/../../src/packet.cpp          \
  $(LOCAL_PATH)/../../src/audiosamples.cpp    \
  $(LOCAL_PATH)/../../src/videoresampler.cpp  \
  $(LOCAL_PATH)/../../src/container.cpp       \
  $(LOCAL_PATH)/../../src/rect.cpp            \
  $(LOCAL_PATH)/../../src/audioresampler.cpp  \
  $(LOCAL_PATH)/../../src/filtercontext.cpp

#FFMPEG_INCDIR   := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH)/include
#FFMPEG_LIBDIR   := $(LOCAL_PATH)/ffmpeg/$(TARGET_ARCH)/lib

FFMPEG_INCDIR   := $(LOCAL_PATH)/ffmpeg/armeabi-v7a/include
FFMPEG_LIBDIR   := $(LOCAL_PATH)/ffmpeg/armeabi-v7a/lib


LOCAL_CXXFLAGS  += -I$(FFMPEG_INCDIR)
LOCAL_LDLIBS    := -L$(FFMPEG_LIBDIR) -lm

LOCAL_STATIC_LIBRARIES := avformat avcodec swscale swresample avutil avfilter

LOCAL_C_INCLUDES := $(GLOBAL_C_INCLUDES)

include $(BUILD_STATIC_LIBRARY)

