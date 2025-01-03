#include "interface.h"
#include "opus_codec.h"

#ifdef __cplusplus
extern "C"
{
#endif
    int OpusCodecInit(const char *libName)
    {
        dlHandler* dlh = dlHandler::GetInstance();
        int ret = dlh->Open(libName);
        if (ret != 0) {
            return ret;
        }
        return 0;
    }

    void OpusCodecFini()
    {
        dlHandler *dlh = dlHandler::GetInstance();
        dlh->Close();
    }

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
        if (!inst || (inputLen > 0 && !input) || !output || !outputLen)
        {
            return -1;
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
        *output = (char *)malloc(*outputLen); // 使用 malloc, 外层go一定要注意 free 内存
        if (*output == nullptr)
        {
            return -1;
        }
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
