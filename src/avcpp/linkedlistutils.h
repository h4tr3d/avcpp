#pragma once

/**
 * Iteration helpers for FFmpeg intrusive linked lists.
 *
 * @file
 * @ingroup core
 */

#include <iterator>

namespace av {

/**
 This deleter does nothing
 */
template<typename T>
struct NullDeleter
{
    /**
     * Do nothing.
     */
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
    /**
     * Delete the element.
     *
     * @param x the element to delete; must come from a matching new
     */
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
    /**
     * Follow the `next` field of an intrusive list.
     *
     * @param x the current element
     *
     * @return the next element, or nullptr at the end of the list
     */
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
    /**
     * Step to the next element of a contiguous array.
     *
     * @param x the current element
     *
     * @return the element that follows it in memory
     */
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
    /**
     * Extract the raw pointer from a wrapper.
     *
     * @param wrapper the wrapper to look into
     *
     * @return whatever `wrapper.raw()` returns
     */
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
    /**
     * Point a wrapper at another element.
     *
     * @param wrapper the wrapper to modify
     * @param ptr     the pointer to store
     */
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
        /// Type of the list element, as FFmpeg declares it.
    typedef T element_type;
        /// avcpp wrapper returned when the iterator is dereferenced.
    typedef W element_wrapper_type;
        /// Functor that produces the next element.
    typedef N next_element_type;
        /// Functor used to obtain the raw pointer of a wrapper.
    typedef C wrapper_cast_type;
        /// Functor used to point a wrapper at another element.
    typedef R wrapper_reset_type;
        /// Deleter called by the destructor.
    typedef D deleter_type;
        /// Iterator category forwarded to the iterator traits.
    typedef I iterator_category;

    template<bool constIterator = false>
    /**
     * Iterator over the list.
     *
     * Holds the raw element pointer plus a wrapper that is updated on every
     * step, so dereferencing yields a wrapper without allocating.
     *
     * @tparam constIterator `true` for the read-only flavour
     */
    class base_iterator
    {
    public:

                /// Iterator traits of this type, kept for introspection.
        std::iterator_traits<base_iterator> d;
        // Iterator interface
                /// Category of the iteration.
        using iterator_category = iterator_category;
                /// Type produced by dereferencing.
        using value_type = typename std::conditional<constIterator, const element_wrapper_type, element_wrapper_type>::type;
                /// Distance type required by the iterator traits.
        using difference_type = ptrdiff_t;
                /// Pointer to the wrapper.
        using pointer = value_type*;
                /// Reference to the wrapper.
        using reference = value_type&;

    private:
        using ElementType = typename std::conditional<constIterator, const element_type, element_type>::type;
        using WrapperType = typename std::conditional<constIterator, const element_wrapper_type, element_wrapper_type>::type;

        ElementType          *ptr;
        WrapperType           wrapper;
        next_element_type     getNextPtr;
        wrapper_reset_type    resetPtr;

    public:
        /**
         * Point the iterator at an element.
         *
         * @param ptr the element to start from; null makes an end iterator
         */
        base_iterator(element_type *ptr) :
            ptr(ptr)
        {
            resetPtr(wrapper, ptr);
        }

        ~base_iterator() {}

        // prefix++
        /**
         * Advance to the next element.
         *
         * @return this iterator
         */
        base_iterator& operator++()
        {
            ptr = getNextPtr(ptr);
            resetPtr(wrapper, ptr);
            return *this;
        }

        // postfix++
        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        base_iterator operator++(int)
        {
            base_iterator tmp = *this;
            operator++();
            return tmp;
        }

        /**
         * Compare positions.
         *
         * @param rhs the iterator to compare with
         *
         * @return `true` when both point at the same element
         */
        bool  operator==(const base_iterator& rhs) const {return ptr == rhs.ptr;}
        /**
         * Compare positions.
         *
         * @param rhs the iterator to compare with
         *
         * @return `true` when the iterators point at different elements
         */
        bool  operator!=(const base_iterator& rhs) const {return !(ptr == rhs.ptr);}

        /**
         * Element at the current position.
         *
         * @return the wrapper of the element
         */
        WrapperType& operator*() noexcept
        {
            return wrapper;
        }

        /**
         * Element at the current position.
         *
         * @return a pointer to the wrapper of the element
         */
        WrapperType* operator->() noexcept
        {
            return &wrapper;
        }

    };

        /// Mutable iterator over the list.
    using iterator = base_iterator<false>;
        /// Read-only iterator over the list.
    using const_iterator = base_iterator<true>;

    /**
     * Create an empty wrapper that owns nothing.
     */
    LinkedListWrapper()
        : m_begin(0),
          m_end(0)
    {
    }

    /**
     * Take ownership of a list.
     *
     * @param begin the first element; the deleter is called on it by the
     *              destructor, so pass a list you own
     */
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


    /**
     * Whether the wrapper holds a list.
     *
     * @return `true` when the first element is not null
     */
    bool isValid()
    {
        return !!m_begin;
    }

    /**
     * The first element of the list.
     *
     * @return the raw pointer passed to the constructor, or nullptr
     */
    element_type *raw()
    {
        return m_begin;
    }

    /**
     * The first element of the list, read-only.
     *
     * @return the raw pointer passed to the constructor, or nullptr
     */
    const element_type *raw() const
    {
        return m_begin;
    }

    /**
     * First element.
     *
     * @return an iterator at the head of the list
     */
    iterator begin()
    {
        return iterator(m_begin);
    }


    /**
     * End of the list.
     *
     * @return the past-the-end iterator
     */
    iterator end()
    {
        return iterator(0);
    }


    /**
     * First element, read-only.
     *
     * @return a const iterator at the head of the list
     */
    const_iterator begin() const
    {
        return const_iterator(m_begin);
    }


    /**
     * End of the list, read-only.
     *
     * @return the past-the-end const iterator
     */
    const_iterator end() const
    {
        return const_iterator(0);
    }

    /**
     * Number of elements in the list.
     *
     * @return the element count
     *
     * @note O(n): the list is traversed from the head.
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
        /// Functor used to obtain the raw pointer of a wrapper.
    wrapper_cast_type  m_wrapperCast;
        /// First element of the list; owned when the deleter frees it.
    element_type      *m_begin;
        /// Last element the wrapper was constructed with.
    element_type      *m_end;

};
}

