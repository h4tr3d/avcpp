#include <utility>
#include <cstring>

#include "channellayout.h"

#if API_NEW_CHANNEL_LAYOUT
namespace av {

static constexpr size_t BufferSize = 128;


std::string channel_name(AVChannel channel)
{
    char buf[BufferSize]{};
    av_channel_name(buf, sizeof(buf), channel);
    return buf;
}

std::string channel_description(AVChannel channel)
{
    char buf[BufferSize]{};
    av_channel_description(buf, sizeof(buf), channel);
    return buf;
}

AVChannel channel_from_string(const std::string &name)
{
    return channel_from_string(name.c_str());
}

AVChannel channel_from_string(const char *name)
{
    return av_channel_from_string(name);
}


ChannelLayoutView::ChannelLayoutView() noexcept
{
}

ChannelLayoutView::ChannelLayoutView(const AVChannelLayout &layout) noexcept
    : m_layout(layout)
{
}

void ChannelLayoutView::dumpLayouts()
{
    //std::string;
    iterate([] (const ChannelLayoutView &ch) {
        char buf[BufferSize]{};
        av_channel_layout_describe(ch.raw(), buf, sizeof(buf));
        std::string channels;
        for (int i = 0; i < 63; i++) {
            auto idx = ch.index(AVChannel(i));
            if (idx >= 0) {
                if (idx)
                    channels.push_back('+');
                channels.append(channel_name(AVChannel(i)));
            }
        }
        return false; // is not done
    });
}

AVChannelLayout *ChannelLayoutView::raw()
{
    return &m_layout;
}

const AVChannelLayout *ChannelLayoutView::raw() const
{
    return &m_layout;
}

int ChannelLayoutView::channels() const noexcept
{
    return m_layout.nb_channels;
}

uint64_t ChannelLayoutView::layout() const noexcept
{
    return m_layout.order == AV_CHANNEL_ORDER_NATIVE ? m_layout.u.mask : 0;
}

uint64_t ChannelLayoutView::subset(uint64_t mask) const noexcept
{
    return av_channel_layout_subset(&m_layout, mask);
}

bool ChannelLayoutView::isValid() const noexcept
{
    return av_channel_layout_check(&m_layout);
}

std::string ChannelLayoutView::describe() const
{
    char buf[BufferSize]{};
    av_channel_layout_describe(&m_layout, buf, sizeof(buf));
    return buf;
}

void ChannelLayoutView::describe(std::string &desc) const
{
    // TBD
    if (desc.capacity()) {
        if (!desc.size())
            desc.resize(desc.capacity());
        av_channel_layout_describe(&m_layout, desc.data(), desc.size());
        desc.resize(std::strlen(desc.data())); // hack?
    } else {
        desc = describe();
    }
}

int ChannelLayoutView::index(AVChannel channel) const
{
    return av_channel_layout_index_from_channel(&m_layout, channel);
}

int ChannelLayoutView::index(const char *name) const
{
    return av_channel_layout_index_from_string(&m_layout, name);
}

AVChannel ChannelLayoutView::channel(unsigned int index) const
{
    return av_channel_layout_channel_from_index(&m_layout, index);
}

AVChannel ChannelLayoutView::channel(const char *name) const
{
    return av_channel_layout_channel_from_string(&m_layout, name);
}

bool ChannelLayoutView::operator==(const ChannelLayoutView &other) const noexcept
{
    return av_channel_layout_compare(&m_layout, &other.m_layout) == 0;
}

ChannelLayout ChannelLayoutView::clone() const
{
    return ChannelLayout{*this};
}


ChannelLayout::ChannelLayout(const ChannelLayout &other)
    : ChannelLayout()
{
    // TODO: error checking
    av_channel_layout_copy(&m_layout, &other.m_layout);
}

//ChannelLayout::ChannelLayout(ChannelLayoutView &&view)
//    : ChannelLayout()
//{
//
//}

ChannelLayout::ChannelLayout(const ChannelLayoutView &view)
    : ChannelLayout()
{
    av_channel_layout_copy(&m_layout, view.raw());
}

ChannelLayout &ChannelLayout::operator=(const ChannelLayout &other)
{
    ChannelLayout(other).swap(*this);
    return *this;
}

ChannelLayout::ChannelLayout(ChannelLayout &&other)
    : ChannelLayout()
{
    m_layout = std::move(other.m_layout);
    memset(&other.m_layout, 0, sizeof(other.m_layout)); // avoid double memory clean up
}

ChannelLayout &ChannelLayout::operator=(ChannelLayout&& other)
{
    ChannelLayout(std::move(other)).swap(*this);
    return *this;
}

ChannelLayout::~ChannelLayout()
{
    av_channel_layout_uninit(&m_layout);
}

ChannelLayout::ChannelLayout(std::bitset<64> mask)
{
    // TODO: error checking
    av_channel_layout_from_mask(&m_layout, mask.to_ullong());
}

ChannelLayout::ChannelLayout(const char *str)
{
    // TODO: error checking
    av_channel_layout_from_string(&m_layout, str);
}

ChannelLayout::ChannelLayout(int channels)
{
    // TODO: error checking
    av_channel_layout_default(&m_layout, channels);
}

void ChannelLayout::release()
{
    memset(&m_layout, 0, sizeof(m_layout));
}

void ChannelLayout::swap(ChannelLayout &other)
{
    //std::swap(*raw(), *other.raw());
    std::swap(m_layout, other.m_layout);
}

} // namespace av

#endif
