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

    ssize_t      audioStream = -1;
    CodecContext adec;
    Stream      ast;
    error_code   ec;

    string format;

    if (argc > 2) {
        format = argv[2];
    }

    int count = 0;

    {
        InputFormat ifmt;
        if (!format.empty()) {
            ifmt.setFormat(format);
        }

        FormatContext ictx;

        ictx.openInput(uri, ifmt, ec);
        if (ec) {
            cerr << "Can't open input\n";
            return 1;
        }

        ictx.findStreamInfo();

        for (size_t i = 0; i < ictx.streamsCount(); ++i) {
            auto st = ictx.stream(i);
            if (st.isAudio()) {
                audioStream = i;
                ast = st;
                break;
            }
        }

        cerr << audioStream << endl;

        if (ast.isNull()) {
            cerr << "Audio stream not found\n";
            return 1;
        }

        if (ast.isValid()) {
            adec = CodecContext(ast);


            Codec codec = findDecodingCodec(adec.raw()->codec_id);

            adec.setCodec(codec);
            adec.setRefCountedFrames(true);

            adec.open(Codec(), ec);
            if (ec) {
                cerr << "Can't open codec\n";
                return 1;
            }
        }

        clog << "NO_PTS: " << AV_NOPTS_VALUE << endl;
        clog << "Duration: " << ictx.duration() << " / " << ictx.duration().seconds()
             << ", Start Time: " << ictx.startTime() << " / " << ictx.startTime().seconds()
             << endl;

        // Substract start time from the Packet PTS and DTS values: PTS starts from the zero
        ictx.substractStartTime(true);

        while (true) {
            Packet pkt = ictx.readPacket(ec);
            if (ec)
            {
                clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
                break;
            }

            // EOF
            if (!pkt)
            {
                break;
            }

            if (pkt.streamIndex() != audioStream) {
                continue;
            }

            clog << "Read packet: " << pkt.pts() << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

            AudioSamples samples = adec.decodeAudio(pkt, ec);

            if (ec) {
                cerr << "Error: " << ec << endl;
                return 1;
            } else if (!samples) {
                //cerr << "Empty frame\n";
                //continue;
            }

            clog << "  Samples: " << samples.samplesCount()
                 << ", ch: " << samples.channelsCount()
                 << ", freq: " << samples.sampleRate()
                 << ", name: " << samples.channelsLayoutString()
                 << ", ref=" << samples.isReferenced() << ":" << samples.refCount()
                 << ", ts: " << samples.pts()
                 << ", tm: " << samples.pts().seconds()
                 << endl;

            //return 0;
            count++;
            if (count > 100)
                break;
        }

        // NOTE: stream decodec must be closed/destroyed before
        //ictx.close();
        //vdec.close();
    }
}

