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
    CodecNotOpened = -7,
    CodecInvalidDecodeProc = -8,
    CodecInvalidEncodeProc = -9,
    CodecDecodingOffsetToLarge = -10,
    CodecInvalidForEncode = -11,
    CodecInvalidForDecoce = -12,
    FrameInvalid = -13,
    DictOutOfRage = -14,
    DictNoKey     = -15,

    FormatCantAddStream = -16,
    FormatAlreadyOpened = -17,
    FormatNullOutputFormat = -18,
    FormatWrongCountOfStreamOptions = -19,
    FormatNoStreams = -20,
    FormatInvalidStreamIndex = -21,
    FormatNotOpened = -22,
    FormatInvalidDirection = -23,
    FormatHeaderNotWriten = -24,
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

inline
void throw_error_code(const std::error_code &ec)
{
    throw AvException(ec);
}

inline
void throw_error_code(AvError errc)
{
    throw AvException(make_error_code(errc));
}

namespace detail { inline std::error_code * throws() { return 0; } }
// From Boost.System
//  Misuse of the error_code object is turned into a noisy failure by
//  poisoning the reference. This particular implementation doesn't
//  produce warnings or errors from popular compilers, is very efficient
//  (as determined by inspecting generated code), and does not suffer
//  from order of initialization problems. In practice, it also seems
//  cause user function error handling implementation errors to be detected
//  very early in the development cycle.
inline std::error_code& throws()
{
    return *detail::throws();
}


/**
 * @brief Throws exception if ec is av::throws() or fill error code
 */
template<typename Category, typename Exception = av::AvException>
void throws_if(std::error_code &ec, int errcode, const Category &cat)
{
    if (&ec == &throws())
    {
        throw Exception(std::error_code(errcode, cat));
    }
    else
    {
        ec = std::error_code(errcode, cat);
    }
}

template<typename T, typename Exception = av::AvException>
void throws_if(std::error_code &ec, T errcode)
{
    if (&ec == &throws())
    {
        throw Exception(make_error_code(errcode));
    }
    else
    {
        ec = make_error_code(errcode);
    }
}


/**
 * @brief clear_if - clear error code if it is not av::throws()
 * @param ec error code to clear
 */
inline
void clear_if(std::error_code &ec)
{
    if (&ec != &throws())
        ec.clear();
}

inline
bool is_error(std::error_code &ec)
{
    if (&ec == &throws())
        return false;
    return (bool)ec;
}

} // ::av

namespace std {
template<> struct is_error_condition_enum<av::AvError> : public true_type {};
// Commented out for future invertigations. Note, for correct comparation, users error enum must
// declared only as is_error_condition_enum<> or is_error_code_enum<>
//template<> struct is_error_code_enum<av::AvError> : public true_type {};
}


#endif // AV_AVERROR_H
