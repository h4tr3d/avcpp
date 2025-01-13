#include <cstdio>
#include <iostream>
#include <filesystem>

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

class File
{
    std::FILE *_fp = nullptr;

public:
    File() = default;

    File(std::FILE *fp)
        : _fp(fp)
    {
    }

    File(std::nullptr_t)
    {
    }

    ~File()
    {
        std::fclose(_fp);
    }

    File(const File &) = delete;
    void operator=(const File &) = delete;

    File(File &&rhs)
        : _fp(std::exchange(rhs._fp, nullptr))
    {
    }

    File &operator=(File &&rhs)
    {
        if (this != std::addressof(rhs)) {
            File(std::move(rhs)).swap(*this);
        }
        return *this;
    }

    void swap(File &rhs)
    {
        using std::swap;
        swap(_fp, rhs._fp);
    }

    void reset(std::FILE *fp = nullptr)
    {
        if (_fp == fp)
            return;
        File(fp).swap(*this);
    }

    std::FILE *get() const
    {
        return _fp;
    }

    operator bool() const noexcept
    {
        return !!_fp;
    }

    operator std::FILE *() const noexcept
    {
        return _fp;
    }
};

int main(int argc, char **argv)
{
    if (argc < 2)
        return 1;

    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

    string uri {argv[1]};

    VideoDecoderContext vdec;
    Stream vst;
    error_code ec;

    {
        size_t counter {};

        File in = std::fopen(uri.c_str(), "rb+");
        if (!in) {
            cerr << "Can't open input\n";
            return 1;
        }

        // Just open h264 codec
        {
            auto codec = av::findDecodingCodec("h264");
            vdec = VideoDecoderContext {codec};
            vdec.open(ec);
            if (ec) {
                cerr << "Can't open codec\n";
                return 1;
            }
        }

        // Load a whole raw H.264 bitstream file into memory. Assumed AnnexB format:
        //  ([start code] NALU) | ( [start code] NALU) | ...
        auto const fsize = std::filesystem::file_size(uri);
        std::vector<uint8_t> data(fsize);
        std::fread(data.data(), 1, data.size(), in);

        // Start code:
        //   0x00 0x00 0x00 0x01
        // or
        //   0x00 0x00 0x01
        // Select shortest and check previous
        const uint8_t startCode[3] = {0x00, 0x00, 0x01};

        auto start = data.begin();
        while (true) {
            // Look for a start code pattern in the bitstream
            auto it = std::search(start, data.end(), std::begin(startCode), std::end(startCode));

            auto const finish = (it == data.begin() || *(it - 1) != 0x00) ? it : it - 1;

            if (start != data.begin())
                start -= 1;

            // FFmpeg expects, that start code present in the AVPacket
            auto const ptr = std::to_address(start);
            auto const size = size_t(finish - start);

            start = it + 1;

            clog << "  Data: " << (void *)ptr << ", size: " << size << endl;

            // Wrap into av::Packet
            Packet pkt {ptr, size, Packet::wrap_data_static {}};

            // ...and decode it into frame
            auto frame = vdec.decode(pkt, ec);

            // For valid Timestamp some NALU processing is needed to extract extra data
            auto ts = frame.pts();

            clog << "  Frame: " << frame.width() << "x" << frame.height() << ", size=" << frame.size() << ", ts=" << ts << ", tm: " << ts.seconds() << ", tb: " << frame.timeBase() << ", ref=" << frame.isReferenced() << ":" << frame.refCount() << endl;

            clog << "       : side_data: " << (void *)frame.raw()->side_data << endl;

            if (frame)
                ++counter;

            if (it == data.end())
                break;
        }

        clog << "Flush frames:\n";
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

            ++counter;
        }

        clog << "total frames: " << counter << endl;
    }
}
