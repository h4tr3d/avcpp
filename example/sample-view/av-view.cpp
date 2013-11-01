#include <iostream>
#include <set>
#include <map>

#include <SDL/SDL.h>
#include <SDL/SDL_thread.h>

#include <boost/thread.hpp>
#include <boost/date_time.hpp>

#include "ffmpeg.h"
#include "codec.h"
#include "containerformat.h"
#include "container.h"
#include "packet.h"
#include "streamcoder.h"
#include "videoresampler.h"

using namespace std;
using namespace av;

#define GET_TIME_NSEC(tm) \
    ((tm.sec * 1000000000L) + tm.nsec)

#define GET_TIME_DIFF_NSEC(timeA, timeB) \
    (((timeA.sec * 1000000000L) + timeA.nsec) - ((timeB.sec * 1000000000L) + timeB.nsec))

/**
 * @brief sleep_msec  sleep for a given amount of milliseconds
 * @param interval    interval in milliseconds to sleep
 */
static void sleep_msec(int64_t interval)
{
    boost::this_thread::disable_interruption di;
    boost::this_thread::sleep(boost::posix_time::milliseconds(interval));
}


/**
 * @brief sleep_usec  sleep for a given amount of microseconds
 * @param interval    interval to sleep in microseconds
 */
static void sleep_usec(uint64_t interval)
{
    boost::this_thread::disable_interruption di;
    boost::xtime time;
    boost::xtime_get(&time, boost::TIME_UTC_);

    uint64_t divider = 1000000000L; // nanosecods in seconds
    uint64_t intervalNsec = interval * 1000;

    time.sec  += intervalNsec / divider;
    time.nsec += intervalNsec % divider;

    boost::this_thread::sleep(time);
}


int main(int argc, char ** argv)
{

    if (argc != 2)
    {
        cout << "Use: " << argv[0] << " URI" << endl;
        ::exit(1);
    }

    string URI(argv[1]);

    // Init FFMPEG
    av::AVInit::init();

    // Init SDL
    if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
    {
        cerr << "Could not initialize SDL - " << SDL_GetError() << endl;
        ::exit(1);
    }

    SDL_Surface *screen = 0;
    SDL_Overlay *bmp    = 0;
    SDL_Rect     rect;
    SDL_Event    event;

    //
    // Prepare in
    //

    ContainerPtr in(new Container());

    if (in->openInput(URI.c_str()))
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
    // Playing
    //

    PacketPtr pkt(new Packet());
    FramePtr  frame(new Frame());
    int stat = 0;

    vector<PacketPtr> packets;

    // SDL
    screen = SDL_SetVideoMode(inW, inH, 0, 0);
    bmp    = SDL_CreateYUVOverlay(inW, inH, SDL_YV12_OVERLAY, screen);

    VideoResamplerPtr resampler(new VideoResampler(inW, inH, PIX_FMT_YUV420P,
                                                   inW, inH, inPF));

    long sec = 0;
    double previousFrameTimeMs = 0.0;
    double frameTimeDeltaMs      = 0.0;
    bool   isFirstFrame        = true;

    boost::xtime lastDisplayTime;
    boost::xtime currentTime;

    boost::xtime_get(&lastDisplayTime, boost::TIME_UTC_);

    while (in->readNextPacket(pkt) >= 0)
    {
        if (video.count(pkt->getStreamIndex()) > 0)
        {
            StreamCoderPtr coder = videoCoders[pkt->getStreamIndex()];

            coder->decodeVideo(frame, pkt);

            if (frame->isComplete())
            {
                long newSec = ((double)frame->getPts() * frame->getTimeBase().getDouble());
                if (newSec > sec)
                {
                    time_t tm = time(0);
                    cout << "Complete frame: " << frame->getPts() << ", " << coder->getTimeBase() << ", " << tm << " / " << ((double)frame->getPts() * frame->getTimeBase().getDouble()) << ", " << frame->getWidth() << "x" << frame->getHeight() << " / " << frame->getSize() << endl;
                    sec = newSec;
                }

                double frameTimeMs = frame->getPts() * frame->getTimeBase().getDouble() * 1000.0;
                frameTimeDeltaMs = frameTimeMs - previousFrameTimeMs;

                boost::xtime_get(&currentTime, boost::TIME_UTC_);
                double displayTimeDeltaMs = GET_TIME_DIFF_NSEC(currentTime, lastDisplayTime) / 1000.0 / 1000.0;

                // need sleep before display
                if (!isFirstFrame && displayTimeDeltaMs < frameTimeDeltaMs)
                {
                    double sleepTimeMs = frameTimeDeltaMs - displayTimeDeltaMs;
                    cout << "Sleep before display: " << sleepTimeMs << endl;
                    sleep_usec(sleepTimeMs * 1000);
                }

                lastDisplayTime = currentTime;

                // Display
                {
                    SDL_LockYUVOverlay(bmp);

                    AVFrame  *pict;
                    FramePtr  outFrame(new Frame(PIX_FMT_YUV420P, inW, inH));

                    pict = outFrame->getAVFrame();

                    pict->data[0] = bmp->pixels[0];
                    pict->data[1] = bmp->pixels[2];
                    pict->data[2] = bmp->pixels[1];

                    pict->linesize[0] = bmp->pitches[0];
                    pict->linesize[1] = bmp->pitches[2];
                    pict->linesize[2] = bmp->pitches[1];

                    resampler->resample(outFrame, frame);

                    SDL_UnlockYUVOverlay(bmp);

                    rect.x = 0;
                    rect.y = 0;
                    rect.w = inW;
                    rect.h = inH;
                    SDL_DisplayYUVOverlay(bmp, &rect);

                    SDL_PollEvent(&event);
                    switch(event.type)
                    {
                        case SDL_QUIT:
                            SDL_Quit();
                            ::exit(0);
                            break;
                        default:
                            break;
                    }
                }

                previousFrameTimeMs = frameTimeMs;
                isFirstFrame = false;
            }
        }
    }

    return 0;
}
