#pragma once

/**
 * Error codes, error categories and the OptionalErrorCode convention.
 *
 * @file
 * @ingroup core
 */

#include <system_error>
#include <exception>

namespace av
{

/**
 * Errors the library raises itself, as opposed to FFmpeg's return codes.
 *
 * The values are carried by `avcpp_category()`, so they compare directly with an
 * `std::error_code` obtained from any avcpp call.
 */
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
    CodecIsNotOpened,
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

    BufferReadonly,

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

    CodecUnsupportedOperation,

    HardwareDeviceUnavailable,
    HardwareDeviceInvalid,
    HardwareConfigNotFound,
    HardwareFramesInvalid,
    HardwareTransferUnsupported,
    HardwareCodecNotFound,
};

/**
 * Carries the caller's choice between exceptions and error codes.
 *
 * A function that can fail takes an `OptionalErrorCode` as its last parameter.
 * The default is throws(), which makes the function throw av::Exception; passing
 * a std::error_code& makes it store the code instead. The object is a
 * non-owning pointer to that variable, so the variable must outlive the call.
 */
class OptionalErrorCode
{
    OptionalErrorCode() {}

public:
    /**
     * Report errors through the given variable.
     *
     * @param ec the variable that receives the error; it must outlive the call
     */
    OptionalErrorCode(std::error_code &ec);

    /**
     * The value that means "the caller wants no error code".
     *
     * @return an empty holder; used by throws() as the default argument
     */
    static OptionalErrorCode null();

    /**
     * Whether the caller supplied an error code variable.
     *
     * @return `true` when a variable was passed
     */
    operator bool() const;

    /**
     * The caller's variable.
     *
     * @return the variable the holder points at
     */
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

    /**
     * Name of the category.
     *
     * @return the category name as a static string
     */
    const char *name() const noexcept override;

    /**
     * Text for an error value.
     *
     * @param ev the error value
     *
     * @return a human-readable description
     */
    std::string message(int ev) const override;
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

/**
 * Error category of the library's own errors.
 *
 * @return the category, a process-wide singleton
 */
inline
const AvcppCategory& avcpp_category()
{
    static AvcppCategory res;
    return res;
}

/**
 * Error category that wraps FFmpeg return codes.
 *
 * @return the category, a process-wide singleton
 */
inline FfmpegCategory& ffmpeg_category()
{
    static FfmpegCategory res;
    return res;
}

inline
/**
 * Build an error condition from an avcpp error.
 *
 * @param errc the error
 *
 * @return the condition in the avcpp category
 */
std::error_condition make_error_condition(av::Errors errc) noexcept
{
    return std::error_condition(static_cast<int>(errc), av::avcpp_category());
}

inline
/**
 * Build an error code from an avcpp error.
 *
 * @param errc the error
 *
 * @return the code in the avcpp category
 */
std::error_code make_error_code(av::Errors errc) noexcept
{
    return std::error_code(static_cast<int>(errc), av::avcpp_category());
}

inline
/**
 * Build an error code from an avcpp error.
 *
 * @param code the error
 *
 * @return the code in the avcpp category
 */
std::error_code make_avcpp_error(Errors code)
{
    return std::error_code(static_cast<int>(code), avcpp_category());
}

inline
/**
 * Build an error condition from an avcpp error.
 *
 * @param code the error
 *
 * @return the condition in the avcpp category
 */
std::error_condition make_avcpp_condition(Errors code)
{
    return std::error_condition(static_cast<int>(code), avcpp_category());
}

inline
/**
 * Build an error code from an FFmpeg return value.
 *
 * @param code the negative AVERROR value
 *
 * @return the code in the FFmpeg category
 */
std::error_code make_ffmpeg_error(int code)
{
    return std::error_code(code, ffmpeg_category());
}

inline
/**
 * Build an error condition from an FFmpeg return value.
 *
 * @param code the negative AVERROR value
 *
 * @return the condition in the FFmpeg category
 */
std::error_condition make_ffmpeg_condition(int code)
{
    return std::error_condition(code, ffmpeg_category());
}

template<typename Exception = av::Exception>
/**
 * Throw an exception carrying the given error code.
 *
 * @tparam Exception exception type to throw
 *
 * @param ec the error to throw
 */
void throw_error_code(const std::error_code& ec)
{
    throw Exception(ec);
}

template<typename Exception = av::Exception>
/**
 * Throw an exception carrying an avcpp error.
 *
 * @tparam Exception exception type to throw
 *
 * @param errc the error to throw
 */
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
 * Throw or store an error, depending on what the caller passed.
 *
 * @param ec      the caller's choice between exceptions and error codes
 * @param errcode the raw error value
 * @param cat     the category the value belongs to
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
/**
 * Throw or store an error code, depending on the caller's choice.
 *
 * @tparam T error type with an std::error_code adapter
 *
 * @param ec      the caller's choice between exceptions and error codes
 * @param errcode the error value
 */
void throws_if(OptionalErrorCode ec, T errcode)
{
    if (ec)
        *ec = make_error_code(errcode);
    else
        throw Exception(make_error_code(errcode));
}

/**
 * Throw or store an error code, depending on the caller's choice.
 *
 * @param ec      the caller's choice between exceptions and error codes
 * @param errcode the error value
 */
static inline void throws_if(OptionalErrorCode ec, const std::error_code& errcode)
{
    if (ec)
        *ec = errcode;
    else
        throw Exception(errcode);
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
/**
 * Whether an error was stored in the caller's variable.
 *
 * @param ec the holder to inspect
 *
 * @return `true` when a variable was passed and it holds an error
 */
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

