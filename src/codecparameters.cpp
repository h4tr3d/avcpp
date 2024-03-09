#include "codecparameters.h"
#include "codeccontext.h"

namespace av {

CodecParametersView::CodecParametersView(AVCodecParameters *codecpar)
    : FFWrapperPtr<AVCodecParameters>(codecpar)
{
}

bool CodecParametersView::isValid() const
{
    return !isNull();
}

void CodecParametersView::copyFrom(CodecParametersView src, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (!src.m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    auto const ret = avcodec_parameters_copy(m_raw, src.m_raw);
    if (ret) {
        throws_if(ec, ret, ffmpeg_category());
    }
}

void CodecParametersView::copyTo(CodecParametersView dst, OptionalErrorCode ec) const
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (!dst.m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    auto const ret = avcodec_parameters_copy(dst.m_raw, m_raw);
    if (ret) {
        throws_if(ec, ret, ffmpeg_category());
    }
}

void CodecParametersView::copyFrom(const CodecContext2 &src, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (!src.raw()) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    auto const ret = avcodec_parameters_from_context(m_raw, src.raw());
    if (ret) {
        throws_if(ec, ret, ffmpeg_category());
    }
}

void CodecParametersView::copyTo(CodecContext2 &dst, OptionalErrorCode ec) const
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    if (!dst.raw()) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    auto const ret = avcodec_parameters_to_context(dst.raw(), m_raw);
    if (ret) {
        throws_if(ec, ret, ffmpeg_category());
    }
}

int CodecParametersView::getAudioFrameDuration(int frame_bytes) const
{
    if (!m_raw)
        return 0;
    return av_get_audio_frame_duration2(const_cast<AVCodecParameters*>(m_raw), frame_bytes);
}



CodecParameters::CodecParameters()
    : CodecParametersView(avcodec_parameters_alloc())
{
}

CodecParameters::~CodecParameters()
{
    avcodec_parameters_free(&m_raw);
}

} // namespace av
