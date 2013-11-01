#include <iostream>
#include <set>
#include <map>

#include "av/ffmpeg.h"
#include "av/codec.h"
#include "av/containerformat.h"
#include "av/container.h"
#include "av/packet.h"
#include "av/streamcoder.h"
#include "av/videoresampler.h"

using namespace std;
using namespace av;

const static string INPUT = "input.mpeg";
const static string OUTPUT = "output.flv";

struct FileStreamDeleter
{
    bool operator() (FILE* &fp)
    {
        fclose(fp);
        fp = 0;
    }
};

class MyWriter : public AbstractWriteFunctor
{
public:
    MyWriter(const char *name)
    {
        namePtr = name;
        fp = boost::shared_ptr<FILE>(fopen(name, "w+"), FileStreamDeleter());

        if (!fp)
        {
            cout << "open stream: " << (void*)fp.get() << " / " << errno << " / " << strerror(errno) << endl;
        }

        cout << "WriterCtor\n";
    }

    ~MyWriter()
    {
        cout << "WriterDtor\n";
    }

    int operator() (uint8_t *buf, int size)
    {
        if (!fp)
            return -1;
        size_t s = fwrite(buf, size, 1, fp.get());
        cout << "write data: " << (void*)fp.get() << " / " << s << " / " << errno << " / " << strerror(errno) << endl;

        return s;
    }

    const char *name() const
    {
        return namePtr.c_str();
    }

private:
    string namePtr;
    boost::shared_ptr<FILE> fp;
};


int main(int argc, char ** argv)
{
    std::string inURI = INPUT;
    std::string outURI = OUTPUT;

    if (argc > 1)
        inURI = std::string(argv[1]);
    
    if (argc > 2)
        outURI = std::string(argv[2]);

    //
    // Prepare in
    //

    ContainerPtr in(new Container());

    if (in->openInput(inURI.c_str()))
    {
        cout << "Open success\n";
    }
    else
        cout << "Open fail\n";

    int streamCount = in->getStreamsCount();
    cout << "Streams: " << streamCount << endl;

    set<int> audio;
    set<int> video;
    map<int, StreamCoderPtr> videoCoders;

    int inW = -1;
    int inH = -1;
    PixelFormat inPF;

    for (int i = 0; i < streamCount; ++i)
    {
        StreamPtr st = in->getStream(i);
        if (st->getAVStream()->codec->codec_type == AVMEDIA_TYPE_VIDEO)
        {
            video.insert(i);

            StreamCoderPtr coder(new StreamCoder(st));
            coder->open();

            inW = coder->getWidth();
            inH = coder->getHeight();
            inPF = coder->getPixelFormat();

            videoCoders[i] = coder;
        }
        else if (st->getAVStream()->codec->codec_type == AVMEDIA_TYPE_AUDIO)
        {
            audio.insert(i);
        }
    }


    //
    // Prepare coder
    //
    ContainerFormatPtr outFormat(new ContainerFormat());
    outFormat->setOutputFormat("flv", 0, 0);

    ContainerPtr out(new Container());
    out->setFormat(outFormat);

    CodecPtr outCodec = Codec::findEncodingCodec(outFormat->getOutputDefaultVideoCodec());

    // add stream
    StreamPtr outStream = out->addNewStream(outCodec);
    StreamCoderPtr outCoder(new StreamCoder(outStream));

    outCoder->setCodec(outCodec);
    outCoder->setTimeBase(Rational(1, 25));
    outCoder->setWidth(384);
    outCoder->setHeight(288);
    outCoder->setPixelFormat(PIX_FMT_YUV420P);
    //outCoder->setPixelFormat(PIX_FMT_YUVJ420P);

    outStream->setFrameRate(Rational(25, 1));

    outCoder->open();

    out->dump();



    //
    // Writing
    //
    ContainerFormatPtr writeFormat(new ContainerFormat());
    writeFormat->setOutputFormat("flv", 0, 0);

    ContainerPtr writer(new Container());
    writer->setFormat(outFormat);

    CodecPtr writeCodec = Codec::findEncodingCodec(writeFormat->getOutputDefaultVideoCodec());

    // add stream
    StreamPtr writeStream = writer->addNewStream(writeCodec);
    StreamCoderPtr writeCoder(new StreamCoder(writeStream));

    writeCoder->setCodec(writeCodec);
    writeCoder->setTimeBase(Rational(1, 25));
    writeCoder->setWidth(384);
    writeCoder->setHeight(288);
    writeCoder->setPixelFormat(PIX_FMT_YUV420P);

    writeStream->setFrameRate(Rational(25, 1));

    writeCoder->open();

    //writer->openOutput("/tmp/test2.flv");
    MyWriter fwriter(outURI);
    //writer->openOutput(fwriter);
    writer->openOutput(outURI);
    writer->dump();
    writer->writeHeader();
    writer->flush();




    //
    // Transcoding
    //

    PacketPtr pkt(new Packet());
    FramePtr  frame(new Frame());
    int stat = 0;

    vector<PacketPtr> packets;

    VideoResamplerPtr resampler(new VideoResampler(outCoder->getWidth(), outCoder->getHeight(), outCoder->getPixelFormat(),
                                                   inW, inH, inPF));

    while (in->readNextPacket(pkt) >= 0)
    {
        if (video.count(pkt->getStreamIndex()) > 0)
        {
            StreamCoderPtr coder = videoCoders[pkt->getStreamIndex()];

            coder->decodeVideo(frame, pkt);

            if (frame->isComplete())
            {
                //cout << "Complete frame: " << frame->getPts() << ", " << frame->getWidth() << "x" << frame->getHeight() << " / " << frame->getSize() << endl;

                FramePtr outFrame(new Frame(outCoder->getPixelFormat(), outCoder->getWidth(), outCoder->getHeight()));

                resampler->resample(outFrame, frame);

                PacketPtr outPkt(new Packet());
                stat = outCoder->encodeVideo(outPkt, outFrame);
                if (stat < 0)
                {
                    cout << "Encoding error...\n";
                }
                else
                {

#if 0
                    //cout << "Encode packet: " << outPkt->isComplete() << endl;
                    packets.push_back(outPkt);
                    // HACK:
                    if (packets.size() > 100)
                        break;
#endif
                    writer->writePacket(outPkt);
                }
            }
        }
    }


#if 0
    //
    // Writing
    //
    ContainerFormatPtr writeFormat(new ContainerFormat());
    writeFormat->setOutputFormat("flv", 0, 0);

    ContainerPtr writer(new Container());
    writer->setFormat(outFormat);

    CodecPtr writeCodec = Codec::findEncodingCodec(writeFormat->getOutputDefaultVideoCodec());

    // add stream
    StreamPtr writeStream = writer->addNewStream(writeCodec);
    StreamCoderPtr writeCoder(new StreamCoder(writeStream));

    writeCoder->setCodec(writeCodec);
    writeCoder->setTimeBase(Rational(1, 25));
    writeCoder->setWidth(384);
    writeCoder->setHeight(288);
    writeCoder->setPixelFormat(PIX_FMT_YUV420P);

    writeStream->setFrameRate(Rational(25, 1));

    writeCoder->open();

    MyWriter fwriter(outURI);
    //writer->openOutput(fwriter);
    writer->openOutput(outURI);
    writer->dump();
    writer->writeHeader();
    writer->flush();

    for (size_t i = 0; i < packets.size(); ++i)
    {
        writer->writePacket(packets.at(i));
    }

    writer->close();
#endif

    return 0;
}
