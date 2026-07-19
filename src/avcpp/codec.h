#pragma once

#include "avcpp/avcpp_export.h"

#include <list>
#include <deque>
#include <memory>

#include "ffmpeg.h"
#include "rational.h"
#include "format.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "channellayout.h"
#include "avutils.h"

namespace av {

class AVCPP_EXPORT Codec : public FFWrapperPtr<const AVCodec>
{
public:
    using FFWrapperPtr<const AVCodec>::FFWrapperPtr;

    const char *name() const;
    const char *longName() const;
    bool        canEncode() const;
    bool        canDecode() const;
    bool        isEncoder() const;
    bool        isDecoder() const;
    AVMediaType type() const;

    std::deque<Rational>       supportedFramerates()    const;
    std::deque<PixelFormat>    supportedPixelFormats()  const;
    std::deque<int>            supportedSamplerates()   const;
    std::deque<SampleFormat>   supportedSampleFormats() const;
    std::deque<uint64_t>       supportedChannelLayouts() const;

#if AVCPP_API_NEW_CHANNEL_LAYOUT
    std::deque<ChannelLayoutView> supportedChannelLayouts2() const;
#endif

    AVCodecID id() const;
};



AVCPP_EXPORT Codec findEncodingCodec(AVCodecID id);
AVCPP_EXPORT Codec findEncodingCodec(const std::string& name);

AVCPP_EXPORT Codec findDecodingCodec(AVCodecID id);
AVCPP_EXPORT Codec findDecodingCodec(const std::string& name);

#if AVCPP_HAS_AVFORMAT
AVCPP_EXPORT Codec findEncodingCodec(const OutputFormat &format, bool isVideo = true);
AVCPP_EXPORT Codec guessEncodingCodec(OutputFormat format, const char *name, const char *url, const char* mime, AVMediaType mediaType);
#endif // if AVCPP_HAS_AVFORMAT

} // ::fmpeg
