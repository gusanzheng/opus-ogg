#include "opus_ogg.h"

int OpusOggCodec::Encode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    return encoder->Encode(input, output, last);
}

int OpusOggCodec::Decode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    return decoder->Decode(input, output, last);
}