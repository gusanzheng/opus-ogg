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

#define MAX_FRAME_SIZE 5760 // 120ms@48kHz
#define MAX_PACKET_SIZE (3 * 1276)

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

    bool initializeEncoder();
    bool initializeOggStream();
    bool writeOpusHeader(std::ofstream &outputFile);
    bool writeOpusComments(std::ofstream &outputFile);

    void cleanup()
    {
        if (streamInitialized)
        {
            ogg_stream_clear(&oggStreamState);
            streamInitialized = false;
        }
    }

public:
    OpusOggEncoder(int sampleRate = 24000, int channels = 1, int frameSize = 480)
        : streamInitialized(false), channels(channels), sampleRate(sampleRate), frameSize(frameSize)
    {
    }

    ~OpusOggEncoder()
    {
        cleanup();
    }

    bool encode(const std::string &inputFileName, const std::string &outputFileName);
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

    bool readPage(std::ifstream &inputFile, ogg_page &page);
    bool initializeDecoder();
    bool parseOpusHeader(const unsigned char *data, size_t len, OpusHeader &header);
    bool skipOpusComments(ogg_packet &packet);

    void cleanup()
    {
        if (streamInitialized)
        {
            ogg_stream_clear(&oggStreamState);
            streamInitialized = false;
        }
        ogg_sync_clear(&oggSyncState);
    }

public:
    OpusOggDecoder() : streamInitialized(false), channels(0), sampleRate(0)
    {
        ogg_sync_init(&oggSyncState);
    }

    ~OpusOggDecoder()
    {
        cleanup();
    }

    bool decode(const std::string &inputFileName, const std::string &outputFileName);
};

#endif // OPUS_OGG_H