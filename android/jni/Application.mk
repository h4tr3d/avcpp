APP_OPTIM    := release
APP_PLATFORM := android-14
APP_STL      := gnustl_static
APP_CPPFLAGS += -frtti
APP_CPPFLAGS += -fexceptions
APP_CPPFLAGS += -std=c++11
APP_CPPFLAGS += -D__STDC_CONSTANT_MACROS
#APP_ABI      := armeabi armeabi-v7a
APP_ABI      := armeabi-v7a
APP_MODULES  := avcpp-android
NDK_TOOLCHAIN_VERSION := 4.8
