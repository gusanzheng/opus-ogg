#include "opus_codec.h"
/**** dlHandler ****/
dlHandler *dlHandler::dlInst = new dlHandler();

dlHandler *dlHandler::GetInstance()
{
    return dlInst;
}
int dlHandler::Open(const char *libName)
{
    void *handle = dlopen(libName, RTLD_NOW);
    if (handle == NULL)
    {
        std::cerr << dlerror() << std::endl;
        return -1;
    }
    libHandle = handle;

    // 清除任何存在的错误
    dlerror();

    // 获取所需符号
    opus_encoder_create = (opus_encoder_create_func)dlsym(handle, "opus_encoder_create");
    opus_encoder_destroy = (opus_encoder_destroy_func)dlsym(handle, "opus_encoder_destroy");
    opus_encoder_ctl = (opus_encoder_ctl_func)dlsym(handle, "opus_encoder_ctl");
    opus_encode = (opus_encode_func)dlsym(handle, "opus_encode");
    opus_strerror = (opus_strerror_func)dlsym(handle, "opus_strerror");

    // 检查是否成功获取符号
    const char *dlsym_error = dlerror();
    if (dlsym_error)
    {
        std::cerr << "Failed to load symbol 'add': " << dlsym_error << std::endl;
        dlclose(handle);
        return -1;
    }
    return 0;
}

/**** OpusCodec ****/

int OpusCodec::Encode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    return encoder->Encode(input, output, last);
}

// not implemented
int OpusCodec::Decode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    return -1;
}

/**** Pcm2OpusEncoder ****/

bool Pcm2OpusEncoder::initializeEncoder()
{
    int err;
    OpusEncoder *enc = dl->opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_AUDIO, &err);
    if (!enc)
    {
        std::cerr << "Failed to create Opus encoder: " << dl->opus_strerror(err) << std::endl;
        return false;
    }
    encoder = enc;

    // 设置编码器参数
    dl->opus_encoder_ctl(encoder, OPUS_SET_VBR(0)); // 0:CBR, 1:VBR
    dl->opus_encoder_ctl(encoder, OPUS_SET_BITRATE(48000));
    dl->opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(8));
    dl->opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    dl->opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16));
    return true;
}

bool Pcm2OpusEncoder::Start()
{
    return initializeEncoder();
}

int Pcm2OpusEncoder::Encode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    int inputLength = input.size();
    std::vector<unsigned char> pcmBuffer(bytesReadPerFrame * 2); // pcm音频数据缓冲区，适当大小
    std::vector<unsigned char> opusData(MAX_PACKET_SIZE);        // opus 数据缓冲区

    for (int index = 0; (inputLength == 0 && last) || index < inputLength;)
    {
        size_t bytesRead;
        if (index == 0)
        {
            size_t cachedBytes = internalBuffer.size();
            // 算上缓存，长度满足一帧
            if (inputLength + cachedBytes >= bytesReadPerFrame)
            {
                std::memcpy(pcmBuffer.data(), internalBuffer.data(), cachedBytes);
                internalBuffer.clear();
                bytesRead = bytesReadPerFrame - cachedBytes;
                std::memcpy(pcmBuffer.data() + cachedBytes, input.data(), bytesRead);
                index += bytesRead;
            }
            else // 算上缓存，长度不满足一帧
            {
                if (last)
                { // 最后一帧了，不缓存了，梭哈
                    std::memcpy(pcmBuffer.data(), internalBuffer.data(), cachedBytes);
                    internalBuffer.clear();
                    bytesRead = inputLength;
                    std::memcpy(pcmBuffer.data() + cachedBytes, input.data(), bytesRead);
                    index += bytesRead;
                    // 填充0
                    std::fill(pcmBuffer.begin() + bytesRead + cachedBytes, pcmBuffer.end(), 0);
                }
                else
                { // 还没结束，继续缓存，等待满足1帧
                    printf("continue cached %d\n", inputLength);
                    internalBuffer.insert(internalBuffer.end(), input.begin(), input.end());
                    break;
                }
            }
        }
        else
        {
            // 剩余长度满足一帧
            if (inputLength - index >= bytesReadPerFrame)
            {
                bytesRead = bytesReadPerFrame;
                std::memcpy(pcmBuffer.data(), input.data() + index, bytesRead);
                index += bytesRead;
            }
            else
            {
                if (last) // 最后一帧了，梭哈
                {
                    bytesRead = inputLength - index;
                    std::memcpy(pcmBuffer.data(), input.data() + index, bytesRead);
                    index += bytesRead;
                    // 填充0
                    std::fill(pcmBuffer.begin() + bytesRead, pcmBuffer.end(), 0);
                }
                else // 不够1帧，缓存起来
                {
                    printf("cached %d\n", inputLength - index);
                    internalBuffer.insert(internalBuffer.end(), input.begin() + index, input.end());
                    break;
                }
            }
        }
        // 编码
        printf("[fn:Pcm2OpusEncoder.Encode] pcmBuffer:%p[len=%d],frameSize:%d,opusData:%p\n", pcmBuffer.data(), pcmBuffer.size(), frameSize, opusData.data());
        int encodedBytes = dl->opus_encode(encoder, reinterpret_cast<const opus_int16 *>(pcmBuffer.data()), frameSize, opusData.data(), MAX_PACKET_SIZE);
        if (encodedBytes < 0)
        {
            std::cerr << "Encoding failed: " << dl->opus_strerror(encodedBytes) << std::endl;
            return -1;
        }

        // 截断为 2 字节（只保留低 16 位）, 按大端序分成字节
        uint16_t truncatedNum = encodedBytes & 0xFFFF;
        uint8_t bytesLen[2];
        bytesLen[0] = (truncatedNum >> 8) & 0xFF; // 高字节
        bytesLen[1] = truncatedNum & 0xFF;        // 低字节
        char *srcBytsLen = reinterpret_cast<char *>(bytesLen);
        output.insert(output.end(), srcBytsLen, srcBytsLen + 2);
        char *srcOpusData = reinterpret_cast<char *>(opusData.data());
        output.insert(output.end(), srcOpusData, srcOpusData + encodedBytes);

        if (inputLength == 0 && last)
        { // 防止无限循环
            break;
        }
    }
    return 0;
}