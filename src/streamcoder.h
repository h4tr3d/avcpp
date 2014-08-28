#ifndef STREAMCODER_H
#define STREAMCODER_H

#include <utility>
#include <memory>
#include <functional>

#include "ffmpeg.h"
#include "stream.h"
#include "codec.h"
#include "rational.h"
#include "frame.h"
#include "videoframe.h"
#include "avutils.h"

namespace av
{

// Forward decl
class Frame;
typedef std::shared_ptr<Frame> FramePtr;
typedef std::weak_ptr<Frame> FrameWPtr;


class StreamCoder;
typedef std::shared_ptr<StreamCoder> StreamCoderPtr;
typedef std::weak_ptr<StreamCoder> StreamCoderWPtr;


class StreamCoder
{
public:
    typedef std::function<void (const PacketPtr&)> EncodedPacketHandler;

public:

    StreamCoder();
    ~StreamCoder();
    explicit StreamCoder(const StreamPtr& m_stream);


    // Common
    void setCodec(const CodecPtr &m_codec);

    bool open();
    bool close();
    bool isOpened() { return m_openedFlag; }

    /**
     * Copy codec context from codec context associated with given stream or other codec context.
     * This functionality useful for remuxing without deconding/encoding. In this case you need not
     * open codecs, only copy context.
     *
     * @param other  stream or codec context
     * @return true if context copied, false otherwise
     */
    /// @{
    bool copyContextFrom(const StreamPtr &other);
    bool copyContextFrom(const StreamCoderPtr &other);
    /// @}

    Rational getTimeBase();
    void setTimeBase(const Rational &value);

    StreamPtr getStream() const;

    AVMediaType getCodecType() const;

    // Video
    int         getWidth() const;
    int         getHeight() const;
    int         getCodedWidth() const;
    int         getCodedHeight() const;
    PixelFormat getPixelFormat() const;
    Rational    getFrameRate();
    int32_t     getBitRate() const;
    std::pair<int, int> getBitRateRange() const;
    int32_t     getGlobalQuality();
    int32_t     getGopSize();
    int         getBitRateTolerance() const;
    int         getStrict() const;
    int         getMaxBFrames() const;
    int         getFrameSize() const;

    void        setWidth(int w); // Note, it also sets coded_width
    void        setHeight(int h); // Note, it also sets coded_height
    void        setCodedWidth(int w);
    void        setCodedHeight(int h);
    void        setPixelFormat(PixelFormat pixelFormat);
    void        setFrameRate(const Rational &frameRate);
    void        setBitRate(int32_t bitRate);
    void        setBitRateRange(const std::pair<int, int> &bitRateRange);
    void        setGlobalQuality(int32_t quality);
    void        setGopSize(int32_t size);
    void        setBitRateTolerance(int bitRateTolerance);
    void        setStrict(int strict);
    void        setMaxBFrames(int maxBFrames);

    // Audio
    int         getSampleRate() const;
    int         getChannels() const;
    AVSampleFormat getSampleFormat() const;
    uint64_t getChannelLayout() const;
    int         getAudioFrameSize() const;
    int         getDefaultAudioFrameSize() const;


    void        setSampleRate(int sampleRate);
    void        setChannels(int channels);
    void        setSampleFormat(AVSampleFormat sampleFormat);
    void        setChannelLayout(uint64_t layout);
    void        setAudioFrameSize(int frameSize);
    void        setDefaultAudioFrameSize(int frameSize);

    // Flags
    void        setFlags(int32_t flags);
    void        addFlags(int32_t flags);
    void        clearFlags(int32_t flags);
    int32_t     getFlags();

    AVCodecContext *getAVCodecContext() { return m_context; }

    // Video
    ssize_t decodeVideo(const FramePtr  &outFrame,  const PacketPtr &inPacket, size_t offset = 0);
    ssize_t encodeVideo(const VideoFramePtr &inFrame,
                        const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

    // Audio
    ssize_t decodeAudio(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset = 0);
    ssize_t encodeAudio(const FramePtr  &inFrame,
                        const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

    bool    isValidForEncode();

private:
    void init();

    ssize_t decodeCommon(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset,
                         int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *));
    ssize_t encodeCommon(const FramePtr  &inFrame,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*),
                         const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

private:
    Direction       m_direction;
    Rational        m_fakePtsTimeBase;
    int64_t         m_fakeNextPts;
    int64_t         m_fakeCurrPts;

    int             m_defaultAudioFrameSize;

    StreamPtr       m_stream;
    CodecPtr        m_codec;
    AVCodecContext *m_context;
    bool            m_openedFlag;
};

} // ::av

#endif // STREAMCODER_H
