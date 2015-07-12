#include "avutils.h"

#include "averror.h"

using namespace std;

namespace av {

const char *AvcppCategory::name() const noexcept
{
    return "AvcppError";
}

std::string AvcppCategory::message(int ev) const
{
    auto ec = static_cast<AvError>(ev);
    switch (ec) {
        case AvError::NoError: return "Success";
        case AvError::Generic: return "Generic error";
        case AvError::CodecStreamInvalid: return "Codec's context parent stream invalid";
        case AvError::Unallocated: return "Action on unallocated object";
        case AvError::CodecInvalidDirection: return "Action impossible with given codec context direction";
        case AvError::CodecAlreadyOpened: return "Codec context already opened";
        case AvError::CodecInvalid: return "Codec context invalid";
        case AvError::CodecDoesNotOpened: return "Codec context does not opened";
        case AvError::CodecInvalidDecodeProc: return "Provided null decode proc";
        case AvError::CodecInvalidEncodeProc: return "Provided null encode proc";
        case AvError::CodecDecodingOffsetToLarge: return "Decoding packet offset biggest packet size";
        case AvError::CodecInvalidForEncode: return "Codec context can't encode data";
        case AvError::CodecInvalidForDecoce: return "Codec context can't decode data";
        case AvError::FrameInvalid: return "Frame invalid (unallocated)";
    }

    return "Uknown AvCpp error";
}

const char *FfmpegCategory::name() const noexcept
{
    return "FFmpegError";
}

std::string FfmpegCategory::message(int ev) const
{
    return error2string(ev);
}

AvException::AvException(const error_code ec)
    : m_ec(ec)
{
}

const char *av::AvException::what() const noexcept
{
    return m_ec.message().c_str();
}

} // ::av


