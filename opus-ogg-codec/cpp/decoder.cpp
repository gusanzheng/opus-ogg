#include "opus_ogg.h"

bool OpusOggDecoder::readPage(std::ifstream &inputFile, ogg_page &page)
{
    while (ogg_sync_pageout(&oggSyncState, &page) != 1)
    {
        char *buffer = ogg_sync_buffer(&oggSyncState, 2048);
        inputFile.read(buffer, 2048);
        size_t bytesRead = inputFile.gcount();

        if (bytesRead == 0)
        {
            return false;
        }
        printf("bytesRead %d\n", bytesRead);
        ogg_sync_wrote(&oggSyncState, bytesRead);
    }
    return true;
}

bool OpusOggDecoder::initializeDecoder()
{
    int err;
    OpusDecoder *dec = opus_decoder_create(sampleRate, channels, &err);
    if (!dec)
    {
        std::cerr << "Failed to create Opus decoder: " << opus_strerror(err) << std::endl;
        return false;
    }
    decoder.reset(dec);
    return true;
}

bool OpusOggDecoder::parseOpusHeader(const unsigned char *data, size_t len, OpusHeader &header)
{
    if (len < 19)
        return false;

    // 验证 "OpusHead" 标识
    if (std::memcmp(data, "OpusHead", 8) != 0)
        return false;

    header.version = data[8];
    header.channels = data[9];
    header.preSkip = (data[10] | (data[11] << 8));
    header.sampleRate = (data[12] | (data[13] << 8) | (data[14] << 16) | (data[15] << 24));
    header.outputGain = (data[16] | (data[17] << 8));
    header.channelMappingFamily = data[18];

    return true;
}

bool OpusOggDecoder::skipOpusComments(ogg_packet &packet)
{
    // 验证 "OpusTags" 标识
    if (packet.bytes < 8 || std::memcmp(packet.packet, "OpusTags", 8) != 0)
    {
        return false;
    }
    return true; // 我们只需验证标识，不需要解析注释内容
}

bool OpusOggDecoder::decode(const std::string &inputFileName, const std::string &outputFileName)
{
    std::ifstream inputFile(inputFileName, std::ios::binary);
    if (!inputFile.is_open())
    {
        std::cerr << "Cannot open input file: " << inputFileName << std::endl;
        return false;
    }

    std::ofstream outputFile(outputFileName, std::ios::binary);
    if (!outputFile.is_open())
    {
        std::cerr << "Cannot open output file: " << outputFileName << std::endl;
        return false;
    }

    // 读取第一个页面，应该包含ID Header
    ogg_page page;
    if (!readPage(inputFile, page))
    {
        std::cerr << "Failed to read first page" << std::endl;
        return false;
    }

    // 初始化Ogg流
    if (ogg_stream_init(&oggStreamState, ogg_page_serialno(&page)) != 0)
    {
        std::cerr << "Failed to initialize Ogg stream" << std::endl;
        return false;
    }
    streamInitialized = true;

    // 提交第一个页面
    if (ogg_stream_pagein(&oggStreamState, &page) < 0)
    {
        std::cerr << "Error reading first page" << std::endl;
        return false;
    }

    // 读取ID Header包
    ogg_packet packet;
    if (ogg_stream_packetout(&oggStreamState, &packet) != 1)
    {
        std::cerr << "Error reading initial header packet" << std::endl;
        return false;
    }

    // 解析Opus头部
    OpusHeader opusHeader;
    if (!parseOpusHeader(packet.packet, packet.bytes, opusHeader))
    {
        std::cerr << "Failed to parse Opus header" << std::endl;
        return false;
    }

    channels = opusHeader.channels;
    sampleRate = opusHeader.sampleRate;

    // 初始化解码器
    if (!initializeDecoder())
    {
        return false;
    }

    // 读取Comment Header包
    printf("--- 1 ---\n");
    if (!readPage(inputFile, page) ||
        ogg_stream_pagein(&oggStreamState, &page) < 0 ||
        ogg_stream_packetout(&oggStreamState, &packet) != 1 ||
        !skipOpusComments(packet))
    {
        std::cerr << "Error reading comment header" << std::endl;
        return false;
    }

    // 处理预跳过采样
    std::vector<opus_int16> skipBuffer(opusHeader.preSkip * channels);
    if (opusHeader.preSkip > 0)
    {
        int ret = opus_decode(decoder.get(), nullptr, 0, skipBuffer.data(), opusHeader.preSkip, 0);
        if (ret < 0)
        {
            std::cerr << "Decoding opusHeader.preSkip error: " << opus_strerror(ret) << std::endl;
        }
    }

    // 开始解码音频数据
    std::vector<opus_int16> pcmBuffer(MAX_FRAME_SIZE * channels);
    int64_t totalSamples = 0;

    while (true)
    {
        // 读取一个音频包
        int result = ogg_stream_packetout(&oggStreamState, &packet);

        if (result == 0)
        {
            // 需要更多数据
            if (!readPage(inputFile, page))
            {
                break; // 文件结束
            }
            if (ogg_stream_pagein(&oggStreamState, &page) < 0)
            {
                std::cerr << "Error reading page" << std::endl;
                return false;
            }
            continue;
        }

        if (result < 0)
        {
            std::cerr << "Corrupt or missing data in bitstream" << std::endl;
            continue;
        }

        // 解码音频包
        int samplesDecoded = opus_decode(decoder.get(), packet.packet, packet.bytes, pcmBuffer.data(), MAX_FRAME_SIZE, 0);

        if (samplesDecoded < 0)
        {
            std::cerr << "Decoding error: " << opus_strerror(samplesDecoded) << std::endl;
            continue;
        }

        // 写入PCM数据
        outputFile.write(reinterpret_cast<const char *>(pcmBuffer.data()), samplesDecoded * channels * sizeof(opus_int16));

        totalSamples += samplesDecoded;
    }

    printf("Decoding channels: %d, sampleRate:%d\n", channels, sampleRate);
    std::cout << "Decoding completed successfully" << std::endl;
    std::cout << "Total decoded samples: " << totalSamples << std::endl;
    std::cout << "Audio duration: " << static_cast<double>(totalSamples) / sampleRate << " seconds" << std::endl;

    return true;
}