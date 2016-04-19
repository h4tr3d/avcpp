#include <signal.h>

#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <functional>
#include <map>

// getopt
#include <unistd.h>

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
#include "videorescaler.h"

using namespace std;
using namespace av;

void usage(int /*argc*/, char **argv, ostream &ost)
{
    ost << "Usage: " << argv[0] << " [arguments] input_stream output_stream\n"
           "  -f format     force input format\n"
           "  -o format     force output format\n"
           "  -d duration   duration for transmuxing (double, seconds)";
}

const static char opts[] = "f:o:d:h";

int main(int argc, char **argv)
{
    string uri;
    string out;
    string iformatName;
    string oformatName;
    Timestamp duration;

    while (true) {
        int opt = getopt(argc, argv, opts);
        if (opt < 0)
            break;

        switch (opt) {
            case 'f':
                iformatName = optarg;
                break;

            case 'o':
                oformatName = optarg;
                break;

            case 'd':
            {
                double val = stod(optarg);
                duration = Timestamp(val * AV_TIME_BASE, AV_TIME_BASE_Q);
                break;
            }

            case 'h':
                usage(argc, argv, cout);
                return 0;

            default:
                usage(argc, argv, cerr);
                return 1;
        }
    }


    if (optind > argc-2) {
        usage(argc, argv, cerr);
        return 1;
    }

    uri = argv[optind + 0];
    out = argv[optind + 1];

    av::init();
    av::set_logging_level(AV_LOG_DEBUG);

    error_code ec;

    //
    // INPUT
    //
    InputFormat   iformat(iformatName);
    FormatContext ictx;

    ictx.openInput(uri, iformat);
    ictx.findStreamInfo();

    //
    // OUTPUT
    //
    OutputFormat oformat(oformatName, out);
    FormatContext octx;

    map<size_t, size_t> streamMapping;

    octx.setFormat(oformat);

    // Copy streams
    for (size_t i = 0, ostIdx = 0; i < ictx.streamsCount(); ++i) {
        auto ist    = ictx.stream(i);
        auto icoder = GenericCodecContext(ist);

        // Source codec can be unsupprted by the target format. Transcoding required or simple skip.
        if (!octx.outputFormat().codecSupported(icoder.codec())) {
            clog << "Input stream " << i << " codec '" << icoder.codec().name() <<
                    "' does not supported by the output format '" << oformat.name() << "'\n";
            continue;
        }

        auto ost = octx.addStream(icoder.codec(), ec);

        // We can omit codec checking above and got error FormatCodecUnsupported (error code or exception)
        if (ec) {
            if (ec == Errors::FormatCodecUnsupported) {
                continue;
            } else {
                cerr << ec.category().name() << ": " << ec.message() << endl;
                return 1;
            }
        }

        ost.setTimeBase(ist.timeBase());

        auto ocoder = GenericCodecContext(ost);

        // copy codec settings
        ocoder.copyContextFrom(icoder);

        // setup mapping
        streamMapping[i] = ostIdx++;
    }

    octx.openOutput(out);
    octx.writeHeader();
    octx.dump();

    //
    // PROCESS
    //
    while (true) {
        auto pkt = ictx.readPacket();
        if (!pkt)
            break;

        auto it = streamMapping.find(pkt.streamIndex());
        string skip;
        if (it == streamMapping.end()) {
            skip = " [skip]";
        }

        clog << "pkt[" << pkt.streamIndex() << "]: tm=" << pkt.pts() << " / " << pkt.pts().seconds() << skip << '\n';

        if (it == streamMapping.end())
            continue;

        pkt.setStreamIndex(it->second);

        octx.writePacket(pkt);

        if (duration) {
            if (pkt.pts() > duration)
                break;
        }
    }

    // Flush output context
    octx.writePacket();
    //octx.flush();
    octx.writeTrailer();
}

