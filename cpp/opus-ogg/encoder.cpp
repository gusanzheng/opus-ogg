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

bool OpusOggEncoder::writeOpusHeader(std::ofstream &outputFile)
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
        outputFile.write(reinterpret_cast<const char *>(og.header), og.header_len);
        outputFile.write(reinterpret_cast<const char *>(og.body), og.body_len);
    }

    return true;
}

bool OpusOggEncoder::writeOpusComments(std::ofstream &outputFile)
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
        outputFile.write(reinterpret_cast<const char *>(og.header), og.header_len);
        outputFile.write(reinterpret_cast<const char *>(og.body), og.body_len);
    }

    return true;
}

bool OpusOggEncoder::encode(const std::string &inputFileName, const std::string &outputFileName)
{
    std::ifstream inputFile(inputFileName, std::ios::binary);
    if (!inputFile.is_open())
    {
        std::cerr << "Cannot open input file: " << inputFileName << std::endl;
        return false;
    }

    std::ofstream outputFile(outputFileName, std::ios::binary | std::ios::app);
    if (!outputFile.is_open())
    {
        std::cerr << "Cannot open output file: " << outputFileName << std::endl;
        return false;
    }

    // 初始化编码器和Ogg流
    if (!initializeEncoder() || !initializeOggStream())
    {
        return false;
    }

    // 写入头部信息
    if (!writeOpusHeader(outputFile) || !writeOpusComments(outputFile))
    {
        std::cerr << "Failed to write Opus headers" << std::endl;
        return false;
    }

    // 准备缓冲区
    std::vector<opus_int16> pcmBuffer(frameSize * channels);
    std::vector<unsigned char> opusData(MAX_PACKET_SIZE);
    int64_t granulepos = 0;
    int packetno = 2; // 从2开始，因为0和1已用于头信息

    // 计算每帧的实际granule增量
    // Opus 内部始终以48kHz工作，需要根据实际采样率进行调整
    const opus_int32 granule_increment = frameSize * (48000.0 / sampleRate);

    // 编码循环
    int index = 0;
    while (true)
    {
        // 读取PCM数据
        inputFile.read(reinterpret_cast<char *>(pcmBuffer.data()), frameSize * channels * sizeof(opus_int16));
        size_t samplesRead = inputFile.gcount() / (channels * sizeof(opus_int16));

        if (samplesRead == 0)
            break;

        // 如果读取的样本不足，用静音填充
        if (samplesRead < frameSize)
        {
            std::fill(pcmBuffer.begin() + samplesRead * channels, pcmBuffer.end(), 0);
        }

        // 编码
        int encodedBytes = opus_encode(encoder.get(), pcmBuffer.data(), frameSize, opusData.data(), MAX_PACKET_SIZE);

        if (encodedBytes < 0)
        {
            std::cerr << "Encoding failed: " << opus_strerror(encodedBytes) << std::endl;
            return false;
        }

        // 创建Ogg包
        ogg_packet op;
        op.packet = opusData.data();
        op.bytes = encodedBytes;
        op.b_o_s = 0;
        op.e_o_s = inputFile.eof() && samplesRead < frameSize ? 1 : 0;
        op.granulepos = granulepos;
        op.packetno = packetno++;
        printf("bytes %d, packetno %d, e_o_s %d, index: %d, samplesRead: %d, encodedBytes: %d\n", granulepos, packetno, op.e_o_s, index++, samplesRead, encodedBytes);
        // 写入包
        if (ogg_stream_packetin(&oggStreamState, &op) != 0)
        {
            std::cerr << "Error while writing packet to Ogg stream" << std::endl;
            return false;
        }

        // 写入页面
        ogg_page og;
        while (ogg_stream_pageout(&oggStreamState, &og) != 0)
        {
            outputFile.write(reinterpret_cast<const char *>(og.header), og.header_len);
            outputFile.write(reinterpret_cast<const char *>(og.body), og.body_len);
        }
        granulepos += granule_increment;
    }

    // 冲刷最后的数据
    ogg_page og;
    while (ogg_stream_flush(&oggStreamState, &og) != 0)
    {
        outputFile.write(reinterpret_cast<const char *>(og.header), og.header_len);
        outputFile.write(reinterpret_cast<const char *>(og.body), og.body_len);
    }

    printf("Encoding channels: %d, sampleRate:%d\n", channels, sampleRate);
    std::cout << "Encoding completed successfully" << std::endl;
    std::cout << "Total samples encoded: " << granulepos << std::endl;
    std::cout << "Audio duration: " << static_cast<double>(granulepos) / 48000.0 << " seconds" << std::endl;

    return true;
}
