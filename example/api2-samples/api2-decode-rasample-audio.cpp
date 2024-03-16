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
    AudioDecoderContext adec;
    Stream      ast;
    error_code   ec;

    {

        //
        // INPUT
        //
        FormatContext ictx;

        ictx.openInput(uri, ec);
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
            adec = AudioDecoderContext(ast);

            Codec codec = findDecodingCodec(adec.raw()->codec_id);

            adec.setCodec(codec);
            adec.setRefCountedFrames(true);

            adec.open(Codec(), ec);
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
        bool eof = false;
        while (!eof) {
            Packet pkt = ictx.readPacket(ec);
            if (ec)
            {
                clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
                break;
            }

            // EOF
            if (!pkt) {
                // Flush decoder
                eof = true;
            } else if (pkt.streamIndex() != audioStream) {
                continue;
            }

            clog << "Read packet: isNull: " << pkt.isNull() << ", " << pkt.pts() << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;

            // --htrd: start

            adec.decode(pkt, [&](auto samples) -> av::dec_enc_handler_result_t {
                    assert(samples);

                    clog << "  Samples [in]: " << samples.samplesCount()
                         << ", ch: " << samples.channelsCount()
                         << ", freq: " << samples.sampleRate()
                         << ", name: " << samples.channelsLayoutString()
                         << ", pts: " << samples.pts().seconds()
                         << ", ref=" << samples.isReferenced() << ":" << samples.refCount()
                         << endl;

                    error_code ec;
                    resampler.push(samples, ec);
                    if (ec) {
                        clog << "Resampler push error: " << ec << ", text: " << ec.message() << endl;
                        return av::unexpected(std::move(ec));
                    }

                    auto ouSamples = AudioSamples::null();
                    while ((ouSamples = resampler.pop(samples.samplesCount()/2, ec))) {
                        clog << "  Samples [ou]: " << ouSamples.samplesCount()
                             << ", ch: " << ouSamples.channelsCount()
                             << ", freq: " << ouSamples.sampleRate()
                             << ", name: " << ouSamples.channelsLayoutString()
                             << ", pts: " << ouSamples.pts().seconds()
                             << ", ref=" << ouSamples.isReferenced() << ":" << ouSamples.refCount()
                             << endl;
                    }

                    if (ec) {
                        clog << "Resampling status: " << ec << ", text: " << ec.message() << endl;
                        return av::unexpected(std::move(ec));
                    }

                    return true;
                }, ec);

            if (ec) {
                cerr << "Error: " << ec << endl;
                return 1;
            }

            // --htrd: end
        }

        // Flush samples
        {
            clog << "Flush resampler:\n";
            while (true) {
                AudioSamples ouSamples(adec.sampleFormat(), 576, adec.channelLayout(), 48000);
                auto hasFrame = resampler.pop(ouSamples, false, ec);
                if (!ec && !hasFrame)
                    hasFrame = resampler.pop(ouSamples, true, ec);

                if (ec) {
                    clog << "Resampling status: " << ec << ", text: " << ec.message() << endl;
                    break;
                } else if (hasFrame == false)
                    break;
                else
                    clog << "  Samples [ou]: " << ouSamples.samplesCount()
                         << ", ch: " << ouSamples.channelsCount()
                         << ", freq: " << ouSamples.sampleRate()
                         << ", name: " << ouSamples.channelsLayoutString()
                         << ", pts: " << ouSamples.pts().seconds()
                         << ", ref=" << ouSamples.isReferenced() << ":" << ouSamples.refCount()
                         << endl;
            }
        }

        // NOTE: stream decodec must be closed/destroyed before
        //ictx.close();
        //vdec.close();
    }
}


