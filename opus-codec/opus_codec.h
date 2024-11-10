#ifndef OPUS_CODEC_H
#define OPUS_CODEC_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <ctime>
#include <opus.h>

const int MAX_PACKET_SIZE = 3828;  // opus 最大数据包 1276

struct OpusEncoderDeleter
{
    void operator()(OpusEncoder *encoder)
    {
        if (encoder)
            opus_encoder_destroy(encoder);
    }
};

class Pcm2OpusEncoder
{
private:
    std::unique_ptr<OpusEncoder, OpusEncoderDeleter> encoder;
    int channels;
    int sampleRate;
    int frameSize;
    opus_int16 sampleSize;        // 每个采样点的大小
    size_t bytesReadPerFrame;     // 每帧读取的字节数
    std::vector<char> internalBuffer;

    bool initializeEncoder();   // 初始化编码器
    void end();

public:
    Pcm2OpusEncoder(int sampleRate = 24000, int channels = 1, int frameSize = 480)
        : channels(channels), sampleRate(sampleRate), frameSize(frameSize)
    {
        sampleSize = channels * sizeof(opus_int16);
        bytesReadPerFrame = frameSize * sampleSize;
        internalBuffer.reserve(bytesReadPerFrame * 2); // 适当大小空间
    }

    ~Pcm2OpusEncoder()
    {
        end();
    }

    bool Start();
    int Encode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

class Opus2PcmDecoder
{
};

class OpusCodec
{
private:
    std::unique_ptr<Pcm2OpusEncoder> encoder;
    std::unique_ptr<Opus2PcmDecoder> decoder;

public:
    OpusCodec(int sampleRate)
        : encoder(std::unique_ptr<Pcm2OpusEncoder>(new Pcm2OpusEncoder(sampleRate, 1, 480))),
          decoder(std::unique_ptr<Opus2PcmDecoder>(new Opus2PcmDecoder()))
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