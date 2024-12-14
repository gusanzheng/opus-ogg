#ifndef OPUS_OGG_H
#define OPUS_OGG_H

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <memory>
#include <cstring>
#include <ctime>
#include <opus/opus.h>
#include <ogg/ogg.h>

const int MAX_FRAME_SIZE = 5760;  // 120ms@48kHz
const int MAX_PACKET_SIZE = 3828; // 3 * 1276

struct OpusHeader
{
    unsigned char version;
    unsigned char channels;
    uint16_t preSkip;
    uint32_t sampleRate;
    int16_t outputGain;
    unsigned char channelMappingFamily;
};

struct OpusEncoderDeleter
{
    void operator()(OpusEncoder *encoder)
    {
        if (encoder)
            opus_encoder_destroy(encoder);
    }
};

class OpusOggEncoder
{
private:
    std::unique_ptr<OpusEncoder, OpusEncoderDeleter> encoder;
    ogg_stream_state oggStreamState;
    bool streamInitialized;
    int channels;
    int sampleRate;
    int frameSize;
    std::vector<char> internalBuffer;

    // Ogg
    int packetno = 0;
    int64_t granulepos = 0;
    opus_int32 granule_increment; // 每帧的实际granule增量
    opus_int16 sampleSize;        // 每个采样点的大小
    size_t bytesReadPerFrame;     // 每帧读取的字节数

    bool initializeEncoder();   // 初始化编码器
    bool initializeOggStream(); // 初始化Ogg流
    bool writeOpusHeader(std::vector<char> &output);
    bool writeOpusComments(std::vector<char> &output);
    void end();

public:
    OpusOggEncoder(int sampleRate = 24000, int channels = 1, int frameSize = 480)
        : streamInitialized(false), channels(channels), sampleRate(sampleRate), frameSize(frameSize)
    {
        granule_increment = frameSize * (48000.0 / sampleRate);
        sampleSize = channels * sizeof(opus_int16);
        bytesReadPerFrame = frameSize * sampleSize;
        internalBuffer.reserve(bytesReadPerFrame * 2); // 适当大小空间
    }

    ~OpusOggEncoder()
    {
        end();
    }

    bool Start();
    int Encode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

struct OpusDecoderDeleter
{
    void operator()(OpusDecoder *decoder)
    {
        if (decoder)
            opus_decoder_destroy(decoder);
    }
};

class OpusOggDecoder
{
private:
    std::unique_ptr<OpusDecoder, OpusDecoderDeleter> decoder;
    ogg_sync_state oggSyncState;
    ogg_stream_state oggStreamState;
    bool streamInitialized;
    int channels;
    int sampleRate;

    // Ogg
    int step = 0;
    OpusHeader opusHeader;

    int readPage(const std::vector<char> &input, int *index, ogg_page &page);
    bool initializeDecoder();
    bool readOpusHeader(const std::vector<char> &input, int *index);
    bool parseOpusHeader(const unsigned char *data, size_t len, OpusHeader &header);
    bool skipOpusComments(ogg_packet &packet);
    void end();

public:
    OpusOggDecoder() : streamInitialized(false), channels(0), sampleRate(0)
    {
        ogg_sync_init(&oggSyncState);
    }

    ~OpusOggDecoder()
    {
        end();
    }

    bool Start();
    int Decode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

class OpusOggCodec
{
private:
    std::unique_ptr<OpusOggEncoder> encoder;
    std::unique_ptr<OpusOggDecoder> decoder;

public:
    OpusOggCodec(int sampleRate)
        : encoder(std::unique_ptr<OpusOggEncoder>(new OpusOggEncoder(sampleRate, 1, 480))),
          decoder(std::unique_ptr<OpusOggDecoder>(new OpusOggDecoder()))
    {
    }
    ~OpusOggCodec() = default;

    bool Start()
    {
        return encoder->Start();
    }
    int Encode(const std::vector<char> &input, std::vector<char> &output, bool last);
    int Decode(const std::vector<char> &input, std::vector<char> &output, bool last);
};

#endif // OPUS_OGG_H