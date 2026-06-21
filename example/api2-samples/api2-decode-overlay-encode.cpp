#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>

#include "audioresampler.h"
#include "av.h"
#include "avutils.h"
#include "codec.h"
#include "ffmpeg.h"
#include "packet.h"
#include "videorescaler.h"

// API2
#include "codec.h"
#include "codeccontext.h"
#include "filters/buffersink.h"
#include "filters/buffersrc.h"
#include "filters/filtergraph.h"
#include "format.h"
#include "formatcontext.h"

using namespace std;
using namespace av;

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri{argv[1]};
    string image{argv[2]};
    string out{argv[3]};

    error_code ec;

    //
    // INPUT
    //
    FormatContext ictx;
    long videoStream = -1;
    VideoDecoderContext vdec;
    Stream vst;

    int count = 0;

    ictx.openInput(uri, ec);
    if (ec)
    {
        cerr << "Can't open input\n";
        return 1;
    }

    ictx.findStreamInfo();

    for (size_t i = 0; i < ictx.streamsCount(); ++i)
    {
        auto st = ictx.stream(i);
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO)
        {
            videoStream = i;
            vst = st;
            break;
        }
    }

    if (vst.isNull())
    {
        cerr << "Video stream not found\n";
        return 1;
    }

    if (vst.isValid())
    {
        vdec = VideoDecoderContext(vst);
        vdec.setRefCountedFrames(true);

        vdec.open(Codec(), ec);
        if (ec)
        {
            cerr << "Can't open codec\n";
            return 1;
        }
    }

    //
    // STATIC IMAGE (more or less identical to INPUT)
    //
    FormatContext static_image_ctx;
    long imageStream = -1;
    VideoDecoderContext static_image_dec;
    Stream image_st;

    static_image_ctx.openInput(image, ec);
    if (ec)
    {
        cerr << "Can't open static image\n";
        return 1;
    }

    static_image_ctx.findStreamInfo();

    for (size_t i = 0; i < static_image_ctx.streamsCount(); ++i)
    {
        auto st = static_image_ctx.stream(i);
        // An image is still a video stream in the ffmpeg world
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO)
        {
            imageStream = i;
            image_st = st;
            break;
        }
    }

    if (image_st.isNull())
    {
        cerr << "Image stream not found\n";
        return 1;
    }

    if (image_st.isValid())
    {
        static_image_dec = VideoDecoderContext(image_st);
        static_image_dec.setRefCountedFrames(true);

        static_image_dec.open(Codec(), ec);
        if (ec)
        {
            cerr << "Can't open codec\n";
            return 1;
        }
    }

    //
    // OUTPUT
    //
    OutputFormat ofrmt;
    FormatContext octx;

    ofrmt.setFormat(string(), out);
    octx.setFormat(ofrmt);

    Codec ocodec = findEncodingCodec(ofrmt);
    VideoEncoderContext encoder{ocodec};

    // Settings
    encoder.setWidth(vdec.width());
    encoder.setHeight(vdec.height());
    if (vdec.pixelFormat() > -1)
        encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());

    encoder.open(Codec(), ec);
    if (ec)
    {
        cerr << "Can't opent encodec\n";
        return 1;
    }

    Stream ost = octx.addStream(encoder);
    ost.setFrameRate(vst.frameRate());

    octx.openOutput(out, ec);
    if (ec)
    {
        cerr << "Can't open output\n";
        return 1;
    }

    octx.dump();
    octx.writeHeader();
    octx.flush();

    // FILTER (OVERLAY)
    FilterGraph filter_graph;

    // Construct the filter descriptor
    stringstream filter_desc;

    // * for the video input
    filter_desc << "buffer@in=video_size=" << vdec.width() << "x" << vdec.height() << ":pix_fmt=" << vdec.pixelFormat()
                << ":time_base=" << encoder.timeBase() << ":pixel_aspect=" << vdec.sampleAspectRatio() << " [in];  ";
    // * for the fixed image input
    filter_desc << "buffer@image=video_size=" << static_image_dec.width() << "x" << static_image_dec.height()
                << ":pix_fmt=" << static_image_dec.pixelFormat() << ":time_base=" << encoder.timeBase()
                << ":pixel_aspect=" << static_image_dec.sampleAspectRatio() << " [image_unscaled];  ";
    filter_desc << "[image_unscaled] scale=" << vdec.width() / 4 << "x" << vdec.height() / 4 << " [image];  ";
    // * for the output
    filter_desc << "[in][image] overlay=x=50:y=50:repeatlast=1 [out];  ";
    filter_desc << "[out] buffersink@out";

    // Setup the filter chain
    clog << "Filter descriptor: " << filter_desc.str() << endl;
    filter_graph.parse(filter_desc.str(), ec);
    if (ec)
    {
        clog << "Error parsing filter chain: " << ec << ", " << ec.message() << endl;
        return 1;
    }
    filter_graph.config(ec);
    if (ec)
    {
        clog << "Error configuring filter chain: " << ec << ", " << ec.message() << endl;
        return 1;
    }

    // Setup the entry/exit points
    BufferSrcFilterContext buffer_video_src{filter_graph.filter("buffer@in")};
    BufferSrcFilterContext buffer_image_src{filter_graph.filter("buffer@image")};
    BufferSinkFilterContext buffer_sink{filter_graph.filter("buffersink@out")};

    // Load the static image into buffer_image_src, it won't change
    {
        auto image_pkt = static_image_ctx.readPacket(ec);
        if (ec)
        {
            clog << "Image reading error: " << ec << ", " << ec.message() << endl;
            return 1;
        }
        auto image_frame = static_image_dec.decode(image_pkt, ec);
        if (ec)
        {
            clog << "Image decoding error: " << ec << ", " << ec.message() << endl;
            return 1;
        }
        image_frame.setTimeBase(encoder.timeBase());
        image_frame.setStreamIndex(0);
        image_frame.setPictureType();
        clog << "Frame for overlaying: pts=" << image_frame.pts() << " / " << image_frame.pts().seconds() << " / "
             << image_frame.timeBase() << ", " << image_frame.width() << "x" << image_frame.height()
             << ", size=" << image_frame.size() << ", ref=" << image_frame.isReferenced() << ":"
             << image_frame.refCount() << " / type: " << image_frame.pictureType() << endl;
        buffer_image_src.addVideoFrame(image_frame, ec);
        if (ec)
        {
            clog << "Image processing error: " << ec << ", " << ec.message() << endl;
            return 1;
        }
        // Very important! Signal that there are no more frames after this one!
        // ffmpeg expects a full video, this will allow the repeatlast=1 in
        // overlay to kick in
        buffer_image_src.writeVideoFrame(VideoFrame::null());
    }

    //
    // PROCESS
    //
    while (true)
    {

        // READING
        Packet pkt = ictx.readPacket(ec);
        if (ec)
        {
            clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
            break;
        }

        bool flushDecoder = false;
        // !EOF
        if (pkt)
        {
            if (pkt.streamIndex() != videoStream)
            {
                continue;
            }

            clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() << " / " << pkt.pts().seconds() << " / "
                 << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;
        }
        else
        {
            flushDecoder = true;
        }

        do
        {
            // DECODING
            auto frame = vdec.decode(pkt, ec);

            count++;
            // if (count > 200)
            //     break;

            bool flushEncoder = false;

            if (ec)
            {
                cerr << "Decoding error: " << ec << endl;
                return 1;
            }
            else if (!frame)
            {
                if (flushDecoder)
                {
                    flushEncoder = true;
                }
            }

            if (frame)
            {
                clog << "Frame from decoder: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / "
                     << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size()
                     << ", ref=" << frame.isReferenced() << ":" << frame.refCount()
                     << " / type: " << frame.pictureType() << endl;

                frame.setTimeBase(encoder.timeBase());
                frame.setStreamIndex(0);
                frame.setPictureType();

                // Push into the filter
                buffer_video_src.addVideoFrame(frame, ec);
                if (ec)
                {
                    clog << "Filter push error: " << ec << ", " << ec.message() << endl;
                    return 1;
                }
            }

            // Pull from the filter until there are no more incoming frames
            VideoFrame filtered;
            while (buffer_sink.getVideoFrame(filtered, ec))
            {
                // Change timebase
                filtered.setTimeBase(encoder.timeBase());
                filtered.setStreamIndex(0);
                filtered.setPictureType();
                clog << "Frame from filter: pts=" << filtered.pts() << " / " << filtered.pts().seconds() << " / "
                     << filtered.timeBase() << ", " << filtered.width() << "x" << filtered.height()
                     << ", size=" << filtered.size() << ", ref=" << filtered.isReferenced() << ":"
                     << filtered.refCount() << " / type: " << filtered.pictureType() << endl;

                if (filtered || flushEncoder)
                {
                    do
                    {
                        // Encode
                        Packet opkt = filtered ? encoder.encode(filtered, ec) : encoder.encode(ec);
                        if (ec)
                        {
                            cerr << "Encoding error: " << ec << endl;
                            return 1;
                        }
                        else if (!opkt)
                        {
                            // cerr << "Empty packet\n";
                            // continue;
                            break;
                        }

                        // Only one output stream
                        opkt.setStreamIndex(0);

                        clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / "
                             << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex()
                             << endl;

                        octx.writePacket(opkt, ec);
                        if (ec)
                        {
                            cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                            return 1;
                        }
                    } while (flushEncoder);
                }
            }
            if (ec && ec.value() != -EAGAIN)
            {
                cerr << "Error reading frame from the filter chain: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            if (flushEncoder)
                break;

        } while (flushDecoder);

        if (flushDecoder)
            break;
    }

    clog << "done" << endl;
    octx.writeTrailer();
}
