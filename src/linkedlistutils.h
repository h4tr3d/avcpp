#ifndef LINKEDLISTUTILS_H
#define LINKEDLISTUTILS_H

#include <iterator>

namespace av {

/**
 This deleter does nothing
 */
template<typename T>
struct NullDeleter
{
    void operator()(T*)  const
    {
    }
};

/**
 This deleter simple call 'delete' operator
 */
template<typename T>
struct PtrDeleter
{
    void operator()(T * x)  const
    {
        delete x;
    }
};


/**
 This functor uses simple notation to take next element from linked list.
 By default linked list have 'next' field that return pointer to next element in list.
 */
template<typename T>
struct DefaultNextElement
{
    T * operator()(T * x) const
    {
        return x->next;
    }
};


/**
 * This functor uses increment operation to take next element in list/array
 */
template<typename T>
struct IncrementNextElement
{
    T * operator()(T * x) const
    {
        return x + 1;
    }
};

/**
 * This functor used by default to take access to raw pointer that wrapped by W wrapper.
 * Functor call method 'T * raw()' to take raw pointer.
 */
template<typename T, typename W>
struct DefaultWrapperCast
{
    T * operator()(const W& wrapper) const
    {
        return wrapper.raw();
    }
};


/**
 * This functor used by default to set new pointer that will be wrapped by W wrapper.
 * Functor call method 'reset(T * ptr)' to set new
 */
template<typename T, typename W>
struct DefaultResetPtr
{
    void operator()(W& wrapper, T * ptr) const
    {
        wrapper.reset(ptr);
    }
};

/**
 Universal wrapper for one directional linked list elements. Next element taked with functor that
 described by N template parameter.

 T - type of wrapped elements
 W - type of element wrapper, must have ctor from T pointer, method reset(T * ptr) to reset current
     pointer
 N - functor that return next element in list
 C - functor that used to cast wrapper to its raw value
 R - functor that used to reset pointer value in wrapper to new one
 D - deleter, by default NullDeleter that does not free any resources
 I - Iterator cathegory (???)
 */
template<typename T,
         typename W,
         typename N = DefaultNextElement<T>,
         typename C = DefaultWrapperCast<T, W>,
         typename R = DefaultResetPtr<T, W>,
         typename D = NullDeleter<T>,
         typename I = std::forward_iterator_tag>
class LinkedListWrapper
{
public:
    typedef T element_type;
    typedef W element_wrapper_type;
    typedef N next_element_type;
    typedef C wrapper_cast_type;
    typedef R wrapper_reset_type;
    typedef D deleter_type;
    typedef I iterator_category;

    template<bool constIterator = false>
    class base_iterator : public std::iterator<iterator_category, typename std::conditional<constIterator, const element_wrapper_type, element_wrapper_type>::type>
    {
    private:
        using ElementType = typename std::conditional<constIterator, const element_type, element_type>::type;
        using WrapperType = typename std::conditional<constIterator, const element_wrapper_type, element_wrapper_type>::type;

        ElementType          *ptr;
        WrapperType           wrapper;
        next_element_type     getNextPtr;
        wrapper_reset_type    resetPtr;

    public:
        base_iterator(element_type *ptr) :
            ptr(ptr)
        {
            resetPtr(wrapper, ptr);
        }

        ~base_iterator() {}

        // prefix++
        base_iterator& operator++()
        {
            ptr = getNextPtr(ptr);
            resetPtr(wrapper, ptr);
            return *this;
        }

        // postfix++
        base_iterator operator++(int)
        {
            base_iterator tmp = *this;
            operator++();
            return tmp;
        }

        bool  operator==(const base_iterator& rhs) const {return ptr == rhs.ptr;}
        bool  operator!=(const base_iterator& rhs) const {return !(ptr == rhs.ptr);}

        WrapperType& operator*() noexcept
        {
            return wrapper;
        }

        WrapperType* operator->() noexcept
        {
            return &wrapper;
        }

    };

    using iterator = base_iterator<false>;
    using const_iterator = base_iterator<true>;

    LinkedListWrapper()
        : m_begin(0),
          m_end(0)
    {
    }

    explicit LinkedListWrapper(element_type * begin)
        : m_begin(begin),
          m_end(begin)
    {
    }


    ~LinkedListWrapper()
    {
        // Free resources
        deleter_type()(m_begin);
    }


    bool isValid()
    {
        return !!m_begin;
    }

    element_type *raw()
    {
        return m_begin;
    }

    const element_type *raw() const
    {
        return m_begin;
    }

    iterator begin()
    {
        return iterator(m_begin);
    }


    iterator end()
    {
        return iterator(0);
    }


    const_iterator begin() const
    {
        return const_iterator(m_begin);
    }


    const_iterator end() const
    {
        return const_iterator(0);
    }

    /**
     Log(N) complexity!
     */
    size_t count() const
    {
        return std::distance(begin(), end());
    }

    /**
     * Log(N) complexity
     * @param idx
     * @return
     */
    element_wrapper_type at(size_t idx) const
    {
        size_t size = count();
        if (idx >= size)
            return element_wrapper_type();

        const_iterator it = begin();
        std::advance(it, idx);

        return *it;
    }

protected:
    wrapper_cast_type  m_wrapperCast;
    element_type      *m_begin;
    element_type      *m_end;

};
}

#endif // LINKEDLISTUTILS_H
