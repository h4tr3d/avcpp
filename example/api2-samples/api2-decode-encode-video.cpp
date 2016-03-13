#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <functional>

#include "av.h"
#include "ffmpeg.h"
#include "codec.h"
#include "packet.h"
#include "videorescaler.h"
#include "audioresampler.h"
#include "avutils.h"

// API2
#include "format.h"
#include "formatcontext.h"
#include "codec.h"
#include "codeccontext.h"

using namespace std;
using namespace av;

int main(int argc, char **argv)
{
    if (argc < 3)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri {argv[1]};
    string out {argv[2]};

    error_code ec;

    //
    // INPUT
    //
    FormatContext ictx;
    ssize_t      videoStream = -1;
    CodecContext vdec;
    Stream2      vst;

    int count = 0;

    ictx.openInput(uri, ec);
    if (ec) {
        cerr << "Can't open input\n";
        return 1;
    }

    for (size_t i = 0; i < ictx.streamsCount(); ++i) {
        auto st = ictx.stream(i);
        if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
            videoStream = i;
            vst = st;
            break;
        }
    }

    if (vst.isNull()) {
        cerr << "Video stream not found\n";
        return 1;
    }

    if (vst.isValid()) {
        vdec = CodecContext(vst);
        vdec.setRefCountedFrames(true);

        vdec.open(Codec(), ec);
        if (ec) {
            cerr << "Can't open codec\n";
            return 1;
        }
    }


    //
    // OUTPUT
    //
    OutputFormat  ofrmt;
    FormatContext octx;

    ofrmt.setFormat("flv", out);
    octx.setFormat(ofrmt);

    Codec        ocodec  = findEncodingCodec(ofrmt);
    Stream2      ost     = octx.addStream(ocodec);
    CodecContext encoder {ost};

    // Settings
    encoder.setWidth(vdec.width());
    encoder.setHeight(vdec.height());
    encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());
    ost.setFrameRate(vst.frameRate());

    octx.openOutput(out, ec);
    if (ec) {
        cerr << "Can't open output\n";
        return 1;
    }

    encoder.open(Codec(), ec);
    if (ec) {
        cerr << "Can't opent encodec\n";
        return 1;
    }

    octx.dump();
    octx.writeHeader();
    octx.flush();


    //
    // PROCESS
    //
    while (true) {

        // READING
        Packet pkt = ictx.readPacket(ec);
        if (ec) {
            clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
            break;
        }

        // EOF
        if (!pkt) {
            break;
        }

        if (pkt.streamIndex() != videoStream) {
            continue;
        }

        clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

        // DECODING
        //VideoFrame2 frame {vdec.pixelFormat(), vdec.width(), vdec.height(), 32};
        auto frame = vdec.decodeVideo(pkt, ec);

        count++;
        if (count > 200)
            break;

        if (ec) {
            cerr << "Decoding error: " << ec << endl;
            return 1;
        } else if (!frame) {
            cerr << "Empty frame\n";
            continue;
        }

        clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

        // Change timebase
        frame.setTimeBase(encoder.timeBase());
        frame.setStreamIndex(0);
        frame.setPictureType();

        clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

        // Encode
        auto opkt = encoder.encodeVideo(frame, ec);
        if (ec) {
            cerr << "Encoding error: " << ec << endl;
            return 1;
        } else if (!opkt) {
            cerr << "Empty packet\n";
            continue;
        }

        // Only one output stream
        opkt.setStreamIndex(0);

        clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

        octx.writePacket(opkt, ec);
        if (ec) {
            cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
            return 1;
        }

    }

    octx.writeTrailer();
}
