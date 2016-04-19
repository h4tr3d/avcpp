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
    if (argc < 2)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri {argv[1]};

    ssize_t      videoStream = -1;
    VideoDecoderContext vdec;
    Stream      vst;
    error_code   ec;

    int count = 0;

    {

        FormatContext ictx;

        ictx.openInput(uri, ec);
        if (ec) {
            cerr << "Can't open input\n";
            return 1;
        }

        cerr << "Streams: " << ictx.streamsCount() << endl;

        ictx.findStreamInfo(ec);
        if (ec) {
            cerr << "Can't find streams: " << ec << ", " << ec.message() << endl;
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

        cerr << videoStream << endl;

        if (vst.isNull()) {
            cerr << "Video stream not found\n";
            return 1;
        }

        if (vst.isValid()) {
            vdec = VideoDecoderContext(vst);


            Codec codec = findDecodingCodec(vdec.raw()->codec_id);

            vdec.setCodec(codec);
            vdec.setRefCountedFrames(true);

            vdec.open({{"threads", "1"}}, Codec(), ec);
            //vdec.open(ec);
            if (ec) {
                cerr << "Can't open codec\n";
                return 1;
            }
        }


        while (Packet pkt = ictx.readPacket(ec)) {
            if (ec) {
                clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
                return 1;
            }

            if (pkt.streamIndex() != videoStream) {
                continue;
            }

            auto ts = pkt.ts();
            clog << "Read packet: " << ts << " / " << ts.seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

            VideoFrame frame = vdec.decode(pkt, ec);

            count++;
            //if (count > 100)
            //    break;

            if (ec) {
                cerr << "Error: " << ec << ", " << ec.message() << endl;
                return 1;
            } else if (!frame) {
                cerr << "Empty frame\n";
                //continue;
            }

            ts = frame.pts();

            clog << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << frame.timeBase() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << endl;

        }

        clog << "Flush frames;\n";
        while (true) {
            VideoFrame frame = vdec.decode(Packet(), ec);
            if (ec) {
                cerr << "Error: " << ec << ", " << ec.message() << endl;
                return 1;
            }
            if (!frame)
                break;
            auto ts = frame.pts();
            clog << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << frame.timeBase() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << endl;
        }

        // NOTE: stream decodec must be closed/destroyed before
        //ictx.close();
        //vdec.close();
    }
}
