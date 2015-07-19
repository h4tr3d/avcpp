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

    switch (ec) {
        case Errors::NoError: return "Success";
        case Errors::Generic: return "Generic error";
        case Errors::Unallocated: return "Action on unallocated object";
        case Errors::InvalidArgument: return "Invalid function or method argument";
        case Errors::OutOfRange: return "Value or index out of allowed range";
        case Errors::CantAllocateFrame: return "Can't allocate frame";
        case Errors::CodecStreamInvalid: return "Codec's context parent stream invalid";
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
        case Errors::FormatCantAddStream: return "Can't add stream to output format";
        case Errors::FormatAlreadyOpened: return "Format already opened";
        case Errors::FormatNullOutputFormat: return "Output format for format context not specified";
        case Errors::FormatWrongCountOfStreamOptions: return "Incorrect count of stream options (findStreamInfo())";
        case Errors::FormatNoStreams: return "Format has no streams";
        case Errors::FormatInvalidStreamIndex: return "There is not streams with given index";
        case Errors::FormatNotOpened: return "Format not opened";
        case Errors::FormatInvalidDirection: return "Incorrect operation for current format direction (input/output)";
        case Errors::FormatHeaderNotWriten: return "Header must be writen before";
        case Errors::ResamplerInvalidParameters: return "Provided invalid parameters for resampler";
        case Errors::ResamplerNotInited: return "Resampler not inited";
        case Errors::ResamplerInputChanges: return "Resampler input parameters changed (mismatch with provided frame)";
        case Errors::ResamplerOutputChanges: return "Resampler output parameters changed (mismatch with provided frame)";
        case Errors::RescalerInvalidParameters: return "Provided invalid parameters for rescaler";
        case Errors::RescalerInternalSwsError: return "Internal SWS error";
        case Errors::FilterNotInFilterGraph: return "Filtern not in filter graph";
        case Errors::FilterGraphDescriptionEmpty: return "Empty graph description";
        case Errors::IncorrectBufferSrcFilter: return "Given filter context not an type of BufferSrc filters";
        case Errors::IncorrectBufferSrcMediaType: return "Incorrect frame media type provided for BufferSrc filter";
        case Errors::IncorrectBufferSinkFilter: return "Given filter context not an type of BufferSink filters";
        case Errors::IncorrectBufferSinkMediaType: return "Incorrect frame media type provided for BufferSink filter";
        case Errors::MixBufferSinkAccess: return "Mix getFrame() and getSamples() calls on BufferSink";
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

} // ::av


