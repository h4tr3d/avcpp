#ifndef AV_AVERROR_H
#define AV_AVERROR_H

#include <system_error>
#include <exception>

namespace av
{

enum class AvError
{
    NoError =  0,
    Generic = -1,
    CodecStreamInvalid = -2,
    Unallocated = -3,
    CodecInvalidDirection = -4,
    CodecAlreadyOpened = -5,
    CodecInvalid = -6,
    CodecDoesNotOpened = -7,
    CodecInvalidDecodeProc = -8,
    CodecInvalidEncodeProc = -9,
    CodecDecodingOffsetToLarge = -10,
    CodecInvalidForEncode = -11,
    CodecInvalidForDecoce = -12,
    FrameInvalid = -13,
};

/**
 * @brief The AvException class
 * Default exception that thows from function that does not accept error code storage.
 *
 * Simple wrap error code value and category
 */
class AvException : public std::exception
{
public:
    AvException(const std::error_code ec);
    virtual const char *what() const noexcept override;

private:
    std::error_code m_ec;
};

inline
void throw_error_code(const std::error_code &ec)
{
    throw AvException(ec);
}

/**
 * @brief The AvcppCategory class
 * Describes internal AvCpp errors
 */
class AvcppCategory : public std::error_category
{
public:

    virtual const char *name() const noexcept override;
    virtual std::string message(int ev) const override;
};

/**
 * @brief The FfmpegCategory class
 * Describers FFmpeg internal errors
 */
class FfmpegCategory : public std::error_category
{
    virtual const char *name() const noexcept override;
    virtual std::string message(int ev) const override;
};

inline
const AvcppCategory& avcpp_category()
{
    static AvcppCategory res;
    return res;
}

inline FfmpegCategory& ffmpeg_category()
{
    static FfmpegCategory res;
    return res;
}

inline
std::error_condition make_error_condition(av::AvError errc) noexcept
{
    return std::error_condition(static_cast<int>(errc), av::avcpp_category());
}

inline
std::error_code make_error_code(av::AvError errc) noexcept
{
    return std::error_code(static_cast<int>(errc), av::avcpp_category());
}

inline
std::error_code make_avcpp_error(int code)
{
    if (code > 0)
        return std::error_code(static_cast<int>(AvError::NoError), avcpp_category());
    return std::error_code(code, avcpp_category());
}

inline
std::error_condition make_avcpp_condition(int code)
{
    if (code > 0)
        return std::error_condition(static_cast<int>(AvError::NoError), avcpp_category());
    return std::error_condition(code, avcpp_category());
}

inline
std::error_code make_avcpp_error(AvError code)
{
    return std::error_code(static_cast<int>(code), avcpp_category());
}

inline
std::error_condition make_avcpp_condition(AvError code)
{
    return std::error_condition(static_cast<int>(code), avcpp_category());
}

inline
std::error_code make_ffmpeg_error(int code)
{
    return std::error_code(code, ffmpeg_category());
}

inline
std::error_condition make_ffmpeg_condition(int code)
{
    return std::error_condition(code, ffmpeg_category());
}

} // ::av

namespace std {
template<> struct is_error_condition_enum<av::AvError> : public true_type {};
// Commented out for future invertigations. Note, for correct comparation, users error enum must
// declared only as is_error_condition_enum<> or is_error_code_enum<>
//template<> struct is_error_code_enum<av::AvError> : public true_type {};
}


#endif // AV_AVERROR_H
