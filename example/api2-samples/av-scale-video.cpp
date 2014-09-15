#include <signal.h>

#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <functional>

#include "av.h"
#include "ffmpeg.h"
#include "codec.h"
#include "containerformat.h"
#include "container.h"
#include "packet.h"
#include "streamcoder.h"
#include "videorescaler.h"
#include "audiosamples.h"
#include "audioresampler.h"
#include "avutils.h"

// API2
#include "format.h"
#include "formatcontext.h"
#include "codec.h"
#include "codeccontext.h"
#include "videorescaler.h"

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
    encoder.setWidth(vdec.width() * 2);
    encoder.setHeight(vdec.height() * 2);
    encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());
    encoder.addFlags(octx.outputFormat().isFlags(AVFMT_GLOBALHEADER) ? CODEC_FLAG_GLOBAL_HEADER : 0);
    ost.setFrameRate(vst.frameRate());
    ost.setTimeBase(encoder.timeBase());

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
    // RESCALER
    //
    VideoRescaler rescaler; // Rescaler will be inited on demaind


    //
    // PROCESS
    //
    while (true) {

        // READING
        Packet pkt;
        if (ictx.readPacket(pkt) < 0)
            break;

        if (pkt.getStreamIndex() != videoStream) {
            continue;
        }

        clog << "Read packet: pts=" << pkt.getPts() << ", dts=" << pkt.getDts() << " / " << pkt.getPts() * pkt.getTimeBase().getDouble() << " / " << pkt.getTimeBase() << " / st: " << pkt.getStreamIndex() << endl;

        // DECODING
        VideoFrame2 inpFrame {vdec.pixelFormat(), vdec.width(), vdec.height()};
        clog << "RefCounted: " << inpFrame.isReferenced() << endl;

        auto st = vdec.decodeVideo(inpFrame, pkt);

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

        clog << "inpFrame: pts=" << inpFrame.pts() << " / " << inpFrame.pts() * inpFrame.timeBase().getDouble() << " / " << inpFrame.timeBase() << ", " << inpFrame.width() << "x" << inpFrame.height() << ", size=" << inpFrame.size() << ", ref=" << inpFrame.isReferenced() << ":" << inpFrame.refCount() << " / type: " << inpFrame.pictureType()  << endl;

        // Change timebase
        inpFrame.setTimeBase(encoder.timeBase());
        inpFrame.setStreamIndex(0);
        inpFrame.setPictureType();

        clog << "inpFrame: pts=" << inpFrame.pts() << " / " << inpFrame.pts() * inpFrame.timeBase().getDouble() << " / " << inpFrame.timeBase() << ", " << inpFrame.width() << "x" << inpFrame.height() << ", size=" << inpFrame.size() << ", ref=" << inpFrame.isReferenced() << ":" << inpFrame.refCount() << " / type: " << inpFrame.pictureType()  << endl;

        // SCALE
        VideoFrame2 outFrame {encoder.pixelFormat(), encoder.width(), encoder.height()};
        st = rescaler.rescale(outFrame, inpFrame);
        if (st < 0) {
            cerr << "Can't rescale frame\n";
            return 1;
        }

        clog << "outFrame: pts=" << outFrame.pts()
             << " / " << outFrame.pts() * outFrame.timeBase().getDouble()
             << " / " << outFrame.timeBase()
             << ", " << outFrame.width() << "x" << outFrame.height()
             << ", size=" << outFrame.size()
             << ", ref=" << outFrame.isReferenced() << ":" << outFrame.refCount()
             << " / type: " << outFrame.pictureType()  << endl;

        // ENCODE
        Packet opkt;
        st = encoder.encodeVideo(opkt, outFrame);
        if (st < 0) {
            cerr << "Encoding error: " << st << endl;
            return 1;
        } else if (st == 0) {
            cerr << "Empty packet\n";
            continue;
        }

        // Only one output stream
        opkt.setStreamIndex(0);

        clog << "Write packet: pts=" << opkt.getPts() << ", dts=" << opkt.getDts() << " / " << opkt.getPts() * opkt.getTimeBase().getDouble() << " / " << opkt.getTimeBase() << " / st: " << opkt.getStreamIndex() << endl;

        if (octx.writePacket(opkt) < 0) {
            cerr << "Error write packet\n";
            return 1;
        }
    }

    octx.writeTrailer();
    ictx.close();
}

