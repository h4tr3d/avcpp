#ifndef AV_CODECCONTEXT_H
#define AV_CODECCONTEXT_H

#include "ffmpeg.h"
#include "stream.h"
#include "frame.h"
#include "codec.h"
#include "dictionary.h"
#include "avutils.h"
#include "averror.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

namespace av {

class CodecContext : public FFWrapperPtr<AVCodecContext>, public noncopyable
{
private:
    void swap(CodecContext &other);

public:
    CodecContext();
    // Stream decoding/encoding
    explicit CodecContext(const Stream2 &st, const Codec& codec = Codec());
    // Stream independ decoding/encoding
    explicit CodecContext(const Codec &codec);
    ~CodecContext();

    // Disable copy/Activate move
    CodecContext(CodecContext &&other);
    CodecContext& operator=(CodecContext &&rhs);

    // Common
    void setCodec(const Codec &codec, std::error_code &ec = throws());
    void setCodec(const Codec &codec, bool resetDefaults, std::error_code &ec = throws());

    void open(std::error_code &ec = throws());
    void open(const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &options, std::error_code &ec = throws());
    void open(Dictionary &&options, std::error_code &ec = throws());
    void open(Dictionary &options, const Codec &codec, std::error_code &ec = throws());
    void open(Dictionary &&options, const Codec &codec, std::error_code &ec = throws());

    void close(std::error_code &ec = throws());

    bool isOpened() const noexcept { return m_isOpened; }
    bool isValid() const noexcept;

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     * @return true if context copied, false otherwise
     */
    /// @{
    void copyContextFrom(const CodecContext &other, std::error_code &ec = throws());

    /// @}

    Rational timeBase() const;
    void     setTimeBase(const Rational &value);

    const Stream2& stream() const;

    Codec       codec() const;
    AVMediaType codecType() const;

    void setOption(const std::string &key, const std::string &val, std::error_code &ec = throws());
    void setOption(const std::string &key, const std::string &val, int flags, std::error_code &ec = throws());

    bool isAudio() const;
    bool isVideo() const;

    // Common
    int frameSize() const;
    int frameNumber() const;

    // Note, set ref counted to enable for multithreaded processing
    bool isRefCountedFrames() const;
    void setRefCountedFrames(bool refcounted) const;

    // Video
    int                 width() const;
    int                 height() const;
    int                 codedWidth() const;
    int                 codedHeight() const;
    AVPixelFormat       pixelFormat() const;
    int32_t             bitRate() const;
    std::pair<int, int> bitRateRange() const;
    int32_t             globalQuality();
    int32_t             gopSize();
    int                 bitRateTolerance() const;
    int                 strict() const;
    int                 maxBFrames() const;

    void setWidth(int w); // Note, it also sets coded_width
    void setHeight(int h); // Note, it also sets coded_height
    void setCodedWidth(int w);
    void setCodedHeight(int h);
    void setPixelFormat(AVPixelFormat pixelFormat);
    void setBitRate(int32_t bitRate);
    void setBitRateRange(const std::pair<int, int> &bitRateRange);
    void setGlobalQuality(int32_t quality);
    void setGopSize(int32_t size);
    void setBitRateTolerance(int bitRateTolerance);
    void setStrict(int strict);
    void setMaxBFrames(int maxBFrames);

    // Audio
    int            sampleRate() const;
    int            channels() const;
    AVSampleFormat sampleFormat() const;
    uint64_t       channelLayout() const;

    void        setSampleRate(int sampleRate);
    void        setChannels(int channels);
    void        setSampleFormat(AVSampleFormat sampleFormat);
    void        setChannelLayout(uint64_t layout);

    // Flags
    /// Access to CODEC_FLAG_* flags
    /// @{
    void        setFlags(int flags);
    void        addFlags(int flags);
    void        clearFlags(int flags);
    int         flags();
    bool        isFlags(int flags);
    /// @}

    // Flags 2
    /// Access to CODEC_FLAG2_* flags
    /// @{
    void        setFlags2(int flags2);
    void        addFlags2(int flags2);
    void        clearFlags2(int flags2);
    int         flags2();
    bool        isFlags2(int flags2);
    /// @}

    //
    // Video
    //

    /**
     * @brief decodeVideo  - decode video packet
     *
     * @param packet   packet to decode
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @param autoAllocateFrame  it true - output will be allocated at the ffmpeg internal, otherwise
     *                           it will be allocated before decode proc call.
     * @return encoded video frame, if error: exception thrown or error code returns, in both cases
     *         output undefined.
     */
    VideoFrame2 decodeVideo(const Packet    &packet,
                            std::error_code &ec = throws(),
                            bool             autoAllocateFrame = true);

    /**
     * @brief decodeVideo - decode video packet with additional parameters
     *
     * @param[in] packet         packet to decode
     * @param[in] offset         data offset in packet
     * @param[out] decodedBytes  amount of decoded bytes
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @param autoAllocateFrame  it true - output will be allocated at the ffmpeg internal, otherwise
     *                           it will be allocated before decode proc call.
     * @return encoded video frame, if error: exception thrown or error code returns, in both cases
     *         output undefined.
     */
    VideoFrame2 decodeVideo(const Packet &packet,
                            size_t offset,
                            size_t &decodedBytes,
                            std::error_code &ec = throws(),
                            bool    autoAllocateFrame = true);

    /**
     * @brief encodeVideo - Flush encoder
     *
     * Stop flushing when returns empty packets
     *
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return
     */
    Packet encodeVideo(std::error_code &ec = throws());

    /**
     * @brief encodeVideo - encode video frame
     *
     * @note Some encoders need some amount of frames before beginning encoding, so it is normal,
     *       that for some amount of frames returns empty packets.
     *
     * @param inFrame  frame to encode
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return
     */
    Packet encodeVideo(const VideoFrame2 &inFrame, std::error_code &ec = throws());

    //
    // Audio
    //
    AudioSamples2 decodeAudio(const Packet &inPacket, std::error_code &ec = throws());
    AudioSamples2 decodeAudio(const Packet &inPacket, size_t offset, std::error_code &ec = throws());

    Packet encodeAudio(std::error_code &ec = throws());
    Packet encodeAudio(const AudioSamples2 &inSamples, std::error_code &ec = throws());

    bool    isValidForEncode();

private:
    void open(const Codec &codec, AVDictionary **options, std::error_code &ec);

    VideoFrame2 decodeVideo(std::error_code &ec,
                            const Packet &packet,
                            size_t offset,
                            size_t *decodedBytes,
                            bool    autoAllocateFrame);

    std::pair<ssize_t, const std::error_category*>
    decodeCommon(AVFrame *outFrame, const Packet &inPacket, size_t offset, int &frameFinished,
                 int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *));

    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket, const AVFrame *inFrame, int &gotPacket,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*));

    template<typename T>
    std::pair<ssize_t, const std::error_category*>
    decodeCommon(T &outFrame,
                 const Packet &inPacket,
                 size_t offset,
                 int &frameFinished,
                 int (*decodeProc)(AVCodecContext *, AVFrame *, int *, const AVPacket *));

    template<typename T>
    std::pair<ssize_t, const std::error_category*>
    encodeCommon(Packet &outPacket,
                 const T &inFrame,
                 int &gotPacket,
                 int (*encodeProc)(AVCodecContext *, AVPacket *, const AVFrame *, int *));

private:
    Direction       m_direction = Direction::INVALID;
    Rational        m_fakePtsTimeBase;
    //Timestamp       m_fakeNextPts;
    //Timestamp       m_fakeCurrPts;

    Stream2         m_stream;
    bool            m_isOpened = false;
};

} // namespace av

#endif // AV_CODECCONTEXT_H
