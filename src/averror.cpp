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
    auto ec = static_cast<Errors>(ev);

#define CASE(code, text) \
    case Errors::code: return text

    switch (ec) {
        case Errors::NoError: return "Success";
        case Errors::Generic: return "Generic error";
        case Errors::CodecStreamInvalid: return "Codec's context parent stream invalid";
        case Errors::Unallocated: return "Action on unallocated object";
        case Errors::CodecInvalidDirection: return "Action impossible with given codec context direction";
        case Errors::CodecAlreadyOpened: return "Codec context already opened";
        case Errors::CodecInvalid: return "Codec context invalid";
        case Errors::CodecNotOpened: return "Codec context does not opened";
        case Errors::CodecInvalidDecodeProc: return "Provided null decode proc";
        case Errors::CodecInvalidEncodeProc: return "Provided null encode proc";
        case Errors::CodecDecodingOffsetToLarge: return "Decoding packet offset biggest packet size";
        case Errors::CodecInvalidForEncode: return "Codec context can't encode data";
        case Errors::CodecInvalidForDecoce: return "Codec context can't decode data";
        case Errors::FrameInvalid: return "Frame invalid (unallocated)";
        case Errors::DictOutOfRage: return "Dictionary index out of range";
        case Errors::DictNoKey: return "Dictionary does not contain entry with given key";

            CASE(FormatCantAddStream, "Can't add stream to output format");
            CASE(FormatAlreadyOpened, "Format already opened");
            CASE(FormatNullOutputFormat, "Output format for format context not specified");
            CASE(FormatWrongCountOfStreamOptions, "Incorrect count of stream options (findStreamInfo())");
            CASE(FormatNoStreams, "Format has no streams");
            CASE(FormatInvalidStreamIndex, "There is not streams with given index");
            CASE(FormatNotOpened, "Format not opened");
            CASE(FormatInvalidDirection, "Incorrect operation for current format direction (input/output)");
            CASE(FormatHeaderNotWriten, "Header must be writen before");
    }

#undef CASE

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

} // ::av


