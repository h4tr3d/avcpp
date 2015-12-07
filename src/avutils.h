#ifndef AV_UTILS_H
#define AV_UTILS_H

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <mutex>
#include <sstream>
#include <algorithm>
#include <functional>

#include "ffmpeg.h"
#include "packet.h"
#include "frame.h"
#include "avtime.h"

//
// Functions
//
namespace av {

template<typename R, typename T>
R lexical_cast(const T& v)
{
    R result;
    std::stringstream ss;
    ss << v;
    ss >> result;
    return result;
}

class noncopyable
{
protected:
    noncopyable() = default;
    noncopyable( const noncopyable& ) = delete;
    void operator=( const noncopyable& ) = delete;
};

/**
 * This method can be used to turn up or down FFmpeg's logging level.
 *
 * @param level  An integer value for level.  Lower numbers
 *               mean less logging.  A negative number tells FFmpeg to
 *               shut up.
 */
void setFFmpegLoggingLevel(int32_t level);


/**
 * Like @see setFFmpegLoggingLevel, but can assept logging level as string.
 *
 * @param level - string representation of loggin level:
 *                'quiet', 'panic', 'fatal', 'error', 'warning', 'info', 'verbose' or 'debug',
 *                it also can be numeric (but in string representation), it it case
 *                boost::lexical_cast will be used and setFFmpegLoggingLevel(int32_t) will be
 *                called.
 */
void setFFmpegLoggingLevel(const std::string& level);


/**
 * @brief dump_binary_buffer
 * Dump binary buffer to std out in HEX view
 * @param buffer        pointer to buffer start
 * @param buffer_size   buffer size
 * @param width         output width in hex values (real text screen with is: sw = width * 3 - 1)
 */
void dumpBinaryBuffer(uint8_t *buffer, int buffer_size, int width = 16);


/**
 * C++ verstion of the av_err2str()
 * @param error - error code to convert to string
 * @return string representation of error code
 */
std::string error2string(int error);

}




//
// Classes
//
namespace av {

struct EmptyDeleter
{
    void operator()(void *) {}
};

/**
 * @brief The AvDeleter struct
 * Unified delete functor for variois FFMPEG/libavformat/libavcodec and so on resource allocators.
 * Can be used with shared_ptr<> and so on.
 */
struct AvDeleter
{
    bool operator() (SwsContext* &swsContext)
    {
        sws_freeContext(swsContext);
        swsContext = 0;
        return true;
    }


    bool operator() (AVCodecContext* &codecContext)
    {
        avcodec_close(codecContext);
        av_free(codecContext);
        codecContext = 0;
        return true;
    }


    bool operator() (AVOutputFormat* &format)
    {
        // Only set format to zero, it can'be freed by user
        format = 0;
        return true;
    }


    bool operator() (AVFormatContext* &formatContext)
    {
        avformat_free_context(formatContext);
        formatContext = 0;
        return true;
    }

    bool operator() (AVFrame* &frame)
    {
        av_freep(&frame);
        frame = 0;
        return true;
    }


    bool operator() (AVPacket* &packet)
    {
        av_free_packet(packet);
        av_free(packet);
        packet = 0;
        return true;
    }


    bool operator() (AVDictionary* &dictionary)
    {
        av_dict_free(&dictionary);
        dictionary = 0;
        return true;
    }

    bool operator ()(AVFilterInOut* &filterInOut)
    {
        avfilter_inout_free(&filterInOut);
        return true;
    }
};


/**
 * Functor to take next element in list/array
 */
struct AvNextElement
{
    const AVFilterPad * operator()(const AVFilterPad * x) const
    {
        if (x)
            return x + 1;
        else
            return 0;
    }


    AVFilterInOut * operator()(AVFilterInOut * x) const
    {
        if (x)
            return x->next;
        else
            return 0;
    }
};



/**
 * Teamplate class to set value for variable when out of scope will be occured. Use RAII idiom.
 *
 * By default type of variable and value is same, but it can be simple overriden, use
 *  @code
 *    ScopedValue<VariableType, OutValueType> scopedValue(variable, false);
 *  @endcode
 * instead
 * @code
 *    ScopedValue<VariableType> scopedValue(variable, false);
 * @endcode
 */
template<typename T, typename V = T>
class ScopedValue
{
public:
    /**
     * Ctor. Store reference to variable and output value.
     * @param var      variable that must be set
     * @param outValue value of variable
     */
    ScopedValue(T &var, const V& outValue)
        : var(var),
          outValue(outValue)
    {
    }

    /**
     * Ctor. Like previous one but automaticaly can assign initial value to variable.
     * @param var      variable that must be set
     * @param inValue  initial value
     * @param outValue output value
     */
    ScopedValue(T &var, const V &inValue, const V &outValue)
        : var(var),
          outValue(outValue)
    {
        this->var = inValue;
    }

    ~ScopedValue()
    {
        var = outValue;
    }

private:
    T& var;
    V  outValue;
};


/**
 * @brief The ScopeOutAction class - guard-type class that allows points callback that will be called
 * at the scope out
 *
 * Example:
 * @code
 * void foo()
 * {
 *     int fd = open(...some args...);
 *     ScopeOutAction action([fd](){
 *         close(fd);
 *     });
 *
 *     try
 *     {
 *         // some actions that can throw exception
 *     }
 *     catch(...)
 *     {
 *         throw; // Not best-practice, only for example
 *     }
 * }
 * @endcode
 *
 * In example above dtor of the action will be called before fd and we correctly close descriptor.
 *
 */
class ScopeOutAction
{
public:
    template<typename Proc>
    ScopeOutAction(const Proc& proc)
        : m_proc(proc)
    {}

    template<typename Proc>
    ScopeOutAction(Proc&& proc)
        : m_proc(std::forward<Proc>(proc))
    {}

    ~ScopeOutAction()
    {
        if (m_proc)
            m_proc();
    }

private:
    std::function<void()> m_proc;
};


/**
 * Simple class to support valid delay between packet or frame processing. Its need, for example,
 * for networking send and live streaming or, simple, for playback.
 *
 * Template class type must be a pointer or smart pointer, that implement valid 'operator->()'
 * Also, this class must have methods 'getPts()' and 'getTimeBase()'
 *
 * Example:
 * @code
 *   PacketTimeSynchronizer packetSync;
 *   while (PacketPtr packet = takePacket())
 *   {
 *      ...
 *      packetSync.doTimeSync(packet);
 *      ...
 *      displayPacket(packet); // or network send
 *   }
 * @endcode
 */
template<typename T>
class TimeSyncronizer
{
public:
    explicit TimeSyncronizer()
    {
        reset();
    }


    /**
     * Reset to default
     */
    inline void reset()
    {
        using av::gettime;
        startTime = gettime();
    }


    /**
     * Update last time. Should be called after displaing or network send
     */
    inline void update()
    {
    }


    /**
     * Take last sleep delta.
     * @return valid sleep delta, can be negate
     */
    inline int64_t getLastSleepDelta() const
    {
        return sleepDelta;
    }

    /**
     * Calculate packet interval and depend on real processing time make valid sleep. This function
     * depend on internal object state, so object must be keept between call.
     *
     * @param syncObject any object that have getPts and getTimeBase methods. Pointer or smart pointer.
     * @param useFakePts use fake PTS field if getPts() return AV_NOPTS_VALUE
     */
    int64_t doTimeSync(const T& syncObject, bool useFakePts = false)
    {
        using av::usleep;
        using av::gettime;

        int64_t currentPts = syncObject.getPts();

        if (currentPts == AV_NOPTS_VALUE)
        {
            if (useFakePts)
            {
                currentPts = syncObject.getFakePts();
            }

            if (currentPts == AV_NOPTS_VALUE)
            {
                return 0;
            }
        }

        int64_t currentTime    = gettime() - startTime; // usec
        int64_t currentPtsUsec = currentPts * syncObject.getTimeBase().getDouble() * 1000 * 1000; // usec

        sleepDelta = currentPtsUsec - currentTime;

        if (sleepDelta > 0)
        {
            usleep(sleepDelta);
        }

        return sleepDelta;
    }

private:
    int64_t startTime;
    int64_t sleepDelta;
};

/// typedefs for synchronizers for Packets and Frames
typedef TimeSyncronizer<Packet>  PacketTimeSynchronizer;


/**
 * Helper class that can call TimeSyncronizer object update() method on scope exit.
 *
 * Example:
 * @code
 *   PacketTimeSynchronizer packetSync;
 *   while (PacketPtr packet = takePacket())
 *   {
 *      TimeSyncronizerScopedUpdater<PacketPtr> syncUpdater(packetSync);
 *      ...
 *      packetSync.doTimeSync(packet);
 *      ...
 *      displayPacket(packet); // or network send
 *   }
 * @endcode
 */
template<typename T>
struct TimeSyncronizerScopedUpdater
{
    typedef TimeSyncronizer<T> TimeSyncronizerType;

    TimeSyncronizerScopedUpdater(TimeSyncronizerType& synchronizer)
        : synchronizer(synchronizer)
    {}

    ~TimeSyncronizerScopedUpdater()
    {
        synchronizer.update();
    }

private:
    TimeSyncronizerType& synchronizer;

};

typedef TimeSyncronizerScopedUpdater<Packet> PacketTimeSynchronizerScopedUpdater;



/**
 * Class to make PTS recalculation. Useful when we take stream from live source and it change it PTS
 * May be used with FramePtr and PacketPtr or others pointer classes that have methods getPts() and
 * setPts()
 */
template<typename T>
class PtsRecalculator
{
public:
    PtsRecalculator()
        : lastPts(AV_NOPTS_VALUE),
          ptsDelta(AV_NOPTS_VALUE),
          ptsOffset(AV_NOPTS_VALUE),
          doOffsetUse(false)
    {
    }

    void useOffset(bool doOffsetUse)
    {
        this->doOffsetUse = doOffsetUse;
    }

    int64_t getLastPts() const
    {
        return lastPts;
    }

    /**
     * Make PTS recalculation.
     *
     * @param object         object wich PTS should be recalculated.
     * @param doSkipEqualPts check for PTS duplcation.
     *
     * @return false if PTS duplicate and doSkipEqualPts == true, otherwise true
     */
    bool recalc(const T& object, bool doSkipEqualPts = true)
    {
        int64_t currentPts     = object->getPts();
        int64_t currentFakePts = object->getFakePts();
        int64_t workPts        = (currentPts == AV_NOPTS_VALUE ? currentFakePts : currentPts);
        int64_t initialPts     = workPts;

        if (doOffsetUse)
        {
            if (ptsOffset == AV_NOPTS_VALUE)
            {
                ptsOffset = workPts;
            }

            workPts -= ptsOffset;
        }

        int64_t newPts = workPts;
        if (lastPts != AV_NOPTS_VALUE && workPts != AV_NOPTS_VALUE)
        {
            if (workPts > lastPts)
            {
                ptsDelta = workPts - lastPts;
            }
            else if (workPts == lastPts && doSkipEqualPts)
            {
                return false;
            }
        }

        if (ptsDelta != AV_NOPTS_VALUE && lastPts != AV_NOPTS_VALUE)
        {
            if (workPts < lastPts)
            {
                newPts = lastPts + ptsDelta;
            }
        }

        if (newPts == lastPts && doSkipEqualPts)
        {
            return false;
        }

        if (newPts != initialPts)
        {
            if (currentPts != AV_NOPTS_VALUE)
                object->setPts(newPts);
            else
                object->setFakePts(newPts);
        }

        lastPts = newPts;

        return true;
    }


private:
    int64_t lastPts;
    int64_t ptsDelta;
    int64_t ptsOffset;
    bool    doOffsetUse;
};



////////////////////////////////////////////////////////////////////////////////////////////////////

template<typename T>
struct EqualComparator
{
    EqualComparator(const T& value)
        : value(value)
    {}

    bool operator() (const T& value) const
    {
        if (this->value == value)
            return true;

        return false;
    }

    const T& value;
};

/**
 * Select more approptiate value from given value list. Useful for AVCodec::supported_framerates,
 * AVCodec::pix_fmts and so on.
 *
 * T - type of value and (by default) list elements
 * L - type of list elements
 * C - end list comparator
 *
 * If T and L different types, T must be have ctor from L.
 *
 * @param value           value to set
 * @param list            list of allowed values
 * @param endOfListValue  end of list value, like
 * @return value if list null or if it present in list, or more appropriate value from list
 */
template<typename T, typename L, typename C>
T guessValue(const T& value, const L * list, C endListComparator)
{
    if (!list)
        return value;

    // move values to array
    std::deque<T> values;
    for (const L * ptr = list; !endListComparator(*ptr); ++ptr)
    {
        T v = *ptr;
        values.push_back(v);
    }

    // sort list
    std::sort(values.begin(), values.end());

    // Search more appropriate range
    int begin = 0;
    int end   = values.size() - 1;
    while ((end - begin) > 1)
    {
        int mid = begin + (end - begin) / 2;

        if (value <= values[mid])
        {
            end = mid;
        }
        else
        {
            begin = mid;
        }
    }

    // distance from VALUE to BEGIN more short or VALUE less then BEGIN
    if (value <= values[begin] || (value - values[begin]) < (values[end] - value))
    {
        return values[begin];
    }

    return values[end];
}


template<typename T, typename Container>
void array_to_container(const T* array, std::size_t nelemnts, Container &container)
{
    if (!array || nelemnts == 0)
        return;
    std::copy_n(array, array + nelemnts, std::back_inserter(container));
}

template<typename T, typename Container, typename Compare>
void array_to_container(const T* array, Container &container, Compare isEnd)
{
    if (!array)
        return;
    T value;
    while (!isEnd(value = *array++))
        container.push_back(value);
}

} // ::av

#endif // AV_UTILS_H
