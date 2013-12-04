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

    void foo();

    StreamCoder();
    ~StreamCoder();
    explicit StreamCoder(const StreamPtr& stream);


    // Common
    void setCodec(const CodecPtr &codec);

    bool open();
    bool close();
    bool isOpened() { return isOpenedFlag; }

    Rational getTimeBase();
    void setTimeBase(const Rational &value);

    StreamPtr getStream() const;

    AVMediaType getCodecType() const;

    // Video
    int         getWidth() const;
    int         getHeight() const;
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

    void        setWidth(int w);
    void        setHeight(int h);
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

    AVCodecContext *getAVCodecContext() { return context; }

    // Video
    ssize_t decodeVideo(const FramePtr  &outFrame,  const PacketPtr &inPacket, size_t offset = 0);
    ssize_t encodeVideo(const PacketPtr &outPacket, const VideoFramePtr &inFrame,
                        const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

    // Audio
    ssize_t decodeAudio(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset = 0);
    ssize_t encodeAudio(const PacketPtr &outPacket, const FramePtr  &inFrame,
                        const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

    bool    isValidForEncode();

private:
    void init();

    ssize_t decodeCommon(const FramePtr &outFrame, const PacketPtr &inPacket, size_t offset,
                         int (*decodeProc)(AVCodecContext*, AVFrame*,int *, const AVPacket *));
    ssize_t encodeCommon(const PacketPtr &outPacket, const FramePtr  &inFrame,
                         int (*encodeProc)(AVCodecContext*, AVPacket*,const AVFrame*, int*),
                         const EncodedPacketHandler &onPacketHandler = EncodedPacketHandler());

private:
    Direction       direction;
    Rational        fakePtsTimeBase;
    int64_t         fakeNextPts;
    int64_t         fakeCurrPts;

    int             defaultAudioFrameSize;

    StreamPtr       stream;
    CodecPtr        codec;
    AVCodecContext *context;
    bool            isOpenedFlag;
};

} // ::av

#endif // STREAMCODER_H
