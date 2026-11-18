#pragma once

// Shared helpers for the hardware tests.
//
// Every test probes the backends of the machine and skips what the runtime
// cannot do, so that a CI machine without devices stays green. Some
// limitations are properties of the FFmpeg build rather than of the machine;
// they are collected here so that both test files agree on them.

#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#include "avcpp/avcompat.h"
#include "avcpp/hwcontext.h"

namespace avtest {

/**
 * @brief Backends exercised by the hardware tests on this platform.
 */
inline std::vector<std::string> curatedDevices()
{
#if defined(_WIN32)
    return { "d3d11va", "dxva2", "qsv", "cuda", "amf" };
#elif defined(__APPLE__)
    return { "videotoolbox" };
#elif defined(__ANDROID__)
    return { "mediacodec" };
#else
    return { "vaapi", "qsv", "cuda", "vdpau" };
#endif
}

/**
 * @brief Splits a comma separated environment value.
 */
inline std::vector<std::string> splitList(const char* value)
{
    std::vector<std::string> result;
    if (!value)
        return result;

    std::istringstream stream(value);
    std::string item;
    while (std::getline(stream, item, ',')) {
        if (!item.empty())
            result.push_back(item);
    }
    return result;
}

/**
 * @brief Backends of this platform after AVCPP_HW_DEVICES and AVCPP_HW_SKIP.
 */
inline std::vector<std::string> selectedDevices()
{
    std::vector<std::string> devices = curatedDevices();

    const std::vector<std::string> include =
        splitList(std::getenv("AVCPP_HW_DEVICES"));
    if (!include.empty()) {
        devices.erase(std::remove_if(devices.begin(), devices.end(),
                                     [&](const std::string& name) {
                                         return std::find(include.begin(), include.end(), name) == include.end();
                                     }),
                      devices.end());
    }

    const std::vector<std::string> exclude =
        splitList(std::getenv("AVCPP_HW_SKIP"));
    if (!exclude.empty()) {
        devices.erase(std::remove_if(devices.begin(), devices.end(),
                                     [&](const std::string& name) {
                                         return std::find(exclude.begin(), exclude.end(), name) != exclude.end();
                                     }),
                      devices.end());
    }

    return devices;
}

/**
 * @brief Reports FFmpeg builds that use libmfx instead of oneVPL for QSV.
 */
inline bool isLibmfxQsv() noexcept
{
#if AVCPP_HAS_HWCONTEXT && (AVCPP_AVUTIL_VERSION_MAJOR < 57) // FFmpeg < 5.0
    return true;
#else
    return false;
#endif
}

/**
 * @brief Reports whether a manually created frames context is unsupported.
 *
 * FFmpeg 4.x VideoToolbox does not implement frames_get_constraints() and
 * does not create a CVPixelBufferPool for a frames context allocated by
 * av_hwframe_ctx_alloc(). The pool is provided by the codec there, so a
 * generic frames context can neither report constraints nor allocate or
 * transfer frames. Codec driven paths still work.
 */
inline bool isVideoToolboxFramesUnsupported(av::hw::DeviceType type) noexcept
{
#if AVCPP_HAS_HWCONTEXT && (AVCPP_AVUTIL_VERSION_MAJOR < 57) // FFmpeg < 5.0
    return type.get() == AV_HWDEVICE_TYPE_VIDEOTOOLBOX;
#else
    (void)type;
    return false;
#endif
}

/**
 * @brief Reports whether QSV frame transfer is known to hang.
 *
 * FFmpeg 4.x uploads and downloads frames through libmfx. With the Ubuntu
 * 22.04 libmfx and iHD stack av_hwframe_transfer_data() never returns: the
 * driver keeps spawning threads until the process is killed. ffmpeg(1) hangs
 * on the same conversion, so this is a runtime limitation, not an avcpp bug.
 */
inline bool isQsvTransferBroken(av::hw::DeviceType type) noexcept
{
    return isLibmfxQsv() && type.get() == AV_HWDEVICE_TYPE_QSV;
}

/**
 * @brief Reports whether QSV encoders are unavailable.
 *
 * The same stack has no media SDK implementation for current devices, so
 * MFXVideoENCODE_Init() rejects every parameter set and avcodec_open2() fails
 * with AVERROR(ENOSYS).
 */
inline bool isQsvEncoderUnavailable(av::hw::DeviceType type) noexcept
{
    return isLibmfxQsv() && type.get() == AV_HWDEVICE_TYPE_QSV;
}

/**
 * @brief Reports whether QSV decoding stays in system memory.
 *
 * Ubuntu 22.04 ships FFmpeg 4.4.2, where the QSV decoder ignores the derived
 * frames context and returns system memory frames: the media driver decodes
 * the stream, but no device frames are produced. Only the deprecated
 * "-hwaccel qsv" path returns frames in video memory there, and avcpp does not
 * use it.
 */
inline bool isQsvHwDecodeUnavailable(av::hw::DeviceType type) noexcept
{
    return isLibmfxQsv() && type.get() == AV_HWDEVICE_TYPE_QSV;
}

} // namespace avtest
