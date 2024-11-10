#include <iostream>
#include <fstream>
#include <vector>
#include <cstdlib>
#include <opus.h>

#define SAMPLE_RATE 24000 // 采样率
#define CHANNELS 1        // 单通道
#define FRAME_SIZE 480             // 20 ms for 24kHz
#define MAX_PACKET_SIZE (3 * 1276) // opus 最大数据包 1276

void pcm2opus(const std::string &inputFile, const std::string &outputFile, std::vector<int> &frameSizes);
void opus2pcm(const std::string &inputFile, const std::string &outputFile, const std::vector<int> &frameSizes);

int main()
{
    // OPUS
    std::vector<int> frameSizes; // 存储每帧编码后的字节长度
    pcm2opus("input.pcm", "output.opus", frameSizes);
    opus2pcm("output.opus", "output.pcm", frameSizes);
    return 0;
}

/***************** OPUS ********************/

void pcm2opus(const std::string &inputFile, const std::string &outputFile, std::vector<int> &frameSizes)
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

    opus_encoder_ctl(encoder, OPUS_SET_BITRATE(48000));
    opus_encoder_ctl(encoder, OPUS_SET_COMPLEXITY(8)); // 8    0~10
    opus_encoder_ctl(encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_MUSIC));

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

        // std::cout << "encoded numBytes: " << numBytes << std::endl;
        frameSizes.push_back(numBytes); // 保存编码后的每帧字节长度
        outFile.write(reinterpret_cast<char *>(opusData), numBytes);
    }

    // 清理资源
    inFile.close();
    outFile.close();
    opus_encoder_destroy(encoder);
}

void opus2pcm(const std::string &inputFile, const std::string &outputFile, const std::vector<int> &frameSizes)
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

    int numFrame = frameSizes.size();
    for (int index = 0; index < numFrame; index++)
    {
        inFile.read(reinterpret_cast<char *>(opusData), frameSizes[index]);
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
        // std::cout << "decoded frameSize: " << framesDecoded << std::endl;
        outFile.write(reinterpret_cast<char *>(pcm), framesDecoded * sizeof(int16_t));
    }

    inFile.close();
    outFile.close();
    opus_decoder_destroy(decoder);
}
