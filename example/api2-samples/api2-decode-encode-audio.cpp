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
    av::setFFmpegLoggingLevel(AV_LOG_TRACE);

    string uri (argv[1]);
    string out (argv[2]);

    ssize_t      audioStream = -1;
    AudioDecoderContext adec;
    Stream       ast;
    error_code   ec;

    int count = 0;

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

            //Codec codec = findDecodingCodec(adec.raw()->codec_id);

            //adec.setCodec(codec);
            //adec.setRefCountedFrames(true);

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
        Stream      ost    = octx.addStream(ocodec);
        AudioEncoderContext enc (ost);

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
#if 1
        enc.setSampleRate(48000);
        enc.setSampleFormat(sampleFmts[0]);
        // Layout
        //enc.setChannelLayout(adec.channelLayout());
        enc.setChannelLayout(AV_CH_LAYOUT_STEREO);
        //enc.setChannelLayout(AV_CH_LAYOUT_MONO);
        enc.setTimeBase(Rational(1, enc.sampleRate()));
        enc.setBitRate(adec.bitRate());
#else
        enc.setSampleRate(adec.sampleRate());
        enc.setSampleFormat(adec.sampleFormat());
        enc.setChannelLayout(adec.channelLayout());
        enc.setTimeBase(adec.timeBase());
        enc.setBitRate(adec.bitRate());
#endif

        octx.openOutput(out, ec);
        if (ec) {
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
            Packet pkt = ictx.readPacket(ec);
            if (ec)
            {
                clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
                break;
            }

            if (pkt.streamIndex() != audioStream) {
                continue;
            }

            clog << "Read packet: isNull=" << (bool)!pkt << ", " << pkt.pts() << "(nopts:" << pkt.pts().isNoPts() << ")" << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;
#if 0
            if (pkt.pts() == AV_NOPTS_VALUE && pkt.timeBase() == Rational())
            {
                clog << "Skip invalid timestamp packet: data=" << (void*)pkt.data()
                     << ", size=" << pkt.size()
                     << ", flags=" << pkt.flags() << " (corrupt:" << (pkt.flags() & AV_PKT_FLAG_CORRUPT) << ";key:" << (pkt.flags() & AV_PKT_FLAG_KEY) << ")"
                     << ", side_data=" << (void*)pkt.raw()->side_data
                     << ", side_data_count=" << pkt.raw()->side_data_elems
                     << endl;
                //continue;
            }
#endif

            auto samples = adec.decode(pkt, ec);
            count++;
            //if (count > 200)
            //    break;

            if (ec) {
                cerr << "Decode error: " << ec << ", " << ec.message() << endl;
                return 1;
            } else if (!samples) {
                cerr << "Empty samples set\n";

                //if (!pkt) // decoder flushed here
                //   break;
                //continue;
            }

            clog << "  Samples [in]: " << samples.samplesCount()
                 << ", ch: " << samples.channelsCount()
                 << ", freq: " << samples.sampleRate()
                 << ", name: " << samples.channelsLayoutString()
                 << ", pts: " << samples.pts().seconds()
                 << ", ref=" << samples.isReferenced() << ":" << samples.refCount()
                 << endl;

            // Empty samples set should not be pushed to the resampler, but it is valid case for the
            // end of reading: during samples empty, some cached data can be stored at the resampler
            // internal buffer, so we should consume it.
            if (samples)
            {
                resampler.push(samples, ec);
                if (ec) {
                    clog << "Resampler push error: " << ec << ", text: " << ec.message() << endl;
                    continue;
                }
            }

            // Pop resampler data
            bool getAll = !samples;
            while (true) {
                AudioSamples ouSamples(enc.sampleFormat(),
                                       enc.frameSize(),
                                       enc.channelLayout(),
                                       enc.sampleRate());

                // Resample:
                bool hasFrame = resampler.pop(ouSamples, getAll, ec);
                if (ec) {
                    clog << "Resampling status: " << ec << ", text: " << ec.message() << endl;
                    break;
                } else if (!hasFrame) {
                    break;
                } else
                    clog << "  Samples [ou]: " << ouSamples.samplesCount()
                         << ", ch: " << ouSamples.channelsCount()
                         << ", freq: " << ouSamples.sampleRate()
                         << ", name: " << ouSamples.channelsLayoutString()
                         << ", pts: " << ouSamples.pts().seconds()
                         << ", ref=" << ouSamples.isReferenced() << ":" << ouSamples.refCount()
                         << endl;

                // ENCODE
                ouSamples.setStreamIndex(0);
                ouSamples.setTimeBase(enc.timeBase());

                Packet opkt = enc.encode(ouSamples, ec);
                if (ec) {
                    cerr << "Encoding error: " << ec << ", " << ec.message() << endl;
                    return 1;
                } else if (!opkt) {
                    //cerr << "Empty packet\n";
                    continue;
                }

                opkt.setStreamIndex(0);

                clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

                octx.writePacket(opkt, ec);
                if (ec) {
                    cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                    return 1;
                }
            }

            // For the first packets samples can be empty: decoder caching
            if (!pkt && !samples)
                break;
        }

        //
        // Is resampler flushed?
        //
        cerr << "Delay: " << resampler.delay() << endl;

        //
        // Flush encoder queue
        //
        clog << "Flush encoder:\n";
        while (true) {
            AudioSamples null(nullptr);
            Packet        opkt = enc.encode(null, ec);
            if (ec || !opkt)
                break;

            opkt.setStreamIndex(0);

            clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

            octx.writePacket(opkt, ec);
            if (ec) {
                cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                return 1;
            }

        }

        octx.flush();
        octx.writeTrailer();
    }
}



