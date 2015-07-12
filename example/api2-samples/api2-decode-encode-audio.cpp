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

    string uri (argv[1]);
    string out (argv[2]);

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
        // OUTPUT
        //
        OutputFormat  ofmt;
        FormatContext octx;

        ofmt = av::guessOutputFormat(out, out);
        clog << "Output format: " << ofmt.name() << " / " << ofmt.longName() << '\n';
        octx.setFormat(ofmt);

        Codec        ocodec = av::findEncodingCodec(ofmt, false);
        Stream2      ost    = octx.addStream(ocodec);
        CodecContext enc (ost);

        clog << ocodec.name() << " / " << ocodec.longName() << ", audio: " << (ocodec.type()==AVMEDIA_TYPE_AUDIO) << '\n';

        auto sampleFmts  = ocodec.supportedSampleFormats();
        auto sampleRates = ocodec.supportedSamplerates();
        auto layouts     = ocodec.supportedChannelLayouts();

        clog << "Supported sample formats:\n";
        for (const auto &fmt : sampleFmts) {
            clog << "  " << av_get_sample_fmt_name(fmt) << '\n';
        }

        clog << "Supported sample rates:\n";
        for (const auto &rate : sampleRates) {
            clog << "  " << rate << '\n';
        }

        clog << "Supported sample layouts:\n";
        for (const auto &lay : layouts) {
            char buf[128] = {0};
            av_get_channel_layout_string(buf,
                                         sizeof(buf),
                                         av_get_channel_layout_nb_channels(lay),
                                         lay);

            clog << "  " << buf << '\n';
        }

        //return 0;


        // Settings
        enc.setSampleRate(48000);
        enc.setSampleFormat(sampleFmts[0]);
        // Layout
        //enc.setChannelLayout(adec.channelLayout());
        enc.setChannelLayout(AV_CH_LAYOUT_STEREO);
        //enc.setChannelLayout(AV_CH_LAYOUT_MONO);
        enc.setTimeBase(Rational(1, enc.sampleRate()));
        enc.setBitRate(adec.bitRate());

        if (!octx.openOutput(out)) {
            cerr << "Can't open output\n";
            return 1;
        }

        enc.open(ec);
        if (ec) {
            cerr << "Can't open encoder\n";
            return 1;
        }

        clog << "Encoder frame size: " << enc.frameSize() << '\n';

        octx.dump();
        octx.writeHeader();
        octx.flush();

        //
        // RESAMPLER
        //
        AudioResampler resampler(enc.channelLayout(),  enc.sampleRate(),  enc.sampleFormat(),
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
            //if (count > 200)
            //    break;

            if (ec) {
                cerr << "Error: " << ec << endl;
                return 1;
            } else if (!samples) {
                //cerr << "Empty frame\n";
                continue;
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
                AudioSamples2 ouSamples(enc.sampleFormat(),
                                        enc.frameSize(),
                                        enc.channelLayout(),
                                        enc.sampleRate());

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

                // ENCODE
                ouSamples.setStreamIndex(0);
                ouSamples.setTimeBase(enc.timeBase());

                Packet opkt = enc.encodeAudio(ec, ouSamples);
                if (!ec) {
                    cerr << "Encoding error: " << ec << endl;
                    return 1;
                } else if (!opkt) {
                    //cerr << "Empty packet\n";
                    continue;
                }

                opkt.setStreamIndex(0);

                clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts() * opkt.timeBase().getDouble() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

                if (octx.writePacket(opkt) < 0) {
                    cerr << "Error write packet\n";
                    return 1;
                }

            }
        }


        //
        // Flush encoder queue
        //
        clog << "Flush encoder:\n";
        while (true) {
            AudioSamples2 null(nullptr);
            Packet        opkt = enc.encodeAudio(ec, null);
            if (ec || !opkt)
                break;

            opkt.setStreamIndex(0);

            clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts() * opkt.timeBase().getDouble() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

            if (octx.writePacket(opkt) < 0) {
                cerr << "Error write packet\n";
                return 1;
            }

        }

        octx.writeTrailer();
    }
}



