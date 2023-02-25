#pragma once

#include <string>
#include <string_view>
#include <bitset>

extern "C" {
#include <libavutil/channel_layout.h>
}

namespace av {

std::string channel_name(AVChannel channel);
std::string channel_description(AVChannel channel);
AVChannel   channel_from_string(const std::string &name);
AVChannel   channel_from_string(const char *name);

class ChannelLayoutView
{
public:
    ChannelLayoutView() noexcept;
    // implicit by design
    ChannelLayoutView(const AVChannelLayout &layout) noexcept;

    // ???
    //+ const AVChannelLayout *av_channel_layout_standard(void **opaque);
    template<typename Callable>
    static void iterate(Callable callable) {
        void *iter = nullptr;
        const AVChannelLayout *playout;
        while ((playout = av_channel_layout_standard(&iter))) {
            if (callable(ChannelLayoutView(*playout)))
                break;
        }
    }

    static void dumpLayouts();

    //+ int av_channel_layout_describe(const AVChannelLayout *channel_layout, char *buf, size_t buf_size);
    //+ int            av_channel_layout_index_from_channel(const AVChannelLayout *channel_layout, enum AVChannel channel);
    //+ int            av_channel_layout_index_from_string(const AVChannelLayout *channel_layout, const char *name);
    //+ enum AVChannel av_channel_layout_channel_from_index(const AVChannelLayout *channel_layout, unsigned int idx);
    //+ enum AVChannel av_channel_layout_channel_from_string(const AVChannelLayout *channel_layout, const char *name);
    //+ uint64_t av_channel_layout_subset(const AVChannelLayout *channel_layout, uint64_t mask);
    //+ int av_channel_layout_check(const AVChannelLayout *channel_layout);
    //+ int av_channel_layout_compare(const AVChannelLayout *chl, const AVChannelLayout *chl1);

    AVChannelLayout       *raw();
    const AVChannelLayout *raw() const;

    int         channels() const noexcept;
    uint64_t    layout() const noexcept;
    uint64_t    subset(uint64_t mask) const noexcept;
    bool        isValid() const noexcept;
    std::string describe() const;
    void        describe(std::string &desc) const;

    int index(AVChannel channel) const;
    int index(const char *name) const;

    AVChannel channel(unsigned int index) const;
    AVChannel channel(const char *name) const;

    bool operator==(const ChannelLayoutView &other) const noexcept;

protected:
    AVChannelLayout m_layout{};
};



class ChannelLayout : public ChannelLayoutView
{
public:
    ChannelLayout() = default;

    using ChannelLayoutView::ChannelLayoutView;

    ChannelLayout(const ChannelLayout& other);
    ChannelLayout& operator=(const ChannelLayout& other);

    ChannelLayout(ChannelLayout&& other);
    ChannelLayout& operator=(ChannelLayout&& other);

    ~ChannelLayout();

    //+ void av_channel_layout_uninit(AVChannelLayout *channel_layout);
    //
    //+ int av_channel_layout_from_mask(AVChannelLayout *channel_layout, uint64_t mask);
    //+ int av_channel_layout_from_string(AVChannelLayout *channel_layout, const char *str);
    //+ void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels);
    //+ int av_channel_layout_copy(AVChannelLayout *dst, const AVChannelLayout *src);

    //explicit ChannelLayout(uint64_t mask);
    explicit ChannelLayout(std::bitset<64> mask);
    explicit ChannelLayout(const char *str);
    explicit ChannelLayout(int channels);

    // Stop control AVChannelLayout
    void release();

private:
    void swap(ChannelLayout &other);

};


} // namespace av

