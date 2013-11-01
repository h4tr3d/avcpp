#ifndef FILTERCONTEXT_HELPER_H
#define FILTERCONTEXT_HELPER_H

#include <boost/thread/mutex.hpp>

#include <iostream>

namespace av {
namespace detail {

// TODO make lock free, use atomic vars
template<typename T>
struct SharedCounterImpl
{
    SharedCounterImpl()
        : counter(1)
    {}

    SharedCounterImpl(int initialValue)
        : counter(initialValue)
    {}

    void increment()
    {
        boost::mutex::scoped_lock lock(mutex);
        counter++;
    }

    void decrement()
    {
        boost::mutex::scoped_lock lock(mutex);
        if (counter > 0)
            counter--;
    }

    T value() const
    {
        boost::mutex::scoped_lock lock(mutex);
        return counter;
    }


private:
    T counter;
    mutable boost::mutex mutex;
};


template<typename T>
struct SharedFlagImpl
{
    SharedFlagImpl()
        : flag(0)
    {}

    void setFlag(T value)
    {
        boost::mutex::scoped_lock lock(mutex);
        flag = value;
    }

    T getFlag() const
    {
        boost::mutex::scoped_lock lock(mutex);
        return flag;
    }

private:
    T flag;
    mutable boost::mutex mutex;
};


struct FilterContextSharedCounter
{
    typedef SharedCounterImpl<long> SharedCounterType;
    typedef SharedFlagImpl<bool>    SharedFlagType;

    FilterContextSharedCounter()
        : counter(0),
          managed(0)
    {
        counter = new SharedCounterType();
        managed = new SharedFlagType();
    }

    FilterContextSharedCounter(const FilterContextSharedCounter& other)
        : counter(other.counter),
          managed(other.managed)
    {
        if (this != &other)
        {
            counter->increment();
        }
    }

    ~FilterContextSharedCounter()
    {
        counter->decrement();
        if (counter->value() <= 0)
        {
            delete counter;
            delete managed;
        }
    }

    FilterContextSharedCounter& operator=(const FilterContextSharedCounter& rhs)
    {
        if (this != &rhs)
        {
            counter = rhs.counter;
            managed = rhs.managed;

            counter->increment();
        }
        return *this;
    }

    long getCount() const
    {
        return counter->value();
    }

    bool isManaged() const
    {
        return managed->getFlag();
    }

    void setManaged(bool managed)
    {
        this->managed->setFlag(managed);
    }

private:
    SharedCounterType *counter;
    SharedFlagType    *managed;
};



} // ::av::detail
} // ::av

#endif // FILTERCONTEXT_HELPER_H
