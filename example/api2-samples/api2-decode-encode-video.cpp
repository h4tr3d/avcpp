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
    VideoDecoderContext vdec;
    Stream      vst;

    int count = 0;

    ictx.openInput(uri, ec);
    if (ec) {
        cerr << "Can't open input\n";
        return 1;
    }

    ictx.findStreamInfo();

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
        vdec = VideoDecoderContext(vst);
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

    ofrmt.setFormat(string(), out);
    octx.setFormat(ofrmt);

    Codec               ocodec  = findEncodingCodec(ofrmt);
    VideoEncoderContext encoder {ocodec};

    // Settings
    encoder.setWidth(vdec.width());
    encoder.setHeight(vdec.height());
    if (vdec.pixelFormat() > -1)
        encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());

    encoder.open(ec);
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


    //
    // PROCESS
    //
    bool eof = false;
    while (!eof) {

        // READING
        Packet pkt = ictx.readPacket(ec);
        if (ec) {
            clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
            break;
        }

        // !EOF
        if (pkt) {
            if (pkt.streamIndex() != videoStream) {
                continue;
            }
            clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;
        } else {
            eof = true; // Null packet on the EOF, flush decoder and exit
        }

        // --htrd: start

        auto encode_proc = [&](auto opkt) -> av::dec_enc_handler_result_t {
            assert(opkt); // assume that packet valid here

            opkt.setStreamIndex(0);

            clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

            error_code ec;
            octx.writePacket(opkt, ec);
            if (ec) {
                cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                return av::unexpected(std::move(ec));
            }

            return true;
        };

        vdec.decode(pkt, [&](auto frame) -> av::dec_enc_handler_result_t {
                count++;

                assert(frame); // assume it valid here always, fix if fail

                clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

                // Change timebase
                frame.setTimeBase(encoder.timeBase());
                frame.setStreamIndex(0);
                frame.setPictureType();

                clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType()  << endl;

                error_code ec;
                encoder.encode(frame, encode_proc, ec);
                if (ec) {
                    cerr << "Encoding error: " << ec << endl;
                    return av::unexpected(std::move(ec));
                }

                return true;
            }, ec);

        if (ec) {
            cerr << "Decoding error: " << ec << endl;
            return 1;
        }

        if (eof) {
            clog << "Delay: " << encoder.raw()->delay << '\n';
            clog << "Flush encoder:\n";
            encoder.encodeFlush(encode_proc, ec);
            if (ec) {
                cerr << "Encoder flushing error: " << ec << endl;
                return 1;
            }
        }
    }

    octx.writeTrailer();
}
