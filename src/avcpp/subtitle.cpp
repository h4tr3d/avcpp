#include "subtitle.h"

#include <cstring>
#include <utility>

#include "avutils.h"

namespace av {

namespace subtitle {

std::string_view defaultAssHeader() noexcept
{
    static constexpr std::string_view header =
        "[Script Info]\n"
        "ScriptType: v4.00+\n"
        "PlayResX: 384\n"
        "PlayResY: 288\n"
        "ScaledBorderAndShadow: yes\n"
        "\n"
        "[V4+ Styles]\n"
        "Format: Name, Fontname, Fontsize, PrimaryColour, SecondaryColour, "
        "OutlineColour, BackColour, Bold, Italic, Underline, StrikeOut, ScaleX, "
        "ScaleY, Spacing, Angle, BorderStyle, Outline, Shadow, Alignment, "
        "MarginL, MarginR, MarginV, Encoding\n"
        "Style: Default,Arial,16,&Hffffff,&Hffffff,&H0,&H0,0,0,0,0,100,100,0,0,1,1,0,2,"
        "10,10,10,0\n"
        "\n"
        "[Events]\n"
        "Format: Layer, Start, End, Style, Name, MarginL, MarginR, MarginV, "
        "Effect, Text\n";

    return header;
}

} // namespace subtitle

SubtitleRect::SubtitleRect(AVSubtitleRect *rect)
    : FFWrapperPtr<AVSubtitleRect>(rect)
{
}

bool SubtitleRect::isValid() const noexcept
{
    return m_raw != nullptr;
}

SubtitleRect::operator bool() const noexcept
{
    return isValid();
}

AVSubtitleType SubtitleRect::type() const noexcept
{
    return RAW_GET(type, SUBTITLE_NONE);
}

void SubtitleRect::setType(AVSubtitleType type) noexcept
{
    RAW_SET(type, type);
}

int SubtitleRect::x() const noexcept
{
    return RAW_GET(x, 0);
}

int SubtitleRect::y() const noexcept
{
    return RAW_GET(y, 0);
}

int SubtitleRect::width() const noexcept
{
    return RAW_GET(w, 0);
}

int SubtitleRect::height() const noexcept
{
    return RAW_GET(h, 0);
}

void SubtitleRect::setArea(int x, int y, int width, int height) noexcept
{
    if (!m_raw)
        return;

    m_raw->x = x;
    m_raw->y = y;
    m_raw->w = width;
    m_raw->h = height;
}

int SubtitleRect::nbColors() const noexcept
{
    return RAW_GET(nb_colors, 0);
}

std::string_view SubtitleRect::text() const noexcept
{
    return wrap2sview(RAW_GET(text, nullptr));
}

std::string_view SubtitleRect::ass() const noexcept
{
    return wrap2sview(RAW_GET(ass, nullptr));
}

void SubtitleRect::setText(std::string_view text, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::FrameInvalid);
        return;
    }

    auto copy = av::strdup(text);
    if (!text.empty() && !copy) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    freep(m_raw->text);
    m_raw->text = copy.release();
}

void SubtitleRect::setAss(std::string_view ass, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::FrameInvalid);
        return;
    }

    auto copy = av::strdup(ass);
    if (!ass.empty() && !copy) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    freep(m_raw->ass);
    m_raw->ass = copy.release();
}

void SubtitleRect::clearText() noexcept
{
    if (m_raw)
        freep(m_raw->text);
}

void SubtitleRect::clearAss() noexcept
{
    if (m_raw)
        freep(m_raw->ass);
}

bool SubtitleRect::isForced() const noexcept
{
    return m_raw && (m_raw->flags & AV_SUBTITLE_FLAG_FORCED);
}

void SubtitleRect::setForced(bool forced) noexcept
{
    if (!m_raw)
        return;

    if (forced)
        m_raw->flags |= AV_SUBTITLE_FLAG_FORCED;
    else
        m_raw->flags &= ~AV_SUBTITLE_FLAG_FORCED;
}

uint8_t *SubtitleRect::data(size_t plane) noexcept
{
    if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
        return nullptr;
    return m_raw->data[plane];
}

const uint8_t *SubtitleRect::data(size_t plane) const noexcept
{
    if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
        return nullptr;
    return m_raw->data[plane];
}

int SubtitleRect::linesize(size_t plane) const noexcept
{
    if (!m_raw || plane >= AV_NUM_DATA_POINTERS)
        return 0;
    return m_raw->linesize[plane];
}

void SubtitleRect::setBitmap(const uint8_t *data, int stride, int width, int height,
                             OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::FrameInvalid);
        return;
    }

    if (!data || width <= 0 || height <= 0 || stride < width) {
        throws_if(ec, Errors::InvalidArgument);
        return;
    }

    const size_t size = static_cast<size_t>(stride) * static_cast<size_t>(height);
    auto copy = av::malloc<uint8_t>(size);
    if (!copy) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    std::memcpy(copy.get(), data, size);
    freep(m_raw->data[0]);
    m_raw->data[0] = copy.release();
    m_raw->linesize[0] = stride;
    m_raw->w = width;
    m_raw->h = height;

    if (m_raw->type == SUBTITLE_NONE)
        m_raw->type = SUBTITLE_BITMAP;
}

void SubtitleRect::setPalette(const uint32_t *palette, int count, OptionalErrorCode ec)
{
    clear_if(ec);

    if (!m_raw) {
        throws_if(ec, Errors::FrameInvalid);
        return;
    }

    if (!palette || count <= 0) {
        throws_if(ec, Errors::InvalidArgument);
        return;
    }

    const size_t size = static_cast<size_t>(count) * sizeof(uint32_t);
    auto copy = av::malloc<uint8_t>(size);
    if (!copy) {
        throws_if(ec, Errors::Unallocated);
        return;
    }

    std::memcpy(copy.get(), palette, size);
    freep(m_raw->data[1]);
    m_raw->data[1] = copy.release();
    m_raw->nb_colors = count;
}

#if AVCPP_CXX_STANDARD >= 20

std::span<const uint8_t> SubtitleRect::bitmap() const noexcept
{
    if (!m_raw || !m_raw->data[0] || m_raw->linesize[0] <= 0 || m_raw->h <= 0)
        return {};

    const size_t size = static_cast<size_t>(m_raw->linesize[0])
                        * static_cast<size_t>(m_raw->h);
    return {m_raw->data[0], size};
}

std::span<const uint32_t> SubtitleRect::palette() const noexcept
{
    if (!m_raw || !m_raw->data[1] || m_raw->nb_colors <= 0)
        return {};

    return {reinterpret_cast<const uint32_t*>(m_raw->data[1]),
            static_cast<size_t>(m_raw->nb_colors)};
}

void SubtitleRect::setBitmap(std::span<const uint8_t> data, int stride, int width,
                             int height, OptionalErrorCode ec)
{
    if (static_cast<size_t>(stride > 0 ? stride : 0)
            * static_cast<size_t>(height > 0 ? height : 0) > data.size()) {
        clear_if(ec);
        throws_if(ec, Errors::InvalidArgument);
        return;
    }

    setBitmap(data.data(), stride, width, height, ec);
}

void SubtitleRect::setPalette(std::span<const uint32_t> palette, OptionalErrorCode ec)
{
    setPalette(palette.data(), static_cast<int>(palette.size()), ec);
}

#endif // AVCPP_CXX_STANDARD >= 20

SubtitleFrame::SubtitleFrame() noexcept
    : m_timeBase(1, AV_TIME_BASE)
{
    m_raw.pts = av::NoPts;
}

SubtitleFrame::SubtitleFrame(SubtitleFrame &&other) noexcept
    : SubtitleFrame()
{
    swap(other);
}

SubtitleFrame &SubtitleFrame::operator=(SubtitleFrame &&other) noexcept
{
    if (this == &other)
        return *this;

    SubtitleFrame tmp(std::move(other));
    swap(tmp);

    return *this;
}

void SubtitleFrame::swap(SubtitleFrame &other) noexcept
{
    std::swap(m_raw, other.m_raw);
    std::swap(m_timeBase, other.m_timeBase);
    std::swap(m_streamIndex, other.m_streamIndex);
    std::swap(m_isComplete, other.m_isComplete);
}

SubtitleFrame::~SubtitleFrame()
{
    clear();
}

SubtitleFrame SubtitleFrame::clone(OptionalErrorCode ec) const
{
    clear_if(ec);

    SubtitleFrame dst;
    dst.m_raw.format = m_raw.format;
    dst.m_raw.start_display_time = m_raw.start_display_time;
    dst.m_raw.end_display_time = m_raw.end_display_time;
    dst.m_raw.pts = m_raw.pts;
    dst.m_timeBase = m_timeBase;
    dst.m_streamIndex = m_streamIndex;
    dst.m_isComplete = m_isComplete;

    for (size_t i = 0; i < numRects(); ++i) {
        const AVSubtitleRect *src = m_raw.rects[i];
        if (!src)
            continue;

        SubtitleRect rect = dst.addRect(src->type, ec);
        if (ec && *ec)
            return {};

        rect.setArea(src->x, src->y, src->w, src->h);
        rect.setForced(src->flags & AV_SUBTITLE_FLAG_FORCED);

        if (src->text) {
            rect.setText(src->text, ec);
            if (ec && *ec)
                return {};
        }

        if (src->ass) {
            rect.setAss(src->ass, ec);
            if (ec && *ec)
                return {};
        }

        if (src->nb_colors > 0 && src->data[1]) {
            rect.setPalette(reinterpret_cast<const uint32_t*>(src->data[1]),
                            src->nb_colors, ec);
            if (ec && *ec)
                return {};
        }

        if (src->data[0] && src->linesize[0] > 0 && src->w > 0 && src->h > 0) {
            rect.setBitmap(src->data[0], src->linesize[0], src->w, src->h, ec);
            if (ec && *ec)
                return {};
        }
    }

    return dst;
}

AVSubtitle *SubtitleFrame::raw() noexcept
{
    return &m_raw;
}

const AVSubtitle *SubtitleFrame::raw() const noexcept
{
    return &m_raw;
}

bool SubtitleFrame::isValid() const noexcept
{
    return m_raw.rects != nullptr || m_raw.num_rects > 0 || isComplete();
}

SubtitleFrame::operator bool() const noexcept
{
    return m_raw.num_rects > 0 || isComplete();
}

uint16_t SubtitleFrame::format() const noexcept
{
    return m_raw.format;
}

void SubtitleFrame::setFormat(uint16_t format) noexcept
{
    m_raw.format = format;
}

bool SubtitleFrame::isText() const noexcept
{
    return m_raw.format != 0;
}

uint32_t SubtitleFrame::startDisplayTime() const noexcept
{
    return m_raw.start_display_time;
}

uint32_t SubtitleFrame::endDisplayTime() const noexcept
{
    return m_raw.end_display_time;
}

void SubtitleFrame::setStartDisplayTime(uint32_t ms) noexcept
{
    m_raw.start_display_time = ms;
}

void SubtitleFrame::setEndDisplayTime(uint32_t ms) noexcept
{
    m_raw.end_display_time = ms;
}

Timestamp SubtitleFrame::pts() const noexcept
{
    return Timestamp(m_raw.pts, m_timeBase);
}

void SubtitleFrame::setPts(const Timestamp &ts)
{
    if (m_timeBase == Rational())
        m_timeBase = ts.timebase();

    m_raw.pts = ts.isNoPts() ? av::NoPts : ts.timestamp(m_timeBase);
}

const Rational &SubtitleFrame::timeBase() const noexcept
{
    return m_timeBase;
}

void SubtitleFrame::setTimeBase(const Rational &value) noexcept
{
    if (m_timeBase == value || value == Rational())
        return;

    if (m_timeBase != Rational() && m_raw.pts != av::NoPts)
        m_raw.pts = m_timeBase.rescale(m_raw.pts, value);

    m_timeBase = value;
}

Timestamp SubtitleFrame::startTime() const noexcept
{
    Timestamp ts(m_raw.pts, m_timeBase);
    if (ts.isNoPts())
        return ts;

    ts += Timestamp(static_cast<int64_t>(m_raw.start_display_time),
                    Rational(1, 1000));
    return ts;
}

Timestamp SubtitleFrame::endTime() const noexcept
{
    Timestamp ts(m_raw.pts, m_timeBase);
    if (ts.isNoPts())
        return ts;

    ts += Timestamp(static_cast<int64_t>(m_raw.end_display_time),
                    Rational(1, 1000));
    return ts;
}

int SubtitleFrame::streamIndex() const noexcept
{
    return m_streamIndex;
}

void SubtitleFrame::setStreamIndex(int index) noexcept
{
    m_streamIndex = index;
}

bool SubtitleFrame::isComplete() const noexcept
{
    return m_isComplete;
}

void SubtitleFrame::setComplete(bool complete) noexcept
{
    m_isComplete = complete;
}

size_t SubtitleFrame::numRects() const noexcept
{
    return static_cast<size_t>(m_raw.num_rects);
}

SubtitleRect SubtitleFrame::rect(size_t index) const
{
    if (index >= numRects())
        return SubtitleRect();
    return SubtitleRect(m_raw.rects[index]);
}

std::vector<SubtitleRect> SubtitleFrame::rects() const
{
    std::vector<SubtitleRect> result;
    result.reserve(numRects());
    for (size_t i = 0; i < numRects(); ++i)
        result.emplace_back(m_raw.rects[i]);
    return result;
}

SubtitleRect SubtitleFrame::addRect(AVSubtitleType type, OptionalErrorCode ec)
{
    clear_if(ec);

    auto rect = av::mallocz<AVSubtitleRect>(sizeof(AVSubtitleRect));
    if (!rect) {
        throws_if(ec, Errors::Unallocated);
        return SubtitleRect();
    }
    rect->type = type;

    const unsigned int count = m_raw.num_rects;
    AVSubtitleRect **rects = static_cast<AVSubtitleRect**>(
        av_realloc_array(m_raw.rects, count + 1, sizeof(AVSubtitleRect*)));
    if (!rects) {
        throws_if(ec, Errors::Unallocated);
        return SubtitleRect();
    }

    m_raw.rects = rects;
    m_raw.rects[count] = rect.release();
    m_raw.num_rects = count + 1;

    return SubtitleRect(m_raw.rects[count]);
}

void SubtitleFrame::clear() noexcept
{
    avsubtitle_free(&m_raw);
    m_raw = AVSubtitle{};
    m_raw.pts = av::NoPts;
    m_timeBase = Rational(1, AV_TIME_BASE);
    m_streamIndex = -1;
    m_isComplete = false;
}

std::string_view SubtitleFrame::text() const noexcept
{
    for (size_t i = 0; i < numRects(); ++i) {
        const AVSubtitleRect *rect = m_raw.rects[i];
        if (rect && rect->text && rect->text[0] != '\0')
            return rect->text;
    }
    return {};
}

std::string_view SubtitleFrame::ass() const noexcept
{
    for (size_t i = 0; i < numRects(); ++i) {
        const AVSubtitleRect *rect = m_raw.rects[i];
        if (rect && rect->ass && rect->ass[0] != '\0')
            return rect->ass;
    }
    return {};
}

SubtitleFrame SubtitleFrame::fromAss(std::string_view ass, OptionalErrorCode ec)
{
    clear_if(ec);

    SubtitleFrame frame;
    SubtitleRect rect = frame.addRect(SUBTITLE_ASS, ec);
    if (ec && *ec)
        return {};

    rect.setAss(ass, ec);
    if (ec && *ec)
        return {};

    return frame;
}

SubtitleFrame SubtitleFrame::fromText(std::string_view text, OptionalErrorCode ec)
{
    clear_if(ec);

    SubtitleFrame frame;
    SubtitleRect rect = frame.addRect(SUBTITLE_TEXT, ec);
    if (ec && *ec)
        return {};

    rect.setText(text, ec);
    if (ec && *ec)
        return {};

    return frame;
}

} // namespace av
