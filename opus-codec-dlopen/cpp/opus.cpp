#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <opus.h>

#define SAMPLE_RATE 24000          // 采样率
#define CHANNELS 1                 // 单通道
#define FRAME_SIZE 480             // 20 ms for 24kHz
#define MAX_PACKET_SIZE (3 * 1276) // opus 最大数据包 1276

void pcm2opus(const std::string &inputFile, const std::string &outputFile);
void opus2pcm(const std::string &inputFile, const std::string &outputFile);

int main()
{
    // OPUS
    pcm2opus("input/1.pcm", "output/1.opus");
    opus2pcm("output/1.opus", "output/1.pcm");
    return 0;
}

/***************** OPUS ********************/

void pcm2opus(const std::string &inputFile, const std::string &outputFile)
{
    std::ifstream inFile(inputFile.c_str(), std::ios::binary);
    if (!inFile)
    {
        std::cerr << "无法打开输入文件: " << inputFile << std::endl;
        return;
    }

    int err;
    OpusEncoder *encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_AUDIO, &err);
    if (!encoder)
    {
        std::cerr << "无法创建 Opus 编码器" << opus_strerror(err) << std::endl;
        return;
    }

    // 设置编码器参数
    opus_encoder_ctl(encoder, OPUS_SET_VBR(0)); // 0:CBR, 1:VBR
    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(48000));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(8)); // 0~10
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));
    opus_encoder_ctl(encoder, OPUS_SET_LSB_DEPTH(16));

    std::ofstream outFile(outputFile.c_str(), std::ios::binary);
    if (!outFile)
    {
        std::cerr << "无法打开输出文件: " << outputFile << std::endl;
        opus_encoder_destroy(encoder);
        return;
    }

    int16_t pcm[FRAME_SIZE];                 // 读取 PCM 数据并进行编码
    unsigned char opusData[MAX_PACKET_SIZE]; // Opus 编码后的数据缓冲区
    while (inFile.read(reinterpret_cast<char *>(pcm), sizeof(pcm)))
    {
        int frameSize = inFile.gcount() / sizeof(int16_t);

        // 如果读取的样本少于 FRAME_SIZE，填充剩余部分
        if (frameSize < FRAME_SIZE)
        {
            std::cout << "读取的样本少于 " << FRAME_SIZE << ", 填充剩余部分, frameSize: " << frameSize << std::endl;
            frameSize = frameSize < 0 ? 0 : frameSize;
            std::fill(pcm + frameSize, pcm + FRAME_SIZE, 0); // 用零填充
            frameSize = FRAME_SIZE;
        }

        int numBytes = opus_encode(encoder, pcm, frameSize, opusData, sizeof(opusData));
        if (numBytes < 0)
        {
            printf("编码失败,code=%d,msg=%s\n", numBytes, opus_strerror(numBytes));
            break;
        }

        // log
        printf("[fn:pcm2opus] read_frameSize:%d, encoded_numBytes:%d\n", frameSize, numBytes);

        // 截断为 2 字节（只保留低 16 位）, 按大端序分成字节
        uint16_t truncatedNum = numBytes & 0xFFFF;
        uint8_t bytes[2];
        bytes[0] = (truncatedNum >> 8) & 0xFF; // 高字节
        bytes[1] = truncatedNum & 0xFF;        // 低字节
        outFile.write(reinterpret_cast<char *>(bytes), 2);
        outFile.write(reinterpret_cast<char *>(opusData), numBytes);
    }

    // 清理资源
    inFile.close();
    outFile.close();
    opus_encoder_destroy(encoder);
}

void opus2pcm(const std::string &inputFile, const std::string &outputFile)
{
    std::ifstream inFile(inputFile.c_str(), std::ios::binary);
    if (!inFile)
    {
        std::cerr << "无法打开输入文件: " << inputFile << std::endl;
        return;
    }

    int err;
    OpusDecoder *decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &err);
    if (!decoder)
    {
        std::cerr << "无法创建 Opus 解码器" << opus_strerror(err) << std::endl;
        return;
    }

    std::ofstream outFile(outputFile.c_str(), std::ios::binary);
    if (!outFile)
    {
        std::cerr << "无法打开输出文件: " << outputFile << std::endl;
        opus_decoder_destroy(decoder);
        return;
    }

    unsigned char opusData[MAX_PACKET_SIZE]; // Opus 编码数据缓冲区
    int16_t pcm[FRAME_SIZE];                 // PCM 数据缓冲区
    int numBytes;

    while (true)
    {
        uint8_t bytes[2];
        inFile.read(reinterpret_cast<char *>(bytes), 2);
        numBytes = inFile.gcount();
        if (numBytes <= 0)
        {
            std::cerr << "读取数据失败或者数据已全部读取。" << std::endl;
            break;
        }
        if (inFile.gcount() != 2)
        {
            std::cerr << "读取opus数据帧长度失败了" << std::endl;
            break;
        }
        // 按大端序将字节组合成一个 16 位整数
        int numBytesPerFrame = (bytes[0] << 8) | bytes[1];
        

        inFile.read(reinterpret_cast<char *>(opusData), numBytesPerFrame);
        numBytes = inFile.gcount();
        if (numBytes <= 0)
        {
            std::cerr << "读取数据失败或者数据已全部读取。" << std::endl;
            break;
        }

        int framesDecoded = opus_decode(decoder, opusData, numBytes, pcm, FRAME_SIZE, 0);
        if (framesDecoded < 0)
        {
            printf("解码失败,code=%d,msg=%s\n", framesDecoded, opus_strerror(framesDecoded));
            break;
        }
        printf("[fn:opus2pcm] numBytesPerFrame:%d, read_numBytes:%d, decoded_frameSize:%d\n", numBytesPerFrame, numBytes, framesDecoded);
        outFile.write(reinterpret_cast<char *>(pcm), framesDecoded * sizeof(int16_t));
    }

    inFile.close();
    outFile.close();
    opus_decoder_destroy(decoder);
}
