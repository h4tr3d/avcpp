#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <functional>

#include "avcpp/avconfig.h"

#include "avcpp/av.h"
#include "avcpp/ffmpeg.h"
#include "avcpp/codec.h"
#include "avcpp/packet.h"
#include "avcpp/videorescaler.h"
#include "avcpp/audioresampler.h"
#include "avcpp/avutils.h"

// API2
#include "avcpp/format.h"
#include "avcpp/formatcontext.h"
#include "avcpp/codec.h"
#include "avcpp/codeccontext.h"

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

#if AVCPP_HAS_PKT_SIDE_DATA
            for (auto side : pkt.sideData()) {
                clog << "  found packet side data: " << side.name() << endl;
                if (side.type() == AV_PKT_DATA_MATROSKA_BLOCKADDITIONAL) {
                    assert(side.data().size() >= sizeof(uint64_t));

                    // BE
                    uint64_t idType{};
                    memcpy(&idType, side.data().data(), sizeof(idType));
                    //idType = std::endian::native == std::endian::little ? std::byteswap(idType) : idType;
                    idType = av_be2ne64(idType);

                    auto payload = side.data().subspan(sizeof(uint64_t));
                    clog << "    matroska block additions id type = " << idType << endl;
                    if (!payload.empty() && idType == 100 /* vendor specific */) {
                        clog << "      block additions data: " << std::string_view{(const char*)payload.data(), payload.size()} << endl;
                    }
                }
            }
#endif

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
