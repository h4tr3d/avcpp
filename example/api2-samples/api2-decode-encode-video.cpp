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

    //
    // INPUT
    //
    FormatContext ictx;
    ssize_t      videoStream = -1;
    CodecContext vdec;
    Stream2      vst;

    int count = 0;

    if (!ictx.openInput(uri)) {
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

        if (!vdec.open()) {
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

    if (!octx.openOutput(out)) {
        cerr << "Can't open output\n";
        return 1;
    }

    if (!encoder.open()) {
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
        Packet pkt;
        if (ictx.readPacket(pkt) < 0)
            break;

        if (pkt.streamIndex() != videoStream) {
            continue;
        }

        clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() << " / " << pkt.pts() * pkt.timeBase().getDouble() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

        // DECODING
        VideoFrame2 frame {vdec.pixelFormat(), vdec.width(), vdec.height(), 32};
        clog << "RefCounted: " << frame.isReferenced() << endl;

        auto st = vdec.decodeVideo(frame, pkt);

        count++;
        if (count > 200)
            break;

        if (st < 0) {
            cerr << "Decoding error: " << st << endl;
            return 1;
        } else if (st == 0) {
            cerr << "Empty frame\n";
            continue;
        }

        clog << "Frame: pts=" << frame.pts() << " / " << frame.pts() * frame.timeBase().getDouble() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

        // Change timebase
        frame.setTimeBase(encoder.timeBase());
        frame.setStreamIndex(0);
        frame.setPictureType();

        clog << "Frame: pts=" << frame.pts() << " / " << frame.pts() * frame.timeBase().getDouble() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

        // Encode
        Packet opkt;
        st = encoder.encodeVideo(opkt, frame);
        if (st < 0) {
            cerr << "Encoding error: " << st << endl;
            return 1;
        } else if (st == 0) {
            cerr << "Empty packet\n";
            continue;
        }

        // Only one output stream
        opkt.setStreamIndex(0);

        clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts() * opkt.timeBase().getDouble() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

        if (octx.writePacket(opkt) < 0) {
            cerr << "Error write packet\n";
            return 1;
        }

    }

    octx.writeTrailer();
}
