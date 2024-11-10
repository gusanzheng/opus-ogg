#ifndef OPUS_CODEC_H
#define OPUS_CODEC_H

#include <iostream>
#include <vector>
#include <memory>
#include <cstring>
#include <dlfcn.h>
#include <opus.h>

typedef OpusEncoder *(*opus_encoder_create_func)(opus_int32 Fs, int channels, int application, int *error);
typedef const char *(*opus_strerror_func)(int error);
typedef int (*opus_encoder_ctl_func)(OpusEncoder *st, int request, ...);
typedef int (*opus_encode_func)(OpusEncoder *st, const opus_int16 *pcm, int frame_size, unsigned char *data, opus_int32 max_data_bytes);
typedef void (*opus_encoder_destroy_func)(OpusEncoder *st);

const int MAX_PACKET_SIZE = 3828; // opus 最大数据包 1276

class dlHandler
{
private:
    static dlHandler *dlInst;
    void *libHandle;
    dlHandler() = default;
    ~dlHandler() = default;

public:
    static dlHandler *GetInstance();
    int Open(const char *libName);
    void Close()
    {
        dlclose(libHandle);
        libHandle = nullptr;
    }

public:
    opus_encoder_create_func opus_encoder_create;
    opus_encoder_destroy_func opus_encoder_destroy;
    opus_encoder_ctl_func opus_encoder_ctl;
    opus_encode_func opus_encode;
    opus_strerror_func opus_strerror;
};

class Pcm2OpusEncoder
{
private:
    dlHandler *dl;
    OpusEncoder *encoder;
    int channels;
    int sampleRate;
    int frameSize;
    opus_int16 sampleSize;    // 每个采样点的大小
    size_t bytesReadPerFrame; // 每帧读取的字节数
    std::vector<char> internalBuffer;

    bool initializeEncoder(); // 初始化编码器

public:
    Pcm2OpusEncoder(int sampleRate = 24000, int channels = 1, int frameSize = 480)
        : dl(dlHandler::GetInstance()), channels(channels), sampleRate(sampleRate), frameSize(frameSize)
    {
        sampleSize = channels * sizeof(opus_int16);
        bytesReadPerFrame = frameSize * sampleSize;
        internalBuffer.reserve(bytesReadPerFrame * 2); // 适当大小空间
    }

    ~Pcm2OpusEncoder()
    {
        if (encoder)
            dl->opus_encoder_destroy(encoder);
        encoder = nullptr;
        dl = nullptr;
    };

    bool Start();
    int Encode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

class OpusCodec
{
private:
    std::unique_ptr<Pcm2OpusEncoder> encoder;

public:
    OpusCodec(int sampleRate)
        : encoder(std::unique_ptr<Pcm2OpusEncoder>(new Pcm2OpusEncoder(sampleRate, 1, 480)))
    {
    }
    ~OpusCodec() = default;

    bool Start()
    {
        return encoder->Start();
    }
    int Encode(const std::vector<char> &input, std::vector<char> &output, bool last);
    int Decode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

#endif // OPUS_CODEC_H