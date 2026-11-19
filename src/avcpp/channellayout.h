#pragma once

/**
 * Audio channel layouts.
 *
 * @file
 * @ingroup frame
 */

#include <string>
#include <string_view>
#include <bitset>

#include "avcompat.h"
#include "avutils.h"

#if AVCPP_API_NEW_CHANNEL_LAYOUT
extern "C" {
#include <libavutil/channel_layout.h>
}

namespace av {

/**
 * Name of a speaker position.
 *
 * @param channel the `AV_CHAN_*` value
 *
 * @return the name FFmpeg uses, for example "FL"; empty for an unknown channel
 */
std::string channel_name(AVChannel channel);
/**
 * Human-readable description of a speaker position.
 *
 * @param channel the `AV_CHAN_*` value
 *
 * @return a description such as "front left"; empty for an unknown channel
 */
std::string channel_description(AVChannel channel);
/**
 * Look a speaker position up by name.
 *
 * @param name the name to resolve, for example "FL"
 *
 * @return the `AV_CHAN_*` value, or `AV_CHAN_NONE` when the name is unknown
 */
AVChannel   channel_from_string(const std::string &name);
/**
 * Look a speaker position up by a C string name.
 *
 * @param name the null-terminated name to resolve
 *
 * @return the `AV_CHAN_*` value, or `AV_CHAN_NONE` when the name is unknown
 */
AVChannel   channel_from_string(const char *name);

class ChannelLayout;

/**
 * Non-owning view of an AVChannelLayout.
 *
 * The view shares the descriptor with its owner, which is enough for every
 * query FFmpeg offers. Use ChannelLayout when the layout has to outlive the
 * FFmpeg structure it came from, or when it has to be built from a mask, a name
 * or a channel count.
 */
class ChannelLayoutView
{
public:
        /**
         * Create an empty, invalid view.
         */
    ChannelLayoutView() noexcept;
    // implicit by design
        /**
         * Wrap an FFmpeg channel layout.
         *
         * The conversion is implicit on purpose, so an `AVChannelLayout` can be
         * passed where a view is expected.
         *
         * @param layout the layout to view; it must outlive the view
         */
    ChannelLayoutView(const AVChannelLayout &layout) noexcept;

    // ???
    //+ const AVChannelLayout *av_channel_layout_standard(void **opaque);
        /**
         * Iterate the standard layouts FFmpeg knows about.
         *
         * @tparam Callable callable taking a `ChannelLayoutView` and returning
         *                  `true` to stop the iteration
         *
         * @param callable invoked once per standard layout
         */
    template<typename Callable>
    static void iterate(Callable callable) {
        void *iter = nullptr;
        const AVChannelLayout *playout;
        while ((playout = av_channel_layout_standard(&iter))) {
            if (callable(ChannelLayoutView(*playout)))
                break;
        }
    }

        /**
         * Print every standard layout through the FFmpeg log system.
         */
    static void dumpLayouts();

    //+ int av_channel_layout_describe(const AVChannelLayout *channel_layout, char *buf, size_t buf_size);
    //+ int            av_channel_layout_index_from_channel(const AVChannelLayout *channel_layout, enum AVChannel channel);
    //+ int            av_channel_layout_index_from_string(const AVChannelLayout *channel_layout, const char *name);
    //+ enum AVChannel av_channel_layout_channel_from_index(const AVChannelLayout *channel_layout, unsigned int idx);
    //+ enum AVChannel av_channel_layout_channel_from_string(const AVChannelLayout *channel_layout, const char *name);
    //+ uint64_t av_channel_layout_subset(const AVChannelLayout *channel_layout, uint64_t mask);
    //+ int av_channel_layout_check(const AVChannelLayout *channel_layout);
    //+ int av_channel_layout_compare(const AVChannelLayout *chl, const AVChannelLayout *chl1);

        /**
         * The underlying FFmpeg layout, mutable.
         *
         * @return a pointer to the layout; valid while the owner lives
         */
    AVChannelLayout       *raw();
        /**
         * The underlying FFmpeg layout, read-only.
         *
         * @return a pointer to the layout; valid while the owner lives
         */
    const AVChannelLayout *raw() const;

        /**
         * Number of channels in the layout.
         *
         * @return the channel count, or zero for an invalid layout
         */
    int         channels() const noexcept;
        /**
         * The layout as a bit mask when it can be expressed as one.
         *
         * @return the `AV_CH_LAYOUT_*` mask, or zero for a custom layout that has
         *         no mask representation
         */
    uint64_t    layout() const noexcept;
        /**
         * Intersection of the layout with a mask.
         *
         * @param mask the `AV_CH_*` bits to keep
         *
         * @return the selected channels as a mask
         */
    uint64_t    subset(uint64_t mask) const noexcept;
        /**
         * Whether the view refers to a usable layout.
         *
         * @return `true` when the order or the mask is set
         */
    bool        isValid() const noexcept;
        /**
         * Text description of the layout.
         *
         * @return a string such as "stereo" or "5.1"
         */
    std::string describe() const;
        /**
         * Text description of the layout into an existing string.
         *
         * @param desc the string that receives the description
         */
    void        describe(std::string &desc) const;

        /**
         * Position of a speaker in the layout.
         *
         * @param channel the `AV_CHAN_*` value to look for
         *
         * @return the zero-based index, or -1 when the channel is not present
         */
    int index(AVChannel channel) const;
        /**
         * Position of a speaker in the layout, by name.
         *
         * @param name the channel name to look for
         *
         * @return the zero-based index, or -1 when the channel is not present
         */
    int index(const char *name) const;

        /**
         * Speaker at a position.
         *
         * @param index zero-based position
         *
         * @return the `AV_CHAN_*` value, or `AV_CHAN_NONE` when the index is out of
         *         range
         */
    AVChannel channel(unsigned int index) const;
        /**
         * Speaker by name.
         *
         * @param name the channel name
         *
         * @return the `AV_CHAN_*` value, or `AV_CHAN_NONE` when the layout does not
         *         contain it
         */
    AVChannel channel(const char *name) const;

        /**
         * Compare two layouts.
         *
         * @param other the layout to compare with
         *
         * @return `true` when both describe the same channels in the same order
         */
    bool operator==(const ChannelLayoutView &other) const noexcept;

        /**
         * Deep copy of the layout.
         *
         * @return an owning copy that can outlive this view
         */
    ChannelLayout clone() const;

protected:
        /// The viewed layout; for the view it is a shallow copy of the descriptor.
    AVChannelLayout m_layout{};
};



/**
 * Owning channel layout.
 *
 * Holds the description allocated by FFmpeg and releases it on destruction;
 * copying duplicates it, moving transfers it. release() exists for the case
 * where the description is handed over to an FFmpeg structure that takes
 * ownership.
 */
class ChannelLayout : public ChannelLayoutView
{
public:
        /**
         * Create an empty layout that owns nothing.
         */
    ChannelLayout() = default;
    //explicit ChannelLayout(ChannelLayoutView &&view);
        /**
         * Copy a view into an owning layout.
         *
         * @param view the layout to copy
         */
    explicit ChannelLayout(const ChannelLayoutView &view);

    using ChannelLayoutView::ChannelLayoutView;

        /**
         * Deep copy of a layout.
         *
         * @param other the layout to copy
         */
    ChannelLayout(const ChannelLayout& other);
        /**
         * Deep copy assignment; the previous contents are released.
         *
         * @param other the layout to copy
         *
         * @return this layout
         */
    ChannelLayout& operator=(const ChannelLayout& other);

        /**
         * Move a layout.
         *
         * @param other the layout to move from; left empty
         */
    ChannelLayout(ChannelLayout&& other);
        /**
         * Move assignment; the previous contents are released.
         *
         * @param other the layout to move from; left empty
         *
         * @return this layout
         */
    ChannelLayout& operator=(ChannelLayout&& other);

        /**
         * Release the owned description.
         */
    ~ChannelLayout();

    //+ void av_channel_layout_uninit(AVChannelLayout *channel_layout);
    //
    //+ int av_channel_layout_from_mask(AVChannelLayout *channel_layout, uint64_t mask);
    //+ int av_channel_layout_from_string(AVChannelLayout *channel_layout, const char *str);
    //+ void av_channel_layout_default(AVChannelLayout *ch_layout, int nb_channels);
    //+ int av_channel_layout_copy(AVChannelLayout *dst, const AVChannelLayout *src);

    //explicit ChannelLayout(uint64_t mask);
        /**
         * Build a layout from a bit mask.
         *
         * @param mask the `AV_CH_*` bits that are present
         */
    explicit ChannelLayout(std::bitset<64> mask);
        /**
         * Build a layout from its textual form.
         *
         * @param str a description such as "stereo" or "FL+FR+FC"; invalid input
         *            leaves the layout empty
         */
    explicit ChannelLayout(const char *str);
        /**
         * Build the default layout for a channel count.
         *
         * @param channels number of channels, for example 2 for stereo
         */
    explicit ChannelLayout(int channels);

    // Stop control AVChannelLayout
        /**
         * Give up control of the owned description.
         *
         * Used after the layout has been moved into an FFmpeg structure that now
         * owns it, so the destructor does not free it twice.
         */
    void release();

private:
        /**
         * Exchange the contents of two layouts.
         *
         * @param other the layout to swap with
         */
    void swap(ChannelLayout &other);

};

} // namespace av

#endif
