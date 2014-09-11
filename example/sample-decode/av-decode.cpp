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

using namespace std;
using namespace av;

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri {argv[1]};

    ssize_t      videoStream = -1;
    CodecContext vdec;
    Stream2      vst;

    {

        FormatContext ictx;

        if (!ictx.openInput(uri)) {
            cerr << "Can't open input\n";
            return 1;
        }

        for (size_t i = 0; i < ictx.streamsCount(); ++i) {
            auto st = ictx.stream(i);
            if (st.mediaType() == AVMEDIA_TYPE_VIDEO) {
                videoStream = i;
                vst = std::move(st);
                break;
            }
        }

        cerr << videoStream << endl;

        if (vst.isNull()) {
            cerr << "Video stream not found\n";
            return 1;
        }

        if (vst.isValid()) {
            vdec = CodecContext(vst);


            Codec codec = findDecodingCodec(vdec.raw()->codec_id);

            vdec.setCodec(codec);
            vdec.setRefCountedFrames(true);



            if (!vdec.open()) {
                cerr << "Can't open codec\n";
                return 1;
            }
        }

        Packet pkt;
        while (ictx.readPacket(pkt) >= 0) {
            clog << "Read packet: " << pkt.getPts() << " / " << pkt.getPts() * pkt.getTimeBase().getDouble() << " / " << pkt.getTimeBase() << " / st: " << pkt.getStreamIndex() << endl;

            if (pkt.getStreamIndex() == videoStream) {
                VideoFrame2 frame;

                auto st = vdec.decodeVideo(frame, pkt);
                if (st < 0) {
                    cerr << "Error: " << st << endl;
                    return 1;
                } else if (st == 0) {
                    cerr << "Empty frame\n";
                    continue;
                }

                clog << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << endl;
            }
        }

        // NOTE: stream decodec must be closed/destroyed before
        ictx.close();
        vdec.close();
    }
}
