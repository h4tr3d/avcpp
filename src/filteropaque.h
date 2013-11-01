#ifndef FILTEROPAQUE_H
#define FILTEROPAQUE_H

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>
#include <boost/function.hpp>

#include <algorithm>

namespace av {

/**
 * Simple wrapper to opaque data that passed to avfilter_init_filter() as last argument
 */
class FilterOpaque
{
public:
    FilterOpaque();
    virtual ~FilterOpaque();
    void *getOpaque() const;

protected:
    void  setOpaque(void *ptr);

private:
    void *opaque;
};

/**
 * Pointers
 */
typedef boost::shared_ptr<FilterOpaque> FilterOpaquePtr;
typedef boost::weak_ptr<FilterOpaque>   FilterOpaqueWPtr;

/**
 * Allocator interface for opaque data
 */
struct FilterOpaqueAllocator
{
    virtual ~FilterOpaqueAllocator(){}
    virtual void * operator()() = 0;
};


/**
 * Deleter interface for opaque data
 */
struct FilterOpaqueDeleter
{
    virtual ~FilterOpaqueDeleter() {}
    virtual void operator()(void * ptr) = 0;
};


/**
 * Template class to managed filter opaque. You must specify
 *  - (T) Type of opaque data, like AVBufferSinkParams
 *  - (A) Functor that used to allocate data for opaque, like:
 * @code
 *struct BufferSinkParamsAllocator
 *{
 *  AVBufferSinkParams * operator()()
 *  {
 *     return av_buffersink_params_alloc();
 *  }
 *};
 * @endcode
 *  - (D) Functor that used to free opaque data, like:
 *@code
 *struct BufferSinkParamsDeleter : public FilterOpaqueDeleter<AVBufferSinkParams>
 *@endcode
 */
template<typename T,
         typename A,
         typename D>
class ManagedFilterOpaque : public FilterOpaque
{
public:
    ManagedFilterOpaque()
    {
        setOpaque(allocator());
    }

    virtual ~ManagedFilterOpaque()
    {
        deleter(getOpaque());
    }

    T * getTypedOpaque()
    {
        return static_cast<T *>(getOpaque());
    }

private:
    A allocator;
    D deleter;
};


} // ::av


#endif // FILTEROPAQUE_H
