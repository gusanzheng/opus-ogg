#include "interface.h"
#include "opus_codec.h"

#ifdef __cplusplus
extern "C"
{
#endif
    int OpusCodecStart(void **inst, int sampleRate)
    {
        OpusCodec *oc = new OpusCodec(sampleRate);
        if (!oc->Start())
        {
            return -1;
        }
        *inst = static_cast<void *>(oc);
        return 0;
    }
    int OpusCodecEnd(void **inst)
    {
        if (!inst)
        {
            return 0;
        }
        OpusCodec *oc = static_cast<OpusCodec *>(*inst);
        delete oc;
        oc = nullptr;
        return 0;
    }

    int OpusCodecEncode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last)
    {
        if (!inst || (inputLen>0 && !input) || !output || !outputLen)
        {
            return -1; // 参数错误
        }

        OpusCodec *oc = static_cast<OpusCodec *>(inst);
        std::vector<char> inputVec(input, input + inputLen);
        std::vector<char> outputVec;
        int ret = oc->Encode(inputVec, outputVec, last);
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
    int OpusCodecDecode(void *inst, const char *input, int inputLen, char **output, int *outputLen, bool last)
    {
        return -1; 
    }

#ifdef __cplusplus
}
#endif
