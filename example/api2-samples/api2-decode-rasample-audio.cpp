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
    Stream2      ast;
    error_code   ec;

    int count = 0;

    {

        //
        // INPUT
        //
        FormatContext ictx;

        if (!ictx.openInput(uri)) {
            cerr << "Can't open input\n";
            return 1;
        }

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

            adec.open(ec);
            if (ec) {
                cerr << "Can't open codec\n";
                return 1;
            }
        }

        //
        // RESAMPLER
        //
        AudioResampler resampler(adec.channelLayout(), 48000,             adec.sampleFormat(),
                                 adec.channelLayout(), adec.sampleRate(), adec.sampleFormat());

        //
        // PROCESS
        //
        while (true) {
            Packet pkt;
            if (ictx.readPacket(pkt) < 0)
                break;

            if (pkt.streamIndex() != audioStream) {
                continue;
            }

            clog << "Read packet: " << pkt.pts() << " / " << pkt.pts() * pkt.timeBase().getDouble() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

            AudioSamples2 samples = adec.decodeAudio(ec, pkt);

            count++;
            if (count > 100)
                break;

            if (ec) {
                cerr << "Error: " << ec << endl;
                return 1;
            } else if (!samples) {
                //cerr << "Empty frame\n";
                //continue;
            }

            clog << "  Samples [in]: " << samples.samplesCount()
                 << ", ch: " << samples.channelsCount()
                 << ", freq: " << samples.sampleRate()
                 << ", name: " << samples.channelsLayoutString()
                 << ", pts: " << (samples.timeBase().getDouble() * samples.pts())
                 << ", ref=" << samples.isReferenced() << ":" << samples.refCount()
                 << endl;

            auto st = resampler.push(samples);
            if (st < 0) {
                clog << "Resampler push error: " << st << ", text: " << av::error2string(st) << endl;
                continue;
            }

            // Pop resampler data
            while (true) {
                AudioSamples2 ouSamples(adec.sampleFormat(), samples.samplesCount()/2, adec.channelLayout(), 48000);

                // Resample:
                //st = resampler.resample(ouSamples, samples);
                st = resampler.pop(ouSamples);
                if (st < 0) {
                    clog << "Resampling status: " << st << ", text: " << av::error2string(st) << endl;
                    break;
                } else if (st == 0) {
                    break;
                } else
                    clog << "  Samples [ou]: " << ouSamples.samplesCount()
                         << ", ch: " << ouSamples.channelsCount()
                         << ", freq: " << ouSamples.sampleRate()
                         << ", name: " << ouSamples.channelsLayoutString()
                         << ", pts: " << (ouSamples.timeBase().getDouble() * ouSamples.pts())
                         << ", ref=" << ouSamples.isReferenced() << ":" << ouSamples.refCount()
                         << endl;
            }
        }

        // Flush samples
        {
            clog << "Flush resampler:\n";
            while (true) {
                AudioSamples2 ouSamples(adec.sampleFormat(), 576, adec.channelLayout(), 48000);
                auto st = resampler.pop(ouSamples);
                if (st == 0)
                    st = resampler.pop(ouSamples, true);

                if (st < 0) {
                    clog << "Resampling status: " << st << ", text: " << av::error2string(st) << endl;
                    break;
                } else if (st == 0)
                    break;
                else
                    clog << "  Samples [ou]: " << ouSamples.samplesCount()
                         << ", ch: " << ouSamples.channelsCount()
                         << ", freq: " << ouSamples.sampleRate()
                         << ", name: " << ouSamples.channelsLayoutString()
                         << ", pts: " << (ouSamples.timeBase().getDouble() * ouSamples.pts())
                         << ", ref=" << ouSamples.isReferenced() << ":" << ouSamples.refCount()
                         << endl;
            }
        }

        // NOTE: stream decodec must be closed/destroyed before
        //ictx.close();
        //vdec.close();
    }
}


