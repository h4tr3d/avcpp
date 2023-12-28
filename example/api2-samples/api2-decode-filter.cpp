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
    if (argc < 2)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri{argv[1]};

    ssize_t videoStream = -1;
    VideoDecoderContext vdec;
    Stream vst;
    error_code ec;

    int count = 0;

    {

        FormatContext ictx;

        ictx.openInput(uri, ec);
        if (ec)
        {
            cerr << "Can't open input\n";
            return 1;
        }

        cerr << "Streams: " << ictx.streamsCount() << endl;

        ictx.findStreamInfo(ec);
        if (ec)
        {
            cerr << "Can't find streams: " << ec << ", " << ec.message() << endl;
            return 1;
        }

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

        cerr << videoStream << endl;

        if (vst.isNull())
        {
            cerr << "Video stream not found\n";
            return 1;
        }

        if (vst.isValid())
        {
            vdec = VideoDecoderContext(vst);

            Codec codec = findDecodingCodec(vdec.raw()->codec_id);

            vdec.setCodec(codec);
            vdec.setRefCountedFrames(true);

            vdec.open({{"threads", "1"}}, Codec(), ec);
            // vdec.open(ec);
            if (ec)
            {
                cerr << "Can't open codec\n";
                return 1;
            }
        }

        // Setup filter
        Filter filter_buffer_src{"buffer"};
        Filter filter_buffer_sink{"buffersink"};
        FilterGraph filter_graph;

        // Input and ouput contexts
        std::stringstream src_args;
        src_args << "video_size=" << vdec.width() << "x" << vdec.height() << ":pix_fmt=" << vdec.pixelFormat()
                 << ":time_base=" << vdec.timeBase() << ":pixel_aspect=" << vdec.sampleAspectRatio();
        clog << "Input arguments: " << src_args.str() << endl;
        FilterContext src_ctx = filter_graph.createFilter(filter_buffer_src, "in", src_args.str(), ec);
        if (ec)
        {
            clog << "Error creating filter source context: " << ec << ", " << ec.message() << endl;
            return 1;
        }
        FilterContext sink_ctx = filter_graph.createFilter(filter_buffer_sink, "out", "", ec);
        if (ec)
        {
            clog << "Error creating filter sink context: " << ec << ", " << ec.message() << endl;
            return 1;
        }

        // Setup the filter chain
        filter_graph.parse("scale=320x200", src_ctx, sink_ctx, ec);
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
        BufferSrcFilterContext buffer_src{src_ctx};
        BufferSinkFilterContext buffer_sink{sink_ctx};

        while (Packet pkt = ictx.readPacket(ec))
        {
            if (ec)
            {
                clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            if (pkt.streamIndex() != videoStream)
            {
                continue;
            }

            VideoFrame frame = vdec.decode(pkt, ec);

            count++;
            // if (count > 100)
            //     break;

            if (ec)
            {
                cerr << "Error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
            else if (!frame)
            {
                cerr << "Empty frame\n";
                continue;
            }

            // Push in the filter
            buffer_src.addVideoFrame(frame, AV_BUFFERSRC_FLAG_KEEP_REF, ec);
            if (ec)
            {
                clog << "Filter push error: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            // Pull until there are more incoming frames
            VideoFrame filtered;
            while (buffer_sink.getVideoFrame(filtered, ec))
            {
                filtered.setTimeBase(frame.timeBase());
                auto ts = filtered.pts();

                clog << "  filtered: " << filtered.width() << "x" << filtered.height() << ", size=" << filtered.size()
                     << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << filtered.timeBase()
                     << ", ref=" << filtered.isReferenced() << ":" << filtered.refCount() << endl;
            }
            if (ec && ec.value() != -EAGAIN)
            {
                clog << "Filter pull error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
        }

        clog << "Flush frames;\n";
        while (true)
        {
            VideoFrame frame = vdec.decode(Packet(), ec);
            if (ec)
            {
                cerr << "Error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
            if (!frame)
                break;

            // Push in the filter
            buffer_src.addVideoFrame(frame, ec);
            if (ec)
            {
                clog << "Filter push error: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            // Pull until there are more incoming frames
            VideoFrame filtered;
            while (buffer_sink.getVideoFrame(filtered, ec))
            {
                filtered.setTimeBase(frame.timeBase());
                auto ts = filtered.pts();
                clog << "  filtered: " << filtered.width() << "x" << filtered.height() << ", size=" << filtered.size()
                     << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << filtered.timeBase()
                     << ", ref=" << filtered.isReferenced() << ":" << filtered.refCount() << endl;
            }
            if (ec && ec.value() != -EAGAIN)
            {
                clog << "Filter pull error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
        }
    }
}
