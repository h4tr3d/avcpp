#include "avlog.h"

#include "videorescaler.h"

using namespace std;

namespace av
{

VideoRescaler::VideoRescaler()
{}

VideoRescaler::VideoRescaler(int dstWidth, int dstHeight, AVPixelFormat dstPixelFormat, int srcWidth, int srcHeight, AVPixelFormat srcPixelFormat) throw(invalid_argument)
    : m_dstWidth(dstWidth),
      m_dstHeight(dstHeight),
      m_dstPixelFormat(dstPixelFormat),
      m_srcWidth(srcWidth),
      m_srcHeight(srcHeight),
      m_srcPixelFormat(srcPixelFormat)
{
    if (dstWidth <= 0 || dstHeight <= 0 || dstPixelFormat == AV_PIX_FMT_NONE) {
        ptr_log(AV_LOG_FATAL,
                "Invalid destination parameters: w=%d, h=%d, pix_fmt=%s\n",
                dstWidth,
                dstHeight,
                av_get_pix_fmt_name(dstPixelFormat));
        throw invalid_argument("Invalid destination parameters");
    }

    getContext();
}

VideoRescaler::VideoRescaler(int dstWidth, int dstHeight, AVPixelFormat dstPixelFormat)
    : VideoRescaler(dstWidth, dstWidth, dstPixelFormat, 0, 0, AV_PIX_FMT_NONE)
{
}

VideoRescaler::VideoRescaler(const VideoRescaler &other)
    : VideoRescaler(other.m_dstWidth, other.m_dstHeight, other.m_dstPixelFormat,
                    other.m_srcWidth, other.m_srcHeight, other.m_srcPixelFormat)
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
    swap(m_raw,            other.m_raw);
}

void VideoRescaler::getContext()
{
    if (m_srcWidth <= 0 || m_srcHeight <= 0 || m_srcPixelFormat == AV_PIX_FMT_NONE ||
        m_srcWidth <= 0 || m_dstHeight <= 0 || m_dstPixelFormat == AV_PIX_FMT_NONE)
    {
        if (m_raw)
            sws_freeContext(m_raw);
        m_raw = nullptr;
        return;
    }

    int32_t flags = 0;

    if (m_srcWidth < m_dstWidth)
        flags = SWS_BICUBIC;
    else
        flags = SWS_AREA;

    m_raw = sws_getCachedContext(m_raw,
                                 m_srcWidth, m_srcHeight, m_srcPixelFormat,
                                 m_dstWidth, m_dstHeight, m_dstPixelFormat,
                                 flags,
                                 nullptr, nullptr, nullptr);
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
        swap(rhs);
        VideoRescaler().swap(rhs);
    }
    return *this;
}


int32_t VideoRescaler::rescale(const VideoFramePtr &dstFrame, const VideoFramePtr &srcFrame)
{
    m_srcWidth       = srcFrame->getWidth();
    m_srcHeight      = srcFrame->getHeight();
    m_srcPixelFormat = srcFrame->getPixelFormat();

    m_dstWidth       = dstFrame->getWidth();
    m_dstHeight      = dstFrame->getHeight();
    m_dstPixelFormat = dstFrame->getPixelFormat();

    // Context can be allocated on demand
    auto ptr = m_raw;
    getContext();

    if (!m_raw) {
        ptr_log(AV_LOG_ERROR, "Can't allocate swsContext for given input/output parameters\n");
        return -1;
    }

    if (ptr != m_raw) {
        ptr_log(AV_LOG_DEBUG, "SwsScale Context was changed\n");
    }

    dstFrame->setPts(srcFrame->getPts());

    AVFrame *inFrame = srcFrame->getAVFrame();
    AVFrame *outFrame = dstFrame->getAVFrame();

    const uint8_t* srcFrameData[AV_NUM_DATA_POINTERS] = {
        inFrame->data[0],
        inFrame->data[1],
        inFrame->data[2],
        inFrame->data[3]
    #if AV_NUM_DATA_POINTERS == 8
        ,
        inFrame->data[4],
        inFrame->data[5],
        inFrame->data[6],
        inFrame->data[7]
    #endif
    };

    int stat = -1;
    stat = sws_scale(m_raw, srcFrameData, inFrame->linesize, 0, m_srcHeight,
                     outFrame->data, outFrame->linesize);

    dstFrame->setQuality(srcFrame->getQuality());
    dstFrame->setTimeBase(srcFrame->getTimeBase());
    dstFrame->setPts(srcFrame->getPts());
    dstFrame->setFakePts(srcFrame->getFakePts());
    dstFrame->setStreamIndex(srcFrame->getStreamIndex());
    dstFrame->setComplete(stat >= 0);

    return stat;
}

int32_t VideoRescaler::rescale(VideoFrame2 &dst, VideoFrame2 &src)
{
    m_srcWidth       = src.width();
    m_srcHeight      = src.height();
    m_srcPixelFormat = src.pixelFormat();

    m_dstWidth       = dst.width();
    m_dstHeight      = dst.height();
    m_dstPixelFormat = dst.pixelFormat();

    // Context can be allocated on demand
    getContext();

    if (!m_raw) {
        ptr_log(AV_LOG_ERROR, "Can't allocate swsContext for given input/output parameters\n");
        return -1;
    }

    dst.setPts(src.pts());

    AVFrame *inpFrame = src.raw();
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

    int stat = -1;
    stat = sws_scale(m_raw, srcFrameData, inpFrame->linesize, 0, m_srcHeight,
                     outFrame->data, outFrame->linesize);

    dst.setQuality(src.quality());
    dst.setTimeBase(src.timeBase());
    dst.setPts(src.pts());
    //dst.setFakePts(src.fakePts());
    dst.setStreamIndex(src.streamIndex());
    dst.setComplete(stat >= 0);

    return stat;
}

bool VideoRescaler::isValid() const
{
    return !isNull();
}


} // ::av
