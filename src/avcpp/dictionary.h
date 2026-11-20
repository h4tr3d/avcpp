#pragma once

/**
 * AVDictionary wrappers: Dictionary and DictionaryArray.
 *
 * @file
 * @ingroup core
 */

#include <tuple>
#include <memory>
#include <iterator>
#include <vector>
#include <type_traits>
#include <cassert>

extern "C" {
#include <libavutil/dict.h>
}

#include "averror.h"

#include "ffmpeg.h"

namespace av {

/**
 * Implements interface to access to the AVDictionary entity.
 *
 * It also add useful extensions, like:
 * - Iterator interface: you can walk via dictionary entries via foreach cycles
 * - Initialization list construction: you can initialize dictionary simple as structure or array
 *
 * This class also provide way to controll owning: you can create entity that does not owning data but
 * provide access to them. You can drop owning by release() call.
 */
class Dictionary : public FFWrapperPtr<AVDictionary>
{
public:
    /**
     * AVDictionary flags mapping
     */
    enum Flags
    {
        /// Do not ignore case
        FlagMatchCase       = AV_DICT_MATCH_CASE,
        /// Special case: Iterate via dictionary
        FlagIgnoreSuffix    = AV_DICT_IGNORE_SUFFIX,
        /// Do not duplicate string key. String must be duplicate before with av_strdup() or allocates
        /// with av_malloc(). Do not use new[] for it! This flag ignores for std::string keys.
        FlagDontStrdupKey   = AV_DICT_DONT_STRDUP_KEY,
        /// Do not duplicate string value. String must be duplicate before with av_strdup() or allocates
        /// with av_malloc(). Do not use new[] for it! This flag ignores for std::string values and
        /// for integer values
        FlagDontStrdupVal   = AV_DICT_DONT_STRDUP_VAL,
        /// Keep existing value if key already exists
        FlagDontOverwrite   = AV_DICT_DONT_OVERWRITE,
        /// Append value to the existing one (string concat)
        FlagAppend          = AV_DICT_APPEND,
    };

    /**
     * Deleter for raw string.
     *
     * Helper for string composer.
     */
    struct AvStringDeleter
    {
        /**
         * Free a string allocated by FFmpeg.
         *
         * @param ptr the string to free; it is reset to null through av_freep()
         */
        void operator()(char *ptr)
        {
            av_freep(&ptr);
        }
    };

    // Back compatible
    /// @cond SKIP
    using RawStringDeleter = AvStringDeleter;
    /// @endcond

    /**
     * RAII holder for strings allocated by FFmpeg internals.
     */
    struct AvStringPtr : public std::unique_ptr<char, AvStringDeleter>
    {
                /// The unique_ptr instantiation this holder builds on.
        using base_class = std::unique_ptr<char, AvStringDeleter>;
        using base_class::unique_ptr;

        /**
         * Access to the holded string with `std::string` interface
         * @return holded raw string pointer, or `nullptr` if string is not allocated
         */
        char* c_str() const noexcept
        {
            return get();
        }

        /**
         * RAW string length in bytes
         *
         * @note to avoid extra size and keeps `sizeof(AvStringPtr) == sizeof(char*)` we do not
         *       cache and holds string size. As a result string length calculates every call and
         *       complexity is `O(n)`, where `n` - count of bytes in string exclude end zero.
         *
         * 8-bit based multibyte string like UTF-8 must be processes with appropriate libraries
         * externally.
         *
         * @return calculated length of string in bytes
         */
        size_t length() const noexcept
        {
            return strlen(c_str());
        }

        /**
         * Array-like char access.
         *
         * This call avoid any checks. When `index` is greater then `length()` behavior undefined.
         *
         * @param index   char index to access
         * @return  char reference to read/modify.
         */
        char& operator[](size_t index) const noexcept
        {
            return c_str()[index];
        }
    };
    // Our contract
    static_assert (sizeof(AvStringPtr) == sizeof(char*), "Size must be same. Check and try again");
    /// @cond SKIP
    using RawStringPtr = AvStringPtr;
    /// @endcond

    // Fwd
    template<bool constIterator>
    class DictionaryIterator;

    /**
     * Dictionary key and value holder and accessor
     */
    class Entry
    {
    public:

        template<bool>
        friend class DictionaryIterator;
        friend class Dictionary;

        /**
         * @brief key - item key accessor
         * @return current item key, or null if entry null
         */
        const char* key() const noexcept;
        /**
         * @brief value - item value accessor
         * @return  current item value or null if entry null
         */
        const char* value() const noexcept;
        /**
         * @name Entry function-like value set inteface
         * @brief set - change value of the current item
         * @param value new value
         * @param flags access flags, FlagDontStrdupVal only accepted, other flags ignored
         */
        /// @{
        /**
         * Change the value of this entry.
         *
         * @param value the new value
         * @param flags `AV_DICT_DONT_STRDUP_VAL` is the only flag that applies here
         */
        void set(const char *value, int flags = 0) noexcept;
        /**
         * Change the value of this entry.
         *
         * @param value the new value
         * @param flags `AV_DICT_DONT_STRDUP_VAL` is the only flag that applies here
         */
        void set(const std::string& value, int flags = 0) noexcept;
        /// @}

        /**
         * @name Entry operator[]-like value set inteface
         * @brief Helper operators. Syntax shugar.
         * @param value  new item value. Always strduped. Use set() to be more flexibility.
         * @return referenct to this
         */
        /// @{
        /**
         * Assign a value through the entry holder.
         *
         * @param value the new value; it is copied
         *
         * @return this entry holder
         */
        Entry& operator=(const char *value) noexcept;
        /**
         * Assign a value through the entry holder.
         *
         * @param value the new value; it is copied
         *
         * @return this entry holder
         */
        Entry& operator=(const std::string &value) noexcept;
        /// @}

        /**
         * @brief operator bool - converts holder to bool
         * returns true if entry valid, false otherwise
         */
        operator bool() const;
        /**
         * @brief isNull - checks that entry invalid (null)
         * @return true if entry null, false otherwise
         */
        bool isNull() const;

        /**
         * Compare two entries.
         *
         * @param lhs the left entry
         * @param rhs the right entry
         *
         * @return `true` when both refer to the same dictionary element
         */
        friend bool operator==(const Entry& lhs, const Entry& rhs);
        /**
         * Compare two entries.
         *
         * @param lhs the left entry
         * @param rhs the right entry
         *
         * @return `true` when they refer to different elements
         */
        friend bool operator!=(const Entry& lhs, const Entry& rhs);

    private:
        AVDictionaryEntry *m_entry = nullptr;
    };


    /**
     * @brief Base dictionary iterator implementation
     */
    template<bool constIterator = false>
    class DictionaryIterator
    {
    public:
        // Iterator interface
                /// Standard iterator category tag.
        using iterator_category = std::forward_iterator_tag;
                /// Type produced by dereferencing.
        using value_type = typename std::conditional<constIterator, const Entry, Entry>::type;
                /// Distance type required by the iterator traits.
        using difference_type = ptrdiff_t;
                /// Pointer to the entry.
        using pointer = value_type*;
                /// Reference to the entry.
        using reference = value_type&;

    private:
        using DictType  = typename std::conditional<constIterator, const Dictionary, Dictionary>::type;
        using EntryType = typename std::conditional<constIterator, const Entry, Entry>::type;

        DictType &m_dict;
        Entry     m_entry;

    public:
        /**
         * Continue the iteration from the given entry.
         *
         * @param dict  the dictionary being iterated
         * @param entry the entry to continue from
         */
        DictionaryIterator(DictType &dict, const Entry& entry) noexcept
            : m_dict(dict),
              m_entry(entry)
        {
        }

        /**
         * Start the iteration at the first entry.
         *
         * @param dict the dictionary to iterate
         */
        DictionaryIterator(DictType &dict) noexcept
            : m_dict(dict)
        {
            operator++();
        }

        /**
         * Advance to the next entry.
         *
         * @return this iterator
         */
        DictionaryIterator& operator++() noexcept
        {
            m_entry.m_entry = av_dict_get(m_dict.raw(), "", m_entry.m_entry, FlagIgnoreSuffix);
            return *this;
        }

        /**
         * Advance, returning the previous position.
         *
         * @return a copy of the iterator before advancing
         */
        DictionaryIterator operator++(int) noexcept
        {
            DictionaryIterator tmp(*this);
            operator++();
            return tmp;
        }

        /**
         * Compare two iterators.
         *
         * @param lhs the left iterator
         * @param rhs the right iterator
         *
         * @return `true` when both point at the same entry
         */
        friend bool operator==(const DictionaryIterator& lhs, const DictionaryIterator& rhs) noexcept
        {
            return lhs.m_entry == rhs.m_entry;
        }

        /**
         * Compare two iterators.
         *
         * @param lhs the left iterator
         * @param rhs the right iterator
         *
         * @return `true` when they point at different entries
         */
        friend bool operator!=(const DictionaryIterator& lhs, const DictionaryIterator& rhs) noexcept
        {
            return lhs.m_entry != rhs.m_entry;
        }

        /**
         * Entry at the current position.
         *
         * @return the entry; modifying its value changes the dictionary
         */
        EntryType& operator*() noexcept
        {
            return m_entry;
        }

        /**
         * Entry at the current position.
         *
         * @return a pointer to the entry
         */
        EntryType* operator->() noexcept
        {
            return &m_entry;
        }

    };

    /**
     * @brief Non-const iterator
     */
    using Iterator      = DictionaryIterator<false>;
    /**
     * @brief Const iterator
     */
    using ConstIterator = DictionaryIterator<true>;

    /**
     * @brief Default ctor
     * do nothig, null dictionary creates
     */
    Dictionary();

    /**
     * @brief Assign ctor
     * Wrap around raw dictionary. Useful to iterate with low-level FFmpeg API
     * @param dict       dictionary to hold
     * @param takeOwning ownershipping flag. If false, Dictionary only wraps access to the AVDictionary
     *                   and do not free resources on destroy.
     */
    explicit Dictionary(AVDictionary *dict, bool takeOwning = true);

    /**
     * @brief Dtor
     * If Dictionary takes ownershipping on AVDictionary, it free allocated resources.
     */
    ~Dictionary();

    /**
     * @brief Copy ctor
     * Make deep copy of dictionary. Takes ownershipping on newly created dict.
     * @param other
     */
    Dictionary(const Dictionary& other);

    /**
     * @brief Copy assign operator
     * Make deep copy of dictionary. Takes ownershipping on newly created dict. Old dict destroyed
     * (resouces freed if there is ownershipping)
     * @param rhs the dictionary to copy
     *
     * @return this dictionary
     */
    Dictionary& operator=(const Dictionary &rhs);

    /**
     * @brief Move ctor
     * Takes resources. Other dictionary moves to uninited state. Ownershpping same to other.
     * @param other
     */
    Dictionary(Dictionary&& other);

    /**
     * @brief Move assign operator
     * Takes resources. Rhs dictionary moves to uninited state. Ownershpping same to rhs. Old
     * dict destroyed (resouces freed if there is ownershipping)
     * @param rhs the dictionary to move from
     *
     * @return this dictionary
     */
    Dictionary& operator=(Dictionary&& rhs);

    /**
     * @brief Initializer ctor
     * Allows create dictionaries in array manier:
     * @code
     * Dictionary dict = {
     *   {"key1", "value1"},
     *   {"key2", "value2"},
     * };
     * @endcode
     *
     * New dictionary owning resources.
     *
     * @param list   init list
     * @param flags  see @ref Flags
     */
    Dictionary(std::initializer_list<std::pair<const char*, const char*>> list, int flags = 0);

    /**
     * @brief Initializer assign operator
     * Allows assign dictionaries in array manier:
     * @code
     * Dictionary dict;
     * ...
     * dict = {
     *   {"key1", "value1"},
     *   {"key2", "value2"},
     * };
     * @endcode
     *
     * New dictionary owning resources. Old dictionary destroes (resource freed if owning)
     *
     * @param list   init list
     *
     * @return this dictionary
     */
    Dictionary& operator=(std::initializer_list<std::pair<const char*, const char*>> list);

    /**
     * @name Iterator interface
     */
    ///@{
    /**
     * First entry.
     *
     * @return an iterator at the first entry, or end() when the dictionary is
     *         empty
     */
    Iterator      begin();
    /**
     * End of the iteration.
     *
     * @return the past-the-end iterator
     */
    Iterator      end();
    /**
     * First entry, read-only.
     *
     * @return a const iterator at the first entry
     */
    ConstIterator begin() const;
    /**
     * End of the read-only iteration.
     *
     * @return the past-the-end const iterator
     */
    ConstIterator end() const;
    /**
     * First entry, read-only, for explicit const traversal.
     *
     * @return a const iterator at the first entry
     */
    ConstIterator cbegin() const;
    /**
     * End of the read-only traversal.
     *
     * @return the past-the-end const iterator
     */
    ConstIterator cend() const;
    ///@}

    /**
     * @brief isOwning - checks resources owning status
     * @return  true if object owning resources
     */
    bool isOwning() const noexcept;

    /**
     * @brief assign - assign new resouces
     * Old dictionary destroyes (if owning). If dict same to the olready holding one, but takeOwning
     * is true and isOwning() false - takes owning. Otherwise do nothing.
     * @param dict       dictionary to hold
     * @param takeOwning owning flag
     */
    void assign(AVDictionary *dict, bool takeOwning = true) noexcept;

    /**
     * @name Index based access operator
     * @brief operator [] - access to the entry via index
     * O(1) complexity.
     * @param index
     * @return
     */
    /// @{
    /**
     * Entry value by position.
     *
     * @param index zero-based position; the order is the dictionary order
     *
     * @return the value, or nullptr when the index is out of range
     *
     * @note O(1): the entries are counted from the head of the list.
     */
    const char* operator[](size_t index) const;
    /**
     * Entry holder by position, for assignment.
     *
     * @param index zero-based position
     *
     * @return an entry holder; assigning to it changes the value
     *
     * @note O(1).
     */
    Entry       operator[](size_t index);
    /// @}

    /**
     * @name Key based access operator
     * @brief operator [] - access to  the entry via key
     * O(n) complexity.
     * @param key
     * @return
     */
    /// @{
    /**
     * Entry value by key.
     *
     * @param key the key to look up
     *
     * @return the value, or nullptr when the key is absent
     *
     * @note O(n): the dictionary is a linked list.
     */
    const char* operator[](const char* key) const;
    /**
     * Entry holder by key, for assignment.
     *
     * @param key the key to look up
     *
     * @return an entry holder; assigning to it sets the value
     *
     * @note O(n).
     */
    Entry       operator[](const char* key) noexcept;
    /// @}

    /**
     * @name Entries counting
     * @brief count/size - returns count of dictionary entries.
     * O(1) complexity.
     * @return
     */
    /// @{
    /**
     * Number of entries.
     *
     * @return the entry count
     *
     * @note O(1).
     */
    size_t      count() const noexcept;
    /**
     * Number of entries.
     *
     * @return the entry count
     */
    size_t      size() const noexcept;
    /// @}

    /**
     * @name Key based getter interface
     * @brief get - gets value by key
     * O(n) complexity.
     * @param key     key for entry access
     * @param flags   see @ref Flags
     * @return nullptr if key does not present
     */
    /// @{
    /**
     * Value for a key.
     *
     * @param key   the key to look up
     * @param flags a combination of `AV_DICT_*` lookup flags
     *
     * @return the value, or nullptr when the key is absent
     */
    const char *get(const char* key, int flags = 0) const noexcept;
    /**
     * Value for a key.
     *
     * @param key   the key to look up
     * @param flags a combination of `AV_DICT_*` lookup flags
     *
     * @return the value, or nullptr when the key is absent
     */
    const char *get(const std::string& key, int flags = 0) const noexcept;
    /// @}


    /**
     * @brief set - set new value for key or add new dictionary item (or create dictionary)
     * If dictionary empty (null), it takes ownershipping too.
     *
     * @param[in]     key    key to change (can be std::string or char*)
     * @param[in]     value  key value (can be std::string, char* or integer)
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @param[in]     flags  see @ref Flags
     *
     * @return nothing; errors are reported through `ec` or an exception
     */
    template<typename Key, typename Value = Key>
    auto set(const Key& key, const Value& value, OptionalErrorCode ec = throws(), int flags = 0) ->
         typename std::enable_if
         <
             (std::is_same<Key, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Key>::type>::type, char*>::value) &&
             (std::is_same<Value, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Value>::type>::type, char*>::value || std::is_integral<Value>::value)
             ,void
         >::type
    {
        clear_if(ec);
        int sts;
        if ((sts = set_priv(key, value, flags)) < 0) {
            throws_if(ec, sts, ffmpeg_category());
        }
    }

    /**
     * @brief parseString - process string with options and fill dictionary
     * String examples:
     * @code
     * foo=bar;foo2=bar2
     * foo:bar&foo2:bar2
     * @endcode
     *
     * @param str       string to process
     * @param keyValSep null-terminated string with chars that interprets as key and value separators
     *                  '=' and ':' in most cases.
     * @param pairsSep  null-terminates string with chars that interprets as pairs (key and value)
     *                  separators. ';' and ',' in most cases.
     * @param flags     See Flags. All flags that omit strdups ignores.
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     */
    template<typename Str, typename Sep1, typename Sep2>
    void parseString(const Str& str, const Sep1& keyValSep, const Sep2& pairsSep, int flags = 0, OptionalErrorCode ec = throws())
    {
        parseString_priv(ec,
                         _to_const_char_ptr(str),
                         _to_const_char_ptr(keyValSep),
                         _to_const_char_ptr(pairsSep),
                         flags);
    }

    /**
     * @brief toString - converts dictionary to the string representation (serialize)
     *
     * This line can be processed via parseString() later.
     *
     * FFmpeg internaly allocated buffer copies to the string and freed.
     *
     * @param[in]  keyValSep   char to separate key and value
     * @param[in] pairsSep    chat to separate key-value pairs.
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     * @return valid string, null-string (std::string()) on error
     *
     * @note \\0 and same separator chars unapplicable.
     *
     */
    std::string toString(const char keyValSep, const char pairsSep, OptionalErrorCode ec = throws()) const;

    /**
     * @brief toRawStringPtr - converts dictionary to the raw string (char*) and protect it with
     * smart pointer (std::unique_ptr).
     *
     * This method omit data copy and returns raw pointer that allocated by av_dict_get_string(). For
     * more safety this block wrapped with no-overhead smart pointer (std::unique_ptr).
     *
     * @see toString()
     *
     * @param[in]  keyValSep   char to separate key and value
     * @param[in]  pairsSep    chat to separate key-value pairs.
     * @param[in,out] ec     this represents the error status on exit, if this is pre-initialized to
     *                       av#throws the function will throw on error instead
     *
     * @return valid string, null on error (check ec)
     */
    AvStringPtr toRawStringPtr(const char keyValSep, const char pairsSep, OptionalErrorCode ec = throws()) const;

    /**
     * @brief copyFrom - copy data from other dictionary.
     *
     * If dictionary already exists, new fields will be addred. Entries with same keys will be
     * overrided according to flags.
     *
     * @param other   dict to copy from
     * @param flags   see @ref Flags
     */
    void copyFrom(const Dictionary& other, int flags = 0) noexcept;

    /**
     * @brief swap - swaps resouces between objects
     * @param other
     */
    void swap(Dictionary& other) noexcept;

        /**
         * @brief release - drops ownershipping
         * @return
         */
    /**
     * Give up ownership of the dictionary.
     *
     * @return the raw dictionary, which the caller must free; the wrapper is
     *         left empty and no longer owning
     */
    AVDictionary* release();


    /**
     * Address of the internal pointer, for FFmpeg calls that take `AVDictionary**`.
     *
     * @return a pointer to the stored pointer
     *
     * @note Pass it to functions such as av_dict_set() and av_dict_copy(); they
     *       may allocate a dictionary when the stored pointer is null.
     */
    AVDictionary **rawPtr() noexcept;

private:
    // Discard access to the reset() method
    using FFWrapperPtr<AVDictionary>::reset;

    int set_priv(const char* key, const char *value, int flags = 0) noexcept;
    int set_priv(const std::string& key, const std::string& value, int flags = 0) noexcept;
    int set_priv(const char* key, int64_t value, int flags = 0) noexcept;
    int set_priv(const std::string& key, int64_t value, int flags = 0) noexcept;

    const char* _to_const_char_ptr(const char *str)
    {
        return str;
    }

    const char* _to_const_char_ptr(const std::string &str)
    {
        return str.c_str();
    }

    int  parseString_priv(const char* str, const char* keyvalSep, const char* pairsSep, int flags);
    void parseString_priv(OptionalErrorCode ec, const char* str, const char* keyvalSep, const char* pairsSep, int flags);


private:
    bool m_owning = true;
};


/**
 * Array of Dictinaries wrapper.
 *
 * Some functions accepts array of dictionaries. This class is a simple proxy to compose such
 * arrays.
 *
 * It is not iterator-friendly and must not be used generally.
 */
class DictionaryArray
{
public:

    DictionaryArray() = default;
    /**
     * Build an array from a list of dictionaries.
     *
     * @param dicts the dictionaries; they are copied into the array
     */
    DictionaryArray(std::initializer_list<Dictionary> dicts);
    /**
     * Wrap an array of raw dictionaries.
     *
     * @param dicts      the array; it is copied, the dictionaries are not
     * @param count      number of elements
     * @param takeOwning `true` frees the dictionaries on destruction
     */
    DictionaryArray(AVDictionary **dicts, size_t count, bool takeOwning = true);

    /**
     * Copy the array; each dictionary is copied deeply.
     *
     * @param rhs the array to copy
     */
    DictionaryArray(const DictionaryArray& rhs);
    /**
     * Assign from another array; each dictionary is copied deeply.
     *
     * @param rhs the array to copy
     *
     * @return this array
     */
    DictionaryArray& operator=(const DictionaryArray& rhs);

    /**
     * Move the array; the source is left empty.
     */
    DictionaryArray(DictionaryArray&&) = default;
    /**
     * Move assignment; the source is left empty.
     *
     * @return this array
     */
    DictionaryArray& operator=(DictionaryArray&&) = default;

    /**
     * Replace the contents with an array of raw dictionaries.
     *
     * @param dicts      the array to wrap; existing contents are released
     * @param count      number of elements
     * @param takeOwning `true` frees the dictionaries on destruction
     */
    void assign(AVDictionary **dicts, size_t count, bool takeOwning = true);
    /**
     * Reserve storage for the given number of dictionaries.
     *
     * @param size expected element count
     */
    void reserve(size_t size);
    /**
     * Resize the array, adding empty dictionaries at the end.
     *
     * @param size the new element count
     */
    void resize(size_t size);

    /**
     * Number of dictionaries in the array.
     *
     * @return the element count
     */
    size_t size() const;

    /**
     * Append a copy of a dictionary.
     *
     * @param dict the dictionary to copy into the array
     */
    void pushBack(const Dictionary &dict);
    /**
     * Append a dictionary, moving it.
     *
     * @param dict the dictionary to move into the array
     */
    void pushBack(Dictionary &&dict);

    /**
     * Dictionary by index.
     *
     * @param index zero-based index
     *
     * @return the dictionary; the array owns it
     */
    const Dictionary& operator[](size_t index) const;
    /**
     * Dictionary by index, mutable.
     *
     * @param index zero-based index
     *
     * @return the dictionary; the array owns it
     */
    Dictionary&       operator[](size_t index);
    /**
     * Dictionary by index with bounds checking.
     *
     * @param index zero-based index; must be less than size()
     *
     * @return the dictionary
     */
    const Dictionary& at(size_t index) const;
    /**
     * Dictionary by index with bounds checking.
     *
     * @param index zero-based index; must be less than size()
     *
     * @return the dictionary
     */
    Dictionary&       at(size_t index);

    /**
     * The raw array, read-only.
     *
     * @return a pointer to the internal array of dictionaries
     *
     * @note Valid until the array is modified.
     */
    const AVDictionary* const* raws() const;
    /**
     * The raw array, mutable.
     *
     * @return a pointer to the internal array of dictionaries
     *
     * @note Valid until the array is modified.
     */
    AVDictionary**             raws();

    /**
     * Give up ownership of the dictionaries.
     *
     * @return the raw array, which the caller must free
     */
    AVDictionary** release();
    /**
     * Exchange the contents of two arrays.
     *
     * @param rhs the array to swap with
     */
    void swap(DictionaryArray &rhs);
private:
    std::vector<AVDictionary*> m_raws;
    std::vector<Dictionary>    m_dicts;
};

} // namespace av

