#ifndef AV_DICTIONARY_H
#define AV_DICTIONARY_H

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
 * @brief The Dictionary class
 *
 * Implements interface to access to the AVDictionary entity
 *
 * It also add useful extensions, like:
 * - Iterator interface: you can walk via dictionary entries via foreach cycles
 * - Initialization list construction: you can initialize dictionary simple as structure or array
 *
 * This class also provide way to controll owning: you can create entity that does not owning data but
 * provide access to them. You can drop owning by release() call.
 *
 */
class Dictionary : public FFWrapperPtr<AVDictionary>
{
public:
    /**
     * @brief The Flags enum
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
     * @brief The RawStringDeleter struct
     * Deleter for raw string. Helper for string composer
     */
    struct RawStringDeleter
    {
        void operator()(char *&ptr)
        {
            av_freep(&ptr);
        }
    };

    /// RAW string implementation
    using RawStringPtr = std::unique_ptr<char, RawStringDeleter>;

    // Fwd
    template<bool constIterator>
    class DictionaryIterator;

    /**
     * @brief The Entry class
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
         * @brief set - change value of the current item
         * @param value new value
         * @param flags access flags, FlagDontStrdupVal only accepted, other flags ignored
         */
        /// @{
        void set(const char *value, int flags = 0) noexcept;
        void set(const std::string& value, int flags = 0) noexcept;
        /// @}

        /**
         * @brief Helper operators. Syntax shugar.
         * @param value  new item value. Always strduped. Use set() to be more flexibility.
         * @return referenct to this
         */
        /// @{
        Entry& operator=(const char *value) noexcept;
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

        friend bool operator==(const Entry& lhs, const Entry& rhs);
        friend bool operator!=(const Entry& lhs, const Entry& rhs);

    private:
        AVDictionaryEntry *m_entry = nullptr;
    };


    /**
     * @brief Base dictionary iterator implementation
     */
    template<bool constIterator = false>
    class DictionaryIterator :
            public std::iterator<std::forward_iterator_tag, typename std::conditional<constIterator, const Entry, Entry>::type>
    {
    private:
        using DictType  = typename std::conditional<constIterator, const Dictionary, Dictionary>::type;
        using EntryType = typename std::conditional<constIterator, const Entry, Entry>::type;

        DictType &m_dict;
        Entry     m_entry;

    public:
        DictionaryIterator(DictType &dict, const Entry& entry) noexcept
            : m_dict(dict),
              m_entry(entry)
        {
        }

        DictionaryIterator(DictType &dict) noexcept
            : m_dict(dict)
        {
            operator++();
        }

        DictionaryIterator& operator++() noexcept
        {
            m_entry.m_entry = av_dict_get(m_dict.raw(), "", m_entry.m_entry, FlagIgnoreSuffix);
            return *this;
        }

        DictionaryIterator operator++(int) noexcept
        {
            DictionaryIterator tmp(*this);
            operator++();
            return tmp;
        }

        friend bool operator==(const DictionaryIterator& lhs, const DictionaryIterator& rhs) noexcept
        {
            return lhs.m_entry == rhs.m_entry;
        }

        friend bool operator!=(const DictionaryIterator& lhs, const DictionaryIterator& rhs) noexcept
        {
            return lhs.m_entry != rhs.m_entry;
        }

        EntryType& operator*() noexcept
        {
            return m_entry;
        }

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
     * @param rhs
     * @return
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
     * @param rhs
     * @return
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
     * @param flags  see Flags
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
     * @param flags  see Flags
     */
    Dictionary& operator=(std::initializer_list<std::pair<const char*, const char*>> list);

    /**
     * @brief Iterator interface
     */
    /// @{
    Iterator      begin();
    Iterator      end();
    ConstIterator begin() const;
    ConstIterator end() const;
    ConstIterator cbegin() const;
    ConstIterator cend() const;
    /// @}

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
     * @brief operator [] - access to the entry via index
     * O(1) complexity.
     * @param index
     * @return
     */
    /// @{
    const char* operator[](size_t index) const throw(AvException);
    Entry       operator[](size_t index) throw(AvException);
    /// @}

    /**
     * @brief operator [] - access to  the entry via key
     * O(n) complexity.
     * @param key
     * @return
     */
    /// @{
    const char* operator[](const char* key) const throw(AvException);
    Entry       operator[](const char* key) noexcept;
    /// @}

    /**
     * @brief count/size - returns count of dictionary entries.
     * O(1) complexity.
     * @return
     */
    /// @{
    size_t      count() const noexcept;
    size_t      size() const noexcept;
    /// @

    /**
     * @brief get - gets value by key
     * O(n) complexity.
     * @param key
     * @param flags - see Flags
     * @return nullptr if key does not present
     */
    /// @{
    const char *get(const char* key, int flags = 0) const noexcept;
    const char *get(const std::string& key, int flags = 0) const noexcept;
    /// @}

    /**
     * @brief set - set new value for key or add new dictionary item (or create dictionary)
     * If dictionary empty (null), it takes ownershipping too.
     *
     * On error throws exception with error code;
     *
     * @param key    key to change (can be std::string or char*)
     * @param value  key value (can be std::string, char* or integer)
     * @param flags  see Flags
     *
     */
    template<typename Key, typename Value = Key>
    typename std::enable_if
    <
      (std::is_same<Key, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Key>::type>::type, char*>::value) &&
      (std::is_same<Value, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Value>::type>::type, char*>::value || std::is_integral<Value>::value)
      ,void
    >::type
    set(const Key& key, const Value& value, int flags = 0) throw(AvException)
    {
        std::error_code ec;
        set<Key, Value>(ec, key, value, flags);
        if (ec)
            throw_error_code(ec);
    }

    /**
     * @brief set - set new value for key or add new dictionary item (or create dictionary)
     * If dictionary empty (null), it takes ownershipping too.
     *
     * @param ec     error code
     * @param key    key to change (can be std::string or char*)
     * @param value  key value (can be std::string, char* or integer)
     * @param flags  see Flags
     *
     */
    template<typename Key, typename Value = Key>
    typename std::enable_if
    <
      (std::is_same<Key, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Key>::type>::type, char*>::value) &&
      (std::is_same<Value, std::string>::value || std::is_same<typename std::remove_cv<typename std::decay<Value>::type>::type, char*>::value || std::is_integral<Value>::value)
      ,void
    >::type
    set(std::error_code &ec, const Key& key, const Value& value, int flags = 0) noexcept
    {
        ec = make_error_code(AvError::NoError);
        int sts;
        if ((sts = set_priv(key, value, flags)) < 0) {
            ec = make_ffmpeg_error(sts);
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
     * This version throw AvException with error code on error.
     *
     * @param str       string to process
     * @param keyValSep null-terminated string with chars that interprets as key and value separators
     *                  '=' and ':' in most cases.
     * @param pairsSep  null-terminates string with chars that interprets as pairs (key and value)
     *                  separators. ';' and ',' in most cases.
     * @param flags     See Flags. All flags that omit strdups ignores.
     * @return 0 on success, <0 on fail
     */
    template<typename Str, typename Sep1, typename Sep2>
    void parseString(const Str& str, const Sep1& keyvalSep, const Sep2& pairsSep, int flags = 0) throw(AvException)
    {
        std::error_code ec;
        parseString(ec, str, keyvalSep, pairsSep, flags);
        if (ec)
            throw_error_code(ec);
    }

    /**
     * @brief parseString - process string with options and fill dictionary
     * String examples:
     * @code
     * foo=bar;foo2=bar2
     * foo:bar&foo2:bar2
     * @endcode
     *
     * @param ec        error code storage, AvError::NoError on success
     * @param str       string to process
     * @param keyValSep null-terminated string with chars that interprets as key and value separators
     *                  '=' and ':' in most cases.
     * @param pairsSep  null-terminates string with chars that interprets as pairs (key and value)
     *                  separators. ';' and ',' in most cases.
     * @param flags     See Flags. All flags that omit strdups ignores.
     * @return 0 on success, <0 on fail
     */
    template<typename Str, typename Sep1, typename Sep2>
    void parseString(std::error_code &ec, const Str& str, const Sep1& keyvalSep, const Sep2& pairsSep, int flags = 0) throw(AvException)
    {
        parseString_priv(ec,
                         _to_const_char_ptr(str),
                         _to_const_char_ptr(keyvalSep),
                         _to_const_char_ptr(pairsSep),
                         flags);
    }

    /**
     * @brief toString - converts dictionary to the string representation (serialize)
     *
     * This line can be processed via parseString() later.
     *
     * On error AvException with error code thrown.
     *
     * FFmpeg internaly allocated buffer copies to the string and freed.
     *
     * @param keyValSep   char to separate key and value
     * @param pairsSep    chat to separate key-value pairs.
     * @return valid string
     *
     * @note \\0 and same separator chars unapplicable.
     *
     */
    std::string toString(const char keyValSep, const char pairsSep) const throw(AvException);

    /**
     * @brief toString - converts dictionary to the string representation (serialize)
     *
     * This line can be processed via parseString() later.
     *
     * FFmpeg internaly allocated buffer copies to the string and freed.
     *
     * @param[out] ec          error code holder
     * @param[in]  keyValSep   char to separate key and value
     * @param[in] pairsSep    chat to separate key-value pairs.
     * @return valid string, null-string (std::string()) on error
     *
     * @note \\0 and same separator chars unapplicable.
     *
     */
    std::string toString(std::error_code &ec, const char keyValSep, const char pairsSep) const noexcept;

    /**
     * @brief toRawStringPtr - converts dictionary to the raw string (char*) and protect it with
     * smart pointer (std::unique_ptr).
     *
     * This method omit data copy and returns raw pointer that allocated by av_dict_get_string(). For
     * more safety this block wrapped with no-overhead smart pointer (std::unique_ptr).
     *
     * On error AvException thrown.
     *
     * @see toString()
     *
     * @param[in]  keyValSep   char to separate key and value
     * @param[in] pairsSep    chat to separate key-value pairs.
     *
     * @return valid string
     */
    RawStringPtr toRawStringPtr(const char keyValSep, const char pairsSep) const throw(AvException);

    /**
     * @brief toRawStringPtr - converts dictionary to the raw string (char*) and protect it with
     * smart pointer (std::unique_ptr).
     *
     * This method omit data copy and returns raw pointer that allocated by av_dict_get_string(). For
     * more safety this block wrapped with no-overhead smart pointer (std::unique_ptr).
     *
     * @see toString()
     *
     * @param[out] ec          error code holder
     * @param[in]  keyValSep   char to separate key and value
     * @param[in]  pairsSep    chat to separate key-value pairs.
     *
     * @return valid string, null on error (check ec)
     */
    RawStringPtr toRawStringPtr(std::error_code &ec, const char keyValSep, const char pairsSep) const noexcept;

    /**
     * @brief copyFrom - copy data from other dictionary.
     *
     * If dictionary already exists, new fields will be addred. Entries with same keys will be
     * overrided according to flags.
     *
     * @param other   dict to copy from
     * @param flags   see Flags
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
    AVDictionary* release();


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
    void parseString_priv(std::error_code &ec, const char* str, const char* keyvalSep, const char* pairsSep, int flags);


private:
    bool m_owning = true;
};


/**
 * @brief The DictionaryArray class
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
    DictionaryArray(std::initializer_list<Dictionary> dicts);
    DictionaryArray(AVDictionary **dicts, size_t count, bool takeOwning = true);

    DictionaryArray(const DictionaryArray& rhs);
    DictionaryArray& operator=(const DictionaryArray& rhs);

    DictionaryArray(DictionaryArray&&) = default;
    DictionaryArray& operator=(DictionaryArray&&) = default;

    void assign(AVDictionary **dicts, size_t count, bool takeOwning = true);
    void reserve(size_t size);
    void resize(size_t size);

    size_t size() const;

    void pushBack(const Dictionary &dict);
    void pushBack(Dictionary &&dict);

    const Dictionary& operator[](size_t index) const;
    Dictionary&       operator[](size_t index);
    const Dictionary& at(size_t index) const;
    Dictionary&       at(size_t index);

    const AVDictionary* const* raws() const;
    AVDictionary**             raws();

    AVDictionary** release();
    void swap(DictionaryArray &rhs);
private:
    std::vector<AVDictionary*> m_raws;
    std::vector<Dictionary>    m_dicts;
};

} // namespace av

#endif // AV_DICTIONARY_H
