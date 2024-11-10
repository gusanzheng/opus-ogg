#include "opus_ogg.h"
bool OpusOggEncoder::initializeEncoder()
{
    int err;
    OpusEncoder *enc = opus_encoder_create(sampleRate, channels, OPUS_APPLICATION_AUDIO, &err);
    if (!enc)
    {
        std::cerr << "Failed to create Opus encoder: " << opus_strerror(err) << std::endl;
        return false;
    }
    encoder.reset(enc);

    // 设置编码器参数
    opus_encoder_ctl(encoder.get(), OPUS_SET_VBR(0)); // 0:CBR, 1:VBR
    opus_encoder_ctl(encoder.get(), OPUS_SET_BITRATE(48000));
    opus_encoder_ctl(encoder.get(), OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(encoder.get(), OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(encoder.get(), OPUS_SET_LSB_DEPTH(16));
    return true;
}

bool OpusOggEncoder::initializeOggStream()
{
    std::srand(std::time(nullptr));
    if (ogg_stream_init(&oggStreamState, std::rand()) != 0)
    {
        std::cerr << "Failed to initialize Ogg stream" << std::endl;
        return false;
    }
    streamInitialized = true;
    return true;
}

bool OpusOggEncoder::writeOpusHeader(std::vector<char> &output)
{
    std::vector<unsigned char> header(19);

    // 填充OpusHead
    std::memcpy(header.data(), "OpusHead", 8);
    header[8] = 1; // 版本
    header[9] = channels;
    // 预跳过采样数 (16bit)
    header[10] = 0;
    header[11] = 0;
    // 采样率 (32bit)
    header[12] = sampleRate & 0xFF;
    header[13] = (sampleRate >> 8) & 0xFF;
    header[14] = (sampleRate >> 16) & 0xFF;
    header[15] = (sampleRate >> 24) & 0xFF;
    // 输出增益 (16bit)
    header[16] = 0;
    header[17] = 0;
    // 声道映射族
    header[18] = 0;

    ogg_packet op;
    op.packet = header.data();
    op.bytes = header.size();
    op.b_o_s = 1;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 0;

    if (ogg_stream_packetin(&oggStreamState, &op) != 0)
    {
        return false;
    }

    ogg_page og;
    while (ogg_stream_flush(&oggStreamState, &og) != 0)
    {
        const char *srcHead = reinterpret_cast<const char *>(og.header);
        output.insert(output.end(), srcHead, srcHead + og.header_len);
        const char *srcBody = reinterpret_cast<const char *>(og.body);
        output.insert(output.end(), srcBody, srcBody + og.body_len);
    }

    return true;
}

bool OpusOggEncoder::writeOpusComments(std::vector<char> &output)
{
    std::string vendor = "pcm2opusogg encoder";
    std::vector<std::string> comments; // 可以添加额外的注释

    // 计算总大小
    size_t size = 8 + 4 + vendor.length() + 4; // OpusTags + vendor length + vendor string + comment count
    for (const auto &comment : comments)
    {
        size += 4 + comment.length(); // 每条注释的长度字段和内容
    }

    std::vector<unsigned char> data(size);
    size_t pos = 0;

    // OpusTags 标识
    std::memcpy(data.data(), "OpusTags", 8);
    pos += 8;

    // Vendor String Length (小端序)
    uint32_t vendorLen = vendor.length();
    data[pos++] = vendorLen & 0xFF;
    data[pos++] = (vendorLen >> 8) & 0xFF;
    data[pos++] = (vendorLen >> 16) & 0xFF;
    data[pos++] = (vendorLen >> 24) & 0xFF;

    // Vendor String
    std::memcpy(data.data() + pos, vendor.data(), vendor.length());
    pos += vendor.length();

    // Comment List Length
    uint32_t commentCount = comments.size();
    data[pos++] = commentCount & 0xFF;
    data[pos++] = (commentCount >> 8) & 0xFF;
    data[pos++] = (commentCount >> 16) & 0xFF;
    data[pos++] = (commentCount >> 24) & 0xFF;

    // Comments
    for (const auto &comment : comments)
    {
        uint32_t len = comment.length();
        data[pos++] = len & 0xFF;
        data[pos++] = (len >> 8) & 0xFF;
        data[pos++] = (len >> 16) & 0xFF;
        data[pos++] = (len >> 24) & 0xFF;
        std::memcpy(data.data() + pos, comment.data(), comment.length());
        pos += comment.length();
    }

    ogg_packet op;
    op.packet = data.data();
    op.bytes = data.size();
    op.b_o_s = 0;
    op.e_o_s = 0;
    op.granulepos = 0;
    op.packetno = 1;

    if (ogg_stream_packetin(&oggStreamState, &op) != 0)
    {
        return false;
    }

    ogg_page og;
    while (ogg_stream_flush(&oggStreamState, &og) != 0)
    {
        const char *srcHead = reinterpret_cast<const char *>(og.header);
        output.insert(output.end(), srcHead, srcHead + og.header_len);
        const char *srcBody = reinterpret_cast<const char *>(og.body);
        output.insert(output.end(), srcBody, srcBody + og.body_len);
    }

    return true;
}

bool OpusOggEncoder::Start()
{
    return initializeEncoder() && initializeOggStream();
}

int OpusOggEncoder::Encode(const std::vector<char> &input, std::vector<char> &output, bool last)
{
    if (granulepos == 0)
    {
        // 写入头部信息
        if (!writeOpusHeader(output) || !writeOpusComments(output))
        {
            std::cerr << "Failed to write Opus headers" << std::endl;
            return -1;
        }
        packetno += 2;
    }

    int inputLength = input.size();                              // 本次输入数据的总长度
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
        int encodedBytes = opus_encode(encoder.get(), reinterpret_cast<const opus_int16 *>(pcmBuffer.data()), frameSize, opusData.data(), MAX_PACKET_SIZE);
        if (encodedBytes < 0)
        {
            std::cerr << "Encoding failed: " << opus_strerror(encodedBytes) << std::endl;
            return -1;
        }

        // 创建Ogg包
        ogg_packet op;
        op.packet = opusData.data();
        op.bytes = encodedBytes;
        op.b_o_s = 0;
        op.e_o_s = last && (index >= inputLength) ? 1 : 0;
        op.granulepos = granulepos;
        op.packetno = packetno++;

        printf("granulepos %d, packetno %d, e_o_s %d, index: %d, encodedBytes: %d\n", granulepos, packetno, op.e_o_s, index, encodedBytes);
        // 写入包
        if (ogg_stream_packetin(&oggStreamState, &op) != 0)
        {
            std::cerr << "Error while writing packet to Ogg stream" << std::endl;
            return -1;
        }

        // 写入页面
        ogg_page og;
        while (ogg_stream_pageout(&oggStreamState, &og) != 0)
        {
            const char *srcHead = reinterpret_cast<const char *>(og.header);
            output.insert(output.end(), srcHead, srcHead + og.header_len);
            const char *srcBody = reinterpret_cast<const char *>(og.body);
            output.insert(output.end(), srcBody, srcBody + og.body_len);
        }
        granulepos += granule_increment;

        if (inputLength == 0 && last)
        { // 防止无限循环
            break;
        }
    }

    if (last)
    {
        // 冲刷最后的数据
        ogg_page og;
        while (ogg_stream_flush(&oggStreamState, &og) != 0)
        {
            const char *srcHead = reinterpret_cast<const char *>(og.header);
            output.insert(output.end(), srcHead, srcHead + og.header_len);
            const char *srcBody = reinterpret_cast<const char *>(og.body);
            output.insert(output.end(), srcBody, srcBody + og.body_len);
        }
    }
    return 0;
}

void OpusOggEncoder::end()
{
    printf("Encoding completed! channels: %d, sampleRate:%d, audio duration: %f s\n", channels, sampleRate, static_cast<double>(granulepos) / 48000.0);
    if (streamInitialized)
    {
        ogg_stream_clear(&oggStreamState);
        streamInitialized = false;
    }
}
