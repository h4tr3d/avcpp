#include <iostream>
#include <set>
#include <map>
#include <memory>
#include <functional>

#include "av.h"
#include "ffmpeg.h"
#include "pixelformat.h"
#include "sampleformat.h"
#include "codec.h"
#include "containerformat.h"
#include "container.h"
#include "packet.h"
#include "streamcoder.h"
#include "videorescaler.h"
#include "audiosamples.h"
#include "audioresampler.h"
#include "avutils.h"
#if 0
#include "filtergraph.h"
#include "filterbufferref.h"
#include "filterinout.h"
#include "filters/buffersink.h"
#endif

using namespace std;
using namespace av;

PacketTimeSynchronizer packetSync;

inline static
void usage(const string& appName)
{
    cout << "Usage: " << appName << " INPUT OUTPUT [filter description]" << endl;
}


void formatWriter(const ContainerPtr& container, const PacketPtr& packet)
{
    packetSync.doTimeSync(packet);

    //if (packet->getPts() != AV_NOPTS_VALUE)
    //    packet->setDts(packet->getPts());

    clog << "Write FRAME: " << packet->getStreamIndex()
         << ", PTS: "       << packet->getPts()
         << ", DTS: "       << packet->getDts()
         << ", timeBase: "  << packet->getTimeBase()
         << ", time: "      << packet->getTimeBase().getDouble() * packet->getPts()
         << ", fakeTime: "  << packet->getTimeBase().getDouble() * packet->getFakePts()
         << endl;

    container->writePacket(packet, true);
}

int main(int argc, char **argv)
{
    // Init FFMPEG
    av::init();
    av::setFFmpegLoggingLevel(AV_LOG_DEBUG);

#if 0
    FilterGraphPtr     graph(new FilterGraph);
    string             filters;
    FilterInOutListPtr inputs;
    FilterInOutListPtr outputs;

    //filters = "yadif=0:-1:0, scale=400:226, drawtext=fontfile=/usr/share/fonts/TTF/DroidSans.ttf:"
    //          "text='tod- %X':x=(w-text_w)/2:y=H-60 :fontcolor=white :box=1:boxcolor=0x00000000@1";
    filters = "[0]scale=iw/2:ih/2,pad=2*iw:ih[left];[1]scale=iw/2:ih/2[right];[left][right]overlay=main_w/2:0";

    if (graph->parse(filters, inputs, outputs) < 0)
    {
        clog << "Error to parse graph" << endl;
        return 1;
    }

    if (graph->config() >= 0)
        graph->dump();

    if (inputs)
    {
        clog << "Inputs count: " << inputs->getCount() << endl;
        FilterInOutList::iterator it = inputs->begin();
        while (it != inputs->end())
        {
            FilterInOut inout = *it;

            clog << "Input: " << inout.getPadIndex() << ", name: " << inout.getName() << ", filter: " << inout.getFilterContext()->getName() << endl;
            ++it;
        }
    }

    if (outputs)
    {
        clog << "Outputs count: " << outputs->getCount() << endl;
        FilterInOutList::iterator it = outputs->begin();
        while (it != outputs->end())
        {
            FilterInOut inout = *it;

            clog << "Output: " << inout.getPadIndex() << ", name: " << inout.getName() << ", filter: " << inout.getFilterContext()->getName() << endl;
            ++it;
        }
    }

    for (unsigned i = 0; i < graph->getFiltersCount(); ++i)
    {
        clog << "Filter: " << graph->getFilter(i)->getName() << endl;
    }

#else

    string srcUri;
    string dstUri;
    string filterDescription = "";

    if (argc < 3)
    {
        usage(argv[0]);
        ::exit(1);
    }

    srcUri = string(argv[1]);
    dstUri = string(argv[2]);

    if (argc > 3)
    {
        filterDescription = string(argv[3]);
    }

    //
    // Prepare input
    //
    ContainerPtr in(new Container());

    if (in->openInput(srcUri.c_str()))
    {
        cout << "Open success\n";
    }
    else
    {
        cout << "Open fail\n";
        return 1;
    }

    int streamCount = in->getStreamsCount();
    cout << "Streams: " << streamCount << endl;

    set<int> audio;
    set<int> video;
    map<int, StreamCoderPtr> decoders;

    int inW = -1;
    int inH = -1;
    PixelFormat inPF;
    Rational inTimeBase;
    Rational inSampleAspectRatio;
    Rational inFrameRate;

    int            inSampleRate    = 0;
    int            inChannels      = 0;
    SampleFormat   inSampleFmt     = AV_SAMPLE_FMT_S16;
    uint64_t       inChannelLayout = 0;


    for (int i = 0; i < streamCount; ++i)
    {
        StreamPtr st = in->getStream(i);
        if (st->getMediaType() == AVMEDIA_TYPE_VIDEO ||
            st->getMediaType() == AVMEDIA_TYPE_AUDIO)
        {
            if (st->getMediaType() == AVMEDIA_TYPE_VIDEO)
                video.insert(i);
            else if (st->getMediaType() == AVMEDIA_TYPE_AUDIO)
                audio.insert(i);

            StreamCoderPtr coder(new StreamCoder(st));
            coder->open();

            if (st->getMediaType() == AVMEDIA_TYPE_VIDEO)
            {
                inW        = coder->getWidth();
                inH        = coder->getHeight();
                inPF       = coder->getPixelFormat();
                inTimeBase = coder->getTimeBase();
                inFrameRate = coder->getFrameRate();
                //inSampleAspectRatio = coder->getAVCodecContext()->sample_aspect_ratio;
                inSampleAspectRatio = st->getSampleAspectRatio();

                if (inSampleAspectRatio == Rational(0, 1))
                    inSampleAspectRatio = coder->getAVCodecContext()->sample_aspect_ratio;

                clog << "Aspect ratio: " << inSampleAspectRatio << endl;
            }
            else
            {
                inSampleRate = coder->getSampleRate();
                inChannels   = coder->getChannels();
                inChannelLayout = coder->getChannelLayout();
                inSampleFmt = coder->getSampleFormat();
            }

            clog << "In: TimeBases coder:" << coder->getTimeBase() << ", stream:" << st->getTimeBase()  << endl;

            decoders[i] = coder;
        }
    }






    //
    // Writing
    //
    ContainerFormatPtr writeFormat(new ContainerFormat());
    writeFormat->setOutputFormat(0, dstUri.c_str(), 0);
    if (!writeFormat->isOutput())
    {
        clog << "Fallback to MPEGTS output format" << endl;
        writeFormat->setOutputFormat("mpegts", 0, 0);
    }

    ContainerPtr writer(new Container());
    writer->setFormat(writeFormat);

    int         outW      = 640;
    int         outH      = 480;
    PixelFormat outPixFmt = PIX_FMT_YUV420P;

    int            outSampleRate    = inSampleRate;
    //int            outSampleRate    = 44100;
    int            outChannels      = inChannels;
    SampleFormat   outSampleFmt     = inSampleFmt;
    uint64_t       outChannelLayout = inChannelLayout;
    int            outFrameSize     = 0;

    map<int, StreamCoderPtr> encoders;
    map<int, int>            streamMapping;

    map<int, StreamCoderPtr>::iterator it;
    int i = 0;
    for (it = decoders.begin(); it != decoders.end(); ++it)
    {
        int originalIndex = it->first;

        CodecPtr codec;
        if (video.count(originalIndex) > 0)
        {
            codec = Codec::findEncodingCodec(writeFormat->getOutputDefaultVideoCodec());
            //continue;
        }
        else
        {
            codec = Codec::findEncodingCodec(writeFormat->getOutputDefaultAudioCodec());
            //continue;
        }

        streamMapping[originalIndex] = i;

        StreamPtr st = writer->addNewStream(codec);
        StreamCoderPtr coder(new StreamCoder(st));

        coder->setCodec(codec);

        if (st->getMediaType() == AVMEDIA_TYPE_VIDEO)
        {
            coder->setWidth(outW);
            coder->setHeight(outH);

            outPixFmt = *codec->getAVCodec()->pix_fmts;
            coder->setPixelFormat(outPixFmt);

            coder->setTimeBase(Rational(1,25));
            //st->setTimeBase(Rational(1,25));

            st->setFrameRate(Rational(25,1));
            coder->setBitRate(500000);
        }
        else if (st->getMediaType() == AVMEDIA_TYPE_AUDIO)
        {
            st->getAVStream()->id = 1;

            if (codec->getAVCodec()->supported_samplerates)
                outSampleRate = *codec->getAVCodec()->supported_samplerates;

            coder->setSampleRate(outSampleRate);
            coder->setChannels(outChannels);
            coder->setSampleFormat(outSampleFmt);
            outChannelLayout = coder->getChannelLayout();

            coder->setTimeBase(Rational(1, outSampleRate));
            //st->setTimeBase(Rational(1, outSampleRate));
        }

        if (!coder->open())
        {
            cerr << "Can't open coder" << endl;
        }

        clog << "Ou: TimeBases coder:" << coder->getTimeBase() << ", stream:" << st->getTimeBase()  << endl;

        if (st->getMediaType() == AVMEDIA_TYPE_AUDIO)
            outFrameSize = coder->getAVCodecContext()->frame_size;

        encoders[i] = coder;
        ++i;
    }

    writer->openOutput(dstUri.c_str());
    writer->dump();
    writer->writeHeader();
    writer->flush();

    //
    // Transcoding
    //

    PacketPtr pkt(new Packet());
    int stat = 0;

#if 0
    // Audio filter graph
    list<int>            dstSampleRates    = {outSampleRate};
    list<SampleFormat> dstSampleFormats  = {outSampleFmt};
    list<uint64_t>       dstChannelLayouts = {outChannelLayout};
    BufferSrcFilterContextPtr srcAudioFilter;
    BufferSinkFilterContextPtr sinkAudioFilter;
    FilterGraphPtr       audioFilterGraph;

    if (!audio.empty())
    {
         audioFilterGraph = FilterGraph::createSimpleAudioFilterGraph(Rational(1, inSampleRate),
                                                                      inSampleRate, inSampleFmt, inChannelLayout,
                                                                      dstSampleRates, dstSampleFormats, dstChannelLayouts,
                                                                      string());


         sinkAudioFilter = filter_cast<BufferSinkFilterContext>(audioFilterGraph->getSinkFilter());
         srcAudioFilter  = audioFilterGraph->getSrcFilter();

         if (sinkAudioFilter)
         {
             sinkAudioFilter->setFrameSize(outFrameSize);
         }

         audioFilterGraph->dump();
    }


    // Video filter graph
#if 0
    string videoFilterDesc = "movie=http\\\\://camproc1/snapshots/logo.png [watermark]; [in][watermark] overlay=0:0:rgb=1,"
                             "drawtext=fontfile=/home/hatred/fifte.ttf:fontsize=20:"
                             "text='%F %T':x=(w-text_w-5):y=H-20 :fontcolor=white :box=0:boxcolor=0x00000000@1 [out]";
#else
    string &videoFilterDesc = filterDescription;
#endif

    BufferSrcFilterContextPtr  srcVideoFilter;
    BufferSinkFilterContextPtr sinkVideoFilter;
    FilterGraphPtr videoFilterGraph =
            FilterGraph::createSimpleVideoFilterGraph(inTimeBase,
                                                      inSampleAspectRatio,
                                                      inFrameRate,
                                                      inW, inH, inPF,
                                                      outW, outH, outPixFmt,
                                                      videoFilterDesc);
    sinkVideoFilter = videoFilterGraph->getSinkFilter();
    srcVideoFilter  = videoFilterGraph->getSrcFilter();

    videoFilterGraph->dump();

    //return 0;
#endif

    // TODO may be fault
    VideoRescaler videoRescaler {outW, outH, outPixFmt, inW, inH, inPF};

    uint64_t samplesCount = 0;
    packetSync.reset();
    while (in->readNextPacket(pkt) >= 0)
    {
        if (streamMapping.find(pkt->getStreamIndex()) == streamMapping.end())
        {
            continue;
        }

        clog << "Input: "  << pkt->getStreamIndex()
             << ", PTS: "  << pkt->getPts()
             << ", DTS: "  << pkt->getDts()
             << ", TB: "   << pkt->getTimeBase()
             << ", time: " << pkt->getTimeBase().getDouble() * pkt->getDts()
             << endl;

        if (video.count(pkt->getStreamIndex()) > 0)
        {
            VideoFramePtr  frame(new VideoFrame());
            StreamCoderPtr coder = decoders[pkt->getStreamIndex()];

            if (pkt->getPts() == AV_NOPTS_VALUE && pkt->getDts() != AV_NOPTS_VALUE)
            {
                pkt->setPts(pkt->getDts());
            }

            auto ret = coder->decodeVideo(frame, pkt);
            frame->setStreamIndex(streamMapping[pkt->getStreamIndex()]);

            clog << "decoding ret: " << ret << ", pkt size: " << pkt->getSize() << endl;

            if (frame->isComplete())
            {
                StreamCoderPtr &encoder = encoders[streamMapping[pkt->getStreamIndex()]];

                clog << "Frame: aspect ratio: " << Rational(frame->getAVFrame()->sample_aspect_ratio)
                     << ", size: " << frame->getWidth() << "x" << frame->getHeight()
                     << ", pix_fmt: " << frame->getPixelFormat()
                     << ", time_base: " << frame->getTimeBase()
                     << endl;

#if 0
                stat = srcVideoFilter->addFrame(frame);
                if (stat < 0)
                {
                    clog << "Can't add video frame to filters chain" << endl;
                    continue;
                }

                while (1)
                {
                    FilterBufferRef frameref;

                    stat = sinkVideoFilter->getBufferRef(frameref, 0);

                    if (stat == AVERROR(EAGAIN) || stat == AVERROR_EOF)
                        break;

                    if (stat < 0)
                        break;

                    if (frameref.isValid())
                    {
                        FramePtr outFrame;
                        frameref.copyToFrame(outFrame);
                        if (outFrame)
                        {
                            VideoFramePtr outVideoFrame = std::static_pointer_cast<VideoFrame>(outFrame);
                            outVideoFrame->setStreamIndex(streamMapping[pkt->getStreamIndex()]);
                            outVideoFrame->setTimeBase(encoder->getTimeBase());
                            outVideoFrame->setPts(pkt->getTimeBase().rescale(pkt->getPts(), outVideoFrame->getTimeBase()));

                            stat = encoder->encodeVideo(outVideoFrame, std::bind(formatWriter, writer, std::placeholders::_1));
                        }
                    }
                }
#else
                FramePtr outFrame = frame;
                VideoFramePtr outVideoFrame = std::static_pointer_cast<VideoFrame>(outFrame);
                outVideoFrame->setStreamIndex(streamMapping[pkt->getStreamIndex()]);
                outVideoFrame->setTimeBase(encoder->getTimeBase());
                outVideoFrame->setPts(pkt->getTimeBase().rescale(pkt->getPts(), outVideoFrame->getTimeBase()));

                stat = encoder->encodeVideo(outVideoFrame, std::bind(formatWriter, writer, std::placeholders::_1));
#endif
            }
        }
        else if (audio.count(pkt->getStreamIndex()) > 0)
        {
            AudioSamplesPtr samples(new AudioSamples());
            StreamCoderPtr  coder = decoders[pkt->getStreamIndex()];

            pkt->setPts(AV_NOPTS_VALUE);
            pkt->setDts(AV_NOPTS_VALUE);

            int size = coder->decodeAudio(samples, pkt);
            samples->setStreamIndex(streamMapping[pkt->getStreamIndex()]);

            //clog << "Packet Size: " << pkt->getSize() << ", encoded bytes: " << size << endl;
            //clog << "Audio Inp PTS: " << pkt->getPts() << ", " << samples->getPts() << endl;
            //clog << "FakePts: " << samples->getFakePts() << ", timeBase: " << samples->getTimeBase() << ", " << samples->getTimeBase().getDouble() * samples->getFakePts() << endl;

            if (samples->isComplete())
            {
                StreamCoderPtr &encoder = encoders[streamMapping[pkt->getStreamIndex()]];

#if 0
                stat = srcAudioFilter->addFrame(samples);
                if (stat < 0)
                {
                    clog << "Can't add audio samples to filters chain" << endl;
                    continue;
                }

                int count = 0;
                while (1)
                {
                    FilterBufferRef samplesref;

                    stat = sinkAudioFilter->getBufferRef(samplesref, 0);

                    if (stat == AVERROR(EAGAIN) || stat == AVERROR_EOF)
                        break;

                    if (stat < 0)
                        break;

                    if (samplesref.isValid())
                    {
                        const AVFilterBufferRefAudioProps *props = samplesref.getAVFilterBufferRef()->audio;

//                        clog << "Cnt:" << count << ", Ch layout:" << props->channel_layout
//                             << ", nb_samples:" << props->nb_samples
//                             << ", rate:" << props->sample_rate
//                             << endl;

                        FramePtr outFrame;
                        samplesref.copyToFrame(outFrame);
                        if (outFrame)
                        {
                            AudioSamplesPtr outSamples = std::static_pointer_cast<AudioSamples>(outFrame);

                            outSamples->setTimeBase(encoder->getTimeBase());
                            outSamples->setFakePts(pkt->getTimeBase().rescale(pkt->getPts(), outSamples->getTimeBase()));
                            outSamples->setStreamIndex(streamMapping[pkt->getStreamIndex()]);

//                            clog << "Samples: " << outSamples->getSamplesCount()
//                                 << ", ts:" << outSamples->getPts() << " / " << AV_NOPTS_VALUE << ", " << (outSamples->getPts() == AV_NOPTS_VALUE)
//                                 << ", tb:" << outSamples->getTimeBase()
//                                 << endl;

                            stat = encoder->encodeAudio(outSamples, std::bind(formatWriter, writer, std::placeholders::_1));
                        }
                    }
                }
#else
                FramePtr outFrame = samples;
                AudioSamplesPtr outSamples = std::static_pointer_cast<AudioSamples>(outFrame);

                outSamples->setTimeBase(encoder->getTimeBase());
                outSamples->setFakePts(pkt->getTimeBase().rescale(pkt->getPts(), outSamples->getTimeBase()));
                outSamples->setStreamIndex(streamMapping[pkt->getStreamIndex()]);

                stat = encoder->encodeAudio(outSamples, std::bind(formatWriter, writer, std::placeholders::_1));
#endif
            }
        }
    }

    // Flush buffers
    set<int> allStreams = video;
    allStreams.insert(audio.begin(), audio.end());
    for (set<int>::const_iterator it = allStreams.begin(); it != allStreams.end(); ++it)
    {
        if (streamMapping.find(*it) == streamMapping.end())
        {
            continue;
        }

        clog << "Flush stream: " << *it << endl;

        PacketPtr pkt(new Packet());
        pkt->setStreamIndex(streamMapping[*it]);
        formatWriter(writer, pkt);
    }

    // Write trailer
    writer->flush();
    writer->writeTrailer();

    /*
    // Must be closed before container
    encoders.clear();

    // Container close
    writer->close();
    writer.reset();

    // decoders
    decoders.clear();

    // Input container
    in->close();
    in.reset();
    */

#endif
    return 0;
}
