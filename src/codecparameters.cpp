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

AVMediaType CodecParametersView::codecType() const
{
    return m_raw ? m_raw->codec_type : AVMEDIA_TYPE_UNKNOWN;
}

void CodecParametersView::codecType(AVMediaType codec_type)
{
    if (m_raw)
        m_raw->codec_type = codec_type;
}


AVMediaType CodecParametersView::mediaType() const
{
    return codecType();
}

void CodecParametersView::mediaType(AVMediaType media_type)
{
    codecType(media_type);
}


AVCodecID CodecParametersView::codecId() const
{
    return m_raw ? m_raw->codec_id : AV_CODEC_ID_NONE;
}

void CodecParametersView::codecId(AVCodecID codec_id)
{
    if (m_raw)
        m_raw->codec_id = codec_id;
}

Codec CodecParametersView::encodingCodec() const
{
    return m_raw ? findEncodingCodec(m_raw->codec_id) : Codec();
}

Codec CodecParametersView::decodingCodec() const
{
    return m_raw ? findDecodingCodec(m_raw->codec_id) : Codec();
}

uint32_t CodecParametersView::codecTag() const
{
    return m_raw ? m_raw->codec_tag : 0u;
}

void CodecParametersView::codecTag(uint32_t codec_tag)
{
    if (m_raw)
        m_raw->codec_tag = codec_tag;
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
