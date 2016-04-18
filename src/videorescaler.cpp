#include "avlog.h"

#include "videorescaler.h"

using namespace std;

namespace av
{

VideoRescaler::VideoRescaler()
{}

VideoRescaler::VideoRescaler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat,
                             int srcWidth, int srcHeight, PixelFormat srcPixelFormat, int32_t flags)
    : m_dstWidth(dstWidth),
      m_dstHeight(dstHeight),
      m_dstPixelFormat(dstPixelFormat),
      m_srcWidth(srcWidth),
      m_srcHeight(srcHeight),
      m_srcPixelFormat(srcPixelFormat),
      m_flags(flags)
{
    if (dstWidth <= 0 || dstHeight <= 0 || dstPixelFormat == AV_PIX_FMT_NONE) {
        fflog(AV_LOG_FATAL,
                "Invalid destination parameters: w=%d, h=%d, pix_fmt=%s\n",
                dstWidth,
                dstHeight,
                av_get_pix_fmt_name(dstPixelFormat));
        throw invalid_argument("Invalid destination parameters");
    }

    getContext(flags);
}

VideoRescaler::VideoRescaler(int dstWidth, int dstHeight, PixelFormat dstPixelFormat, int32_t flags)
    : VideoRescaler(dstWidth, dstHeight, dstPixelFormat, 0, 0, AV_PIX_FMT_NONE, flags)
{
}

VideoRescaler::VideoRescaler(const VideoRescaler &other)
    : VideoRescaler(other.m_dstWidth, other.m_dstHeight, other.m_dstPixelFormat,
                    other.m_srcWidth, other.m_srcHeight, other.m_srcPixelFormat,
                    other.m_flags)
{
}

VideoRescaler::VideoRescaler(VideoRescaler &&other)
    : VideoRescaler()
{
    swap(other);
}

VideoRescaler::~VideoRescaler()
{
    if (m_raw)
        sws_freeContext(m_raw);
}

void VideoRescaler::swap(VideoRescaler &other) noexcept
{
    using std::swap;
    swap(m_dstWidth,       other.m_dstWidth);
    swap(m_dstHeight,      other.m_dstHeight);
    swap(m_dstPixelFormat, other.m_dstPixelFormat);
    swap(m_srcWidth,       other.m_srcWidth);
    swap(m_srcHeight,      other.m_srcHeight);
    swap(m_srcPixelFormat, other.m_srcPixelFormat);
    swap(m_flags,          other.m_flags);
    swap(m_raw,            other.m_raw);
}

void VideoRescaler::getContext(int32_t flags)
{
    if (m_srcWidth <= 0 || m_srcHeight <= 0 || m_srcPixelFormat == AV_PIX_FMT_NONE ||
        m_srcWidth <= 0 || m_dstHeight <= 0 || m_dstPixelFormat == AV_PIX_FMT_NONE)
    {
        if (m_raw)
            sws_freeContext(m_raw);
        m_raw = nullptr;
        return;
    }

    if (flags == SwsFlagAuto) {
        if (m_srcWidth < m_dstWidth)
            flags = SWS_BICUBIC;
        else
            flags = SWS_AREA;
    }

    m_raw = sws_getCachedContext(m_raw,
                                 m_srcWidth, m_srcHeight, m_srcPixelFormat,
                                 m_dstWidth, m_dstHeight, m_dstPixelFormat,
                                 flags,
                                 nullptr, nullptr, nullptr);
}

bool VideoRescaler::validate(int width, int height, PixelFormat pixelFormat)
{
    if (width > 0 && height > 0 && pixelFormat != AV_PIX_FMT_NONE)
        return true;
    return false;
}

VideoRescaler& VideoRescaler::operator=(const VideoRescaler &rhs)
{
    if (this != &rhs)
        VideoRescaler(rhs).swap(*this);
    return *this;
}

VideoRescaler& VideoRescaler::operator=(VideoRescaler &&rhs)
{
    if (this != &rhs) {
        VideoRescaler(std::move(rhs)).swap(*this);
    }
    return *this;
}

void VideoRescaler::rescale(VideoFrame &dst, const VideoFrame &src, error_code &ec)
{
    m_srcWidth       = src.width();
    m_srcHeight      = src.height();
    m_srcPixelFormat = src.pixelFormat();

    m_dstWidth       = dst.width();
    m_dstHeight      = dst.height();
    m_dstPixelFormat = dst.pixelFormat();

    clear_if(ec);

    // Context can be allocated on demand
    getContext(m_flags);

    if (!m_raw) {
        fflog(AV_LOG_ERROR, "Can't allocate swsContext for given input/output parameters\n");
        throws_if(ec, Errors::RescalerInvalidParameters);
        return;
    }

    dst.setPts(src.pts());

    const AVFrame *inpFrame = src.raw();
    AVFrame *outFrame = dst.raw();

    const uint8_t* srcFrameData[AV_NUM_DATA_POINTERS] = {
        inpFrame->data[0],
        inpFrame->data[1],
        inpFrame->data[2],
        inpFrame->data[3]
    #if AV_NUM_DATA_POINTERS == 8
        ,
        inpFrame->data[4],
        inpFrame->data[5],
        inpFrame->data[6],
        inpFrame->data[7]
    #endif
    };

    int sts = sws_scale(m_raw, srcFrameData, inpFrame->linesize, 0, m_srcHeight,
                         outFrame->data, outFrame->linesize);
    if (sts < 0) {
        throws_if(ec, sts, ffmpeg_category());
        return;
    } else if (sts == 0) {
        throws_if(ec, Errors::RescalerInternalSwsError);
        return;
    }

    dst.setQuality(src.quality());
    dst.setTimeBase(src.timeBase());
    dst.setPts(src.pts());
    dst.setStreamIndex(src.streamIndex());
    dst.setComplete(true);
}

VideoFrame VideoRescaler::rescale(const VideoFrame &src, error_code &ec)
{
    VideoFrame dst{m_dstPixelFormat, m_dstWidth, m_dstHeight};
    rescale(dst, src, ec);
    return dst;
}

bool VideoRescaler::isValid() const
{
    return !isNull();
}


} // ::av
