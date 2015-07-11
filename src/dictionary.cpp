#include <iostream>

#include "dictionary.h"

using namespace std;

namespace av {

Dictionary::Dictionary()
{
}

Dictionary::Dictionary(AVDictionary * dict, bool takeOwning)
    : FFWrapperPtr<AVDictionary>(dict),
      m_owning(takeOwning)
{
}

Dictionary::~Dictionary()
{
    if (m_owning && m_raw)
    {
        av_dict_free(&m_raw);
    }
}

Dictionary::Dictionary(const Dictionary & other)
    : FFWrapperPtr<AVDictionary>()
{
    copyFrom(other);
}

Dictionary &Dictionary::operator=(const Dictionary & rhs)
{
    if (this != &rhs)
    {
        Dictionary(rhs).swap(*this);
    }
    return *this;
}

Dictionary::Dictionary(Dictionary &&other)
    : Dictionary(other.m_raw, other.m_owning)
{
    other.m_owning = true;
    other.m_raw    = nullptr;
}

Dictionary& Dictionary::operator=(Dictionary&& rhs)
{
    if (this != &rhs)
    {
        Dictionary(std::move(rhs)).swap(*this);
    }
    return *this;
}

Dictionary::Dictionary(std::initializer_list<std::pair<const char *, const char *>> list, int flags)
{
    for (auto const &item : list)
    {
        set(std::get<0>(item), std::get<1>(item), flags);
    }
}

Dictionary &Dictionary::operator=(std::initializer_list<std::pair<const char *, const char *>> list)
{
    Dictionary(std::move(list)).swap(*this);
    return *this;
}

Dictionary::Iterator Dictionary::begin()
{
    return Iterator(*this);
}

Dictionary::Iterator Dictionary::end()
{
    return Iterator(*this, Entry());
}

Dictionary::ConstIterator Dictionary::begin() const
{
    return ConstIterator(*this);
}

Dictionary::ConstIterator Dictionary::end() const
{
    return ConstIterator(*this, Entry());
}

Dictionary::ConstIterator Dictionary::cbegin() const
{
    return ConstIterator(*this);
}

Dictionary::ConstIterator Dictionary::cend() const
{
    return ConstIterator(*this, Entry());
}

bool Dictionary::isOwning() const noexcept
{
    return m_owning;
}

void Dictionary::assign(AVDictionary *dict, bool takeOwning) noexcept
{
    if (raw() == dict) {
        if (!m_owning && takeOwning) {
            m_owning = takeOwning;
        }
        return;
    }

    Dictionary(dict, takeOwning).swap(*this);
}

const char *Dictionary::operator[](size_t index) const throw(std::out_of_range)
{
    if (index >= count())
        throw std::out_of_range("AVDictionary index out of range");

    // Take a first element
    auto entry = av_dict_get(m_raw, "", nullptr, FlagIgnoreSuffix);
    if (!entry)
        return nullptr;
    entry += index; // switch to next element
    return entry->value;
}

Dictionary::Entry Dictionary::operator[](size_t index) throw(std::out_of_range)
{
    Entry holder;
    if (index >= count())
        throw std::out_of_range("AVDictionary index out of range");

    // Take a first element
    auto entry = av_dict_get(m_raw, "", nullptr, FlagIgnoreSuffix);
    if (!entry)
        return holder;
    entry += index; // switch to next element
    holder.m_entry = entry;
    return holder;

}

const char *Dictionary::operator[](const char *key) const noexcept
{
    auto entry = av_dict_get(m_raw, key, nullptr, 0);
    if (!entry)
        return nullptr;
    return entry->value;
}

Dictionary::Entry Dictionary::operator[](const char *key) noexcept
{
    Entry holder;
    auto  prev = m_raw;
    auto entry = av_dict_get(m_raw, key, nullptr, 0);
    if (entry) {
        holder.m_entry = entry;
        return holder;
    }

    if (av_dict_set(&m_raw, key, "", 0) < 0) {
        return holder;
    }

    if (prev == nullptr) {
        m_owning = true;
    }

    // we can optimize here: new key will be last in array, so we can quickly access to them
    entry = av_dict_get(m_raw, "", nullptr, FlagIgnoreSuffix);
    if (!entry)
        return holder;
    entry += count()-1;
    holder.m_entry = entry;
    return holder;
}

size_t Dictionary::count() const noexcept
{
    return static_cast<size_t>(av_dict_count(m_raw));
}

size_t Dictionary::size() const noexcept
{
    return count();
}

const char *Dictionary::get(const char * key, int flags) const noexcept
{
    auto entry = av_dict_get(m_raw, key, nullptr, flags);
    if (entry)
    {
        return entry->value;
    }
    return nullptr;
}

const char *Dictionary::get(const std::string & key, int flags) const noexcept
{
    return get(key.c_str(), flags);
}

int Dictionary::set(const char * key, const char * value, int flags) noexcept
{
    auto prev = m_raw;
    int sts = av_dict_set(&m_raw, key, value, flags);
    if (sts >= 0 && prev == nullptr) {
        m_owning = true;
    }
    return sts;
}

int Dictionary::set(const std::string & key, const std::string & value, int flags) noexcept
{
    // We MUST dup this keys/vals
    flags &= ~(FlagDontStrdupKey|FlagDontStrdupVal);
    return set(key.c_str(), value.c_str(), flags);
}

int Dictionary::set(const char * key, int64_t value, int flags) noexcept
{
    auto prev = m_raw;
    int sts = av_dict_set_int(&m_raw, key, value, flags);
    if (sts >= 0 && prev == nullptr) {
        m_owning = true;
    }
    return sts;
}

int Dictionary::set(const std::string & key, int64_t value, int flags) noexcept
{
    // We MUST dup this key
    flags &= ~(FlagDontStrdupKey);
    return set(key.c_str(), value, flags);
}

int Dictionary::parseString(const char * str, const char * keyValSep, const char * pairsSep, int flags) noexcept
{
    return av_dict_parse_string(&m_raw, str, keyValSep, pairsSep, flags);
}

int Dictionary::parseString(const std::string & str, const std::string & keyValSep, const std::string & pairsSep, int flags) noexcept
{
    return parseString(str.c_str(), keyValSep.c_str(), pairsSep.c_str(), flags);
}

int Dictionary::getString(std::string &str, const char keyValSep, const char pairsSep) const noexcept
{
    char *buf = nullptr;
    int sts = av_dict_get_string(m_raw, &buf, keyValSep, pairsSep);
    if (sts >= 0)
    {
        str = std::move(std::string(buf));
        av_freep(&buf);
    }
    else
    {
        str = std::string();
    }
    return sts;
}

int Dictionary::getString(RawStringPtr &ptr, const char keyValSep, const char pairsSep) const noexcept
{
    char *buf = nullptr;
    int sts = av_dict_get_string(m_raw, &buf, keyValSep, pairsSep);
    if (sts >= 0)
    {
        ptr.reset(buf);
    }
    else
    {
        ptr.reset();
    }
    return sts;
}

void Dictionary::copyFrom(const Dictionary & other, int flags) noexcept
{
    av_dict_copy(&m_raw, other.m_raw, flags);
}

void Dictionary::swap(Dictionary & other) noexcept
{
    using std::swap;
    swap(m_raw,    other.m_raw);
    swap(m_owning, other.m_owning);
}

AVDictionary *Dictionary::release()
{
    m_owning = false;
    auto ptr = m_raw;
    m_raw = nullptr;
    return ptr;
}

AVDictionary **Dictionary::rawPtr() noexcept
{
    return &m_raw;
}

const char *Dictionary::Entry::key() const noexcept
{
    return (m_entry ? m_entry->key : nullptr);
}

const char *Dictionary::Entry::value() const noexcept
{
    return (m_entry ? m_entry->value : nullptr);
}

int Dictionary::Entry::set(const char * value, int flags) noexcept
{
    if (!m_entry)
        return 0; // TODO: return error?

    av_freep(&m_entry->value);
    if (flags & FlagDontStrdupVal)
    {
        m_entry->value = const_cast<char*>(value);
    }
    else
    {
        m_entry->value = av_strdup(value);
    }
    return 0;
}

int Dictionary::Entry::set(const std::string & value, int flags) noexcept
{
    flags &= ~FlagDontStrdupVal;
    return set(value.c_str(), flags);
}

bool Dictionary::Entry::isNull() const
{
    return m_entry == nullptr;
}

Dictionary::Entry &Dictionary::Entry::operator=(const string & value) noexcept
{
    set(value);
    return *this;
}

Dictionary::Entry &Dictionary::Entry::operator=(const char * value) noexcept
{
    set(value);
    return *this;
}

av::Dictionary::Entry::operator bool() const
{
    return m_entry != nullptr;
}

bool operator==(const Dictionary::Entry & lhs, const Dictionary::Entry & rhs)
{
    return lhs.m_entry == rhs.m_entry;
}

bool operator!=(const Dictionary::Entry & lhs, const Dictionary::Entry & rhs)
{
    return lhs.m_entry != rhs.m_entry;
}

DictionaryArray::DictionaryArray(std::initializer_list<Dictionary> dicts)
{
    m_raws.reserve(dicts.size());
    m_dicts.reserve(dicts.size());
    for (auto& d : dicts)
    {
        m_dicts.push_back(std::move(d));
        m_raws.push_back(m_dicts.end()->raw());
    }
}

DictionaryArray::DictionaryArray(AVDictionary ** dicts, size_t count, bool takeOwning)
{
    assign(dicts, count, takeOwning);
}

DictionaryArray::DictionaryArray(const DictionaryArray &rhs)
    : m_dicts(rhs.m_dicts)
{
    m_raws.reserve(m_dicts.size());
    for (auto &dict : m_dicts)
    {
        m_raws.push_back(dict.raw());
    }
}

DictionaryArray &DictionaryArray::operator=(const DictionaryArray &rhs)
{
    if (this != &rhs)
    {
        DictionaryArray(rhs).swap(*this);
    }
    return *this;
}

void DictionaryArray::assign(AVDictionary **dicts, size_t count, bool takeOwning)
{
    if (m_raws.data() != dicts)
    {
        m_raws.clear();
        m_raws.resize(count);
    }
    else
        count = m_raws.size();

    m_dicts.clear();
    m_dicts.reserve(count);

    for (size_t i = 0; i < count; ++i)
    {
        m_raws[i] = dicts[i];
        m_dicts.push_back(Dictionary(m_raws[i], takeOwning));
    }
}

void DictionaryArray::reserve(size_t size)
{
    m_raws.reserve(size);
    m_dicts.reserve(size);
}

void DictionaryArray::resize(size_t size)
{
    m_raws.resize(size);
    m_dicts.resize(size);
}

size_t DictionaryArray::size() const
{
    assert(m_raws.size() == m_dicts.size());
    return m_raws.size();
}

void DictionaryArray::pushBack(Dictionary&& dict)
{
    m_dicts.push_back(std::move(dict));
    m_raws.push_back(m_dicts.end()->raw());
}

void DictionaryArray::pushBack(const Dictionary & dict)
{
    m_dicts.push_back(dict);
    m_raws.push_back(m_dicts.end()->raw());
}

Dictionary &DictionaryArray::operator[](size_t index)
{
    return m_dicts[index];
}

const Dictionary &DictionaryArray::at(size_t index) const
{
    return m_dicts.at(index);
}

Dictionary &DictionaryArray::at(size_t index)
{
    return m_dicts.at(index);
}

const AVDictionary * const *DictionaryArray::raws() const
{
    return m_raws.data();
}

AVDictionary **DictionaryArray::raws()
{
    return m_raws.data();
}

AVDictionary **DictionaryArray::release()
{
    for (auto& d : m_dicts)
    {
        d.release();
    }
    return m_raws.data();
}

void DictionaryArray::swap(DictionaryArray & rhs)
{
    std::swap(rhs.m_dicts, m_dicts);
    std::swap(rhs.m_raws,  m_raws);
}

const Dictionary &DictionaryArray::operator[](size_t index) const
{
    return m_dicts[index];
}

} // namespace av

