#include "interface.h"
#include "opus_ogg.h"

#ifdef __cplusplus
extern "C"
{
#endif

#include "opus_ogg.h"

    int OpusOggCodecStart(void **inst, int sampleRate)
    {
        OpusOggCodec *ooc = new OpusOggCodec(sampleRate);
        if (!ooc->Start())
        {
            return -1;
        }
        *inst = static_cast<void *>(ooc);
        return 0;
    }
    int OpusOggCodecEnd(void **inst)
    {
        if (!inst)
        {
            return 0;
        }
        OpusOggCodec *ooc = static_cast<OpusOggCodec *>(*inst);
        delete ooc;
        ooc = nullptr;
        return 0;
    }

    int OpusOggCodecEncode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last)
    {
        if (!inst || (inputLen>0 && !input) || !output || !outputLen)
        {
            return -1; // 参数错误
        }

        OpusOggCodec *ooc = static_cast<OpusOggCodec *>(inst);
        std::vector<char> inputVec(input, input + inputLen);
        std::vector<char> outputVec;
        int ret = ooc->Encode(inputVec, outputVec, last);
        if (ret != 0)
        {
            return ret;
        }
        *outputLen = outputVec.size();
        *output = new char[*outputLen]; // 注意: 外层go一定要注意释放内存
        std::memcpy(*output, outputVec.data(), *outputLen);

        return 0;
    }
    
    // not implemented
    // int OpusOggCodecDecode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last)
    // {
    //     return -1; 
    // }
    int OpusOggCodecDecode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last)
    {
        if (!inst || (inputLen>0 && !input)|| !output || !outputLen)
        {
            return -1; // 参数错误
        }

        OpusOggCodec *ooc = static_cast<OpusOggCodec *>(inst);
        std::vector<char> inputVec(input, input + inputLen);
        std::vector<char> outputVec;
        int ret = ooc->Decode(inputVec, outputVec, last);
        if (ret != 0)
        {
            return ret;
        }
        *outputLen = outputVec.size();
        *output = new char[*outputLen]; // 注意: 外层go一定要注意释放内存
        std::memcpy(*output, outputVec.data(), *outputLen);

        return 0;
    }

#ifdef __cplusplus
}
#endif
