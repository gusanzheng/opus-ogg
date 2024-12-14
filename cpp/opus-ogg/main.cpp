#include "opus_ogg.h"

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        std::cerr << "Usage: " << argv[0] << " encode/decode <input.pcm> <output.opus>" << std::endl;
        return 1;
    }

    std::string mode(argv[1]);
    if (mode == "encode")
    {
        OpusOggEncoder encoder(24000, 1, 480);
        if (!encoder.encode(argv[2], argv[3]))
        {
            std::cerr << "Encoding failed" << std::endl;
            return 1;
        }
    }
    else if (mode == "decode")
    {
        OpusOggDecoder decoder;
        if (!decoder.decode(argv[2], argv[3]))
        {
            std::cerr << "Decoding failed" << std::endl;
            return 1;
        }
    }
    else
    {
        std::cerr << "Invalid mode" << std::endl;
        return 1;
    }

    return 0;
}