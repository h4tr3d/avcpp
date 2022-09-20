#pragma once

#include <system_error>
#include <exception>

namespace av
{

enum class Errors
{
    NoError = 0,
    Generic,
    Unallocated,
    InvalidArgument,
    OutOfRange,
    CantAllocateFrame,
    CodecStreamInvalid,
    CodecInvalidDirection,
    CodecAlreadyOpened,
    CodecInvalid,
    CodecNotOpened,
    CodecInvalidDecodeProc,
    CodecInvalidEncodeProc,
    CodecDecodingOffsetToLarge,
    CodecInvalidForEncode,
    CodecInvalidForDecoce,
    CodecInvalidMediaType,
    FrameInvalid,
    DictOutOfRage,
    DictNoKey,

    FormatCantAddStream,
    FormatAlreadyOpened,
    FormatNullOutputFormat,
    FormatWrongCountOfStreamOptions,
    FormatNoStreams,
    FormatInvalidStreamIndex,
    FormatNotOpened,
    FormatInvalidDirection,
    FormatHeaderNotWriten,
    FormatCodecUnsupported,

    ResamplerInvalidParameters,
    ResamplerNotInited,
    ResamplerInputChanges,
    ResamplerOutputChanges,

    RescalerInvalidParameters,
    RescalerInternalSwsError,

    FilterNotInFilterGraph,
    FilterGraphDescriptionEmpty,

    IncorrectBufferSrcFilter,
    IncorrectBufferSrcMediaType,

    IncorrectBufferSinkFilter,
    IncorrectBufferSinkMediaType,
    MixBufferSinkAccess,
};

class OptionalErrorCode
{
    OptionalErrorCode() {}

public:
    OptionalErrorCode(std::error_code &ec);

    static OptionalErrorCode null();

    operator bool() const;

    std::error_code& operator*();

private:
    std::error_code *m_ec = nullptr;
};

/**
 * @brief The AvException class
 * Default exception that thows from function that does not accept error code storage.
 *
 * Simple wrap error code value and category
 */
class Exception : public std::system_error
{
public:
    using std::system_error::system_error;
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
std::error_condition make_error_condition(av::Errors errc) noexcept
{
    return std::error_condition(static_cast<int>(errc), av::avcpp_category());
}

inline
std::error_code make_error_code(av::Errors errc) noexcept
{
    return std::error_code(static_cast<int>(errc), av::avcpp_category());
}

inline
std::error_code make_avcpp_error(Errors code)
{
    return std::error_code(static_cast<int>(code), avcpp_category());
}

inline
std::error_condition make_avcpp_condition(Errors code)
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

template<typename Exception = av::Exception>
void throw_error_code(const std::error_code& ec)
{
    throw Exception(ec);
}

template<typename Exception = av::Exception>
void throw_error_code(Errors errc)
{
    throw Exception(make_error_code(errc));
}

/**
 * @brief Helper to construct null OptionalErrorCode object
 * @return null OptionalErrorCode object
 */
inline OptionalErrorCode throws()
{
    return OptionalErrorCode::null();
}

/**
 * @brief Throws exception if ec is av::throws() or fill error code
 */
template<typename Category, typename Exception = av::Exception>
void throws_if(OptionalErrorCode ec, int errcode, const Category &cat)
{
    if (ec)
        *ec = std::error_code(errcode, cat);
    else
        throw Exception(std::error_code(errcode, cat));
}

template<typename T, typename Exception = av::Exception>
void throws_if(OptionalErrorCode ec, T errcode)
{
    if (ec)
        *ec = make_error_code(errcode);
    else
        throw Exception(make_error_code(errcode));
}


/**
 * @brief clear_if - clear error code if it is not av::throws()
 * @param ec error code to clear
 */
inline
void clear_if(OptionalErrorCode ec)
{
    if (ec)
        (*ec).clear();
}

inline
bool is_error(OptionalErrorCode ec)
{
    if (ec)
        return static_cast<bool>(*ec);

    return false;
}

} // ::av

namespace std {
template<> struct is_error_condition_enum<av::Errors> : public true_type {};
// Commented out for future invertigations. Note, for correct comparation, users error enum must
// declared only as is_error_condition_enum<> or is_error_code_enum<>
//template<> struct is_error_code_enum<av::AvError> : public true_type {};
}

