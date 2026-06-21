#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <set>

#include "avcpp/audioresampler.h"
#include "avcpp/av.h"
#include "avcpp/avutils.h"
#include "avcpp/codec.h"
#include "avcpp/ffmpeg.h"
#include "avcpp/packet.h"
#include "avcpp/videorescaler.h"

// API2
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"
#include "avcpp/format.h"
#include "avcpp/formatcontext.h"
#include "avcpp/filters/buffersink.h"
#include "avcpp/filters/buffersrc.h"
#include "avcpp/filters/filtergraph.h"


using namespace std;
using namespace av;

// Example that uses a complex filter to produce a good
// animated GIF from a video

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri{argv[1]};
    string out{argv[2]};

    long videoStream = -1;
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

        //
        // OUTPUT
        //
        OutputFormat ofrmt;
        FormatContext octx;

        ofrmt.setFormat(string(), out);
        octx.setFormat(ofrmt);

        Codec ocodec = findEncodingCodec(ofrmt);
        cout << "Codec: " << ocodec.name() << endl;
        VideoEncoderContext encoder {ocodec};

        // Settings
        encoder.setWidth(320);
        encoder.setHeight(200);
        encoder.setPixelFormat(AV_PIX_FMT_PAL8);
        encoder.setTimeBase(Rational {1, 1000});
        encoder.setBitRate(vdec.bitRate());

        encoder.open(Codec(), ec);
        if (ec) {
            cerr << "Can't opent encodec\n";
            return 1;
        }

        Stream ost = octx.addStream(encoder);
        ost.setFrameRate(vst.frameRate());

        octx.openOutput(out, ec);
        if (ec) {
            cerr << "Can't open output\n";
            return 1;
        }

        octx.dump();
        octx.writeHeader();
        octx.flush();

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
        string filterChain;
        if (argc > 3) {
            filterChain = argv[3];
        } else {
            filterChain = "scale=320x200:-1:flags=lanczos,split [s0][s1]; "
                "[s0] palettegen [p]; [s1][p] paletteuse[f]; ";
        }
        cout << "Filter: " << filterChain << endl;
        filter_graph.parse(filterChain, src_ctx, sink_ctx, ec);
        if (ec) {
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

            frame.setTimeBase(encoder.timeBase());
            frame.setStreamIndex(0);
            frame.setPictureType();

            // Push into the filter
            cout << "Pushing frame into filter: " << frame.width() << "x" << frame.height() << ", size=" << frame.size()
                 << ", ts=" << frame.pts() << ", tb: " << frame.timeBase() << endl;
            buffer_src.addVideoFrame(frame, AV_BUFFERSRC_FLAG_KEEP_REF, ec);
            if (ec)
            {
                clog << "Filter push error: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            // Pull until there are no more incoming frames
            VideoFrame filtered;
            while (buffer_sink.getVideoFrame(filtered, ec))
            {
                filtered.setTimeBase(encoder.timeBase());
                auto ts = filtered.pts();

                clog << "  filtered: " << filtered.width() << "x" << filtered.height() << ", size=" << filtered.size()
                     << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << filtered.timeBase()
                     << ", ref=" << filtered.isReferenced() << ":" << filtered.refCount() << endl;

                // Encode
                Packet opkt = filtered ? encoder.encode(filtered, ec) : encoder.encode(ec);
                if (ec) {
                    cerr << "Encoding error: " << ec << endl;
                    return 1;
                }

                if (opkt) {
                    // Only one output stream
                    opkt.setStreamIndex(0);

                    clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

                    octx.writePacket(opkt, ec);
                    if (ec) {
                        cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                        return 1;
                    }
                }
            }
            if (ec && ec.value() != -EAGAIN)
            {
                clog << "Filter pull error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
            cout << "No more frames available in filter" << endl;
        }

        clog << "Flush frames\n";
        // Push a null frame at then end for filters that
        // need the full input before generating any output
        // (animated GIF with a palette is one of them)
        auto null_frame = VideoFrame::null();
        buffer_src.addVideoFrame(null_frame, 0, ec);
        // Pull until there are no more incoming frames
        VideoFrame filtered;
        while (buffer_sink.getVideoFrame(filtered, ec))
        {
            filtered.setTimeBase(encoder.timeBase());
            auto ts = filtered.pts();
            clog << "  filtered: " << filtered.width() << "x" << filtered.height() << ", size=" << filtered.size()
                    << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << filtered.timeBase()
                    << ", ref=" << filtered.isReferenced() << ":" << filtered.refCount() << endl;

            // Encode
            Packet opkt = filtered ? encoder.encode(filtered, ec) : encoder.encode(ec);
            if (ec) {
                cerr << "Encoding error: " << ec << endl;
                return 1;
            }

            if (opkt) {
                // Only one output stream
                opkt.setStreamIndex(0);

                clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

                octx.writePacket(opkt, ec);
                if (ec) {
                    cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                    return 1;
                }
            }
        }
        cout << "Filter flushed, frames: " << count << endl;
        octx.writeTrailer();
    }
}
