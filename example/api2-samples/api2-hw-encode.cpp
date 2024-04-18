/**
 * Smoke sample for the HW encoding with current AvCpp API state.
 *
 * Thanks to https://github.com/mojie126, for more-less complete sample.
 *
 * Known issues: error on the Matroska header writing.
 *
 */


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


// check CUDA accessibility
bool isCudaAvailable()
{
    AVHWDeviceType type = av_hwdevice_find_type_by_name("cuda");
    return type != AV_HWDEVICE_TYPE_NONE;
}

// check QSV accessibility
bool isQsvAvailable()
{
    AVHWDeviceType type = av_hwdevice_find_type_by_name("qsv");
    return type != AV_HWDEVICE_TYPE_NONE;
}

// Get ecnoder
const av::Codec getEncoder(bool use_cuda, bool use_qsv)
{
    if (use_cuda) {
        auto enc = av::findEncodingCodec("h264_nvenc");
        if (!enc.isNull())
            return enc;
    }
    if (use_qsv) {
        auto enc = av::findEncodingCodec("h264_qsv");
        if (!enc.isNull())
            return enc;
    }
    return av::findEncodingCodec("libx264"); // Fall-back to SW one
}

// HW context
AVHWDeviceType getHwDeviceType(bool use_cuda, bool use_qsv)
{
    if (use_cuda)
        return av_hwdevice_find_type_by_name("cuda");
    if (use_qsv)
        return av_hwdevice_find_type_by_name("qsv");
    return AV_HWDEVICE_TYPE_NONE;
}

int main(int argc, char **argv) {
    if (argc < 3)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri{argv[1]};
    string out{argv[2]};

    error_code ec;

    //
    // INPUT
    //
    FormatContext ictx;
    ssize_t videoStream = -1;
    // VideoDecoderContext vdec;
    Stream vst;

    // int count = 0;

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
    bool use_cuda = isCudaAvailable();
    bool use_qsv = isQsvAvailable();
    // Get encoder and hardware context
    auto _encoder = getEncoder(use_cuda, use_qsv);
    AVHWDeviceType hw_type = getHwDeviceType(use_cuda, use_qsv); // --htrd: never use
    clog << "test hw: " << av_hwdevice_get_type_name(hw_type) << endl;
    //av::VideoDecoderContext vdec(vst, _encoder);
    av::VideoDecoderContext vdec;

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
    OutputFormat ofrmt;
    FormatContext octx;

    ofrmt.setFormat(string(), out);
    octx.setFormat(ofrmt);

    //Codec ocodec = findEncodingCodec(ofrmt);
    VideoEncoderContext encoder{_encoder};

    // Settings
    encoder.setWidth(vdec.width());
    encoder.setHeight(vdec.height());
    if (vdec.pixelFormat() > -1)
        encoder.setPixelFormat(vdec.pixelFormat());
    encoder.setTimeBase(Rational{1, 1000});
    encoder.setBitRate(vdec.bitRate());

    encoder.open(Codec(), ec);
    if (ec) {
        cerr << "Can't opent encodec\n";
        return 1;
    }

    Stream ost = octx.addStream(encoder);
    ost.setFrameRate(vst.frameRate());
    ost.setTimeBase(vst.timeBase());
    ost.setAverageFrameRate(vst.averageFrameRate());

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
    for (int64_t opkt_index = 0;;) {
        // READING
        Packet pkt = ictx.readPacket(ec);
        if (ec) {
            clog << "Packet reading error: " << ec << ", " << ec.message() << endl;
            break;
        }

        bool flushDecoder = false;
        // !EOF
        if (pkt) {
            if (pkt.streamIndex() != videoStream) {
                continue;
            }

            clog << "Read packet: pts=" << pkt.pts() << ", dts=" << pkt.dts() << " / " << pkt.pts().seconds() << " / " << pkt.timeBase() << " / st: " << pkt.streamIndex() << endl;
        } else {
            flushDecoder = true;
        }

        do {
            // DECODING
            auto frame = vdec.decode(pkt, ec);

            //count++;
            //if (count > 200)
            //    break;

            bool flushEncoder = false;

            if (ec) {
                cerr << "Decoding error: " << ec << endl;
                return 1;
            } else if (!frame) {
                //cerr << "Empty frame\n";
                //flushDecoder = false;
                //continue;

                if (flushDecoder) {
                    flushEncoder = true;
                }
            }

            if (frame) {
                clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType() << endl;

                // Change timebase
                frame.setTimeBase(encoder.timeBase());
                frame.setStreamIndex(0);
                frame.setPictureType();

                clog << "Frame: pts=" << frame.pts() << " / " << frame.pts().seconds() << " / " << frame.timeBase() << ", " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << " / type: " << frame.pictureType() << endl;
            }

            if (frame || flushEncoder) {
                do {
                    // Encode
                    Packet opkt = frame ? encoder.encode(frame, ec) : encoder.encode(ec);
                    if (ec) {
                        cerr << "Encoding error: " << ec << endl;
                        return 1;
                    } else if (!opkt) {
                        //cerr << "Empty packet\n";
                        //continue;
                        break;
                    }

                    // Only one output stream
                    opkt.setStreamIndex(0);
                    opkt.setTimeBase(1 / ost.frameRate());
                    // Trick: when timeBase is a 1/FPS, PTS value - just an index of the packet
                    // CUDA encoder provides wrong PTS
                    opkt.setPts({opkt_index++, 1 / ost.frameRate()});
                    opkt.setDts(opkt.pts());

                    clog << "Write packet: pts=" << opkt.pts() << ", dts=" << opkt.dts() << " / " << opkt.pts().seconds() << " / " << opkt.timeBase() << " / st: " << opkt.streamIndex() << endl;

                    octx.writePacket(opkt, ec);
                    if (ec) {
                        cerr << "Error write packet: " << ec << ", " << ec.message() << endl;
                        return 1;
                    }
                } while (flushEncoder);
            }

            if (flushEncoder)
                break;
        } while (flushDecoder);

        if (flushDecoder)
            break;
    }

    octx.writeTrailer();
}
