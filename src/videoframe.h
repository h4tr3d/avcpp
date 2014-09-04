#ifndef AV_VIDEOFRAME_H
#define AV_VIDEOFRAME_H

#include "frame.h"

namespace av {

class VideoFrame : public Frame
{
public:
    VideoFrame();
    VideoFrame(PixelFormat pixelFormat, int width, int height);
    VideoFrame(const vector<uint8_t> &data, PixelFormat pixelFormat, int width, int height);
    VideoFrame(const uint8_t *data, size_t dataSize, AVPixelFormat pixelFormat, int width, int height);
    VideoFrame(const AVFrame    *frame);
    VideoFrame(const VideoFrame &frame);
    virtual ~VideoFrame();

    PixelFormat            getPixelFormat() const;
    void                   setPixelFormat(PixelFormat pixFmt);
    int                    getWidth() const;
    int                    getHeight() const;
    bool                   isKeyFrame() const;
    void                   setKeyFrame(bool isKey);

    int                    getQuality() const;
    void                   setQuality(int quality);

    AVPictureType          getPictureType() const;
    void                   setPictureType(AVPictureType type);

    const AVPicture&       getPicture() const;

    // virtual
    virtual int getSize() const override;
    virtual bool isValid() const override;
    virtual std::shared_ptr<Frame> clone() override;

protected:
    void init(PixelFormat pixelFormat, int width, int height);
    virtual void setupDataPointers(const AVFrame *m_frame) override;

};

typedef std::shared_ptr<VideoFrame> VideoFramePtr;
typedef std::weak_ptr<VideoFrame>   VideoFrameWPtr;

} // namespace av

#endif // AV_VIDEOFRAME_H
